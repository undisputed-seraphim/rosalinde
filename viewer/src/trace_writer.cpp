#include "trace_writer.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>

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
	const auto parent = std::filesystem::path(path).parent_path();
	if (!parent.empty())
		std::filesystem::create_directories(parent);
	_out.open(path, std::ios::binary | std::ios::trunc);
	if (!_out) {
		spdlog::error("cannot open trace file: {}", path);
		return false;
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
	return true;
}

bool TraceWriter::enabled(std::string_view kind) const {
	return _out.is_open() && (_all || _categories.contains(std::string(kind)));
}

void TraceWriter::write(const asb::TraceEvent& event) {
	if (!enabled(event.kind))
		return;
	_out << "{\"frame\":" << event.frame << ",\"kind\":\"" << asb::trace::quote(event.kind) << "\",\"event\":\""
		 << asb::trace::quote(event.event) << '"';
	if (!event.fields.empty())
		_out << ',' << event.fields;
	_out << "}\n";
}

void TraceWriter::write_native_call(const asb::NativeCall& call) {
	if (!enabled("vm"))
		return;
	_out << "{\"frame\":" << call.frame << ",\"kind\":\"vm\",\"event\":\"call\",\"name\":\""
		 << asb::trace::quote(call.name) << "\",\"call\":\"" << asb::trace::quote(call.call) << "\",\"caller\":\""
		 << asb::trace::quote(call.caller) << "\",\"handled\":" << asb::trace::boolean(call.handled);
	if (!call.return_type.empty()) {
		_out << ",\"return_type\":\"" << asb::trace::quote(call.return_type) << "\",\"return\":\""
			 << asb::trace::quote(call.result) << '"';
	}
	_out << "}\n";
}
