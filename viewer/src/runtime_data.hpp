#pragma once

#include <scarlett/asb.hpp>

#include <functional>
#include <string>

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
	std::function<int(int)> sound_length;
};

ViewerRuntimeData
load_viewer_runtime_data(const cpk::archive& archive, const std::string& chara_table, const std::string& data_dir);

void configure_host(asb::Host& host, const ViewerRuntimeData& data);
