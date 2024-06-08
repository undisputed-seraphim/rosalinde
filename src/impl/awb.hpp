#pragma once

#include <cstdint>
#include <iosfwd>

class AFS2 {
public:
#pragma pack(push, 1)
    struct header {
        char afs2[4];
        uint8_t version;
        uint8_t offset_size;
        uint16_t id_size;
        uint32_t numfiles;
        uint16_t alignment;
        uint16_t subkey;
    };
#pragma pack(pop)

    AFS2(std::istream&);
    AFS2(std::istream&&);

private:

};