#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

struct Job {
	struct dir_and_path {
		std::string dir;
		std::string path;
	};
	dir_and_path mbs, ftx;					  // mbs and ftx filenames. Most will have just one each
	std::map<std::string, uint32_t> variants; // Flags for each variant of the character
};

inline Job::dir_and_path split_path(std::string_view p) {
	const auto pos = p.find_last_of('/');
	if (pos == std::string_view::npos)
		return {"", std::string(p)};
	return {std::string(p.substr(0, pos)), std::string(p.substr(pos + 1))};
}

extern const std::map<std::string, Job, std::less<>> BattleBGs;
extern const std::map<std::string, Job, std::less<>> Characters;