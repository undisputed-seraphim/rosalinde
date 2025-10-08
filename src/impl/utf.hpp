#pragma once

#include <array>
#include <iosfwd>
#include <map>
#include <optional>
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

protected:
	storage_type _fields;
};

template <typename Traits>
class UTFTable : protected UTF {
	static_assert(std::size(Traits::Fields) > 0);
	static_assert(sizeof(Traits::Entry) > 1);

public:
	static constexpr auto Fields = Traits::Fields;
	static constexpr auto NumFields = std::size(Fields);
	using entry_tuple = Traits::Entry;
	using UTF::UTF;

	size_t size() const noexcept { return this->find_col(Fields[0])->second.values.size(); }

	entry_tuple at(const size_t i) const {
		static_assert(std::tuple_size_v<entry_tuple> > 0, "Tuple must have at least 1 element.");
		return variant_vector_to_tuple_impl<0>(i, entry_tuple{});
	}

	class iterator {
	private:
		friend UTFTable;
		const UTFTable* utf;
		size_t i;

		iterator(const UTFTable* utf_, size_t i_)
			: utf(utf_)
			, i(i_) {}
		iterator(const iterator&) = delete;

	public:
		iterator(iterator&&) = default;

		entry_tuple operator*() const noexcept { return utf->at(i); }
		entry_tuple operator->() const noexcept { return utf->at(i); }

		iterator& operator++() noexcept {
			++i;
			return *this;
		}

		iterator& operator--() noexcept {
			--i;
			return *this;
		}

		bool operator==(const iterator& other) const noexcept { return i == other.i; }

		std::strong_ordering operator<=>(const iterator& other) const noexcept { return i <=> other.i; }
	};
	using const_iterator = const iterator;
	const_iterator begin() const { return iterator(this, 0); }
	const_iterator end() const { return iterator(this, this->size()); }
	const_iterator cbegin() const { return iterator(this, 0); }
	const_iterator cend() const { return iterator(this, this->size()); }

	using UTF::operator<<;
	using UTF::operator>>;
	friend std::istream& operator>>(std::istream& is, UTFTable<Traits>& table) { return table.operator>>(is); }
	friend std::ostream& operator<<(std::ostream& os, const UTFTable<Traits>& table) { return table.operator<<(os); }

private:
	template <std::size_t I = 0>
	constexpr auto variant_vector_to_tuple_impl(const size_t i, entry_tuple&& tup) const {
		if constexpr (I < std::tuple_size_v<std::remove_reference_t<entry_tuple>>) {
			using T = std::decay_t<decltype(std::get<I>(tup))>;
			if (auto iter = this->find_col(Fields[I]); iter != UTF::end()) {
				if (auto optval = iter->second.cast_at<T>(i)) {
					// TODO: Probably not a good idea to leave it unset.
					// throw exception?
					std::get<I>(tup) = optval.value();
				}
			}
			return variant_vector_to_tuple_impl<I + 1>(i, std::forward<entry_tuple>(tup));
		} else {
			return std::forward<entry_tuple>(tup);
		}
	}
};
