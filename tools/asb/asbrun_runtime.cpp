#include "asbrun_runtime.hpp"

#include <scarlett/host.hpp>

#include <criware/acb.hpp>
#include <criware/cpk.hpp>

#include <filesystem>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace asbrun {

std::function<int(int)> load_sound_lengths(const cpk::archive& archive) {
	std::unordered_map<int, int> lengths;
	for (const char* name : {"bgm.acb", "se.acb", "voice_jp.acb", "voice_btl_jp.acb", "voice_add_jp.acb"}) {
		auto entry = archive.find("Sound", name);
		if (entry == archive.end())
			continue;
		std::vector<char> buffer;
		archive.extract(*entry, buffer);
		auto acb = criware::ACB::parse(
			std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size()));
		for (const auto& cue : acb.cues()) {
			const auto* waveform = acb.find_waveform(cue.waveform_id);
			if (!waveform || waveform->sampling_rate == 0)
				continue;
			lengths.emplace(
				static_cast<int>(cue.cue_id),
				static_cast<int>(
					static_cast<double>(waveform->num_samples) / static_cast<double>(waveform->sampling_rate) * 60.0));
		}
	}
	return [lengths = std::move(lengths)](int cue) {
		auto it = lengths.find(cue);
		if (it != lengths.end())
			return it->second;
		it = lengths.find(cue + 0x1ca68);
		return it != lengths.end() ? it->second : 0;
	};
}

void load_runtime_data(
	RuntimeData& data,
	const cpk::archive& archive,
	const std::string& chara_table,
	const std::string& data_dir) {
	if (!chara_table.empty())
		data.chars = asb::load_char_table(chara_table);

	const std::filesystem::path base = data_dir;
	const std::filesystem::path charaset = base / "charaset.tsv";
	const std::filesystem::path classes = base / "classes.tsv";
	const std::filesystem::path charanpc = base / "charanpc_assets.tsv";
	if (std::filesystem::exists(charaset) && std::filesystem::exists(classes)) {
		data.chara_sets = asb::load_chara_set(
			charaset.string(), classes.string(), std::filesystem::exists(charanpc) ? charanpc.string() : std::string());
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

	data.sound_length = load_sound_lengths(archive);
}

void configure_host(asb::Host& host, const RuntimeData& data) {
	if (!data.chars.empty())
		host.set_char_table(data.chars);
	if (!data.chara_sets.by_key.empty())
		host.set_chara_set_table(data.chara_sets);
	host.set_facility_name([&data](const std::string& key) { return data.facilities.resolve(key); });
	host.set_stage_table(data.stages);
	host.set_drop_tables(data.drops);
	if (data.sound_length)
		host.set_sound_length(data.sound_length);
}

} // namespace asbrun
