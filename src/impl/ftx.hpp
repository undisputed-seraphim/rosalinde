#pragma once

#include <fstream>
#include <span>
#include <string>
#include <utility>
#include <vector>

class FTX {
public:
	struct Entry {
		std::string name;
		uint64_t offset;
		uint64_t size;
		int32_t width;
		int32_t height;
		std::vector<char> rgba;
	};

	FTX() noexcept;
	FTX(FTX&&) noexcept;

	bool parse(std::istream&);
	bool parse(std::istream&&);

	using iterator = std::vector<Entry>::iterator;
	using const_iterator = std::vector<Entry>::const_iterator;
	using size_type = std::vector<Entry>::size_type;
	iterator begin();
	iterator end();
	const_iterator begin() const;
	const_iterator end() const;
	const_iterator cbegin() const;
	const_iterator cend() const;

	size_type size() const noexcept;
	bool empty() const noexcept;

private:
	std::vector<Entry> _entries;
	mutable std::vector<char> _buffer;
};
