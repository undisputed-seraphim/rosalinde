#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

class UTF {
public:
	class field {
	public:
		struct data_t {
			uint32_t offset;
			uint32_t size;
		};

		using value_t = std::variant<
			uint8_t,
			int8_t,
			uint16_t,
			int16_t,
			uint32_t,
			int32_t,
			uint64_t,
			int64_t,
			float,
			double,
			std::string,
			data_t,
			std::monostate>;
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
			// Pointer (32bit) types
			STRING = 0xA,
			DATA = 0xB,
			INVALID,
		};

		std::vector<value_t> values;
		type type_{type::INVALID};
		bool has_default{false};
		bool valid{false};

		field();
		field(type t, bool valid_);

		void push_back(value_t&&);

		template <typename Cast>
		static std::optional<Cast> try_cast_to(const value_t& value) {
			return std::visit(
				[](auto&& arg) -> std::optional<Cast> {
					using T = std::decay_t<decltype(arg)>;
					if constexpr (std::is_convertible_v<T, Cast>) {
						return static_cast<Cast>(arg);
					}
					return std::nullopt;
				},
				value);
		}

		template <typename T>
		const std::optional<T> cast_at(size_t i) const {
			return try_cast_to<T>(values[i]);
		}

		const value_t& at(size_t i) const;
	};

	UTF();
	UTF(const UTF&) = delete;
	UTF(UTF&&) = default;

	using storage_type = std::map<std::string, field, std::less<>>;
	using size_type = storage_type::size_type;
	using iterator = storage_type::iterator;
	using const_iterator = storage_type::const_iterator;
	const_iterator begin() const;
	const_iterator end() const;
	const_iterator cbegin() const;
	const_iterator cend() const;

	size_type num_cols() const noexcept;
	bool contains_col(std::string_view name) const;
	const_iterator find_col(std::string_view name) const;
	bool empty() const noexcept;

	std::istream& operator>>(std::istream& is);
	std::ostream& operator<<(std::ostream& os) const;
	friend std::istream& operator>>(std::istream&, UTF&);
	friend std::ostream& operator<<(std::ostream&, const UTF&);

	static bool decipher(std::vector<char>&);

	static UTF data_as_subtable(std::istream& is, const UTF::field::data_t& data);

protected:
	storage_type _fields;
};

// ============================================================================
// Compile-time column schema
// ============================================================================

#include <span>
#include <utility>

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
	using storage = std::tuple<std::vector<typename Cols::type>...>;
	static constexpr auto names = std::array<std::string_view, count>{
		static_cast<std::string_view>(Cols::name)...};
	template <size_t I> using column_type = typename std::tuple_element_t<I, std::tuple<Cols...>>::type;
	static constexpr size_t index_of(std::string_view n) noexcept {
		for (size_t i = 0; i < count; ++i) if (names[i] == n) return i;
		return static_cast<size_t>(-1);
	}
};

template <typename... Cols>
constexpr auto make(Cols...) -> column_list<Cols...> { return {}; }

template <fixed_string N, typename T>
constexpr auto col() -> column_def<N, T> { return {}; }

} // namespace schema

// ============================================================================
// row_view<Schema> — lightweight row proxy
// ============================================================================

template <typename Schema>
struct row_view {
	using storage_t = typename Schema::storage;
	const storage_t* columns;
	size_t index;
	template <size_t I> const auto& get() const { return std::get<I>(*columns)[index]; }
};

template <typename S> struct std::tuple_size<row_view<S>> : std::integral_constant<size_t, S::count> {};
template <size_t I, typename S> struct std::tuple_element<I, row_view<S>> { using type = typename S::template column_type<I>; };
template <size_t I, typename S> decltype(auto) get(const row_view<S>& rv) { return rv.template get<I>(); }

template <schema::fixed_string Name, typename S>
decltype(auto) get(const row_view<S>& rv) {
	static_assert(S::index_of(Name) != static_cast<size_t>(-1), "Unknown column name");
	constexpr size_t I = S::index_of(Name);
	return rv.template get<I>();
}

// ============================================================================
// table<Schema> — typed column-major table, parses via existing UTF
// ============================================================================

template <typename Schema>
class table {
public:
	using storage_t = typename Schema::storage;
	using row_type = row_view<Schema>;

	row_type operator[](size_t i) const { return {&_columns, i}; }

	class row_iterator {
		const table* _tbl; size_t _row;
	public:
		using value_type = row_type;
		using difference_type = std::ptrdiff_t;
		row_iterator() noexcept : _tbl(nullptr), _row(0) {}
		row_iterator(const table* t, size_t r) noexcept : _tbl(t), _row(r) {}
		row_type operator*() const noexcept { return {&_tbl->_columns, _row}; }
		row_iterator& operator++() noexcept { ++_row; return *this; }
		row_iterator operator++(int) noexcept { auto c = *this; ++_row; return c; }
		bool operator==(const row_iterator& o) const noexcept = default;
	};

	row_iterator begin() const { return {this, 0}; }
	row_iterator end()   const { return {this, _num_rows}; }

	template <size_t I> auto& column() { return std::get<I>(_columns); }
	template <size_t I> const auto& column() const { return std::get<I>(_columns); }

	template <schema::fixed_string Name>
	auto& column() {
		constexpr size_t I = Schema::index_of(Name);
		static_assert(I != static_cast<size_t>(-1), "Unknown column name");
		return column<I>();
	}
	template <schema::fixed_string Name>
	const auto& column() const {
		constexpr size_t I = Schema::index_of(Name);
		static_assert(I != static_cast<size_t>(-1), "Unknown column name");
		return column<I>();
	}

	template <schema::fixed_string... Names>
	auto rows() const {
		return std::views::zip(column<Names>()...);
	}

	template <schema::fixed_string... Names>
	size_t row_count() const noexcept {
		return (std::min)({column<Names>().size()...});
	}

	template <schema::fixed_string... Names>
	auto rows() {
		return std::views::zip(column<Names>()...);
	}

	template <schema::fixed_string... Names>
	size_t row_count() noexcept {
		return (std::min)({column<Names>().size()...});
	}

	static constexpr bool has_column(std::string_view name) noexcept { return Schema::index_of(name) != static_cast<size_t>(-1); }
	static constexpr size_t column_index(std::string_view name) noexcept { return Schema::index_of(name); }

	size_t size() const noexcept { return _num_rows; }
	bool empty() const noexcept { return _num_rows == 0; }

	friend std::istream& operator>>(std::istream& is, table& t) {
		UTF utf; is >> utf;
		auto first = utf.find_col(Schema::names[0]);
		t._num_rows = (first != utf.end()) ? first->second.values.size() : 0;
		t.populate(utf, std::make_index_sequence<Schema::count>{});
		return is;
	}

private:
	storage_t _columns{};
	size_t _num_rows = 0;

	template <size_t... Is>
	void populate(const UTF& utf, std::index_sequence<Is...>) { ((populate_one<Is>(utf)), ...); }

	template <size_t I>
	void populate_one(const UTF& utf) {
		using ST = typename Schema::template column_type<I>;
		auto it = utf.find_col(Schema::names[I]);
		if (it == utf.end()) return;
		auto& vec = std::get<I>(_columns);
		vec.reserve(it->second.values.size());
		for (const auto& val : it->second.values)
			std::visit([&](const auto& v) {
				if constexpr (std::is_convertible_v<std::decay_t<decltype(v)>, ST>) vec.push_back(static_cast<ST>(v));
			}, val);
	}
};
