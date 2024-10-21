#pragma once

#include <iosfwd>
#include <optional>
#include <string>
#include <unordered_map>
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
		enum class type {
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
			DATA_ARRAY = 0xB,
			INVALID,
		};

		std::string name;
		std::vector<value_t> values;
		type type_{type::INVALID};
		bool has_default{false};
		bool valid{false};

		field() = default;
		field(const std::string& name_)
			: name(name_) {}
		field(const std::string& name_, type t, bool valid_)
			: name(name_)
			, type_(t)
			, valid(valid_) {}
		field(const std::string& name_, const std::vector<value_t>& values_)
			: name(name_)
			, values(values_)
			, type_(static_cast<type>(values_.front().index()))
			, valid(true) {}

		void push_back(const value_t&);

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
	};

	UTF();
	UTF(std::istream&);
	UTF(std::istream&&);
	UTF(std::vector<char>);
	UTF(const UTF&) = delete;
	UTF(UTF&&) = default;

	using size_type = std::vector<field>::size_type;
	using iterator = std::vector<field>::iterator;
	using const_iterator = std::vector<field>::const_iterator;
	iterator begin();
	iterator end();
	const_iterator begin() const;
	const_iterator end() const;
	const_iterator cbegin() const;
	const_iterator cend() const;

	size_type size() const noexcept;
	const field& at(size_type index) const;
	const field& at(const std::string& name) const;
	bool contains(const std::string& name) const;
	const_iterator find(const std::string& name) const;
	bool empty() const noexcept;

	static void dump(const UTF&);

private:
	void parse(std::istream&&);

	std::vector<field> _fields;
	std::unordered_map<std::string, uint64_t> _lut;
};