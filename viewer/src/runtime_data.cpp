#include "runtime_data.hpp"

#include "audio_banks.hpp"
#include "voice_lines.hpp"

#include <data_path.hpp>

#include <scarlett/host.hpp>

#include <criware/cpk.hpp>
#include <impl/dbreg.hpp>

#include <spdlog/spdlog.h>

#include <bit>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

bool read_cpk_entry(const cpk::archive& archive, const char* name, std::vector<char>& out) {
	const auto entry = archive.find("DbReg", name);
	if (entry == archive.end())
		return false;
	try {
		archive.extract(*entry, out);
	} catch (const std::exception&) {
		return false;
	}
	return true;
}

std::optional<asb::MessageTiming> load_message_timing(const cpk::archive& archive) {
	std::vector<char> catalog_bytes;
	std::vector<char> value_bytes;
	if (!read_cpk_entry(archive, "DbRegDb.drg", catalog_bytes) ||
		!read_cpk_entry(archive, "DbReg.drg", value_bytes))
		return std::nullopt;

	const auto as_bytes = [](const std::vector<char>& bytes) {
		return std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
	};
	const auto catalog = dbreg::parse(as_bytes(catalog_bytes));
	const auto values = dbreg::parse(as_bytes(value_bytes));
	if (!catalog || !values || catalog->type != dbreg::kind::catalog || values->type != dbreg::kind::values)
		return std::nullopt;

	asb::MessageTiming timing;
	int voice_off_seen = 0;
	for (size_t i = 0; i < catalog->keys.size(); ++i) {
		const auto key = catalog->keys[i].key_view();
		int value = 0;
		if (i < values->values.size())
			value = static_cast<int>(std::lround(std::bit_cast<float>(values->values[i])));
		else
			value = static_cast<int>(std::lround(catalog->keys[i].default_value));

		if (key == "VOICE_WAIT_MIN")
			timing.voice_wait_min = value;
		else if (key == "PERIOD_WAIT_TIME_VOICE_OFF_MIN")
			timing.voice_off_min = value;
		else if (key == "PERIOD_WAIT_TIME_VOICE_OFF") {
			if (voice_off_seen++ == 0)
				timing.voice_off_per_char = value;
			else
				timing.sprite_per_char = value;
		} else if (key == "PERIOD_WAIT_TIME_VOICE_ON")
			timing.voice_on_extra = value;
		else if (key == "SELECTMSG_WAIT")
			timing.select_wait = value;
	}

	spdlog::debug(
		"dbreg: message timing voice_min={} off={}/char off_min={} on={} sprite={}/char select={}",
		timing.voice_wait_min,
		timing.voice_off_per_char,
		timing.voice_off_min,
		timing.voice_on_extra,
		timing.sprite_per_char,
		timing.select_wait);
	return timing;
}

} // namespace

ViewerRuntimeData
load_viewer_runtime_data(const cpk::archive& archive, const std::string& chara_table, const std::string& data_dir) {
	ViewerRuntimeData data;
	if (!chara_table.empty())
		data.chars = asb::load_char_table(chara_table);

	const std::filesystem::path base = data_dir.empty() ? rosalinde_data_dir() : data_dir;
	if (!std::filesystem::exists(base))
		throw std::runtime_error("runtime data directory not found: " + base.string());
	for (const char* name :
		 {"charaset.tsv",
		  "classes.tsv",
		  "stages.tsv",
		  "facility_pos.tsv",
		  "drop_tables.tsv",
		  "uc_enums.tsv",
		  "battlebgs.tsv"}) {
		if (!std::filesystem::exists(base / name))
			throw std::runtime_error("runtime data is incomplete: " + (base / name).string());
	}
	const std::filesystem::path charaset = base / "charaset.tsv";
	const std::filesystem::path classes = base / "classes.tsv";
	const std::filesystem::path charanpc = base / "charanpc_assets.tsv";
	const std::filesystem::path chara_colors = base / "chara_colors.tsv";
	if (std::filesystem::exists(charaset) && std::filesystem::exists(classes)) {
		data.chara_sets = asb::load_chara_set(
			charaset.string(),
			classes.string(),
			std::filesystem::exists(charanpc) ? charanpc.string() : std::string(),
			std::filesystem::exists(chara_colors) ? chara_colors.string() : std::string());
	}

	const std::filesystem::path enums = base / "uc_enums.tsv";
	if (std::filesystem::exists(enums))
		data.facilities = asb::load_facility_names(enums.string());

	const std::filesystem::path stages = base / "stages.tsv";
	const std::filesystem::path facility_pos = base / "facility_pos.tsv";
	const std::filesystem::path regions = base / "country_regions.tsv";
	if (std::filesystem::exists(stages) && std::filesystem::exists(facility_pos)) {
		data.stages = asb::load_stage_tables(
			stages.string(),
			facility_pos.string(),
			std::filesystem::exists(regions) ? regions.string() : std::string());
	}

	const std::filesystem::path drops = base / "drop_tables.tsv";
	if (std::filesystem::exists(drops) && std::filesystem::exists(enums))
		data.drops = asb::load_drop_tables(drops.string(), enums.string());

	data.audio_banks = std::make_shared<viewer::AudioBanks>(archive);
	data.sound_length = [banks = data.audio_banks](int cue) {
		return cue > 0 ? banks->cue_length_frames(static_cast<uint32_t>(cue)) : 0;
	};
	if (const auto timing = load_message_timing(archive))
		data.message_timing = *timing;

	data.voice_lines = std::make_shared<viewer::VoiceLines>();
	std::filesystem::path voice_path = base / "voice_line_table.tsv";
	if (!std::filesystem::exists(voice_path) && base.filename() == "generated")
		voice_path = base.parent_path() / "voice_line_table.tsv";
	if (!data.voice_lines->load(voice_path))
		data.voice_lines.reset();
	return data;
}

void configure_host(asb::Host& host, const ViewerRuntimeData& data) {
	if (!data.chars.empty())
		host.set_char_table(data.chars);
	if (!data.chara_sets.by_key.empty())
		host.set_chara_set_table(data.chara_sets);
	host.set_facility_name([&facilities = data.facilities](const std::string& key) { return facilities.resolve(key); });
	host.set_stage_table(data.stages);
	host.set_drop_tables(data.drops);
	if (data.sound_length)
		host.set_sound_length(data.sound_length);
	host.set_message_timing(data.message_timing);
}
