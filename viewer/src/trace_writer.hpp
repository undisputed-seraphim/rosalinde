#pragma once

#include <scarlett/trace.hpp>

#include <fstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

class TraceWriter {
public:
	bool open(const std::string& path, std::string_view categories);
	void set_filter(std::vector<long> frames, std::vector<std::string> objects, int limit);
	bool enabled(std::string_view kind) const;
	void write(const asb::TraceEvent& event);
	void write_native_call(const asb::NativeCall& call);

private:
	bool frame_allowed(long frame) const;
	bool object_allowed(std::string_view fields, std::string_view call = {}) const;
	bool emit_allowed(long frame, std::string_view fields, std::string_view call = {}) const;
	void count_event();

	bool _all = false;
	std::ofstream _out;
	std::unordered_set<std::string> _categories;
	std::vector<long> _frames;
	std::vector<std::string> _objects;
	int _limit = 0;
	size_t _written = 0;
};
