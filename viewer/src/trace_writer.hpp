#pragma once

#include <scarlett/trace.hpp>

#include <fstream>
#include <string>
#include <string_view>
#include <unordered_set>

class TraceWriter {
public:
	bool open(const std::string& path, std::string_view categories);
	bool enabled(std::string_view kind) const;
	void write(const asb::TraceEvent& event);
	void write_native_call(const asb::NativeCall& call);

private:
	bool _all = false;
	std::ofstream _out;
	std::unordered_set<std::string> _categories;
};
