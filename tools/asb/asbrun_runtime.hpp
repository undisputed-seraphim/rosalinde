#pragma once

#include <scarlett/asb.hpp>

#include <cstdint>
#include <functional>
#include <string>

namespace cpk {
class archive;
}

namespace asb {
class Host;
}

namespace asbrun {

struct RuntimeData {
	asb::CharTable chars;
	asb::CharaSetTable chara_sets;
	asb::FacilityNames facilities;
	asb::StageTable stages;
	asb::DropTables drops;
	std::function<int(int)> sound_length;
};

std::function<int(int)> load_sound_lengths(const cpk::archive& archive);

void load_runtime_data(
	RuntimeData& data,
	const cpk::archive& archive,
	const std::string& chara_table = {},
	const std::string& data_dir = {});

void configure_host(asb::Host& host, const RuntimeData& data);

} // namespace asbrun
