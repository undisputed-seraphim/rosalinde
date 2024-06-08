#pragma once

#include <string>
#include <string_view>
#include <vector>

class CPK {
public:
	struct entry {
		uint32_t id;
		uint64_t offset;
		uint64_t compressed_size;
		uint64_t decompressed_size;
		std::string name;
		std::string path;

		bool operator<(const entry&) const noexcept;
	};

	CPK(std::string_view path);
	CPK(const CPK&) = delete;
	CPK(CPK&&) noexcept;

	const std::string& filepath() const noexcept { return _filepath; }

	bool extract(const entry&, std::vector<char>&) const;

	using size_type = std::vector<entry>::size_type;
	using iterator = std::vector<entry>::iterator;
	using const_iterator = std::vector<entry>::const_iterator;

	iterator begin() noexcept;
	iterator end() noexcept;
	const_iterator begin() const noexcept;
	const_iterator end() const noexcept;
	const_iterator cbegin() const noexcept;
	const_iterator cend() const noexcept;
	const_iterator at_id(uint32_t id) const noexcept;
	const_iterator by_name(std::string_view name, std::string_view path) const noexcept;
	const_iterator by_name(std::string_view fullname) const noexcept;

	size_type size() const noexcept { return _files.size(); }
	bool empty() const noexcept { return _files.empty(); }

private:
	void unpack(std::istream&);

	std::string _filepath;
	std::vector<entry> _files;
	mutable std::vector<char> _buffer;
};