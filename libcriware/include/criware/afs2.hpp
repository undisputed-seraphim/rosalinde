#pragma once

#include <cstdint>
#include <iosfwd>
#include <vector>

class AFS2 {
private:
	struct Entry {
		uint16_t id;
		uint32_t offset;
		uint32_t size;
		char fileName[128];
	};
	std::vector<Entry> _entries;

public:
	AFS2() {}
	AFS2(const AFS2&) = delete;
	AFS2(AFS2&&) = default;

	friend std::istream& operator>>(std::istream&, AFS2&);
};
