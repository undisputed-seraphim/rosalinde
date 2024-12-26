#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

class CPK {
public:
	struct directory_entry {
		uint32_t id;
		uint64_t offset;
		uint64_t compressed_size;
		uint64_t decompressed_size;
		std::filesystem::path _path;

		const std::filesystem::path& path() const noexcept;
		operator const std::filesystem::path&() const noexcept;
		std::uintmax_t file_size() const;

		bool operator<(const directory_entry&) const noexcept;
	};

	explicit CPK(std::filesystem::path);
	CPK(const CPK&) = delete;
	CPK(CPK&&) noexcept;

	const std::vector<char>& extract(const directory_entry&) const;
	bool extract(const directory_entry&, std::vector<char>&) const;

	using size_type = std::vector<directory_entry>::size_type;
	using iterator = std::vector<directory_entry>::iterator;
	using const_iterator = std::vector<directory_entry>::const_iterator;

	iterator begin() noexcept;
	iterator end() noexcept;
	const_iterator begin() const noexcept;
	const_iterator end() const noexcept;
	const_iterator cbegin() const noexcept;
	const_iterator cend() const noexcept;
	const_iterator at_id(uint32_t id) const noexcept;
	const_iterator by_name(const std::filesystem::path&) const;

	size_type size() const noexcept { return _files.size(); }
	bool empty() const noexcept { return _files.empty(); }

private:
	void unpack(std::istream&&);

	std::filesystem::path _filepath;
	std::vector<directory_entry> _files;
	mutable std::vector<char> _buffer;
};