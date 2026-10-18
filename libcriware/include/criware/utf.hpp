#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// ============================================================================
// Compile-time column schema
// ============================================================================

namespace schema {

template <size_t N>
struct fixed_string {
	char data[N]{};
	constexpr fixed_string() = default;
	constexpr fixed_string(const char (&s)[N]) { std::copy_n(s, N, data); }
	constexpr operator std::string_view() const noexcept { return {data, N - 1}; }
	constexpr auto operator<=>(const fixed_string&) const = default;
};

template <fixed_string Name, typename T>
struct column_def {
	static constexpr auto name = Name;
	using type = T;
};

template <typename... Cols>
struct column_list {
	static constexpr size_t count = sizeof...(Cols);
	static constexpr auto names = std::array<std::string_view, count>{static_cast<std::string_view>(Cols::name)...};
	template <size_t I>
	using column_type = typename std::tuple_element_t<I, std::tuple<Cols...>>::type;
	template <template <typename> class Wrapper>
	using apply = std::tuple<Wrapper<typename Cols::type>...>;

	static constexpr size_t index_of(std::string_view n) noexcept {
		for (size_t i = 0; i < count; ++i)
			if (names[i] == n)
				return i;
		return static_cast<size_t>(-1);
	}
};

template <typename... Cols>
constexpr auto make(Cols...) -> column_list<Cols...> {
	return {};
}

template <fixed_string N, typename T>
constexpr auto col() -> column_def<N, T> {
	return {};
}

} // namespace schema

namespace utf {

enum class type : uint8_t {
	UINT8 = 0,
	INT8 = 1,
	UINT16 = 2,
	INT16 = 3,
	UINT32 = 4,
	INT32 = 5,
	UINT64 = 6,
	INT64 = 7,
	FLOAT = 8,
	DOUBLE = 9,
	STRING = 0xA,
	DATA = 0xB,
};

struct blob_ref {
	uint64_t offset = 0;
	uint32_t size = 0;
};

constexpr size_t type_size(type t) noexcept {
	switch (t) {
	case type::UINT8:
	case type::INT8:
		return 1;
	case type::UINT16:
	case type::INT16:
		return 2;
	case type::UINT32:
	case type::INT32:
	case type::FLOAT:
	case type::STRING:
		return 4;
	case type::UINT64:
	case type::INT64:
	case type::DOUBLE:
	case type::DATA:
		return 8;
	}
	return 0;
}

constexpr bool is_integer(type t) noexcept { return t <= type::INT64; }

class parse_error : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

using value = std::variant<uint64_t, int64_t, double, std::string_view, blob_ref, std::monostate>;

bool decipher(std::span<char> bytes);
inline bool decipher(std::vector<char>& bytes) { return decipher(std::span<char>(bytes.data(), bytes.size())); }

struct column_desc {
	std::string_view name;
	type type_ = type::UINT8;
	bool has_default = false;
	bool has_row_data = false;
	bool valid = false;
	size_t row_offset = 0;
	const uint8_t* default_ptr = nullptr;
	size_t default_size = 0;

	size_t value_count(size_t rows) const noexcept { return has_row_data ? rows : (has_default ? 1 : 0); }
};

class raw_table {
public:
	raw_table() = default;

	static raw_table parse(std::span<const uint8_t> data);

	size_t row_count() const noexcept { return _rows; }
	size_t column_count() const noexcept { return _columns.size(); }
	const column_desc& column(size_t i) const { return _columns.at(i); }
	const column_desc* find(std::string_view name) const noexcept;
	std::span<const uint8_t> source() const noexcept { return _data; }

	std::span<const uint8_t> blob(blob_ref ref) const;
	std::string_view string_at(uint64_t offset) const;

	const uint8_t* row_ptr(size_t row, const column_desc& column) const;
	std::string_view read_string(const uint8_t* p) const;
	blob_ref read_blob(const uint8_t* p) const;
	uint64_t read_uint(const uint8_t* p, type t) const;
	int64_t read_int(const uint8_t* p, type t) const;
	double read_double(const uint8_t* p, type t) const;

	value value_at(size_t row, size_t column) const;

private:
	std::span<const uint8_t> _data;
	size_t _rows = 0;
	size_t _row_length = 0;
	size_t _rows_offset = 0;
	size_t _string_offset = 0;
	size_t _data_offset = 0;
	std::vector<column_desc> _columns;
};

template <class T>
constexpr type schema_type() {
	if constexpr (std::is_same_v<T, uint8_t>)
		return type::UINT8;
	else if constexpr (std::is_same_v<T, int8_t>)
		return type::INT8;
	else if constexpr (std::is_same_v<T, uint16_t>)
		return type::UINT16;
	else if constexpr (std::is_same_v<T, int16_t>)
		return type::INT16;
	else if constexpr (std::is_same_v<T, uint32_t>)
		return type::UINT32;
	else if constexpr (std::is_same_v<T, int32_t>)
		return type::INT32;
	else if constexpr (std::is_same_v<T, uint64_t>)
		return type::UINT64;
	else if constexpr (std::is_same_v<T, int64_t>)
		return type::INT64;
	else if constexpr (std::is_same_v<T, float>)
		return type::FLOAT;
	else if constexpr (std::is_same_v<T, double>)
		return type::DOUBLE;
	else if constexpr (std::is_same_v<T, std::string_view>)
		return type::STRING;
	else if constexpr (std::is_same_v<T, blob_ref>)
		return type::DATA;
	else
		static_assert(sizeof(T) == 0, "unsupported UTF schema type");
}

template <class T>
constexpr bool schema_compatible(type actual) {
	constexpr type expected = schema_type<T>();
	if (actual == expected)
		return true;
	if constexpr (std::is_integral_v<T>)
		return is_integer(actual);
	else if constexpr (std::is_same_v<T, double>)
		return actual == type::FLOAT || actual == type::DOUBLE;
	else
		return false;
}

template <class T>
struct column_storage {
	std::vector<T> values;
	std::optional<T> default_value;
	bool has_row_data = false;
	bool present = false;
};

template <class T>
T read_value(const raw_table& raw, const column_desc& desc, const uint8_t* p);

template <class T>
class column_view {
public:
	column_view(const column_storage<T>* column, size_t rows) noexcept
		: _column(column)
		, _rows(rows) {}

	size_t size() const noexcept { return _rows; }
	bool empty() const noexcept { return _rows == 0; }
	bool has_row_data() const noexcept { return _column->has_row_data; }
	bool defaulted() const noexcept { return !_column->has_row_data && _column->default_value.has_value(); }
	bool present() const noexcept { return _column->present; }

	T operator[](size_t i) const {
		if (_column->has_row_data)
			return _column->values[i];
		if (_column->default_value)
			return *_column->default_value;
		throw std::out_of_range("UTF column has no value");
	}

	T at(size_t i) const {
		if (i >= _rows)
			throw std::out_of_range("UTF column index out of range");
		return (*this)[i];
	}

private:
	const column_storage<T>* _column;
	size_t _rows;
};

template <class Schema>
class table;

template <class Schema>
struct row_view {
	const table<Schema>* table_;
	size_t index;
};

template <class Schema>
class table {
public:
	using storage_t = typename Schema::template apply<column_storage>;
	using row_type = row_view<Schema>;

	table() = default;
	table(const table&) = delete;
	table& operator=(const table&) = delete;
	table(table&&) = default;
	table& operator=(table&&) = default;

	class row_iterator {
		const table* _table;
		size_t _row;

	public:
		using value_type = row_type;
		using difference_type = std::ptrdiff_t;

		row_iterator() noexcept
			: _table(nullptr)
			, _row(0) {}
		row_iterator(const table* t, size_t r) noexcept
			: _table(t)
			, _row(r) {}

		row_type operator*() const noexcept { return {_table, _row}; }
		row_iterator& operator++() noexcept {
			++_row;
			return *this;
		}
		row_iterator operator++(int) noexcept {
			auto c = *this;
			++_row;
			return c;
		}
		bool operator==(const row_iterator& o) const noexcept = default;
	};

	static table parse(std::span<const uint8_t> data) {
		table t;
		t._raw = raw_table::parse(data);
		t._rows = t._raw.row_count();
		t.load(std::make_index_sequence<Schema::count>{});
		return t;
	}

	size_t size() const noexcept { return _rows; }
	bool empty() const noexcept { return _rows == 0; }
	row_type operator[](size_t i) const noexcept { return {this, i}; }
	row_iterator begin() const noexcept { return {this, 0}; }
	row_iterator end() const noexcept { return {this, _rows}; }

	template <size_t I>
	auto column_at() const {
		using T = typename Schema::template column_type<I>;
		return column_view<T>(&std::get<I>(_columns), _rows);
	}

	template <schema::fixed_string Name>
	auto column() const {
		constexpr size_t I = Schema::index_of(Name);
		static_assert(I != static_cast<size_t>(-1), "Unknown UTF column");
		return column_at<I>();
	}

	std::span<const uint8_t> blob(blob_ref ref) const { return _raw.blob(ref); }
	std::span<const uint8_t> source() const noexcept { return _raw.source(); }
	const raw_table& raw() const noexcept { return _raw; }

private:
	raw_table _raw;
	storage_t _columns{};
	size_t _rows = 0;

	template <size_t... Is>
	void load(std::index_sequence<Is...>) {
		(load_one<Is>(), ...);
	}

	template <size_t I>
	void load_one() {
		using T = typename Schema::template column_type<I>;
		auto& column = std::get<I>(_columns);
		const auto* desc = _raw.find(Schema::names[I]);
		if (!desc)
			throw parse_error("UTF table is missing column");
		if (!schema_compatible<T>(desc->type_))
			throw parse_error("UTF column has an incompatible type");

		if (desc->has_default) {
			column.default_value = read_value<T>(_raw, *desc, desc->default_ptr);
			column.present = true;
		}
		if (desc->has_row_data) {
			column.values.resize(_rows);
			for (size_t row = 0; row < _rows; ++row)
				column.values[row] = read_value<T>(_raw, *desc, _raw.row_ptr(row, *desc));
			column.has_row_data = true;
			column.present = true;
		}
		if (!column.present)
			throw parse_error("UTF column has no row data or default");
	}
};

template <class T>
T read_value(const raw_table& raw, const column_desc& desc, const uint8_t* p) {
	if constexpr (std::is_same_v<T, std::string_view>) {
		return raw.read_string(p);
	} else if constexpr (std::is_same_v<T, blob_ref>) {
		return raw.read_blob(p);
	} else if constexpr (std::is_integral_v<T>) {
		if constexpr (std::is_signed_v<T>)
			return static_cast<T>(raw.read_int(p, desc.type_));
		else
			return static_cast<T>(raw.read_uint(p, desc.type_));
	} else if constexpr (std::is_floating_point_v<T>) {
		return static_cast<T>(raw.read_double(p, desc.type_));
	}
}

template <size_t I, class Schema>
auto get(const row_view<Schema>& row) {
	return row.table_->template column_at<I>()[row.index];
}

template <schema::fixed_string Name, class Schema>
auto get(const row_view<Schema>& row) {
	constexpr size_t I = Schema::index_of(Name);
	static_assert(I != static_cast<size_t>(-1), "Unknown UTF column");
	return row.table_->template column_at<I>()[row.index];
}

} // namespace utf

template <class Schema>
struct std::tuple_size<utf::row_view<Schema>> : std::integral_constant<size_t, Schema::count> {};

template <size_t I, class Schema>
struct std::tuple_element<I, utf::row_view<Schema>> {
	using type = typename Schema::template column_type<I>;
};
