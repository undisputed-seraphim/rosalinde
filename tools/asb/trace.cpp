#include <scarlett/asb.hpp>
#include <scarlett/host.hpp>
#include <scarlett/runtime.hpp>
#include <scarlett/trace.hpp>

#include <criware/cpk.hpp>

#include <angelscript.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {

std::vector<uint8_t> read_file(const std::string& path) {
	std::ifstream f(path, std::ios::binary);
	if (!f)
		throw std::runtime_error("cannot open " + path);
	return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

std::vector<uint8_t> extract_entry(const std::string& cpk, const std::string& entry) {
	auto pos = entry.find_last_of('/');
	std::string dir = pos == std::string::npos ? "" : entry.substr(0, pos);
	std::string name = pos == std::string::npos ? entry : entry.substr(pos + 1);
	CPKTable table = TopLevelCpk(std::filesystem::absolute(cpk)).getTableOfContents();
	std::vector<char> buf;
	if (auto e = table.find_file(dir, name); e == table.end())
		throw std::runtime_error("not found in CPK: " + entry);
	else
		table.extract(*e, buf);
	return std::vector<uint8_t>(buf.begin(), buf.end());
}

std::string_view trim(std::string_view value) {
	while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
		value.remove_prefix(1);
	while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
		value.remove_suffix(1);
	return value;
}

struct Filter {
	bool all = false;
	std::unordered_set<std::string> categories;

	explicit Filter(std::string_view text) {
		all = text.empty() || text == "all";
		while (!text.empty()) {
			const auto comma = text.find(',');
			const auto item = trim(text.substr(0, comma));
			if (!item.empty())
				categories.emplace(item);
			if (comma == std::string_view::npos)
				break;
			text.remove_prefix(comma + 1);
		}
		all = all || categories.contains("all");
	}

	bool contains(std::string_view kind) const { return all || categories.contains(std::string(kind)); }
};

void write_event(std::ostream& out, const asb::TraceEvent& event) {
	out << "{\"frame\":" << event.frame << ",\"kind\":\"" << asb::trace::quote(event.kind) << "\",\"event\":\""
		<< asb::trace::quote(event.event) << '"';
	if (!event.fields.empty())
		out << ',' << event.fields;
	out << "}\n";
}

} // namespace

int main(int argc, char** argv) try {
	std::string input, cpk, entry, func, out_path, categories;
	long frames = 200000;
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		auto next = [&]() { return (i + 1 < argc) ? argv[++i] : ""; };
		if (a == "--input")
			input = next();
		else if (a == "--cpk")
			cpk = next();
		else if (a == "--asb")
			entry = next();
		else if (a == "--func")
			func = next();
		else if (a == "--frames")
			frames = std::stol(next());
		else if (a == "--out")
			out_path = next();
		else if (a == "--categories" || a == "--trace-categories")
			categories = next();
		else {
			std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
			return 2;
		}
	}

	std::vector<uint8_t> bytes = !input.empty() ? read_file(input) : extract_entry(cpk, entry);
	if (bytes.empty())
		throw std::runtime_error("no input");

	asb::Module mod = asb::parse(std::span<const uint8_t>(bytes.data(), bytes.size()));
	if (!mod.error.empty())
		throw std::runtime_error("decode failed: " + mod.error);

	std::ofstream file;
	std::ostream* out = &std::cout;
	if (!out_path.empty() && out_path != "-") {
		const auto parent = std::filesystem::path(out_path).parent_path();
		if (!parent.empty())
			std::filesystem::create_directories(parent);
		file.open(out_path, std::ios::binary | std::ios::trunc);
		if (!file)
			throw std::runtime_error("cannot open " + out_path);
		out = &file;
	}

	Filter filter(categories);
	size_t event_count = 0;
	asb::Host host;
	host.set_call_log([](const std::string&) {});
	host.set_trace([&](const asb::TraceEvent& event) {
		if (filter.contains(event.kind)) {
			write_event(*out, event);
			++event_count;
		}
	});
	host.register_module(mod);
	if (!host.load(bytes))
		return 3;

	std::vector<std::string> candidates;
	if (!func.empty())
		candidates.push_back(func);
	candidates.push_back("scene00");
	candidates.push_back("MAIN");
	asIScriptFunction* target = host.find_function(candidates);
	if (!target) {
		std::fprintf(stderr, "[asb_trace] no entry function\n");
		return 4;
	}

	asb::RuntimeSession session(host, !input.empty() ? input : entry);
	session.set_max_frames(frames);
	if (!session.start(target))
		throw std::runtime_error(session.error());
	while (!session.finished() && !session.limit_reached()) {
		if (session.advance() == asb::RuntimeSession::StepResult::Error)
			throw std::runtime_error(session.error());
	}

	std::fprintf(
		stderr, "[asb_trace] %s: %ld frame(s), %zu event(s)\n", target->GetName(), session.frame(), event_count);
	return 0;
} catch (const std::exception& e) {
	std::fprintf(stderr, "[asb_trace] %s\n", e.what());
	return 1;
}
