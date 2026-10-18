#pragma once

#include <cstddef>
#include <string>

namespace cpk {
class archive;
}

namespace asbrun {

struct ScanOptions {
	bool summary = false;
	bool scene = false;
	bool scene_index = false;
	bool scene_ir = false;
	bool disasm = false;
	std::string disasm_dir;
	std::string json_path;
	std::string filter;
	std::string chara_file;
	size_t limit = 0;
};

int scan(const cpk::archive& archive, const ScanOptions& options);

int export_chara(const cpk::archive& archive, const std::string& out_path);
int export_enums(const cpk::archive& archive, const std::string& out_path);
int export_bgs(const cpk::archive& archive, const std::string& out_path);

} // namespace asbrun
