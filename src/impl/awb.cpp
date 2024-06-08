#include "awb.hpp"
#include "../utils.hpp"

#include <cstring>

AFS2::AFS2(std::istream& is) {}

AFS2::AFS2(std::istream&& is) {

}

void parse(std::istream& is) {
    const auto hdr = read_value<AFS2::header>(is);
    if (::strncmp(hdr.afs2, "AFS2", sizeof(hdr)) != 0) {
        return;
    }
    if (hdr.numfiles == 0) {
        return;
    }
    const uint64_t offset = is.tellg();

    
}
