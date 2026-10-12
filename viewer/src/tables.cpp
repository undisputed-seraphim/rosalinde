#include "tables.hpp"

#include <cstdlib>
#include <fstream>

namespace {
std::string data_dir() {
	if (const char* e = std::getenv("VIEWER_DATA"))
		return e;
	return VIEWER_DATA_DIR;
}

std::map<std::string, Job, std::less<>> load_characters() {
	std::map<std::string, Job, std::less<>> out;
	std::ifstream f(data_dir() + "/classes.tsv");
	std::string line;
	while (std::getline(f, line)) {
		if (line.empty() || line[0] == '#')
			continue;
		std::vector<std::string> c;
		size_t s = 0;
		for (size_t i = 0; i <= line.size(); ++i)
			if (i == line.size() || line[i] == '\t') {
				c.push_back(line.substr(s, i - s));
				s = i + 1;
			}
		if (c.size() < 4)
			continue;
		Job j;
		j.mbs = {"Chara", c[2]};
		j.ftx = {"Chara", c[3]};
		out.emplace(c[0], std::move(j));
	}
	std::ifstream v(data_dir() + "/variants.tsv");
	while (std::getline(v, line)) {
		if (line.empty() || line[0] == '#')
			continue;
		std::vector<std::string> c;
		size_t s = 0;
		for (size_t i = 0; i <= line.size(); ++i)
			if (i == line.size() || line[i] == '\t') {
				c.push_back(line.substr(s, i - s));
				s = i + 1;
			}
		if (c.size() < 3)
			continue;
		auto it = out.find(c[0]);
		if (it == out.end())
			continue;
		it->second.variants[c[1]] = static_cast<uint32_t>(std::stoul(c[2], nullptr, 0));
	}
	return out;
}

std::map<std::string, Job, std::less<>> load_battlebgs() {
	std::map<std::string, Job, std::less<>> out;
	std::ifstream f(data_dir() + "/battlebgs.tsv");
	std::string line;
	while (std::getline(f, line)) {
		if (line.empty() || line[0] == '#')
			continue;
		std::vector<std::string> c;
		size_t s = 0;
		for (size_t i = 0; i <= line.size(); ++i)
			if (i == line.size() || line[i] == '\t') {
				c.push_back(line.substr(s, i - s));
				s = i + 1;
			}
		if (c.size() < 3)
			continue;
		Job j;
		j.mbs = {"BattleBG", c[1]};
		j.ftx = {"BattleBG", c[2]};
		out.emplace(c[0], std::move(j));
	}
	return out;
}
} // namespace

// clang-format off
const std::map<std::string, Job, std::less<>> Characters = load_characters();

const std::map<std::string, Job, std::less<>> BattleBGs = load_battlebgs();
// clang-format on