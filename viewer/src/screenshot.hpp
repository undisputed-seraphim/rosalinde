#pragma once

#include <cstdint>
#include <filesystem>
#include <span>

namespace screenshot {

bool write_png(const std::filesystem::path& path, uint32_t width, uint32_t height, std::span<const uint8_t> rgba);
bool write_ppm(const std::filesystem::path& path, uint32_t width, uint32_t height, std::span<const uint8_t> rgba);

bool write(const std::filesystem::path& path, uint32_t width, uint32_t height, std::span<const uint8_t> rgba);

} // namespace screenshot
