#include "utf.hpp"
#include "utils.hpp"

#include <iostream>
#include <spanstream>
#include <streambuf>

class cpk_streambuf : public std::streambuf {
	std::streambuf* src;
	char ch;
	uint32_t j;

public:
	cpk_streambuf(std::streambuf* sbuf)
		: src(sbuf)
		, j(0x655F) {}

	int underflow() {
		traits_type::int_type i = src->sbumpc();
		if (!traits_type::eq_int_type(i, traits_type::eof())) {
			i ^= (j & 0xFF);
			j += 0x4115;
			ch = traits_type::to_char_type(i);
			setg(&ch, &ch, &ch + 1); // make one read position available
		}
		return i;
	}
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

std::string read_string(const uint32_t block_offset, std::istream& iss) {
	uint32_t offset = read_value_swap_endian<uint32_t>(iss);
	offset += block_offset;

	const size_t tell = iss.tellg();
	iss.seekg(offset, std::ios::beg);
	std::string value;
	std::getline(iss, value, char(0)).seekg(tell, std::ios::beg);
	return value;
}

std::vector<char> read_data(const uint32_t block_offset, std::istream& iss) {
	uint32_t offset = read_value_swap_endian<uint32_t>(iss);
	offset += block_offset;
	uint32_t length = read_value_swap_endian<uint32_t>(iss);

	const size_t tell = iss.tellg();
	iss.seekg(offset, std::ios::beg);
	std::vector<char> value(length, '\0');
	iss.read(value.data(), length).seekg(tell, std::ios::beg);
	return value;
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
		return (read_data(header.data_offset, is));
	default:
		return (std::monostate{});
	}
}

constexpr const char ciphered_utf[] = {0x1F, 0x9E, 0xF3, 0xF5};

bool is_ciphered_utf(std::istream& is) {
	char magic[4];
	is.read(magic, sizeof(magic));
	is.seekg(-sizeof(magic), std::ios::cur);
	return 0 == ::strncmp(magic, ciphered_utf, sizeof(magic));
}

UTF::UTF() {}

UTF::UTF(std::istream& is) {
	auto* buf = is.rdbuf();
	if (is_ciphered_utf(is)) {
		// Assumes no cipher, for now
	} else {
		parse(std::move(is));
	}
}
UTF::UTF(std::istream&& is) {
	auto* buf = is.rdbuf();
	if (is_ciphered_utf(is)) {
		// Assumes no cipher, for now
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
	parse(std::ispanstream(bytes));
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
	chunk_header hdr;
	is >> hdr;

	hdr.rows_offset += 8;
	hdr.data_offset += 8;
	hdr.string_offset += 8;

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
				const auto val = std::get<std::vector<char>>(value);
				std::cout << "data of length " << val.size() << ';';
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
