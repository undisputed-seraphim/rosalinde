#include <scarlett/asb.hpp>
#include <scarlett/host.hpp>

#include <angelscript.h>
#include <criware/cpk.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
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

} // namespace

int main(int argc, char** argv) try {
	std::string input, cpk, entry, func, eval, disasm;
	unsigned max_calls = 200;
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
		else if (a == "--eval")
			eval = next();
		else if (a == "--disasm")
			disasm = next();
		else if (a == "--max-calls")
			max_calls = std::stoul(next());
		else {
			std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
			return 2;
		}
	}
	(void)max_calls;

	if (!eval.empty()) {
		asb::Host host;
		if (!host.compile(eval))
			return 3;
		asIScriptFunction* target = host.find_function({func.empty() ? "main" : func, "scene00", "MAIN"});
		if (!target) {
			std::fprintf(stderr, "[as] no entry function\n");
			return 4;
		}
		host.execute(target);
		return 0;
	}

	std::vector<uint8_t> bytes = !input.empty() ? read_file(input) : extract_entry(cpk, entry);
	if (bytes.empty())
		throw std::runtime_error("no input");

	asb::Module mod = asb::parse(std::span<const uint8_t>(bytes.data(), bytes.size()));
	std::fprintf(
		stderr,
		"asb decode: consumed=%zu/%zu funcs=%zu used_fns=%zu used_types=%zu error=%s\n",
		mod.consumed,
		mod.total,
		mod.functions.size(),
		mod.used_functions.size(),
		mod.used_types.size(),
		mod.error.c_str());
	for (const auto& b : mod.binds)
		std::fprintf(stderr, "[as] bind from '%s': %s\n", b.import_from.c_str(), b.sig.decl().c_str());

	if (!disasm.empty()) {
		for (const auto& f : mod.functions)
			if (f && f->sig.name == disasm) {
				std::printf("%s\n", asb::disassemble(mod, *f).c_str());
				return 0;
			}
		std::fprintf(stderr, "no such function: %s\n", disasm.c_str());
		return 5;
	}

	asb::Host host;
	host.register_module(mod);
	if (!host.load(bytes))
		return 3;

	asIScriptModule* m = host.module();
	asUINT cnt = m->GetFunctionCount();
	std::fprintf(stderr, "functions=%u\n", cnt);
	for (asUINT i = 0; i < cnt; ++i)
		std::fprintf(stderr, "  func[%u] %s\n", i, m->GetFunctionByIndex(i)->GetDeclaration());

	std::vector<std::string> candidates;
	if (!func.empty())
		candidates.push_back(func);
	candidates.push_back("scene00");
	candidates.push_back("MAIN");
	asIScriptFunction* target = host.find_function(candidates);
	if (!target) {
		std::fprintf(stderr, "[as] no entry function found\n");
		return 4;
	}
	std::fprintf(stderr, "[as] running %s\n", target->GetDeclaration());

	host.execute(target);
	return 0;
} catch (const std::exception& e) {
	std::fprintf(stderr, "error: %s\n", e.what());
	return 1;
}
