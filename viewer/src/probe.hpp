#pragma once

#include <scarlett/scene.hpp>

#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class ProbeWriter {
public:
	bool open(const std::string& dir, const std::vector<long>& frames);
	bool enabled() const { return _enabled; }
	bool done() const { return !_frames.empty() && (_enabled ? _written.size() >= _frames.size() : true); }
	bool written(long frame) const { return _written.contains(frame); }
	bool wants(long frame) const;
	bool begin(long frame);
	void end();
	std::ostream& draw() { return _draw; }
	bool write_scene(long frame, std::string_view json, std::string_view summary);
	void write_state(long frame, std::string_view json);
	void write_summary(long frame, std::string_view text);

private:
	std::string path(std::string_view prefix, long frame, std::string_view extension) const;

	std::string _dir;
	std::set<long> _frames;
	std::set<long> _written;
	std::ofstream _draw;
	long _frame = -1;
	bool _enabled = false;
};

std::string scene_state_json(const asb::SceneState& scene);
std::string scene_state_text(const asb::SceneState& scene);
