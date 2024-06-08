#include "otm.hpp"
#include "mbs.hpp"

#include <istream>
#include <string_view>

#pragma pack(push, 1)
struct header {
    char otbm[4];
    uint32_t datasize;
    uint32_t headersize;
    char filename[32];

    static constexpr std::string_view MAGIC = "OTBM";
};
#pragma pack(pop)

OTM::OTM(std::istream& is) {
    
}