#pragma once

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

class MBS {
public:
	class data_t;

	MBS(std::istream&);
	MBS(std::istream&&);
	MBS(const MBS&&) = delete;
	MBS(MBS&&) noexcept = default;
	~MBS() noexcept;

	const std::string& filename() const noexcept { return _filename; }

	void dump() const;

private:
	std::unique_ptr<data_t> read(std::istream&);

	std::string _filename;
	std::unique_ptr<data_t> _dataptr;
};
