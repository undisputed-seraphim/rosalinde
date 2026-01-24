#include <criware/substream.hpp>

#include <string>
#include <sstream>
#include <catch2/catch_all.hpp>

TEST_CASE("substreambuf") {
    static const std::string sample = "a very very long string";
    auto ss = std::stringstream(sample);

    static constexpr uint32_t offset = 2, size = 4;
    auto ssbuf = substreambuf(ss.rdbuf(), offset, offset + size);
    std::string buffer(size, '\0');
    auto is = std::istream(&ssbuf);
    is.read(&buffer[0], size);
    REQUIRE(buffer == sample.substr(offset, size));
}
