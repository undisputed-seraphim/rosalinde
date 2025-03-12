#include "utf.hpp"
#include "utils.hpp"

#include <iostream>
#include <spanstream>
#include <streambuf>

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

#pragma pack(push, 1)
struct chunk_header {
	char magic[4];
	uint32_t table_size;
	uint32_t rows_offset;
	uint32_t string_offset;
	uint32_t data_offset;
	uint32_t table_name;
	uint16_t num_columns;
	uint16_t row_length;
	uint32_t num_rows;

	std::istream& operator>>(std::istream& i);
	friend std::istream& operator>>(std::istream&, chunk_header&);
};
#pragma pack(pop)

static std::string read_string(const uint32_t block_offset, std::istream& iss) {
	uint32_t offset = read_value_swap_endian<uint32_t>(iss);
	offset += block_offset;

	const size_t tell = iss.tellg();
	iss.seekg(offset, std::ios::beg);
	std::string value;
	std::getline(iss, value, char(0)).seekg(tell, std::ios::beg);
	return value;
}

static UTF::field::data_t read_data2(const uint32_t block_offset, std::istream& iss) {
	uint32_t offset = read_value_swap_endian<uint32_t>(iss);
	offset += block_offset;
	uint32_t length = read_value_swap_endian<uint32_t>(iss);
	return {offset, length};
}

std::istream& chunk_header::operator>>(std::istream& i) {
	i.read(magic, sizeof(magic));
	table_size = read_value_swap_endian<decltype(table_size)>(i);
	rows_offset = read_value_swap_endian<decltype(rows_offset)>(i);
	string_offset = read_value_swap_endian<decltype(string_offset)>(i);
	data_offset = read_value_swap_endian<decltype(data_offset)>(i);
	table_name = read_value_swap_endian<decltype(table_name)>(i);
	num_columns = read_value_swap_endian<decltype(num_columns)>(i);
	row_length = read_value_swap_endian<decltype(row_length)>(i);
	num_rows = read_value_swap_endian<decltype(num_rows)>(i);
	return i;
}

std::istream& operator>>(std::istream& i, chunk_header& chunk) { return chunk.operator>>(i); }

void UTF::field::push_back(const UTF::field::value_t& value) {
	const auto ftype = static_cast<type>(value.index());
	if (type_ == type::INVALID) {
		type_ = ftype;
	} else if (type_ != ftype) {
		throw std::runtime_error("Mismatched field type!");
	}
	values.push_back(value);
	valid = true;
}

UTF::field::value_t read_type(const chunk_header& header, UTF::field::type type_, std::istream& is) {
	switch (type_) {
	case UTF::field::type::UINT8:
		return (read_value_swap_endian<uint8_t>(is));
	case UTF::field::type::INT8:
		return (read_value_swap_endian<int8_t>(is));
	case UTF::field::type::UINT16:
		return (read_value_swap_endian<uint16_t>(is));
	case UTF::field::type::INT16:
		return (read_value_swap_endian<int16_t>(is));
	case UTF::field::type::UINT32:
		return (read_value_swap_endian<uint32_t>(is));
	case UTF::field::type::INT32:
		return (read_value_swap_endian<int32_t>(is));
	case UTF::field::type::UINT64:
		return (read_value_swap_endian<uint64_t>(is));
	case UTF::field::type::INT64:
		return (read_value_swap_endian<int64_t>(is));
	case UTF::field::type::FLOAT:
		return (read_value_swap_endian<float>(is));
	case UTF::field::type::DOUBLE:
		return (read_value_swap_endian<double>(is));
	case UTF::field::type::STRING:
		return (read_string(header.string_offset, is));
	case UTF::field::type::DATA_ARRAY:
		return (read_data2(header.data_offset, is));
	default:
		return (std::monostate{});
	}
}

constexpr const char ciphered_utf[] = {0x1F, 0x9E, 0xF3, 0xF5};

static bool is_ciphered_utf(std::istream& is) {
	char magic[4];
	is.read(magic, sizeof(magic));
	is.seekg(-sizeof(magic), std::ios::cur);
	return 0 == ::strncmp(magic, ciphered_utf, sizeof(magic));
}

UTF::UTF() {}

UTF::UTF(std::istream& is) {
	if (is_ciphered_utf(is)) {
		auto sb = utf_streambuf(is.rdbuf());
		is.set_rdbuf(&sb);
		parse(std::move(is));
	} else {
		parse(std::move(is));
	}
}
UTF::UTF(std::istream&& is) {
	if (is_ciphered_utf(is)) {
		auto sb = utf_streambuf(is.rdbuf());
		is.set_rdbuf(&sb);
		parse(std::move(is));
	} else {
		parse(std::move(is));
	}
}

UTF::UTF(std::vector<char> bytes) {
	if (bytes.empty()) {
		return;
	}

	if (::strncmp(bytes.data(), ciphered_utf, sizeof(ciphered_utf)) == 0) {
		for (uint32_t i = 0, j = 0x655F; i < bytes.size(); i++, j *= 0x4115) {
			bytes[i] ^= (j & 0xFF);
		}
	}
	constexpr const char _utf[] = {'@', 'U', 'T', 'F'};
	if (::strncmp(bytes.data(), _utf, sizeof(_utf)) != 0) {
		return;
	}
	parse(std::ispanstream(std::span(bytes)));
	/*
	auto iss = std::ispanstream(bytes, std::ios::binary);
	if (is_ciphered_utf(iss)) {
		parse(cpk_istream(iss));
	} else {
		constexpr const char _utf[] = {'@', 'U', 'T', 'F'};
		if (::strncmp(bytes.data(), _utf, sizeof(_utf)) != 0) {
			return;
		}
		parse(std::move(iss));
	}
	*/
}

UTF::iterator UTF::begin() { return _fields.begin(); }
UTF::iterator UTF::end() { return _fields.end(); }
UTF::const_iterator UTF::begin() const { return _fields.begin(); }
UTF::const_iterator UTF::end() const { return _fields.end(); }
UTF::const_iterator UTF::cbegin() const { return _fields.cbegin(); }
UTF::const_iterator UTF::cend() const { return _fields.cbegin(); }

UTF::size_type UTF::size() const noexcept { return _fields.size(); }
const UTF::field& UTF::at(size_type index) const { return _fields.at(index); }
const UTF::field& UTF::at(const std::string& name) const { return _fields.at(_lut.at(name)); }
bool UTF::contains(const std::string& name) const { return _lut.contains(name); }
UTF::const_iterator UTF::find(const std::string& key) const {
	return _lut.contains(key) ? _fields.begin() + _lut.at(key) : _fields.end();
}
bool UTF::empty() const noexcept { return _fields.empty(); }

void UTF::parse(std::istream&& is) {
	const uint64_t offset = is.tellg();
	chunk_header hdr;
	is >> hdr;

	hdr.rows_offset += 8 + offset;
	hdr.data_offset += 8 + offset;
	hdr.string_offset += 8 + offset;

	for (uint32_t i = 0; i < hdr.num_columns; ++i) {
		const uint8_t flags = read_value<uint8_t>(is);
		std::string fname = (flags & 0x10) ? read_string(hdr.string_offset, is) : "";
		const auto ftype = static_cast<field::type>(flags & 0xF);
		const bool has_default = ((flags & 0x20) != 0);
		const bool is_valid = ((flags & 0x40) != 0);
		field f(std::move(fname), ftype, is_valid);
		if (has_default) {
			f.push_back(read_type(hdr, f.type_, is));
			f.has_default = true;
		}
		if (_lut.try_emplace(f.name, _fields.size()).second) {
			_fields.push_back(std::move(f));
		}
	}
	for (uint32_t i = 0, j = 0; i < hdr.num_rows; i++, j += hdr.row_length) {
		for (auto& field : _fields) {
			if (!field.has_default && field.valid) {
				field.push_back(read_type(hdr, field.type_, is));
			}
		}
	}
}

void UTF::dump(const UTF& table) {
	for (const auto& [name, values, type, has_default, valid] : table) {
		if (values.empty() || !valid) {
			std::cout << name << " empty..." << std::endl;
			continue;
		}

		std::cout << name << " has " << values.size() << " value(s) ";
		for (const auto& value : values) {
			switch (type) {
			case field::type::UINT8:
			case field::type::INT8:
			case field::type::UINT16:
			case field::type::INT16:
			case field::type::UINT32:
			case field::type::INT32:
			case field::type::INT64: {
				const int64_t val = UTF::field::try_cast_to<int64_t>(value).value_or(-1);
				std::cout << val << ',';
				break;
			}
			case field::type::UINT64: {
				const uint64_t val = UTF::field::try_cast_to<uint64_t>(value).value_or(-1);
				std::cout << val << ',';
				break;
			}
			case field::type::FLOAT:
			case field::type::DOUBLE: {
				const double val = UTF::field::try_cast_to<double>(value).value_or(-1);
				std::cout << val << ',';
				break;
			}
			case field::type::STRING: {
				const auto val = std::get<std::string>(value);
				std::cout << val << ',';
				break;
			}
			case field::type::DATA_ARRAY: {
				const auto val = std::get<field::data_t>(value);
				std::cout << "data of length " << val.size << "; ";
				break;
			}
			default: {
				std::cout << "Got a monostate...";
			}
			}
		}
		std::cout << '\n';
	}
}
