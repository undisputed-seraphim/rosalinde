#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <istream>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#pragma pack(push, 1)
struct header {
	char magic[8];
	uint32_t decompress_size;
	uint32_t compressed_size;
};
#pragma pack(pop)

static constexpr std::string_view CRILAYLA = "CRILAYLA";

// Match finder structure adapted from AuroraLib.Compression's LzChainMatchFinder (MIT).
namespace {

constexpr uint32_t kWindow = 0x2000;
constexpr uint32_t kMinDistance = 3;
constexpr uint32_t kMinMatch = 3;
constexpr uint32_t kMaxMatch = 0xFFFF;
constexpr uint32_t kHashBits = 15;
constexpr uint32_t kHashSize = 1u << kHashBits;
constexpr uint32_t kMaxChain = 32;

uint32_t hash3(const uint8_t* p) {
	const uint32_t v =
		static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16);
	return (v * 2654435761u) >> (32 - kHashBits);
}

uint32_t match_length(const uint8_t* a, const uint8_t* b, uint32_t max) {
	uint32_t len = 0;
	while (len + 8 <= max) {
		uint64_t x, y;
		std::memcpy(&x, a + len, sizeof(x));
		std::memcpy(&y, b + len, sizeof(y));
		const uint64_t diff = x ^ y;
		if (diff != 0) {
			if constexpr (std::endian::native == std::endian::little)
				return len + (std::countr_zero(diff) >> 3);
			else
				return len + (std::countl_zero(diff) >> 3);
		}
		len += 8;
	}
	while (len < max && a[len] == b[len])
		++len;
	return len;
}

} // namespace

unsigned int layla_compress(uint8_t* dest, uint32_t* const destLen, const uint8_t* const src, const uint32_t srcLen) {
	if (srcLen < 0x100)
		return 0;
	const uint64_t data_len = static_cast<uint64_t>(srcLen) - 0x100;
	const uint64_t worst_case = data_len + data_len / 8 + 0x120;
	if (worst_case > *destLen || data_len > INT32_MAX)
		return 0;

	uint8_t* const odest = dest;
	const uint32_t tail_len = srcLen - 0x100;
	std::vector<uint8_t> reversed(tail_len);
	for (uint32_t i = 0; i < tail_len; ++i)
		reversed[i] = src[srcLen - 1 - i];

	uint32_t m = *destLen - 1;
	uint64_t bit_acc = 0;
	uint32_t bit_count = 0;
	auto put_bits = [&](uint32_t value, uint32_t count) {
		bit_acc = (bit_acc << count) | (value & ((1u << count) - 1));
		bit_count += count;
		while (bit_count >= 8) {
			dest[m--] = static_cast<uint8_t>((bit_acc >> (bit_count - 8)) & 0xFF);
			bit_count -= 8;
			bit_acc &= (1ull << bit_count) - 1;
		}
	};

	auto emit_literal = [&](uint8_t value) {
		put_bits(0, 1);
		put_bits(value, 8);
	};

	auto emit_match = [&](uint32_t distance, uint32_t length) {
		put_bits(1, 1);
		put_bits(distance - kMinDistance, 13);
		static constexpr uint32_t vle_bits[]{2, 3, 5, 8};
		static constexpr uint32_t vle_flags[]{0x3, 0x7, 0x1F, 0xFF};
		uint32_t remaining = length - kMinMatch;
		uint32_t level = 0;
		while (remaining >= vle_flags[level]) {
			put_bits(vle_flags[level], vle_bits[level]);
			remaining -= vle_flags[level];
			if (level < 3)
				++level;
		}
		put_bits(remaining, vle_bits[level]);
	};

	std::vector<int32_t> head(kHashSize, -1);
	std::vector<int32_t> previous(kWindow, -1);
	auto insert = [&](uint32_t pos) {
		if (pos + 2 >= tail_len)
			return;
		const uint32_t h = hash3(reversed.data() + pos);
		previous[pos & (kWindow - 1)] = head[h];
		head[h] = static_cast<int32_t>(pos);
	};

	uint32_t pos = 0;
	while (pos < tail_len) {
		uint32_t best_length = 0;
		uint32_t best_distance = 0;
		if (pos + kMinMatch <= tail_len) {
			const uint32_t h = hash3(reversed.data() + pos);
			int32_t cur = head[h];
			const uint32_t max_length = std::min<uint32_t>(kMaxMatch, tail_len - pos);
			uint32_t attempts = kMaxChain;
			while (cur >= 0 && attempts-- > 0) {
				const uint32_t candidate = static_cast<uint32_t>(cur);
				const uint32_t distance = pos - candidate;
				if (distance > kWindow)
					break;
				if (distance >= kMinDistance) {
					const uint32_t length =
						match_length(reversed.data() + candidate, reversed.data() + pos, max_length);
					if (length > best_length) {
						best_length = length;
						best_distance = distance;
						if (length == max_length)
							break;
					}
				}
				cur = previous[candidate & (kWindow - 1)];
			}
		}

		if (best_length >= kMinMatch) {
			emit_match(best_distance, best_length);
			const uint32_t end = pos + best_length;
			while (pos < end)
				insert(pos++);
		} else {
			emit_literal(reversed[pos]);
			insert(pos++);
		}
	}

	if (bit_count != 0)
		dest[m--] = static_cast<uint8_t>(bit_acc << (8 - bit_count));
	dest[m--] = 0;
	dest[m] = 0;
	while (((*destLen - m) & 3) != 0)
		dest[m--] = 0;

	const uint32_t payload = *destLen - m;
	const uint8_t* const payload_src = dest + m;
	uint32_t header_words[] = {0x4c495243, 0x414c5941, srcLen - 0x100, payload};
	for (uint32_t j = 0; j < 4; ++j) {
		uint32_t value = header_words[j];
		for (uint32_t i = 0; i < 4; ++i) {
			odest[i + j * 4] = static_cast<uint8_t>(value & 0xFF);
			value >>= 8;
		}
	}
	std::memmove(odest + 0x10, payload_src, payload);
	std::memcpy(odest + 0x10 + payload, src, 0x100);
	*destLen = payload + 0x110;
	return *destLen;
}

struct span_byte_source {
	std::span<const char> data;
	size_t pos;
	bool ok = true;

	explicit span_byte_source(std::span<const char> d)
		: data(d)
		, pos(d.size()) {}

	uint8_t next() {
		if (pos == 0) {
			ok = false;
			return 0;
		}
		return static_cast<uint8_t>(data[--pos]);
	}
};

struct stream_byte_source {
	std::istream& in;
	uint64_t start;
	uint64_t pos;
	std::unique_ptr<char[]> buf;
	size_t buf_size;
	size_t cur = 0;
	size_t end = 0;
	bool ok = true;

	stream_byte_source(std::istream& in_, uint64_t start_, uint64_t size)
		: in(in_)
		, start(start_)
		, pos(size)
		, buf()
		, buf_size(static_cast<size_t>(std::min<uint64_t>(size, 1ull << 20))) {
		if (buf_size != 0)
			buf = std::make_unique_for_overwrite<char[]>(buf_size);
	}

	uint8_t next() {
		if (cur == end && !refill()) {
			ok = false;
			return 0;
		}
		return static_cast<uint8_t>(buf[cur++]);
	}

	bool refill() {
		if (pos == 0 || buf_size == 0)
			return false;
		const size_t n = static_cast<size_t>(std::min<uint64_t>(pos, buf_size));
		in.seekg(static_cast<std::streamoff>(start + pos - n), std::ios::beg);
		if (!in) {
			ok = false;
			return false;
		}
		in.read(buf.get(), static_cast<std::streamsize>(n));
		if (!in) {
			ok = false;
			return false;
		}
		std::reverse(buf.get(), buf.get() + n);
		pos -= n;
		cur = 0;
		end = n;
		return true;
	}
};

template <typename Source>
struct bit_reader {
	Source& src;
	uint32_t acc = 0;
	uint8_t nbits = 0;

	explicit bit_reader(Source& s)
		: src(s) {}

	uint32_t read(uint8_t n) {
		while (nbits < n) {
			acc = (acc << 8) | src.next();
			nbits += 8;
		}
		nbits -= n;
		const uint32_t value = (acc >> nbits) & ((1u << n) - 1);
		acc &= (1u << nbits) - 1;
		return value;
	}
};

template <typename Source>
void layla_decompress(const header& h, Source& src, std::span<char> out) {
	bit_reader<Source> bits(src);
	auto all_n_bits = [](uint16_t value, uint8_t n) -> bool { return value == (1 << n) - 1; };

	const uint32_t data_size = h.decompress_size;
	for (uint32_t data_written = 0; data_written < data_size;) {
		if (bits.read(1) == 0) {
			const uint8_t byte = static_cast<uint8_t>(bits.read(8));
			out[data_size - 1 - data_written] = static_cast<char>(byte);
			data_written++;
		} else {
			auto offset = static_cast<uint32_t>(bits.read(13)) + 3;

			uint32_t ref_count = 3;
			constexpr uint8_t vle_n_bits[]{2, 3, 5, 8};
			for (uint16_t i = 0;; i = std::min(++i, (uint16_t)3)) {
				const uint8_t n_bits = vle_n_bits[i];
				const uint16_t vle_length = static_cast<uint16_t>(bits.read(n_bits));
				ref_count += vle_length;
				if (!all_n_bits(vle_length, n_bits)) {
					break;
				}
			}
			if (ref_count > data_size - data_written) {
				src.ok = false;
				return;
			}
			offset += data_size - 1 - data_written;
			if (offset >= data_size || offset + 1 < ref_count) {
				src.ok = false;
				return;
			}
			while (ref_count--) {
				out[data_size - 1 - data_written] = out[offset--];
				data_written++;
			}
		}
	}
}

int64_t compress(const std::vector<char>& src, std::vector<char>& dst) {
	if (src.size() < 0x100 || src.size() > UINT32_MAX)
		return -1;

	const size_t capacity = src.size() + src.size() / 8 + 0x200;
	if (capacity > UINT32_MAX)
		return -1;

	dst.resize(capacity);
	uint32_t size = static_cast<uint32_t>(dst.size());
	const auto ret =
		layla_compress((uint8_t*)dst.data(), &size, (const uint8_t*)src.data(), static_cast<uint32_t>(src.size()));
	if (ret == 0)
		return -1;
	dst.resize(ret);
	return static_cast<int64_t>(ret);
}

int64_t decompress(std::span<const char> in, std::span<char> out) {
	header h;
	if (in.size() < sizeof(h)) {
		return -1;
	}
	std::memcpy((char*)&h, in.data(), sizeof(h));
	if (std::string_view(h.magic, sizeof(h.magic)) != CRILAYLA) {
		return -1;
	}

	static constexpr size_t meta = 256;
	const uint64_t total = static_cast<uint64_t>(h.decompress_size) + meta;
	if (out.size() != total) {
		return -1;
	}
	if (in.size() < sizeof(h) + meta) {
		return -1;
	}
	if (static_cast<uint64_t>(h.compressed_size) + sizeof(h) + meta != in.size()) {
		return -1;
	}

	std::memcpy(out.data(), in.data() + in.size() - meta, meta);
	span_byte_source src(in.subspan(sizeof(h), h.compressed_size));
	layla_decompress(h, src, out.subspan(meta));
	if (!src.ok) {
		return -1;
	}

	return static_cast<int64_t>(total);
}

int64_t decompress(std::istream& in, uint64_t offset, uint64_t file_size, std::span<char> out) {
	header h;
	in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
	if (!in) {
		return -1;
	}
	in.read(reinterpret_cast<char*>(&h), sizeof(h));
	if (!in) {
		return -1;
	}
	if (std::string_view(h.magic, sizeof(h.magic)) != CRILAYLA) {
		return -1;
	}

	static constexpr size_t meta = 256;
	const uint64_t total = static_cast<uint64_t>(h.decompress_size) + meta;
	if (out.size() != total) {
		return -1;
	}
	if (static_cast<uint64_t>(h.compressed_size) + sizeof(h) + meta != file_size) {
		return -1;
	}

	const uint64_t payload_start = offset + sizeof(h);
	in.seekg(static_cast<std::streamoff>(payload_start + h.compressed_size), std::ios::beg);
	if (!in) {
		return -1;
	}
	in.read(out.data(), static_cast<std::streamsize>(meta));
	if (!in) {
		return -1;
	}

	stream_byte_source src(in, payload_start, h.compressed_size);
	layla_decompress(h, src, out.subspan(meta));
	if (!src.ok) {
		return -1;
	}

	return static_cast<int64_t>(total);
}
