#include "field_catalog.hpp"

#include "asset_loader.hpp"
#include "tables.hpp"

#include <impl/ufb.hpp>

#include <spdlog/spdlog.h>

#include <cctype>
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

std::optional<StageLayer> FieldCatalog::far_layer(int area, int variant) const {
	if (const auto* exact = find_layer(area, variant, 'f', ".mpd"))
		return StageLayer{path_of(*exact), texture_of(*exact), true};
	for (const auto& asset : _assets) {
		if (extension(asset.main) != ".mpd")
			continue;
		const std::string key = lower(stem(asset.main));
		if (!has_area_stem(key, area))
			continue;
		const std::string token = key.substr(("ac" + two(area) + "_").size());
		if (token.find('f') == std::string::npos || token.find('p') != std::string::npos)
			continue;
		return StageLayer{path_of(asset), texture_of(asset), true};
	}
	return std::nullopt;
}

std::string FieldCatalog::path_of(const Asset& asset) const {
	return normalize(asset.main);
}

std::string FieldCatalog::texture_of(const Asset& asset) const {
	return normalize(asset.texture);
}

std::optional<StageBackground> FieldCatalog::background(int stage_type, int coa_preset, std::string_view entry) {
	switch (stage_type) {
	case 179:
	case 180:
		if (auto it = BattleBGs.find("BGOpening010"); it != BattleBGs.end())
			return StageBackground{join(it->second.mbs), join(it->second.ftx), {}};
		return std::nullopt;
	case 78:
	case 79:
	case 80:
	case 82:
	case 83: {
		// TODO(stage): replace this with the engine capital-stage table.
		// The engine maps the five COA countries to BattleBG/StBtCapital_*.bbs:
		// COA_1 Cornia -> StBtCapital_C, COA_2 Drakenhold -> StBtCapital_D,
		// COA_3 Elheim -> StBtCapital_E, COA_4 Bastorias -> StBtCapital_B,
		// COA_5 Albion -> StBtCapital_A. Each .bbs lists the primary capital
		// MBS/FTX plus roads, night sky and BGEffect layers. Parse UBBS or
		// generate a table from the engine data instead of naming only the
		// primary BGBtCapital asset here. Stage 81/182 are the later war/
		// montage profile; their BGOpening090 track selection is still open.
		static constexpr const char* capital_by_coa[] = {
			nullptr,
			"BGBtCapital_C_a",
			"BGBtCapital_D_a",
			"BGBtCapital_E_a",
			"BGBtCapital_B_01",
			"BGBtCapital_A_a",
		};
		const char* key = coa_preset >= 1 && coa_preset <= 5 ? capital_by_coa[coa_preset] : "BGOpening090";
		if (auto it = BattleBGs.find(key); it != BattleBGs.end())
			return StageBackground{join(it->second.mbs), join(it->second.ftx), {}};
		return std::nullopt;
	}
	case 81:
	case 182:
		if (auto it = BattleBGs.find("BGOpening090"); it != BattleBGs.end())
			return StageBackground{join(it->second.mbs), join(it->second.ftx), {}};
		return std::nullopt;
	default:
		break;
	}

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

	StageBackground result;
	if (mbs)
		result.mbs = path_of(*mbs);
	const Asset& texture_asset = mbs ? *mbs : *mpd;
	result.ftx = texture_of(texture_asset);

	if (const auto far = far_layer(area, variant))
		result.layers.push_back(*far);
	if (mpd && (!mbs || path_of(*mpd) != result.mbs))
		result.layers.push_back(StageLayer{path_of(*mpd), texture_of(*mpd), false});

	const std::string row_name = row ? row->name : std::string("<none>");
	spdlog::debug(
		"field: type={} stage={} country={} area=AC{} variant={} mbs={} layers={}",
		stage_type,
		row_name,
		row ? row->country : -1,
		two(area),
		variant,
		result.mbs.empty() ? "<none>" : result.mbs,
		result.layers.size());
	for (const auto& layer : result.layers)
		spdlog::debug("field: layer{} {}", layer.far ? " far" : "", layer.mpd);
	return result;
}
