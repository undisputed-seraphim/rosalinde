#pragma once

#include "stage_asset.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class AssetLoader;

class FieldCatalog {
public:
	explicit FieldCatalog(const AssetLoader& loader, std::string data_dir = {});

	std::optional<StageBackground> background(int stage_type, std::string_view entry);

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

	bool load();
	bool load_assets();
	bool load_stages();
	const Asset* find_layer(int area, int variant, char kind, std::string_view extension) const;
	std::optional<StageLayer> far_layer(int area, int variant) const;
	int area_for_country(int country) const;
	int area_for_entry(std::string_view entry) const;
	int stage_variant(std::string_view name) const;
	std::string path_of(const Asset& asset) const;
	std::string texture_of(const Asset& asset) const;

	const AssetLoader& _loader;
	std::string _data_dir;
	std::vector<Asset> _assets;
	std::vector<StageRow> _stages;
	bool _attempted = false;
	bool _ready = false;
};
