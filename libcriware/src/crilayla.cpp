#include <algorithm>
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

// WARNING: Not sure if the compression algorithm is correct.
unsigned int layla_compress(uint8_t* dest, uint32_t* const destLen, const uint8_t* const src, const uint32_t srcLen) {
	uint32_t n = srcLen - 1, m = *destLen - 0x1, T = 0, d = 0, p, q, i, j, k;
	uint8_t* odest = dest;
	for (; n >= 0x100;) {
		j = n + 3 + 0x2000;
		if (j > srcLen)
			j = srcLen;
		for (i = n + 3, p = 0; i < j; i++) {
			for (k = 0; k <= n - 0x100; k++) {
				if (*(src + n - k) != *(src + i - k))
					break;
			}
			if (k > p) {
				q = i - n - 3;
				p = k;
			}
		}
		if (p < 3) {
			d = (d << 9) | (*(src + n--));
			T += 9;
		} else {
			d = (((d << 1) | 1) << 13) | q;
			T += 14;
			n -= p;
			if (p < 6) {
				d = (d << 2) | (p - 3);
				T += 2;
			} else if (p < 13) {
				d = (((d << 2) | 3) << 3) | (p - 6);
				T += 5;
			} else if (p < 44) {
				d = (((d << 5) | 0x1f) << 5) | (p - 13);
				T += 10;
			} else {
				d = ((d << 10) | 0x3ff);
				T += 10;
				p -= 44;
				for (;;) {
					for (; T >= 8;) {
						*(dest + m--) = (d >> (T - 8)) & 0xff;
						T -= 8;
						d = d & ((1 << T) - 1);
					}
					if (p < 255)
						break;
					d = (d << 8) | 0xff;
					T += 8;
					p = p - 0xff;
				}
				d = (d << 8) | p;
				T += 8;
			}
		}
		for (; T >= 8;) {
			*(dest + m--) = (d >> (T - 8)) & 0xff;
			T -= 8;
			d = d & ((1 << T) - 1);
		}
	}
	if (T != 0) {
		*(dest + m--) = d << (8 - T);
	}
	*(dest + m--) = 0;
	*(dest + m) = 0;
	for (;;) {
		if (((*destLen - m) & 3) == 0)
			break;
		*(dest + m--) = 0;
	}
	*destLen = *destLen - m;
	dest += m;
	uint32_t l[] = {0x4c495243, 0x414c5941, srcLen - 0x100, *destLen};
	for (j = 0; j < 4; j++) {
		for (i = 0; i < 4; i++) {
			*(odest + i + j * 4) = l[j] & 0xff;
			l[j] >>= 8;
		}
	}
	for (j = 0, odest += 0x10; j < *destLen; j++) {
		*(odest++) = *(dest + j);
	}
	for (j = 0; j < 0x100; j++) {
		*(odest++) = *(src + j);
	}
	*destLen += 0x110;
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
	dst.resize(src.size());
	uint32_t size = 0;
	return layla_compress((uint8_t*)dst.data(), &size, (const uint8_t*)src.data(), src.size());
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
