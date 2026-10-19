#pragma once

#include <scarlett/asb.hpp>
#include <scarlett/trace.hpp>

#include <cstdio>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

namespace asb::diag {

struct CallStat {
	size_t calls = 0;
	size_t stubs = 0;
	long first = -1;
	long last = -1;
};

class CallCensus {
public:
	void observe(const NativeCall& call) {
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

	const std::map<std::string, CallStat>& calls() const { return _calls; }

private:
	std::map<std::string, CallStat> _calls;
};

namespace detail {

inline std::string native_name(const Signature& signature) {
	if (signature.name_space.empty())
		return signature.name;
	return signature.name_space + "::" + signature.name;
}

inline bool is_addon_native(const Signature& signature) {
	if (signature.name.starts_with("$"))
		return true;
	if (signature.object_type) {
		const std::string& type = signature.object_type->name;
		if (type == "string" || type.starts_with("array"))
			return true;
	}
	return false;
}

inline std::map<std::string, std::string> imported_natives(const Module& module) {
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

inline std::string json_escape(std::string_view value) {
	std::string out;
	out.reserve(value.size() + 8);
	for (unsigned char c : value) {
		switch (c) {
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			if (c < 0x20) {
				char buf[8];
				std::snprintf(buf, sizeof(buf), "\\u%04x", c);
				out += buf;
			} else {
				out += static_cast<char>(c);
			}
		}
	}
	return out;
}

} // namespace detail

inline std::string call_census_text(
	const Module& module,
	const CallCensus& census,
	std::string_view source,
	std::string_view function) {
	const auto imported = detail::imported_natives(module);
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

inline std::string call_census_json(
	const Module& module,
	const CallCensus& census,
	std::string_view source,
	std::string_view function) {
	const auto imported = detail::imported_natives(module);
	std::string json =
		"{\"source\":\"" + detail::json_escape(source) + "\",\"function\":\"" + detail::json_escape(function) +
		"\",\"native\":[";
	bool first = true;
	for (const auto& [name, stat] : census.calls()) {
		json += first ? "" : ",";
		first = false;
		json += "{\"name\":\"" + detail::json_escape(name) + "\"";
		if (auto it = imported.find(name); it != imported.end())
			json += ",\"decl\":\"" + detail::json_escape(it->second) + "\"";
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
		json += "{\"name\":\"" + detail::json_escape(name) + "\",\"decl\":\"" + detail::json_escape(declaration) +
				"\"}";
	}
	json += "],\"stubs\":[";
	first = true;
	for (const auto& [name, stat] : census.calls()) {
		if (stat.stubs == 0)
			continue;
		json += first ? "" : ",";
		first = false;
		json += "{\"name\":\"" + detail::json_escape(name) + "\",\"calls\":" + std::to_string(stat.calls) +
				",\"stubs\":" + std::to_string(stat.stubs) + "}";
	}
	json += "]}";
	return json;
}

} // namespace asb::diag
