#pragma once

#include <scarlett/asb.hpp>
#include <scarlett/text_timing.hpp>

#include <functional>
#include <memory>
#include <string>

namespace viewer {
class AudioBanks;
class VoiceLines;
} // namespace viewer

namespace cpk {
class archive;
}

namespace asb {
class Host;
}

struct ViewerRuntimeData {
	asb::CharTable chars;
	asb::CharaSetTable chara_sets;
	asb::FacilityNames facilities;
	asb::StageTable stages;
	asb::DropTables drops;
	std::shared_ptr<viewer::AudioBanks> audio_banks;
	std::shared_ptr<viewer::VoiceLines> voice_lines;
	std::function<int(int)> sound_length;
	asb::MessageTiming message_timing;
};

ViewerRuntimeData
load_viewer_runtime_data(const cpk::archive& archive, const std::string& chara_table, const std::string& data_dir);

void configure_host(asb::Host& host, const ViewerRuntimeData& data);
