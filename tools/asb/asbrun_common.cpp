#include "asbrun_common.hpp"

#include <criware/cpk.hpp>

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace asbrun {

std::vector<uint8_t> read_file(const std::string& path) {
	std::ifstream in(path, std::ios::binary);
	if (!in)
		throw std::runtime_error("cannot open " + path);
	return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), {});
}

std::vector<uint8_t> extract_entry(const cpk::archive& archive, std::string_view path) {
	while (!path.empty() && path.front() == '/')
		path.remove_prefix(1);
	auto item = archive.find(path);
	if (item == archive.end())
		throw std::runtime_error("not found in CPK: " + std::string(path));
	std::vector<char> buffer;
	archive.extract(*item, buffer);
	return std::vector<uint8_t>(buffer.begin(), buffer.end());
}

std::vector<uint8_t> extract_entry(const std::string& cpk_path, std::string_view path) {
	cpk::archive archive(std::filesystem::absolute(cpk_path));
	return extract_entry(archive, path);
}

std::string json_escape(std::string_view value) {
	std::string out;
	out.reserve(value.size() + 8);
	for (unsigned char c : value) {
		switch (c) {
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			if (c < 0x20) {
				char buffer[8];
				std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
				out += buffer;
			} else {
				out += static_cast<char>(c);
			}
		}
	}
	return out;
}

std::string lower(std::string value) {
	for (char& c : value)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return value;
}

bool ends_with(std::string_view value, std::string_view suffix) {
	return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string_view trim(std::string_view value) {
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
		value.remove_prefix(1);
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
		value.remove_suffix(1);
	return value;
}

} // namespace asbrun
