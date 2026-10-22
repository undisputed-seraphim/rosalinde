#pragma once

#include <string>

// Return the directory containing generated runtime tables. Build-tree data is
// preferred while it exists; an installed binary falls back to the install
// location. Explicit command-line and environment overrides are handled by the
// callers before this default is consulted.
std::string rosalinde_data_dir();
