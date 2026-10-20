#include "field_catalog.hpp"

#include "asset_loader.hpp"
#include "tables.hpp"

#include <impl/ufb.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <span>
#include <sstream>
#include <utility>

namespace {

std::string lower(std::string text) {
	for (char& c : text)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return text;
}

std::string normalize(std::string path) {
	for (char& c : path)
		if (c == '\\')
			c = '/';
	return path;
}

std::string stem(std::string path) {
	path = normalize(std::move(path));
	const size_t slash = path.find_last_of('/');
	const size_t dot = path.find_last_of('.');
	if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
		return slash == std::string::npos ? path : path.substr(slash + 1);
	path.resize(dot);
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string extension(std::string_view path) {
	const size_t slash = path.find_last_of('/');
	const size_t dot = path.find_last_of('.');
	if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash))
		return {};
	return lower(std::string(path.substr(dot)));
}

std::string two(int value) {
	std::ostringstream out;
	out << std::setfill('0') << std::setw(2) << value;
	return out.str();
}

bool has_area_stem(std::string_view stem_text, int area) {
	const std::string prefix = "ac" + two(area) + "_";
	return stem_text.size() > prefix.size() && stem_text.compare(0, prefix.size(), prefix) == 0;
}

std::string join(const Job::dir_and_path& path) {
	return path.dir.empty() ? path.path : path.dir + "/" + path.path;
}

} // namespace

FieldCatalog::FieldCatalog(const AssetLoader& loader, std::string data_dir)
	: _loader(loader)
	, _data_dir(std::move(data_dir)) {}

std::optional<bbs::file> FieldCatalog::load_bbs(std::string_view path) const {
	auto resolved = _loader.resolve(path);
	if (!resolved)
		return std::nullopt;
	auto entry = _loader.cpk().find(resolved->dir, resolved->path);
	if (entry == _loader.cpk().end())
		return std::nullopt;
	std::vector<char> bytes;
	_loader.cpk().extract(*entry, bytes);
	try {
		return bbs::parse(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()));
	} catch (...) {
		return std::nullopt;
	}
}

bool FieldCatalog::load_bbs_catalog() {
	if (_bbs_catalog_loaded)
		return !_bbs_catalog.empty();
	_bbs_catalog_loaded = true;
	const std::string dir = _data_dir.empty() ? viewer_data_dir() : _data_dir;
	std::ifstream file(dir + "/bbs.tsv");
	if (!file)
		return false;
	std::string line;
	while (std::getline(file, line)) {
		if (line.empty() || line.front() == '#')
			continue;
		std::istringstream row(line);
		std::string index;
		BbsEntry entry;
		if (!std::getline(row, index, '\t') || !std::getline(row, entry.path, '\t'))
			continue;
		std::getline(row, entry.label);
		try {
			_bbs_catalog.emplace(std::stoi(index), std::move(entry));
		} catch (...) {
		}
	}
	return !_bbs_catalog.empty();
}

std::optional<std::string> FieldCatalog::resolve_bbs_asset(std::string_view name) const {
	if (name.empty())
		return std::nullopt;
	if (auto direct = _loader.resolve(name))
		return join(*direct);
	for (const char* prefix : {"BattleBG/", "Effect/", "GUI/", "FieldBG/", "Chara/"}) {
		const std::string path = std::string(prefix) + std::string(name);
		if (auto found = _loader.resolve(path))
			return join(*found);
	}
	const std::string wanted = lower(std::string(name));
	for (const auto& entry : _loader.cpk()) {
		if (lower(std::string(entry.name)) != wanted)
			continue;
		return std::string(entry.dir) + "/" + std::string(entry.name);
	}
	return std::nullopt;
}

std::optional<StageComposition> FieldCatalog::bbs_background(int stage_type) {
	if (!load_bbs_catalog())
		return std::nullopt;
	const auto entry = _bbs_catalog.find(stage_type);
	if (entry == _bbs_catalog.end())
		return std::nullopt;
	auto file = load_bbs(entry->second.path);
	if (!file)
		return std::nullopt;

	// TODO(stage): UcStStage +0xab0 (stage_data) is the likely source of the
	// base layer and whether these BBS objects replace or overlay it.
	StageComposition result;
	result.layers.reserve(file->objects.size());
	for (size_t i = 0; i < file->objects.size(); ++i) {
		const auto& object = file->objects[i];
		if (object.mbs_index >= file->mbs_names.size())
			continue;
		auto mbs = resolve_bbs_asset(file->mbs_names[object.mbs_index]);
		if (!mbs)
			continue;

		std::optional<std::string> ftx;
		if (object.mbs_index < file->ftx_names.size())
			ftx = resolve_bbs_asset(file->ftx_names[object.mbs_index]);
		if (!ftx) {
			std::string stem = file->mbs_names[object.mbs_index];
			const size_t dot = stem.find_last_of('.');
			if (dot != std::string::npos)
				stem.resize(dot);
			ftx = resolve_bbs_asset(stem + ".ftx");
		}
		if (!ftx)
			continue;

		StageLayer placed;
		placed.mbs = std::move(*mbs);
		placed.ftx = std::move(*ftx);
		placed.track_index = object.track_index;
		placed.x = object.x;
		placed.y = object.y;
		placed.z = object.z;
		placed.scale = object.scale_x > 0.0f ? object.scale_x : 1.0f;
		placed.alpha = std::clamp(static_cast<int>(std::lround(object.alpha * 255.0f)), 0, 255);
		placed.order = static_cast<int>(i);
		result.layers.push_back(std::move(placed));
	}
	if (result.layers.empty())
		return std::nullopt;
	spdlog::debug(
		"bbs: stage={} path={} label={} layers={}", stage_type, entry->second.path, entry->second.label, result.layers.size());
	return result;
}

int FieldCatalog::area_for_country(int country) const {
	switch (country) {
	case 0:
		return 1;
	case 1:
		return 2;
	case 2:
		return 0;
	case 3:
		return 3;
	case 4:
		return 4;
	default:
		return -1;
	}
}

int FieldCatalog::area_for_entry(std::string_view entry) const {
	const size_t at = entry.find("Dr_OW_");
	if (at == std::string_view::npos || at + 6 >= entry.size())
		return -1;
	switch (entry[at + 6]) {
	case 'A':
		return 1;
	case 'B':
		return 2;
	case 'C':
		return 0;
	case 'D':
		return 3;
	case 'E':
		return 4;
	case 'Z':
		return 10;
	default:
		return -1;
	}
}

int FieldCatalog::stage_variant(std::string_view name) const {
	std::string_view token;
	const size_t drama = name.find("Dr_OW_");
	if (drama != std::string_view::npos && name.size() > drama + 7) {
		const size_t separator = name.find('_', drama + 7);
		if (separator != std::string_view::npos)
			token = name.substr(separator + 1);
	} else {
		const size_t underscore = name.find_last_of('_');
		if (underscore != std::string_view::npos && underscore + 1 < name.size())
			token = name.substr(underscore + 1);
	}
	if (token.empty())
		return 0;
	size_t first_digit = 0;
	while (first_digit < token.size() && !std::isdigit(static_cast<unsigned char>(token[first_digit])))
		++first_digit;
	if (first_digit == token.size())
		return 0;
	int value = 0;
	for (; first_digit < token.size() && std::isdigit(static_cast<unsigned char>(token[first_digit])); ++first_digit)
		value = value * 10 + (token[first_digit] - '0');
	return value;
}

bool FieldCatalog::load_stages() {
	std::ifstream file((_data_dir.empty() ? viewer_data_dir() : _data_dir) + "/stages.tsv");
	if (!file)
		return false;
	std::string line;
	while (std::getline(file, line)) {
		if (line.empty() || line[0] == '#')
			continue;
		std::vector<std::string> columns;
		size_t start = 0;
		for (size_t i = 0; i <= line.size(); ++i)
			if (i == line.size() || line[i] == '\t') {
				columns.push_back(line.substr(start, i - start));
				start = i + 1;
			}
		if (columns.size() < 2 || columns[0].empty())
			continue;
		StageRow row;
		row.name = columns[0];
		row.base_lv = std::atoi(columns[1].c_str());
		row.cue = columns.size() > 2 ? std::atoi(columns[2].c_str()) : -1;
		row.country = columns.size() > 3 ? std::atoi(columns[3].c_str()) : -1;
		_stages.push_back(std::move(row));
	}
	return !_stages.empty();
}

bool FieldCatalog::load_assets() {
	const auto resolved = _loader.resolve("FieldBG/Field.ufb");
	if (!resolved)
		return false;
	const auto& cpk = _loader.cpk();
	const auto entry = cpk.find(resolved->dir, resolved->path);
	if (entry == cpk.end())
		return false;
	std::vector<char> bytes;
	try {
		cpk.extract(*entry, bytes);
	} catch (const std::exception& e) {
		spdlog::warn("field: Field.ufb extract failed: {}", e.what());
		return false;
	}
	const std::span<const uint8_t> data(
		reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
	// TODO: use an asset-only UFB parser here. ufb::parse copies the full
	// 6,134,722-record terrain/object stream (~147 MB) even though the field
	// catalog only needs the 39 asset pairs. Add ufb::parse_assets() that
	// validates the header/string pool and reads only asset_count * 2 strings;
	// keep the full parser for tools that need records.
	const auto parsed = ufb::parse(data);
	if (!parsed)
		return false;
	_assets.reserve(parsed->assets.size());
	for (const auto& asset : parsed->assets)
		_assets.push_back({normalize(asset.main), normalize(asset.texture)});
	spdlog::debug("field: {} assets from Field.ufb", _assets.size());
	return !_assets.empty();
}

bool FieldCatalog::load() {
	if (_attempted)
		return _ready;
	_attempted = true;
	const bool assets = load_assets();
	const bool stages = load_stages();
	_ready = assets;
	spdlog::debug("field: catalog assets={} stages={}", assets, stages);
	return _ready;
}

const FieldCatalog::Asset* FieldCatalog::find_layer(int area, int variant, char kind, std::string_view wanted_ext) const {
	const std::string target = "ac" + two(area) + "_" + two(variant) + static_cast<char>(std::tolower(kind));
	for (const auto& asset : _assets) {
		if (extension(asset.main) != wanted_ext)
			continue;
		if (lower(stem(asset.main)) == target)
			return &asset;
	}
	return nullptr;
}

std::optional<StageMpdLayer> FieldCatalog::far_layer(int area, int variant) const {
	if (const auto* exact = find_layer(area, variant, 'f', ".mpd"))
		return StageMpdLayer{path_of(*exact), texture_of(*exact), true};
	for (const auto& asset : _assets) {
		if (extension(asset.main) != ".mpd")
			continue;
		const std::string key = lower(stem(asset.main));
		if (!has_area_stem(key, area))
			continue;
		const std::string token = key.substr(("ac" + two(area) + "_").size());
		if (token.find('f') == std::string::npos || token.find('p') != std::string::npos)
			continue;
		return StageMpdLayer{path_of(asset), texture_of(asset), true};
	}
	return std::nullopt;
}

std::string FieldCatalog::path_of(const Asset& asset) const {
	return normalize(asset.main);
}

std::string FieldCatalog::texture_of(const Asset& asset) const {
	return normalize(asset.texture);
}

std::optional<StageComposition> FieldCatalog::background(int stage_type, int coa_preset, std::string_view entry) {
	(void)coa_preset;
	if (auto bbs = bbs_background(stage_type))
		return bbs;

	if (!load())
		return std::nullopt;

	const StageRow* row = nullptr;
	if (stage_type >= 1 && static_cast<size_t>(stage_type) <= _stages.size())
		row = &_stages[static_cast<size_t>(stage_type - 1)];

	int area = row ? area_for_country(row->country) : -1;
	int variant = row ? stage_variant(row->name) : 0;
	if (area < 0) {
		area = area_for_entry(entry);
		variant = stage_variant(entry);
	}
	if (area < 0)
		return std::nullopt;

	const Asset* mbs = find_layer(area, variant, 'p', ".mbs");
	const Asset* mpd = find_layer(area, variant, 'p', ".mpd");
	if (!mbs && !mpd && variant != 0) {
		mbs = find_layer(area, 0, 'p', ".mbs");
		mpd = find_layer(area, 0, 'p', ".mpd");
	}
	if (!mbs && !mpd)
		return std::nullopt;

	StageComposition result;
	if (mbs)
		result.mbs = path_of(*mbs);
	const Asset& texture_asset = mbs ? *mbs : *mpd;
	result.ftx = texture_of(texture_asset);

	if (const auto far = far_layer(area, variant))
		result.mpd_layers.push_back(*far);
	if (mpd && (!mbs || path_of(*mpd) != result.mbs))
		result.mpd_layers.push_back(StageMpdLayer{path_of(*mpd), texture_of(*mpd), false});

	const std::string row_name = row ? row->name : std::string("<none>");
	spdlog::debug(
		"field: type={} stage={} country={} area=AC{} variant={} mbs={} layers={}",
		stage_type,
		row_name,
		row ? row->country : -1,
		two(area),
		variant,
		result.mbs.empty() ? "<none>" : result.mbs,
		result.mpd_layers.size());
	for (const auto& layer : result.mpd_layers)
		spdlog::debug("field: layer{} {}", layer.far ? " far" : "", layer.mpd);
	return result;
}
