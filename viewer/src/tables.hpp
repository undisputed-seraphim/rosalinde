#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct Job {
    struct dir_and_path {
        std::string dir;
        std::string path;
    };
    dir_and_path mbs, ftx;					// mbs and ftx filenames. Most will have just one each
    std::unordered_map<std::string, uint32_t> variants; // Flags for each variant of the character
};

extern const std::unordered_map<std::string, Job> BattleBGs;
extern const std::unordered_map<std::string, Job> Characters;