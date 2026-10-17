#pragma once

#include <cstdint>
#include <istream>
#include <optional>
#include <span>
#include <string>
#include <vector>

// NND (node network data, NodeNetworkData/*.nnd). All fields little endian.
//
// 8-byte header:
//   0x00  u32 version
//   0x04  u32 data_words   number of u32 words in the node block
//
// The node block is a sequence of variable-length node records:
//   u32 id
//   u32 area
//   u32 edge_count
//   edge_count * { u32 id, u32 flags, f32 cost }
//
// After the node block comes the name block:
//   u32 name_count
//   (name_count + 1) * { u32 meta0, u32 meta1 }
//   name_count * char name[64]       UTF-8, NUL padded, zero filled
//   u32 group_count                  equals name_count in the corpus
//   group_count * {
//       u32 link_count
//       link_count * { u32 edge_a, u32 edge_b }
//   }
//
// Every link value references an edge id from the same file's node block.
//
// Verified against the 302 files in Unicorn.CPK:
//
//   - version is 2 in all files. The grammar above consumes every byte exactly.
//   - 68866 nodes, 1355370 edges, 994 names, 337833 links.
//   - node.id is a per-file local index (149..2045, reused across files).
//   - node.area is a small group/zone id (158..1238).
//   - node degrees range 0..102; 8 and 18..21 are the most common.
//   - edge.id is a large shared id (348654..2723382). 16478 of the 41030
//     distinct ids appear in more than one file.
//   - edge.flags is 0 or 1, with exactly 677685 of each in the corpus.
//   - edge.cost is a positive float distance (6801 distinct values; 18.0,
//     2.45, 25.46, 1.0 ... are common).
//   - Names are overworld places: Gate_*, Bridge_*, region names such as
//     瘴気_平原61 / Z_1通常_道51. The (name_count + 1) meta pairs are small
//     codes (34 distinct pairs; (1, 1) and (0, 0) dominate); their meaning is
//     not decoded.
//   - link pairs connect two edge ids at a named place; exact semantics open.
//
// This is the overworld/field army movement graph, not a battle map.
//
//   - The filename rectangle is the stage/world rectangle. There are 151
//     ground files and 151 FLY_ files, and every ground rectangle has an
//     exact-bounds FLY_ counterpart. Both layers carry the same 165 names.
//     Ground: 61251 nodes / 1234144 edges. FLY: 7615 / 121226.
//   - All 103 scripts that call StLoadNodeNetwork are St_OW_* (overworld);
//     the usual sequence is StSetStageRect -> StLoadNodeNetwork ->
//     StCreateFacility_StageRect.
//   - Cross-checked with viewer/data/facility_pos.tsv: 432 of 485 facilities
//     fall inside a tile, and the 14 Gate_* labels occur verbatim in the name
//     tables, each inside the matching tile. The 53 outliers are special POIs
//     (ANEMOI, STONESLAB, DIGGAME, HEROSTATUE, BLACKMARKET, STONECIRCLE,
//     GRAVEYARD, 聖所, 遺跡) near a tile edge.
//   - The tiles span the country-region quads and their centers distribute
//     across raw country ids 0..4, so this is world-space field data rather
//     than a per-battle layout.
//   - A spatial join is possible without further format work: tile bounds come
//     from the filename, facilities and country polygons from
//     viewer/data/facility_pos.tsv and country_regions.tsv. The records carry
//     no x/y of their own; the file rectangle is the spatial anchor.
namespace nnd {

struct edge {
	uint32_t id;
	uint32_t flags;
	float cost;
};

struct node {
	uint32_t id;
	uint32_t area;
	std::vector<edge> edges;
};

struct link {
	uint32_t edge_a;
	uint32_t edge_b;
};

struct name {
	std::string text;
	uint32_t meta0;
	uint32_t meta1;
	std::vector<link> links;
};

struct file {
	uint32_t version = 0;
	uint32_t data_words = 0;
	std::vector<node> nodes;
	std::vector<name> names;
	// The name block stores name_count + 1 meta pairs. The first name_count are
	// attached to names[], this is the remaining one.
	uint32_t trailing_meta0 = 0;
	uint32_t trailing_meta1 = 0;
};

std::optional<file> parse(std::span<const uint8_t> data);
std::optional<file> parse(std::istream& is);
std::optional<file> parse(std::istream&& is);

} // namespace nnd
