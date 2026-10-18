#pragma once

#include <scarlett/scene.hpp>

#include <cstdio>

namespace asbrun {

void write_timeline_json(FILE* out, const asb::Timeline& timeline);

} // namespace asbrun
