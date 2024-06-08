#pragma once

#include <iosfwd>

// NOTES ON OTBM
// Magic: 'OTBM'
// Followed by data size (4 bytes)
// Header size is 120 bytes total.
// Possibly 24 ~ 32 bytes string for file name

class OTM {
public:
    OTM(std::istream&);
};
