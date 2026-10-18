#pragma once

#include <filesystem>
#include <span>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <criware/utf.hpp>

namespace cpk {

using toc_schema = decltype(schema::make(
	schema::col<"DirName", std::string_view>(),
	schema::col<"FileName", std::string_view>(),
	schema::col<"FileSize", uint64_t>(),
	schema::col<"ExtractSize", uint64_t>(),
	schema::col<"FileOffset", uint64_t>(),
	schema::col<"ID", uint64_t>()));

using top_schema = decltype(schema::make(schema::col<"TocOffset", uint64_t>()));

struct entry {
	std::string_view dir;
	std::string_view name;
	uint64_t file_size = 0;
	uint64_t extract_size = 0;
	uint64_t offset = 0;
	uint64_t id = 0;
};

class archive {
public:
	using const_iterator = std::vector<entry>::const_iterator;

	explicit archive(std::filesystem::path path);

	archive(const archive&) = delete;
	archive& operator=(const archive&) = delete;
	archive(archive&&) = default;
	archive& operator=(archive&&) = default;

	size_t size() const noexcept { return _entries.size(); }
	bool empty() const noexcept { return _entries.empty(); }

	const entry& operator[](size_t i) const noexcept { return _entries[i]; }
	const entry& at(size_t i) const { return _entries.at(i); }

	const_iterator begin() const noexcept { return _entries.begin(); }
	const_iterator end() const noexcept { return _entries.end(); }

	const_iterator find(std::string_view dir, std::string_view name) const;
	const_iterator find(std::string_view path) const;
	bool contains(std::string_view dir, std::string_view name) const;
	bool contains(std::string_view path) const { return find(path) != end(); }

	std::span<const entry> entries() const noexcept { return _entries; }
	const std::filesystem::path& path() const noexcept { return _path; }

	void extract(const entry& item, std::vector<char>& out) const;
	void extract(const entry& item, std::span<char> out) const;
	void extract(std::string_view dir, std::string_view name, std::vector<char>& out) const;
	void extract(std::string_view dir, std::string_view name, std::span<char> out) const;
	void extract_to_file(const entry& item, const std::filesystem::path& path) const;

private:
	struct key {
		std::string_view dir;
		std::string_view name;
		bool operator==(const key&) const = default;
	};

	struct key_hash {
		size_t operator()(const key& k) const noexcept {
			const size_t h = std::hash<std::string_view>{}(k.dir);
			return h ^ (std::hash<std::string_view>{}(k.name) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
		}
	};

	std::filesystem::path _path;
	std::vector<char> _toc_storage;
	std::vector<entry> _entries;
	std::unordered_map<key, size_t, key_hash> _index;
};

} // namespace cpk
