#pragma once

#include <impl/mbs/sections.hpp>

#include <cstdint>
#include <vector>

// Track liveness is external game logic. Landscape backgrounds select all
// independent element tracks; state backgrounds select one mutually-exclusive
// state. The selector is responsible for excluding unavailable variants.
std::vector<uint32_t> select_background_tracks(const mbs::v77& v77);
