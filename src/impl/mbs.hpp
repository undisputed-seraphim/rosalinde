#pragma once

#include <string>
#include <vector>

#include "mbs/sections.hpp"

class MBS {
public:
	MBS();
	MBS(const MBS&&) = delete;
	MBS(MBS&&) noexcept;
	~MBS() noexcept;

	static MBS From(std::istream&);
	static MBS From(const std::vector<char>&);

	void parse(std::istream&);

	const std::string& filename() const noexcept { return _filename; }

	const mbs::v77& get() const;

private:
	std::string _filename;
	mbs::v77 data;
};
