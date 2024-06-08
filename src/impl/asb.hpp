#pragma once

#include <iosfwd>

// This seems to be a compiled binary.
// File names of the original source file is embedded within, extension .ascp.
// Some of them contain a MAIN function.

class ASB {
public:
	ASB(std::istream&);

private:
	void parse(std::istream&);
};