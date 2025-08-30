#pragma once

#include <array>
#include <glm/glm.hpp>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "mbs/sections.hpp"

class MBS {
public:
	MBS(std::istream&);
	MBS(std::istream&&);
	MBS(const MBS&&) = delete;
	MBS(MBS&&) noexcept;
	~MBS() noexcept;

	const std::string& filename() const noexcept { return _filename; }

	const mbs::v77& get() const;

private:
	class data_t;
	mbs::v77 read(std::istream&);

	std::string _filename;
	//std::unique_ptr<data_t> _dataptr;
	mbs::v77 data;
};
