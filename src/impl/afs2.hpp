#pragma once

#include <iosfwd>
#include <vector>

namespace AFS2 {

struct Entry {
	uint32_t id;
	uint32_t offset;
	uint32_t size;
};

class AFS2 {
public:
	AFS2(std::istream&& acb, std::istream&& awb);


private:
	std::vector<Entry> _awb_entries;
};

} // namespace AFS2
