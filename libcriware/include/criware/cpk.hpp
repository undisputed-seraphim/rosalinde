#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <criware/utf.hpp>

// ============================================================================
// CPKTable — typed access to the asset table-of-contents
// ============================================================================

using CPKSchema = decltype(schema::make(
	schema::col<"DirName",     std::string>(),
	schema::col<"FileName",    std::string>(),
	schema::col<"FileSize",    uint64_t>(),
	schema::col<"ExtractSize", uint64_t>(),
	schema::col<"FileOffset",  uint64_t>(),
	schema::col<"ID",          uint64_t>()));

class CPKTable {
public:
	using table_type = table<CPKSchema>;
	using iterator = table_type::row_iterator;
	using row_type = table_type::row_type;

	CPKTable(std::filesystem::path, uint64_t offset);

	void parse_toc(std::span<const uint8_t> data);

	void extract(const row_type& entry, std::vector<char>& out) const;
	void extract(std::string_view dir, std::string_view name, std::vector<char>& out) const;
	iterator find_file(std::string_view dir, std::string_view file) const;
	size_t size() const noexcept { return _table.size(); }

	iterator begin() const { return _table.begin(); }
	iterator end() const { return _table.end(); }
	row_type operator[](size_t i) const { return _table[i]; }

	template <size_t I> auto& column() { return _table.column<I>(); }
	template <size_t I> const auto& column() const { return _table.column<I>(); }
	template <schema::fixed_string Name> auto& column() { return _table.column<Name>(); }
	template <schema::fixed_string Name> const auto& column() const { return _table.column<Name>(); }

	template <schema::fixed_string... Names>
	auto rows() const { return _table.rows<Names...>(); }

	template <schema::fixed_string... Names>
	auto rows() { return _table.rows<Names...>(); }

	template <schema::fixed_string... Names>
	size_t row_count() const noexcept { return _table.row_count<Names...>(); }

	static constexpr bool has_column(std::string_view name) noexcept { return table_type::has_column(name); }
	static constexpr size_t column_index(std::string_view name) noexcept { return table_type::column_index(name); }

private:
	table<CPKSchema> _table;
	std::filesystem::path _path;
	uint64_t _offset;
	mutable std::vector<char> _buffer = {};
};

// ============================================================================
// TopLevelCpk
// ============================================================================

using TopLevelCPKSchema = decltype(schema::make(
	schema::col<"TocOffset", uint64_t>()));

class TopLevelCpk {
public:
	TopLevelCpk(std::filesystem::path);
	TopLevelCpk(const TopLevelCpk&) = delete;
	TopLevelCpk(TopLevelCpk&&) = default;
	CPKTable getTableOfContents() const;
private:
	table<TopLevelCPKSchema> _table;
	std::filesystem::path _path;
	mutable std::vector<char> _buffer = {};
};
