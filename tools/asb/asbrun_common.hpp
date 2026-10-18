#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cpk {
class archive;
}

namespace asbrun {

std::vector<uint8_t> read_file(const std::string& path);
std::vector<uint8_t> extract_entry(const cpk::archive& archive, std::string_view path);
std::vector<uint8_t> extract_entry(const std::string& cpk_path, std::string_view path);

std::string json_escape(std::string_view value);
std::string lower(std::string value);
bool ends_with(std::string_view value, std::string_view suffix);
std::string_view trim(std::string_view value);

} // namespace asbrun
