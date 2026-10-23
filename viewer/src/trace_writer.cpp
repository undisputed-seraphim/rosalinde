#include "trace_writer.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <utility>

namespace {

std::string_view trim(std::string_view value) {
	while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
		value.remove_prefix(1);
	while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
		value.remove_suffix(1);
	return value;
}

} // namespace

bool TraceWriter::open(const std::string& path, std::string_view categories) {
	_out = nullptr;
	_file = std::ofstream{};
	if (path == "-") {
		_out = &std::cout;
	} else {
		const auto parent = std::filesystem::path(path).parent_path();
		if (!parent.empty())
			std::filesystem::create_directories(parent);
		_file.open(path, std::ios::binary | std::ios::trunc);
		if (!_file) {
			spdlog::error("cannot open trace file: {}", path);
			return false;
		}
		_out = &_file;
	}
	_categories.clear();
	_all = categories.empty() || categories == "all";
	while (!categories.empty()) {
		const auto comma = categories.find(',');
		const auto item = trim(categories.substr(0, comma));
		if (!item.empty())
			_categories.emplace(item);
		if (comma == std::string_view::npos)
			break;
		categories.remove_prefix(comma + 1);
	}
	if (_categories.contains("all"))
		_all = true;
	_written = 0;
	return true;
}

void TraceWriter::set_filter(std::vector<long> frames, std::vector<std::string> objects, int limit) {
	_frames = std::move(frames);
	_objects = std::move(objects);
	_limit = std::max(0, limit);
	std::sort(_frames.begin(), _frames.end());
	_frames.erase(std::unique(_frames.begin(), _frames.end()), _frames.end());
}

bool TraceWriter::enabled(std::string_view kind) const {
	return _out != nullptr && (_all || _categories.contains(std::string(kind)));
}

bool TraceWriter::frame_allowed(long frame) const {
	return _frames.empty() || std::binary_search(_frames.begin(), _frames.end(), frame);
}

bool TraceWriter::object_allowed(std::string_view fields, std::string_view call) const {
	if (_objects.empty())
		return true;
	const bool has_target_field =
		fields.find("\"object\"") != std::string_view::npos || fields.find("\"name\"") != std::string_view::npos;
	if (!has_target_field && call.empty())
		return true;
	for (const auto& object : _objects) {
		if (fields.find(object) != std::string_view::npos || call.find(object) != std::string_view::npos)
			return true;
	}
	return false;
}

bool TraceWriter::emit_allowed(long frame, std::string_view fields, std::string_view call) const {
	if (!frame_allowed(frame) || !object_allowed(fields, call))
		return false;
	return _limit == 0 || _written < static_cast<size_t>(_limit);
}

void TraceWriter::count_event() { ++_written; }

void TraceWriter::write(const asb::TraceEvent& event) {
	if (!enabled(event.kind) || !emit_allowed(event.frame, event.fields))
		return;
	*_out << "{\"frame\":" << event.frame << ",\"kind\":\"" << asb::trace::quote(event.kind) << "\",\"event\":\""
		  << asb::trace::quote(event.event) << '"';
	if (!event.fields.empty())
		*_out << ',' << event.fields;
	*_out << "}\n";
	count_event();
}

void TraceWriter::write_native_call(const asb::NativeCall& call) {
	if (!enabled("vm") || !emit_allowed(call.frame, call.name + " " + call.call, call.call))
		return;
	*_out << "{\"frame\":" << call.frame << ",\"kind\":\"vm\",\"event\":\"call\",\"name\":\""
		  << asb::trace::quote(call.name) << "\",\"call\":\"" << asb::trace::quote(call.call) << "\",\"caller\":\""
		  << asb::trace::quote(call.caller) << "\",\"handled\":" << asb::trace::boolean(call.handled);
	if (!call.return_type.empty()) {
		*_out << ",\"return_type\":\"" << asb::trace::quote(call.return_type) << "\",\"return\":\""
			  << asb::trace::quote(call.result) << '"';
	}
	*_out << "}\n";
	count_event();
}
