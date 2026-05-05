#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct Job {
	struct dir_and_path {
		std::string dir;
		std::string path;
	};
	dir_and_path mbs, ftx;					  // mbs and ftx filenames. Most will have just one each
	std::map<std::string, uint32_t> variants; // Flags for each variant of the character
};

extern const std::map<std::string, Job, std::less<>> BattleBGs;
extern const std::map<std::string, Job, std::less<>> Characters;