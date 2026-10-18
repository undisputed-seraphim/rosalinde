#include <criware/utf.hpp>

#include <bit>
#include <cstring>
#include <limits>

namespace utf {
namespace {

uint16_t read_be16(const uint8_t* p) noexcept {
	return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

uint32_t read_be32(const uint8_t* p) noexcept {
	return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
		   (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

uint64_t read_be64(const uint8_t* p) noexcept { return (static_cast<uint64_t>(read_be32(p)) << 32) | read_be32(p + 4); }

} // namespace

bool decipher(std::span<char> bytes) {
	constexpr const unsigned char ciphered_utf[] = {0x1F, 0x9E, 0xF3, 0xF5};
	if (bytes.size() < sizeof(ciphered_utf) || std::memcmp(bytes.data(), ciphered_utf, sizeof(ciphered_utf)) != 0)
		return false;

	uint32_t j = 0x655F;
	for (size_t i = 0; i < bytes.size(); ++i, j *= 0x4115)
		bytes[i] = static_cast<char>(static_cast<uint8_t>(bytes[i]) ^ (j & 0xFF));
	return true;
}

raw_table raw_table::parse(std::span<const uint8_t> data) {
	raw_table table;
	if (data.size() < 32 || std::memcmp(data.data(), "@UTF", 4) != 0)
		throw parse_error("not a UTF table");
	if (data.size() < 8)
		throw parse_error("truncated UTF header");

	const uint32_t table_size = read_be32(data.data() + 4);
	const size_t rows_offset = static_cast<size_t>(read_be16(data.data() + 10)) + 8;
	const size_t string_offset = static_cast<size_t>(read_be32(data.data() + 12)) + 8;
	const size_t data_offset = static_cast<size_t>(read_be32(data.data() + 16)) + 8;
	const uint16_t num_columns = read_be16(data.data() + 24);
	const size_t row_length = read_be16(data.data() + 26);
	const size_t num_rows = read_be32(data.data() + 28);

	if (table_size > data.size() - 8)
		throw parse_error("UTF table is larger than its buffer");
	if (rows_offset > data.size() || string_offset > data.size() || data_offset > data.size())
		throw parse_error("UTF table offset is out of range");
	if (rows_offset < 32 || num_columns > rows_offset - 32)
		throw parse_error("UTF column definitions exceed the row area");
	if (row_length != 0 && num_rows > (data.size() - rows_offset) / row_length)
		throw parse_error("UTF rows exceed the table buffer");

	table._data = data;
	table._rows = num_rows;
	table._row_length = row_length;
	table._rows_offset = rows_offset;
	table._string_offset = string_offset;
	table._data_offset = data_offset;
	table._columns.reserve(num_columns);

	size_t cursor = 32;
	size_t row_bytes = 0;
	for (uint16_t i = 0; i < num_columns; ++i) {
		if (cursor >= rows_offset)
			throw parse_error("truncated UTF column definition");

		const uint8_t flags = data[cursor++];
		const type column_type = static_cast<type>(flags & 0xF);
		if (type_size(column_type) == 0)
			throw parse_error("invalid UTF column type");

		column_desc desc;
		desc.type_ = column_type;
		desc.has_default = (flags & 0x20) != 0;
		desc.valid = (flags & 0x40) != 0;
		desc.has_row_data = desc.valid && !desc.has_default;

		if (flags & 0x10) {
			if (cursor + 4 > rows_offset)
				throw parse_error("truncated UTF column name");
			const uint32_t name_index = read_be32(data.data() + cursor);
			cursor += 4;
			desc.name = table.string_at(string_offset + name_index);
		}

		desc.row_offset = row_bytes;
		if (desc.has_row_data)
			row_bytes += type_size(column_type);

		if (desc.has_default) {
			const size_t size = type_size(column_type);
			if (cursor + size > rows_offset)
				throw parse_error("truncated UTF column default");
			desc.default_ptr = data.data() + cursor;
			desc.default_size = size;
			cursor += size;
		}

		table._columns.push_back(desc);
	}

	if (row_bytes > row_length)
		throw parse_error("UTF row length is smaller than its columns");
	return table;
}

const column_desc* raw_table::find(std::string_view name) const noexcept {
	for (const auto& column : _columns)
		if (column.name == name)
			return &column;
	return nullptr;
}

const uint8_t* raw_table::row_ptr(size_t row, const column_desc& column) const {
	if (!column.has_row_data || row >= _rows)
		throw parse_error("UTF column has no row data");
	return _data.data() + _rows_offset + row * _row_length + column.row_offset;
}

std::string_view raw_table::string_at(uint64_t offset) const {
	if (offset >= _data.size())
		throw parse_error("UTF string offset is out of range");
	const char* start = reinterpret_cast<const char*>(_data.data() + offset);
	const void* end = std::memchr(start, 0, _data.size() - static_cast<size_t>(offset));
	if (!end)
		throw parse_error("UTF string is not terminated");
	return {start, static_cast<size_t>(static_cast<const char*>(end) - start)};
}

std::span<const uint8_t> raw_table::blob(blob_ref ref) const {
	if (ref.offset > _data.size() || ref.size > _data.size() - ref.offset)
		throw parse_error("UTF data reference is out of range");
	return _data.subspan(static_cast<size_t>(ref.offset), ref.size);
}

std::string_view raw_table::read_string(const uint8_t* p) const { return string_at(_string_offset + read_be32(p)); }

blob_ref raw_table::read_blob(const uint8_t* p) const { return {_data_offset + read_be32(p), read_be32(p + 4)}; }

uint64_t raw_table::read_uint(const uint8_t* p, type t) const {
	switch (t) {
	case type::UINT8:
		return p[0];
	case type::UINT16:
		return read_be16(p);
	case type::UINT32:
		return read_be32(p);
	case type::UINT64:
		return read_be64(p);
	case type::INT8:
		return static_cast<int8_t>(p[0]);
	case type::INT16:
		return static_cast<int16_t>(read_be16(p));
	case type::INT32:
		return static_cast<int32_t>(read_be32(p));
	case type::INT64:
		return static_cast<int64_t>(read_be64(p));
	default:
		throw parse_error("UTF column is not an integer");
	}
}

int64_t raw_table::read_int(const uint8_t* p, type t) const {
	switch (t) {
	case type::INT8:
		return static_cast<int8_t>(p[0]);
	case type::INT16:
		return static_cast<int16_t>(read_be16(p));
	case type::INT32:
		return static_cast<int32_t>(read_be32(p));
	case type::INT64:
		return static_cast<int64_t>(read_be64(p));
	default:
		return static_cast<int64_t>(read_uint(p, t));
	}
}

double raw_table::read_double(const uint8_t* p, type t) const {
	switch (t) {
	case type::FLOAT:
		return std::bit_cast<float>(read_be32(p));
	case type::DOUBLE:
		return std::bit_cast<double>(read_be64(p));
	default:
		return static_cast<double>(read_int(p, t));
	}
}

value raw_table::value_at(size_t row, size_t column_index) const {
	const auto& column = _columns.at(column_index);
	const uint8_t* p = nullptr;
	if (column.has_row_data)
		p = row_ptr(row, column);
	else if (column.has_default)
		p = column.default_ptr;
	else
		return std::monostate{};

	switch (column.type_) {
	case type::UINT8:
	case type::UINT16:
	case type::UINT32:
	case type::UINT64:
		return read_uint(p, column.type_);
	case type::INT8:
	case type::INT16:
	case type::INT32:
	case type::INT64:
		return read_int(p, column.type_);
	case type::FLOAT:
	case type::DOUBLE:
		return read_double(p, column.type_);
	case type::STRING:
		return read_string(p);
	case type::DATA:
		return read_blob(p);
	}
	return std::monostate{};
}

} // namespace utf
