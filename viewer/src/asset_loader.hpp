#pragma once

#include "sprite_layer.hpp"
#include "tables.hpp"

#include <criware/cpk.hpp>
#include <filesystem>
#include <memory>
#include <string>

class AssetLoader {
public:
	AssetLoader(std::filesystem::path cpk_path);

	std::unique_ptr<SpriteLayer>
	load_layer(const Job& job, uint32_t trackid, const std::string& class_name, const std::string& variant_name) const;

	BackgroundScene load_background(const Job& job) const;

	const std::vector<std::string>& class_names() const { return _class_names; }

private:
	CPKTable _cpkt;
	std::vector<std::string> _class_names;
};
