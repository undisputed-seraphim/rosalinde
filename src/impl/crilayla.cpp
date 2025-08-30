#include <algorithm>
#include <cstring>
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

void layla_decompress(const header& h, const std::span<const char>& in, std::span<char> out) {
	uint32_t bit_pos = 0;
	auto read_n = [&](char nbits) -> uint16_t {
		uint16_t ans = 0;
		while (bit_pos / 8 < h.compressed_size && nbits--)
			ans <<= 1, ans |= ((in[bit_pos / 8] >> (7 - bit_pos % 8 /* LE */)) & 1), bit_pos++;
		return ans;
	};

	auto all_n_bits = [](auto value, char n) -> bool { return value == (1 << n) - 1; };

	const uint32_t data_size = h.decompress_size;
	uint32_t data_written = 0;
	while (data_written < data_size) {
		if (read_n(1) == 0) {
			uint8_t byte = read_n(8); // verbatim byte. into the back.
			out[data_size - 1 - data_written] = byte;
			data_written++;
		} else {
			auto offset = read_n(13) + 3; // backwards from the *back* of the output stream
			uint32_t ref_count = 3;		  // previous bytes referenced. 3 minimum
			constexpr uint8_t vle_n_bits[]{2, 3, 5, 8};
			for (int i = 0, n_bits = vle_n_bits[0];; i++, i = std::min(i, 3), n_bits = vle_n_bits[i]) {
				const uint16_t vle_length = read_n(n_bits);
				ref_count += vle_length;
				if (!all_n_bits(vle_length, n_bits)) {
					break;
				}
			}
			// fill in the referenced bytes from the *back* of the output buffer
			offset = data_size - 1 - data_written + offset;
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

int64_t decompress(std::span<char> in, std::vector<char>& out) {
	header h;
	std::memcpy((char*)&h, in.data(), sizeof(h));
	if (std::string_view(h.magic, sizeof(h.magic)) != CRILAYLA) {
		return -1;
	}

	static constexpr int meta = 256;
	out.resize(h.decompress_size + meta);

	// TODO We can optimize here.
	std::memcpy(out.data(), in.data() + in.size() - meta, meta);
	in = in.subspan(sizeof(h), in.size() - meta - sizeof(h));
	std::reverse(in.begin(), in.end());
	layla_decompress(h, in, std::span(out).subspan(meta));

	return h.decompress_size;
}
