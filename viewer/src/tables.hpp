#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct Job {
    std::vector<std::string> mbs, ftx;					// mbs and ftx filenames. Most will have just one each
    std::unordered_map<std::string, uint32_t> variants; // Flags for each variant of the character
};

extern const std::unordered_map<std::string, Job> BattleBGs;
extern const std::unordered_map<std::string, Job> Characters;