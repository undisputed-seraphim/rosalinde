#include "asbrun_trace.hpp"

#include "asbrun_common.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace asbrun {

TraceFilter::TraceFilter(std::string_view categories) {
	_all = categories.empty() || categories == "all";
	while (!categories.empty()) {
		const size_t comma = categories.find(',');
		const std::string_view item = trim(categories.substr(0, comma));
		if (!item.empty())
			_categories.emplace(item);
		if (comma == std::string_view::npos)
			break;
		categories.remove_prefix(comma + 1);
	}
	_all = _all || _categories.contains("all");
}

bool TraceFilter::contains(std::string_view kind) const { return _all || _categories.contains(std::string(kind)); }

void write_trace_event(std::ostream& out, const asb::TraceEvent& event) {
	out << "{\"frame\":" << event.frame << ",\"kind\":\"" << asb::trace::quote(event.kind) << "\",\"event\":\""
		<< asb::trace::quote(event.event) << '"';
	if (!event.fields.empty())
		out << ',' << event.fields;
	out << "}\n";
}

void write_native_call(std::ostream& out, const asb::NativeCall& call) {
	out << "{\"frame\":" << call.frame << ",\"kind\":\"vm\",\"event\":\"call\",\"name\":\""
		<< asb::trace::quote(call.name) << "\",\"call\":\"" << asb::trace::quote(call.call) << "\",\"caller\":\""
		<< asb::trace::quote(call.caller) << "\",\"handled\":" << asb::trace::boolean(call.handled);
	if (!call.return_type.empty()) {
		out << ",\"return_type\":\"" << asb::trace::quote(call.return_type) << "\",\"return\":\""
			<< asb::trace::quote(call.result) << '"';
	}
	out << "}\n";
}

void CallCensus::observe(const asb::NativeCall& call) {
	if (call.name.starts_with("$"))
		return;
	auto& stat = _calls[call.name];
	++stat.calls;
	if (!call.handled)
		++stat.stubs;
	if (stat.first < 0)
		stat.first = call.frame;
	stat.last = call.frame;
}

namespace {

std::string native_name(const asb::Signature& signature) {
	if (signature.name_space.empty())
		return signature.name;
	return signature.name_space + "::" + signature.name;
}

bool is_addon_native(const asb::Signature& signature) {
	if (signature.name.starts_with("$"))
		return true;
	if (signature.object_type) {
		const std::string& type = signature.object_type->name;
		if (type == "string" || type.starts_with("array"))
			return true;
	}
	return false;
}

std::map<std::string, std::string> imported_natives(const asb::Module& module) {
	std::map<std::string, std::string> imported;
	for (size_t i = 0; i < module.used_functions.size(); ++i) {
		if (i >= module.used_function_kind.size() || module.used_function_kind[i] != 'a' || !module.used_functions[i])
			continue;
		const auto& signature = module.used_functions[i]->sig;
		if (is_addon_native(signature))
			continue;
		imported.emplace(native_name(signature), signature.decl());
	}
	return imported;
}

} // namespace

std::string call_census_text(
	const asb::Module& module,
	const CallCensus& census,
	std::string_view source,
	std::string_view function) {
	const auto imported = imported_natives(module);
	std::ostringstream out;
	out << "source: " << source << '\n';
	out << "function: " << function << '\n';
	out << std::left << std::setw(38) << "native" << std::right << std::setw(8) << "calls" << std::setw(8) << "stubs"
		<< std::setw(8) << "first" << std::setw(8) << "last" << '\n';
	for (const auto& [name, stat] : census.calls()) {
		out << std::left << std::setw(38) << name << std::right << std::setw(8) << stat.calls << std::setw(8)
			<< stat.stubs << std::setw(8) << stat.first << std::setw(8) << stat.last << '\n';
	}

	size_t uncalled_count = 0;
	for (const auto& [name, _] : imported)
		if (!census.calls().contains(name))
			++uncalled_count;
	out << "\nuncalled natives (" << uncalled_count << "):\n";
	for (const auto& [name, declaration] : imported) {
		if (census.calls().contains(name))
			continue;
		out << "  " << name << '\n';
		if (!declaration.empty())
			out << "    " << declaration << '\n';
	}

	size_t stub_count = 0;
	for (const auto& [_, stat] : census.calls())
		if (stat.stubs > 0)
			++stub_count;
	out << "\nunhandled natives (" << stub_count << "):\n";
	for (const auto& [name, stat] : census.calls())
		if (stat.stubs > 0)
			out << "  " << name << "  calls=" << stat.calls << " stubs=" << stat.stubs << '\n';
	return out.str();
}

std::string call_census_json(
	const asb::Module& module,
	const CallCensus& census,
	std::string_view source,
	std::string_view function) {
	const auto imported = imported_natives(module);
	std::string json =
		"  {\"source\":\"" + json_escape(source) + "\",\"function\":\"" + json_escape(function) + "\",\"native\":[";
	bool first = true;
	for (const auto& [name, stat] : census.calls()) {
		json += first ? "" : ",";
		first = false;
		json += "{\"name\":\"" + json_escape(name) + "\"";
		if (auto it = imported.find(name); it != imported.end())
			json += ",\"decl\":\"" + json_escape(it->second) + "\"";
		json += ",\"calls\":" + std::to_string(stat.calls) + ",\"stubs\":" + std::to_string(stat.stubs) +
				",\"first\":" + std::to_string(stat.first) + ",\"last\":" + std::to_string(stat.last) + "}";
	}
	json += "],\"uncalled\":[";
	first = true;
	for (const auto& [name, declaration] : imported) {
		if (census.calls().contains(name))
			continue;
		json += first ? "" : ",";
		first = false;
		json += "{\"name\":\"" + json_escape(name) + "\",\"decl\":\"" + json_escape(declaration) + "\"}";
	}
	json += "],\"stubs\":[";
	first = true;
	for (const auto& [name, stat] : census.calls()) {
		if (stat.stubs == 0)
			continue;
		json += first ? "" : ",";
		first = false;
		json += "{\"name\":\"" + json_escape(name) + "\",\"calls\":" + std::to_string(stat.calls) +
				",\"stubs\":" + std::to_string(stat.stubs) + "}";
	}
	json += "]}";
	return json;
}

} // namespace asbrun
