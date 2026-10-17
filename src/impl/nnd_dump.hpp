#pragma once

#include "nnd.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <string_view>

namespace nnd {

using namespace std::literals;

inline void dump_csv(const file& f, const std::filesystem::path& outdir) {
	std::filesystem::create_directories(outdir);

	size_t edge_count = 0;
	size_t link_count = 0;
	for (const auto& n : f.nodes)
		edge_count += n.edges.size();
	for (const auto& n : f.names)
		link_count += n.links.size();

	{
		auto ofs = std::ofstream(outdir / "header.csv");
		ofs << "version,data_words,node_count,edge_count,name_count,link_count,"
			<< "trailing_meta0,trailing_meta1\n";
		ofs << std::format(
			"{},{},{},{},{},{},{},{}\n"sv,
			f.version,
			f.data_words,
			f.nodes.size(),
			edge_count,
			f.names.size(),
			link_count,
			f.trailing_meta0,
			f.trailing_meta1);
	}

	{
		auto ofs = std::ofstream(outdir / "nodes.csv");
		ofs << "index,id,area,edge_count\n";
		for (size_t i = 0; i < f.nodes.size(); ++i) {
			const auto& n = f.nodes[i];
			ofs << std::format("{},{},{},{}\n"sv, i, n.id, n.area, n.edges.size());
		}
	}

	{
		auto ofs = std::ofstream(outdir / "edges.csv");
		ofs << "node_index,node_id,index,id,flags,cost\n";
		for (size_t i = 0; i < f.nodes.size(); ++i) {
			const auto& n = f.nodes[i];
			for (size_t j = 0; j < n.edges.size(); ++j) {
				const auto& e = n.edges[j];
				ofs << std::format("{},{},{},{},{},{}\n"sv, i, n.id, j, e.id, e.flags, e.cost);
			}
		}
	}

	{
		auto ofs = std::ofstream(outdir / "names.csv");
		ofs << "index,text,meta0,meta1,link_count\n";
		for (size_t i = 0; i < f.names.size(); ++i) {
			const auto& n = f.names[i];
			ofs << std::format("{},{},{},{},{}\n"sv, i, n.text, n.meta0, n.meta1, n.links.size());
		}
	}

	{
		auto ofs = std::ofstream(outdir / "links.csv");
		ofs << "name_index,name,index,edge_a,edge_b\n";
		for (size_t i = 0; i < f.names.size(); ++i) {
			const auto& n = f.names[i];
			for (size_t j = 0; j < n.links.size(); ++j) {
				const auto& l = n.links[j];
				ofs << std::format("{},{},{},{},{}\n"sv, i, n.text, j, l.edge_a, l.edge_b);
			}
		}
	}
}

} // namespace nnd
