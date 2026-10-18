#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <criware/utf.hpp>

// ============================================================================
// CPKTable — typed access to the asset table-of-contents
// ============================================================================

using CPKSchema = decltype(schema::make(
	schema::col<"DirName", std::string_view>(),
	schema::col<"FileName", std::string_view>(),
	schema::col<"FileSize", uint64_t>(),
	schema::col<"ExtractSize", uint64_t>(),
	schema::col<"FileOffset", uint64_t>(),
	schema::col<"ID", uint64_t>()));

class CPKTable {
public:
	using table_type = utf::table<CPKSchema>;
	using iterator = table_type::row_iterator;
	using row_type = table_type::row_type;

	CPKTable(std::filesystem::path, uint64_t offset);
	CPKTable(const CPKTable&) = delete;
	CPKTable& operator=(const CPKTable&) = delete;
	CPKTable(CPKTable&&) = default;
	CPKTable& operator=(CPKTable&&) = default;

	void parse_toc(std::vector<char>&& data);

	void extract(const row_type& entry, std::vector<char>& out) const;
	void extract(const row_type& entry, std::span<char> out) const;
	void extract(std::string_view dir, std::string_view name, std::vector<char>& out) const;
	void extract(std::string_view dir, std::string_view name, std::span<char> out) const;
	void extract_to_file(const row_type& entry, const std::filesystem::path& path) const;
	iterator find_file(std::string_view dir, std::string_view file) const;
	size_t size() const noexcept { return _table.size(); }

	iterator begin() const { return _table.begin(); }
	iterator end() const { return _table.end(); }
	row_type operator[](size_t i) const { return _table[i]; }

	template <size_t I>
	auto column() const {
		return _table.template column_at<I>();
	}
	template <schema::fixed_string Name>
	auto column() const {
		return _table.template column<Name>();
	}

private:
	std::vector<char> _toc_storage;
	utf::table<CPKSchema> _table;
	std::filesystem::path _path;
	uint64_t _offset;
};

// ============================================================================
// TopLevelCpk
// ============================================================================

using TopLevelCPKSchema = decltype(schema::make(schema::col<"TocOffset", uint64_t>()));

class TopLevelCpk {
public:
	TopLevelCpk(std::filesystem::path);
	TopLevelCpk(const TopLevelCpk&) = delete;
	TopLevelCpk(TopLevelCpk&&) = default;
	CPKTable getTableOfContents() const;

private:
	utf::table<TopLevelCPKSchema> _table;
	std::filesystem::path _path;
	mutable std::vector<char> _buffer = {};
};
