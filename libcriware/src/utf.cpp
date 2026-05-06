#include <criware/utf.hpp>
#include <criware/byte_reader.hpp>
#include <criware/endian_swap.hpp>

#include <cstring>
#include <iostream>

template <typename T>
static T read_be(byte_reader& r) {
	T val = r.read<T>();
	if constexpr (sizeof(T) == 1) return val;
	return swap_endian(val);
}

bool UTF::decipher(std::vector<char>& bytes) {
	constexpr const unsigned char ciphered_utf[] = {0x1F, 0x9E, 0xF3, 0xF5};
	if (::strncmp(bytes.data(), reinterpret_cast<const char*>(ciphered_utf), sizeof(ciphered_utf)) == 0) {
		uint32_t j = 0x655F;
		for (uint32_t i = 0; i < bytes.size(); i++, j *= 0x4115) {
			bytes[i] ^= (j & 0xFF);
		}
		return true;
	}
	return false;
}

UTF::field::field() = default;
UTF::field::field(type t, bool valid_)
	: type_(t)
	, valid(valid_) {}

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

UTF UTF::parse(std::span<const uint8_t> data) {
	byte_reader r(data);

	char magic[4];
	std::memcpy(magic, r.cur, 4);
	r.skip(4);
	if (::strncmp(magic, "@UTF", 4) != 0) {
		return {};
	}

	const uint32_t table_size = read_be<uint32_t>(r);
	const uint32_t rows_offset = read_be<uint32_t>(r) + 8;
	const uint32_t string_offset = read_be<uint32_t>(r) + 8;
	const uint32_t data_offset = read_be<uint32_t>(r) + 8;
	(void)table_size;
	read_be<uint32_t>(r); // table_name
	const uint16_t num_columns = read_be<uint16_t>(r);
	read_be<uint16_t>(r); // row_length
	const uint32_t num_rows = read_be<uint32_t>(r);

	struct col_info {
		std::string name;
		field f;
	};
	std::vector<col_info> cols;
	cols.reserve(num_columns);

	for (uint16_t c = 0; c < num_columns; ++c) {
		const uint8_t flags = r.read<uint8_t>();
		std::string fname;
		if (flags & 0x10) {
			const uint32_t name_idx = read_be<uint32_t>(r);
			size_t saved = r.tell();
			r.seek(string_offset + name_idx);
			const uint8_t* start = r.cur;
			while (*r.cur != 0) r.skip(1);
			fname = std::string(reinterpret_cast<const char*>(start), r.cur - start);
			r.skip(1); // null
			r.seek(saved);
		}
		const auto ftype = static_cast<field::type>(flags & 0xF);
		const bool has_default = (flags & 0x20);
		const bool is_valid = (flags & 0x40);

		field f(ftype, is_valid);
		if (has_default) {
			switch (ftype) {
			case field::type::UINT8: f.push_back(r.read<uint8_t>()); break;
			case field::type::INT8: f.push_back(r.read<int8_t>()); break;
			case field::type::UINT16: f.push_back(read_be<uint16_t>(r)); break;
			case field::type::INT16: f.push_back(read_be<int16_t>(r)); break;
			case field::type::UINT32: f.push_back(read_be<uint32_t>(r)); break;
			case field::type::INT32: f.push_back(read_be<int32_t>(r)); break;
			case field::type::UINT64: f.push_back(read_be<uint64_t>(r)); break;
			case field::type::INT64: f.push_back(read_be<int64_t>(r)); break;
			case field::type::FLOAT: f.push_back(read_be<float>(r)); break;
			case field::type::DOUBLE: f.push_back(read_be<double>(r)); break;
			case field::type::STRING: {
				uint32_t idx = read_be<uint32_t>(r);
				size_t saved = r.tell();
				r.seek(string_offset + idx);
				const uint8_t* start = r.cur;
				while (*r.cur != 0) r.skip(1);
				f.push_back(std::string(reinterpret_cast<const char*>(start), r.cur - start));
				r.skip(1);
				r.seek(saved);
				break;
			}
			case field::type::DATA: {
				uint32_t off = read_be<uint32_t>(r);
				uint32_t len = read_be<uint32_t>(r);
				f.push_back(field::data_t{data_offset + off, len});
				break;
			}
			default: break;
			}
			f.has_default = true;
		}
		cols.push_back({std::move(fname), std::move(f)});
	}

	r.seek(rows_offset);
	for (uint32_t i = 0; i < num_rows; ++i) {
		for (auto& [_, f] : cols) {
			if (f.has_default || !f.valid) continue;
			switch (f.type_) {
			case field::type::UINT8: f.push_back(r.read<uint8_t>()); break;
			case field::type::INT8: f.push_back(r.read<int8_t>()); break;
			case field::type::UINT16: f.push_back(read_be<uint16_t>(r)); break;
			case field::type::INT16: f.push_back(read_be<int16_t>(r)); break;
			case field::type::UINT32: f.push_back(read_be<uint32_t>(r)); break;
			case field::type::INT32: f.push_back(read_be<int32_t>(r)); break;
			case field::type::UINT64: f.push_back(read_be<uint64_t>(r)); break;
			case field::type::INT64: f.push_back(read_be<int64_t>(r)); break;
			case field::type::FLOAT: f.push_back(read_be<float>(r)); break;
			case field::type::DOUBLE: f.push_back(read_be<double>(r)); break;
			case field::type::STRING: {
				uint32_t idx = read_be<uint32_t>(r);
				size_t saved = r.tell();
				r.seek(string_offset + idx);
				const uint8_t* start = r.cur;
				while (*r.cur != 0) r.skip(1);
				f.push_back(std::string(reinterpret_cast<const char*>(start), r.cur - start));
				r.skip(1);
				r.seek(saved);
				break;
			}
			case field::type::DATA: {
				uint32_t off = read_be<uint32_t>(r);
				uint32_t len = read_be<uint32_t>(r);
				f.push_back(field::data_t{data_offset + off, len});
				break;
			}
			default: f.push_back(std::monostate{}); break;
			}
		}
	}

	UTF utf;
	for (auto& [name, f] : cols) {
		utf._fields.emplace(std::move(name), std::move(f));
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

struct visitor {
	std::ostream& os;
	template <std::floating_point F>
	void operator()(const F& v) const {
		os << std::fixed << static_cast<float>(v);
	}
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

std::ostream& operator<<(std::ostream& os, const UTF& utf) { return utf.operator<<(os); }