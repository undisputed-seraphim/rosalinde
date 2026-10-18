#pragma once

#include <scarlett/asb.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace asbrun {

std::string harvest_json(const asb::Module& module, const std::string& name, size_t size, bool summary);
std::string scene_json(const asb::Module& module, const std::string& name);
std::string scene_index_json(const asb::Module& module, const std::string& name);
std::string scene_ir_json(const asb::Module& module, const std::string& name, const asb::CharTable* chars);
bool is_asset(std::string_view value);

} // namespace asbrun
