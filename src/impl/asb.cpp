#include "asb.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace asb {

auto parse(std::span<const uint8_t> data) -> File {
	File file;

	auto read_str_at = [&](size_t at) -> std::string {
		size_t end = at;
		while (end < data.size() && data[end] != 0) ++end;
		return std::string(reinterpret_cast<const char*>(&data[at]), end - at);
	};
	auto is_fn_name = [](const std::string& s) -> bool {
		if (s.size() < 2 || s.size() > 64) return false;
		return std::all_of(s.begin(), s.end(),
						   [](char c) { return std::isalnum(c) || c == '_'; });
	};

	std::memcpy(file.header.data(), data.data(), 8);

	size_t src_path_pos = std::string::npos;
	for (size_t i = 8; i + 3 < data.size(); ++i) {
		std::string s = read_str_at(i);
		if ((s.find("C:/") == 0 || s.find("c:/") == 0) && s.ends_with(".ascp")) {
			src_path_pos = i;
			file.source_path = s;
			break;
		}
	}

	std::vector<size_t> fn_offsets;
	for (size_t i = 8; i < data.size() - 1; ++i) {
		if ((data[i] == 0x08 || data[i] == 0x0a) && i + 2 < data.size() && std::isalpha(data[i + 1])) {
			std::string name = read_str_at(i + 1);
			if (is_fn_name(name)) fn_offsets.push_back(i);
		}
	}

	for (size_t k = 0; k < fn_offsets.size(); ++k) {
		size_t marker_pos = fn_offsets[k];
		size_t name_pos = marker_pos + 1;
		std::string name = read_str_at(name_pos);
		size_t bc_start = name_pos + name.size() + 1;
		size_t bc_end = (k + 1 < fn_offsets.size()) ? fn_offsets[k + 1] : data.size();

		if (src_path_pos != std::string::npos && bc_start < src_path_pos && bc_end > src_path_pos) {
			bc_end = src_path_pos;
		}

		file.functions.emplace_back();
		file.functions.back().name = name;
		if (bc_start < bc_end && bc_start < data.size()) {
			size_t len = std::min(bc_end, data.size()) - bc_start;
			file.functions.back().bytecode.assign(
				data.begin() + bc_start, data.begin() + bc_start + len);
		}
	}

	return file;
}

} // namespace asb
