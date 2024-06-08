#pragma once

#include <iosfwd>

class ACB {
public:
    ACB(std::istream&);
    ACB(std::istream&&);
};