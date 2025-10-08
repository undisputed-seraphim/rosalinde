#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "utf.hpp"

struct CPKTraits {
	static constexpr std::array<std::string_view, 6> Fields =
		{"DirName", "FileName", "FileSize", "ExtractSize", "FileOffset", "ID"};
	using Entry = std::tuple<std::string_view, std::string_view, uint64_t, uint64_t, uint64_t, uint64_t>;

	static bool IsValidEntry(const Entry& e) noexcept;
};

class CPKTable : public UTFTable<CPKTraits> {
public:
	using Base = UTFTable<CPKTraits>;
	using Base::operator>>;
	using Base::operator<<;
	using Base::entry_tuple;
	using Base::Fields;
	using Base::NumFields;
	using iterator = Base::iterator;

	CPKTable(std::filesystem::path, uint64_t offset);

	void extract(const entry_tuple& entry, std::vector<char>& out) const;
	void extract(std::string_view dir, std::string_view name, std::vector<char>& out) const;

	iterator find_file(std::string_view dir, std::string_view file) const;

private:
	std::filesystem::path _path;
	uint64_t _offset;
	mutable std::vector<char> _buffer = {};
};

//====

struct TopLevelCPKTraits {
	static constexpr std::array<std::string_view, 1> Fields = {"TocOffset"};
	using Entry = std::tuple<uint64_t>;
	// UpdateDateTime FileSize ContentOffset ContentSize TocOffset TocSize TocCrc HtocOffset HtocSize EtocOffset
	// EtocSize ItocOffset ItocSize ItocCrc GtocOffset GtocSize GtocCrc HgtocOffset HgtocSize EnabledPackedSize
	// EnabledDataSize TotalDataSize Tocs Files Groups Attrs TotalFiles Directories Updates Version Revision Align
	// Sorted EnableFileName EID CpkMode Tvers Comment Codec DpkItoc EnableTocCrc EnableFileCrc CrcMode CrcTable
};

class TopLevelCpk : private UTFTable<TopLevelCPKTraits> {
	using Base = UTFTable<TopLevelCPKTraits>;

public:
	TopLevelCpk(std::filesystem::path);
	TopLevelCpk(const TopLevelCpk&) = delete;
	TopLevelCpk(TopLevelCpk&&) = default;

	CPKTable getTableOfContents() const;

private:
	std::filesystem::path _path;
	mutable std::vector<char> _buffer = {};
};