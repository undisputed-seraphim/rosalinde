#pragma once

#include "stage_asset.hpp"

#include <impl/bbs.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

class AssetLoader;

// The arguments DrCreateStage passes to the engine: stage type, COA preset,
// weather index, and the script entry used to disambiguate field stages.
struct StageRequest {
	int stage_type = 0;
	int coa_preset = 0;
	int weather_type = 0;
	std::string_view entry;
};

class FieldCatalog {
public:
	explicit FieldCatalog(const AssetLoader& loader, std::string data_dir = {});

	std::optional<StageComposition> background(const StageRequest& request);

private:
	struct Asset {
		std::string main;
		std::string texture;
	};

	struct StageRow {
		std::string name;
		int base_lv = 0;
		int cue = -1;
		int country = -1;
	};

	struct BbsEntry {
		std::string path;
		std::string label;
	};

	bool load();
	bool load_assets();
	bool load_stages();
	bool load_bbs_catalog();
	std::optional<bbs::file> load_bbs(std::string_view path) const;
	std::optional<StageComposition> bbs_background(const StageRequest& request);
	std::optional<std::string> resolve_bbs_asset(std::string_view name) const;
	const Asset* find_layer(int area, int variant, char kind, std::string_view extension) const;
	std::optional<StageMpdLayer> far_layer(int area, int variant) const;
	int area_for_country(int country) const;
	int area_for_entry(std::string_view entry) const;
	int stage_variant(std::string_view name) const;
	std::string path_of(const Asset& asset) const;
	std::string texture_of(const Asset& asset) const;

	const AssetLoader& _loader;
	std::string _data_dir;
	std::vector<Asset> _assets;
	std::vector<StageRow> _stages;
	std::unordered_map<int, BbsEntry> _bbs_catalog;
	bool _bbs_catalog_loaded = false;
	bool _attempted = false;
	bool _ready = false;
};
