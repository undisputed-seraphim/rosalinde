#include "data_path.hpp"

#include <cstdlib>
#include <filesystem>

#ifndef ROSALINDE_BUILD_DATA_DIR
#define ROSALINDE_BUILD_DATA_DIR "viewer/data/generated"
#endif

#ifndef ROSALINDE_INSTALL_DATA_DIR
#define ROSALINDE_INSTALL_DATA_DIR "share/rosalinde"
#endif

std::string rosalinde_data_dir() {
	if (const char* value = std::getenv("ROSALINDE_DATA_DIR"))
		return value;
	if (const char* value = std::getenv("VIEWER_DATA"))
		return value;
#ifdef __linux__
	std::error_code error;
	const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
	if (!error) {
		const auto installed = executable.parent_path().parent_path() / "share" / "rosalinde";
		if (std::filesystem::exists(installed))
			return installed.string();
	}
#endif
	if (std::filesystem::exists(ROSALINDE_BUILD_DATA_DIR))
		return ROSALINDE_BUILD_DATA_DIR;
	return ROSALINDE_INSTALL_DATA_DIR;
}
