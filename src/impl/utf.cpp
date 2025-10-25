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
	const uint32_t offset = read_value_swap_endian<uint32_t>(iss) + block_offset;
	const size_t tell = iss.tellg();
	iss.seekg(offset, std::ios::beg);
	std::string value;
	std::getline(iss, value, char(0)).seekg(tell, std::ios::beg);
	return value;
}

static UTF::field::data_t read_data(const uint32_t block_offset, std::istream& iss) {
	const uint32_t offset = read_value_swap_endian<uint32_t>(iss) + block_offset;
	const uint32_t length = read_value_swap_endian<uint32_t>(iss);
	return {offset, length};
}

UTF::field::field() = default;
UTF::field::field(type t, bool valid_)
	: type_(t)
	, valid(valid_) {}

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

void UTF::field::push_back(UTF::field::value_t&& value) {
	const auto ftype = static_cast<type>(value.index());
	if (type_ == type::INVALID) {
		type_ = ftype;
	} else if (type_ != ftype) {
		throw std::runtime_error("Mismatched field type!");
	}
	values.push_back(std::move(value));
	valid = true;
}

const UTF::field::value_t& UTF::field::at(size_t i) const { return values.at(i); }

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
	case UTF::field::type::DATA:
		return (read_data(header.data_offset, is));
	default:
		return (std::monostate{});
	}
}

bool UTF::decipher(std::vector<char>& bytes) {
	constexpr const char ciphered_utf[] = {0x1F, 0x9E, 0xF3, 0xF5};
	if (::strncmp(bytes.data(), ciphered_utf, sizeof(ciphered_utf)) == 0) {
		for (uint32_t i = 0, j = 0x655F; i < bytes.size(); i++, j *= 0x4115) {
			bytes[i] ^= (j & 0xFF);
		}
		return true;
	}
	return false;
}

UTF UTF::data_as_subtable(std::istream& is, const UTF::field::data_t& data) {
	is.seekg(data.offset, std::ios::beg);
	UTF utf;
	is >> utf;
	if (const auto size = static_cast<uint64_t>(is.tellg()) - data.offset; size != data.size) {
		std::cout << "Warning: subtable size mismatch: expected " << data.size << ", got " << size << '\n';
	}	
	return utf;
}

UTF::UTF() {}

UTF::const_iterator UTF::begin() const { return _fields.begin(); }
UTF::const_iterator UTF::end() const { return _fields.end(); }
UTF::const_iterator UTF::cbegin() const { return _fields.cbegin(); }
UTF::const_iterator UTF::cend() const { return _fields.cbegin(); }

UTF::size_type UTF::num_cols() const noexcept { return _fields.size(); }
bool UTF::contains_col(std::string_view name) const { return _fields.contains(name); }
UTF::const_iterator UTF::find_col(std::string_view name) const { return _fields.find(name); }
bool UTF::empty() const noexcept { return _fields.empty(); }

static bool validate_header(const chunk_header& h) noexcept {
	constexpr const char _utf[] = {'@', 'U', 'T', 'F'};
	return 0 == ::strncmp(h.magic, _utf, sizeof(_utf));
}

// Currently this can only handle deciphered streams.
std::istream& UTF::operator>>(std::istream& is) {
	const uint64_t offset = is.tellg();
	chunk_header hdr;
	is >> hdr;

	if (!validate_header(hdr)) {
		printf("Invalid header, returning early.\n");
		return is;
	}

	hdr.rows_offset += 8 + offset;
	hdr.data_offset += 8 + offset;
	hdr.string_offset += 8 + offset;

	std::vector<std::pair<std::string, field>> temp;
	for (uint32_t i = 0; i < hdr.num_columns; ++i) {
		const uint8_t flags = read_value<uint8_t>(is);
		std::string fname = (flags & 0x10) ? read_string(hdr.string_offset, is) : "";
		const auto ftype = static_cast<UTF::field::type>(flags & 0xF);
		const bool has_default = ((flags & 0x20) != 0);
		const bool is_valid = ((flags & 0x40) != 0);
		UTF::field f(ftype, is_valid);
		if (has_default) {
			f.push_back(read_type(hdr, f.type_, is));
			f.has_default = true;
		}
		temp.push_back({std::move(fname), std::move(f)});
	}
	for (uint32_t i = 0; i < hdr.num_rows; i++) {
		for (auto& [_, field] : temp) {
			if (!field.has_default && field.valid) {
				field.push_back(read_type(hdr, field.type_, is));
			}
		}
	}
	for (auto&& [name, field] : temp) {
		this->_fields.emplace(std::move(name), std::move(field));
	}

	return is;
}

struct visitor {
	std::ostream& os;
	template <std::floating_point F>
	void operator()(const F& v) const { os << std::fixed << static_cast<float>(v); }
	void operator()(const int8_t& v) const { os << static_cast<int>(v); }
	void operator()(const uint8_t& v) const { os << static_cast<unsigned>(v); }
	void operator()(const UTF::field::data_t& v) const { os << "(data of length " << v.size << ")"; }
	void operator()(const std::monostate& v) const { os << "(invalid variant!)"; }
	void operator()(auto&& v) const { os << v; }
};

std::ostream& UTF::operator<<(std::ostream& os) const {
	visitor v{os};
	for (const auto& [name, field] : _fields) {
		if (field.values.empty() || !field.valid) {
			os << name << " is empty...\n";
			continue;
		}
		os << name << " has " << field.values.size() << " value(s): ";
		int i = 0;
		for (const auto& value : field.values) {
			std::visit(v, value);
			os << ' ';
			if (++i > 10)
				break;
		}
		os << '\n';
	}
	return os;
}

std::istream& operator>>(std::istream& is, UTF& utf) { return utf.operator>>(is); }
std::ostream& operator<<(std::ostream& os, const UTF& utf) { return utf.operator<<(os); }