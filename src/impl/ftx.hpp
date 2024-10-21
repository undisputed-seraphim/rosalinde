#pragma once

#include <istream>
#include <span>
#include <string>
#include <vector>

namespace FTX {

struct Entry {
	std::string name;
	std::vector<char> rgba;
	int32_t width;
	int32_t height;
};

std::vector<Entry> parse(std::istream&);
std::vector<Entry> parse(std::istream&&);
std::vector<Entry> parse(std::span<char>);

} // namespace FTX