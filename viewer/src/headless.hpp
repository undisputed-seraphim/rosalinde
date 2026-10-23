#pragma once

#include "viewer_config.hpp"

// Run a scene without creating a window or GL context. Reuses PlaybackRuntime
// so the same Host providers, prelude and Wait scheduler are active.
int run_headless(const ViewerConfig& config);
