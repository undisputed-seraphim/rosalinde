#include <chrono>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <vector>

constexpr const char ciphered_utf[] = {0x1F, 0x9E, 0xF3, 0xF5};

class utf_streambuf final : public std::streambuf {
	std::streambuf* _sbuf;
	uint32_t _j;
	static constexpr uint32_t j0 = 0x655F;
	static constexpr uint32_t t = 0x4115;

public:
	utf_streambuf(std::streambuf* sbuf)
		: _sbuf(sbuf)
		, _j(j0) {}
	utf_streambuf(const utf_streambuf&) = delete;
	utf_streambuf(utf_streambuf&&) noexcept = default;

	utf_streambuf& operator=(utf_streambuf&&) noexcept = default;

protected:
	pos_type seekoff(off_type off, std::ios::seekdir dir, std::ios::openmode which) override final {
		const auto oldpos = _sbuf->pubseekoff(0, std::ios::cur, which);
		const auto newpos = _sbuf->pubseekoff(off, dir, which);
		int64_t delta = newpos - oldpos;
		if (delta < 0) {
			_j = j0;
			delta = newpos;
		}
		for (int64_t i = 0; i < delta; ++i) {
			_j *= t;
		}
		return newpos;
	}

	pos_type seekpos(pos_type pos, std::ios::openmode which) override final {
		const auto oldpos = _sbuf->pubseekoff(0, std::ios::cur, which);
		const auto newpos = _sbuf->pubseekpos(pos, which);
		int64_t delta = newpos - oldpos;
		if (delta < 0) {
			_j = j0;
			delta = newpos;
		}
		for (int64_t i = 0; i < delta; ++i) {
			_j *= t;
		}
		return newpos;
	}

	int_type underflow() override final {
		int_type i = _sbuf->sgetc();
		if (!traits_type::eq_int_type(i, traits_type::eof())) {
			i ^= (_j & 0xFF);
		}
		return i;
	}

	int_type uflow() override final {
		int_type i = _sbuf->sbumpc();
		if (!traits_type::eq_int_type(i, traits_type::eof())) {
			i ^= (_j & 0xFF);
			_j *= t;
		}
		return i;
	}

	std::streamsize xsgetn(char_type* s, std::streamsize count) override final {
		const auto get = _sbuf->sgetn(s, count);
		for (int64_t i = 0; i < get; ++i) {
			s[i] ^= (_j & 0xFF);
			_j *= t;
		}
		return get;
	}

	std::streambuf* setbuf(char_type* buffer, std::streamsize count) override final {
		return _sbuf->pubsetbuf(buffer, count);
	}
	void imbue(const std::locale& l) override final { _sbuf->pubimbue(l); }
	std::streamsize showmanyc() override final { return _sbuf->in_avail(); }
};

class cpk_istream final : public std::istream {
	utf_streambuf _sbuf;

public:
	cpk_istream(std::istream& is)
		: std::istream(&_sbuf)
		, _sbuf(is.rdbuf()) {}
	cpk_istream(const cpk_istream&) = delete;
	cpk_istream(cpk_istream&&) noexcept = default;

	cpk_istream& operator=(cpk_istream&&) noexcept = default;
};

void naive(std::vector<char>& buffer) {
	for (uint32_t i = 0, j = 0x655F; i < buffer.size(); i++, j *= 0x4115) {
		buffer[i] ^= (j & 0xFF);
	}
}

bool verify_before(const std::vector<char>& buffer) {
	const bool correct = (::strncmp(buffer.data(), ciphered_utf, sizeof(ciphered_utf)) == 0);
	if (!correct) {
		std::cout << "Not a ciphered UTF file!\n";
	}
	return correct;
}

bool verify_after(const std::vector<char>& buffer) {
	static constexpr const char _utf[] = {'@', 'U', 'T', 'F'};
	const bool correct = (::strncmp(buffer.data(), _utf, sizeof(_utf)) == 0);
	if (correct) {
        std::cout << "Decipher successful!\n";
	} else {
        std::cout << "Unsuccessful deciphering!\n";
		for (int i = 0; i < 16; ++i) {
			if (std::isprint(static_cast<unsigned char>(buffer[i]))) {
				printf("%c ", buffer[i]);
			} else {
				printf("%02X ", static_cast<unsigned char>(buffer[i]));
			}
		}
		printf("\n");
    }
	return correct;
}

template <typename Fn>
void time(Fn&& fn) {
	const auto start = std::chrono::high_resolution_clock::now();
    fn();
	const auto delta = std::chrono::high_resolution_clock::now() - start;
	std::cout << "Function took " << delta << std::endl;
}

int main(int argc, char* argv[]) {
    auto ifs = std::ifstream(argv[1], std::ios::ate | std::ios::binary);
    const auto size = ifs.tellg();
    ifs.seekg(0);
    std::vector<char> buffer(size, '\0');
    ifs.read(buffer.data(), size);
	if (!verify_before(buffer)) {
		return 1;
	}

	time([&buffer](){
		naive(buffer);
	});

	verify_after(buffer);
}