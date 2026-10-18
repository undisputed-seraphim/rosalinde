#include "dump.hpp"

#include <impl/nnd.hpp>

#include <cstdint>
#include <iostream>

namespace dump {
namespace {

std::span<const uint8_t> as_bytes(std::span<char> data) {
	return {reinterpret_cast<const uint8_t*>(data.data()), data.size()};
}

} // namespace

int dump_nnd(const context& ctx, std::span<char> data) {
	auto parsed = nnd::parse(as_bytes(data));
	if (!parsed) {
		std::cerr << "d: " << ctx.path << ": not a valid NND file\n";
		return 1;
	}
	const auto& file = *parsed;

	size_t edge_count = 0;
	size_t link_count = 0;
	for (const auto& node : file.nodes)
		edge_count += node.edges.size();
	for (const auto& name : file.names)
		link_count += name.links.size();

	const size_t node_limit = ctx.opt.limit_set ? ctx.opt.limit : file.nodes.size();
	const size_t shown_nodes = node_limit < file.nodes.size() ? node_limit : file.nodes.size();

	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "nnd");
		json.field("version", file.version);
		json.field("data_words", file.data_words);
		json.field("node_count", file.nodes.size());
		json.field("edge_count", edge_count);
		json.field("name_count", file.names.size());
		json.field("link_count", link_count);
		json.begin_array("trailing_meta");
		json.number(file.trailing_meta0);
		json.number(file.trailing_meta1);
		json.end_array();
		if (shown_nodes != file.nodes.size())
			json.field("truncated", true);

		json.begin_array("nodes");
		for (size_t i = 0; i < shown_nodes; ++i) {
			const auto& node = file.nodes[i];
			json.begin_object();
			json.field("index", i);
			json.field("id", node.id);
			json.field("area", node.area);
			json.begin_array("edges");
			for (size_t j = 0; j < node.edges.size(); ++j) {
				const auto& edge = node.edges[j];
				json.begin_object();
				json.field("index", j);
				json.field("id", edge.id);
				json.field("flags", edge.flags);
				json.field("cost", edge.cost);
				json.end_object();
			}
			json.end_array();
			json.end_object();
		}
		json.end_array();

		json.begin_array("names");
		for (size_t i = 0; i < file.names.size(); ++i) {
			const auto& name = file.names[i];
			json.begin_object();
			json.field("index", i);
			json.field("text", name.text);
			json.field("meta0", name.meta0);
			json.field("meta1", name.meta1);
			json.begin_array("links");
			for (size_t j = 0; j < name.links.size(); ++j) {
				const auto& link = name.links[j];
				json.begin_object();
				json.field("index", j);
				json.field("edge_a", link.edge_a);
				json.field("edge_b", link.edge_b);
				json.end_object();
			}
			json.end_array();
			json.end_object();
		}
		json.end_array();
		json.end_object();
		std::cout << '\n';
		return 0;
	}

	write_metadata_text(ctx, "nnd");
	std::cout << "version=" << file.version << " data_words=" << file.data_words << " nodes=" << file.nodes.size()
			  << " edges=" << edge_count << " names=" << file.names.size() << " links=" << link_count << '\n';
	std::cout << "trailing_meta=(" << file.trailing_meta0 << "," << file.trailing_meta1 << ")\n";
	std::cout << "nodes:\n";
	for (size_t i = 0; i < shown_nodes; ++i) {
		const auto& node = file.nodes[i];
		std::cout << "  [" << i << "] id=" << node.id << " area=" << node.area << '\n';
		for (size_t j = 0; j < node.edges.size(); ++j) {
			const auto& edge = node.edges[j];
			std::cout << "    edge[" << j << "] id=" << edge.id << " flags=" << edge.flags << " cost=" << edge.cost
					  << '\n';
		}
	}
	if (shown_nodes != file.nodes.size())
		std::cout << "  ... " << (file.nodes.size() - shown_nodes) << " more nodes\n";

	if (!file.names.empty()) {
		std::cout << "names:\n";
		for (size_t i = 0; i < file.names.size(); ++i) {
			const auto& name = file.names[i];
			std::cout << "  [" << i << "] \"" << name.text << "\" meta=(" << name.meta0 << "," << name.meta1 << ")\n";
			for (size_t j = 0; j < name.links.size(); ++j) {
				const auto& link = name.links[j];
				std::cout << "    link[" << j << "] edge_a=" << link.edge_a << " edge_b=" << link.edge_b << '\n';
			}
		}
	}
	return 0;
}

} // namespace dump
