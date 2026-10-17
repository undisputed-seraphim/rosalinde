#include "nnd.hpp"

#include <criware/byte_reader.hpp>

#include <unordered_set>
#include <utility>

namespace nnd {

namespace {

constexpr uint32_t expected_version = 2;
constexpr size_t name_bytes = 64;
constexpr size_t name_min_bytes = 16;
constexpr size_t name_stride = 76;

} // namespace

std::optional<file> parse(std::span<const uint8_t> data) {
	if (data.size() < 8)
		return std::nullopt;

	byte_reader r(data);
	file f;
	f.version = r.read<uint32_t>();
	f.data_words = r.read<uint32_t>();

	if (f.version != expected_version)
		return std::nullopt;
	if (f.data_words > (data.size() - 8) / sizeof(uint32_t))
		return std::nullopt;

	size_t words_left = f.data_words;
	while (words_left != 0) {
		if (words_left < 3)
			return std::nullopt;

		node n;
		n.id = r.read<uint32_t>();
		n.area = r.read<uint32_t>();
		const uint32_t edge_count = r.read<uint32_t>();
		words_left -= 3;

		if (edge_count > words_left / 3)
			return std::nullopt;
		n.edges.resize(edge_count);
		for (auto& e : n.edges) {
			e.id = r.read<uint32_t>();
			e.flags = r.read<uint32_t>();
			e.cost = r.read<float>();
		}
		words_left -= size_t{edge_count} * 3;

		f.nodes.push_back(std::move(n));
	}

	const size_t name_start = 8 + size_t{f.data_words} * sizeof(uint32_t);
	const std::span<const uint8_t> rest = data.subspan(name_start);
	if (rest.size() < name_min_bytes)
		return std::nullopt;

	byte_reader nr(rest);
	const uint32_t name_count = nr.read<uint32_t>();
	if (name_count > (rest.size() - name_min_bytes) / name_stride)
		return std::nullopt;

	std::vector<std::pair<uint32_t, uint32_t>> meta(size_t{name_count} + 1);
	for (auto& [a, b] : meta) {
		a = nr.read<uint32_t>();
		b = nr.read<uint32_t>();
	}

	f.names.resize(name_count);
	for (size_t i = 0; i < f.names.size(); ++i) {
		const auto bytes = nr.read_bytes(name_bytes);
		size_t len = 0;
		while (len < name_bytes && bytes[len] != '\0')
			++len;
		if (len == name_bytes)
			return std::nullopt;
		f.names[i].text.assign(reinterpret_cast<const char*>(bytes.data()), len);
		f.names[i].meta0 = meta[i].first;
		f.names[i].meta1 = meta[i].second;
	}

	f.trailing_meta0 = meta.back().first;
	f.trailing_meta1 = meta.back().second;

	const uint32_t group_count = nr.read<uint32_t>();
	if (group_count != name_count)
		return std::nullopt;

	std::unordered_set<uint32_t> edge_ids;
	size_t total_edges = 0;
	for (const auto& n : f.nodes)
		total_edges += n.edges.size();
	edge_ids.reserve(total_edges);
	for (const auto& n : f.nodes) {
		for (const auto& e : n.edges)
			edge_ids.insert(e.id);
	}

	for (auto& n : f.names) {
		const uint32_t link_count = nr.read<uint32_t>();
		if (link_count > (nr.size - nr.tell()) / (sizeof(uint32_t) * 2))
			return std::nullopt;

		n.links.resize(link_count);
		for (auto& l : n.links) {
			l.edge_a = nr.read<uint32_t>();
			l.edge_b = nr.read<uint32_t>();
			if (!edge_ids.contains(l.edge_a) || !edge_ids.contains(l.edge_b))
				return std::nullopt;
		}
	}

	if (!nr.eof())
		return std::nullopt;

	return f;
}

std::optional<file> parse(std::istream& is) {
	std::vector<uint8_t> buffer;
	char c;
	while (is.get(c))
		buffer.push_back(static_cast<uint8_t>(c));
	return parse(std::span<const uint8_t>(buffer.data(), buffer.size()));
}

std::optional<file> parse(std::istream&& is) { return parse(is); }

} // namespace nnd
