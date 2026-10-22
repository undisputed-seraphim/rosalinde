#include "tables.hpp"

#include <data_path.hpp>

#include <fstream>

std::string viewer_data_dir() { return rosalinde_data_dir(); }

namespace {

std::map<std::string, Job, std::less<>> load_battlebgs() {
	std::map<std::string, Job, std::less<>> out;
	std::ifstream f(viewer_data_dir() + "/battlebgs.tsv");
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

const std::map<std::string, Job, std::less<>> BattleBGs = load_battlebgs();