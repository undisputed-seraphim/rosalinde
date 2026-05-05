#pragma once

#include <cstdint>
#include <istream>
#include <span>
#include <string>
#include <vector>

#include "mbs/sections.hpp"

namespace mbs {

enum class Version : uint16_t {
	v66 = 0x66,
	v6b = 0x6b,
	v6d = 0x6d,
	v6e = 0x6e,
	v72 = 0x72,
	v76 = 0x76,
	v77 = 0x77,
};

} // namespace mbs

class MBS {
public:
	MBS();
	MBS(const MBS&&) = delete;
	MBS(MBS&&) noexcept;
	~MBS() noexcept;

	static MBS From(std::istream&);
	static MBS From(const std::vector<char>&);
	static MBS From(std::span<const uint8_t>);

	// Primary parse — works on a full file buffer.
	void parse(std::span<const uint8_t>);

	// Convenience — drains the stream, delegates to the span path.
	void parse(std::istream&);

	const std::string& filename() const noexcept { return _filename; }
	mbs::Version version() const noexcept { return _version; }

	const mbs::v77& get() const;

private:
	std::string _filename;
	mbs::Version _version = mbs::Version::v77;
	mbs::v77 data;
};
