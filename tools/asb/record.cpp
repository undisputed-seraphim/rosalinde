#include <scarlett/asb.hpp>
#include <scarlett/host.hpp>
#include <scarlett/scene.hpp>

#include <angelscript.h>
#include <criware/cpk.hpp>

#include <cstdint>
#include <cstdio>
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

void write_json(FILE* out, const asb::Timeline& tl) {
	std::fprintf(out, "{\n\t\"source\": \"%s\",\n\t\"function\": \"%s\",\n", tl.source.c_str(), tl.function.c_str());
	std::fprintf(out, "\t\"keyframes\": [\n");
	for (size_t k = 0; k < tl.keyframes.size(); ++k) {
		const asb::Keyframe& f = tl.keyframes[k];
		std::fprintf(out, "\t\t{\n\t\t\t\"frame\": %ld,\n", f.frame);
		std::fprintf(
			out,
			"\t\t\t\"camera\": {\"pos\": [%g, %g, %g], \"has_pos\": %s, \"zoom\": %g, \"has_zoom\": %s, "
			"\"accel\": %g, \"decel\": %g},\n",
			f.camera.pos[0],
			f.camera.pos[1],
			f.camera.pos[2],
			f.camera.has_pos ? "true" : "false",
			f.camera.zoom,
			f.camera.has_zoom ? "true" : "false",
			f.camera.accel,
			f.camera.decel);
		std::fprintf(out, "\t\t\t\"entities\": [\n");
		for (size_t i = 0; i < f.entities.size(); ++i) {
			const asb::RuntimeEntity& e = f.entities[i];
			std::string aliases;
			for (size_t j = 0; j < e.aliases.size(); ++j) {
				if (j)
					aliases += ", ";
				aliases += "\"" + e.aliases[j] + "\"";
			}
			std::fprintf(
				out,
				"\t\t\t\t{\"name\": \"%s\", \"kind\": \"%s\", \"space\": \"%s\", \"mbs\": \"%s\", \"ftx\": \"%s\", "
				"\"track\": \"%s\", "
				"\"track_index\": %d, \"pos\": [%g, %g, %g], \"scale\": %g, \"layer\": %d, \"visible\": %s, "
				"\"order\": %d, \"aliases\": [%s]}%s\n",
				e.name.c_str(),
				e.kind.c_str(),
				e.space == asb::RuntimeEntity::Space::Ui ? "ui" : "world",
				e.mbs.c_str(),
				e.ftx.c_str(),
				e.track.c_str(),
				e.track_index,
				e.pos[0],
				e.pos[1],
				e.pos[2],
				e.scale,
				e.layer,
				e.visible ? "true" : "false",
				e.order,
				aliases.c_str(),
				i + 1 < f.entities.size() ? "," : "");
		}
		std::fprintf(out, "\t\t\t]\n\t\t}%s\n", k + 1 < tl.keyframes.size() ? "," : "");
	}
	std::fprintf(out, "\t]\n}\n");
}

} // namespace

int main(int argc, char** argv) try {
	std::string input, cpk, entry, func, json, chara_table;
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
		else if (a == "--json")
			json = next();
		else if (a == "--chara-table")
			chara_table = next();
		else {
			std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
			return 2;
		}
	}

	std::vector<uint8_t> bytes = !input.empty() ? read_file(input) : extract_entry(cpk, entry);
	if (bytes.empty())
		throw std::runtime_error("no input");

	asb::Module mod = asb::parse(std::span<const uint8_t>(bytes.data(), bytes.size()));

	asb::Host host;
	host.register_module(mod);
	host.set_call_log([](const std::string&) {});
	if (!host.load(bytes))
		return 3;

	asb::CharTable chars;
	if (!chara_table.empty()) {
		chars = asb::load_char_table(chara_table);
		host.set_char_table(chars);
	}

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
	std::fprintf(stderr, "[as] recording %s (max %ld frames)\n", target->GetDeclaration(), frames);

	std::string source = !input.empty() ? input : entry;
	asb::Timeline tl = host.run(target, source, frames);

	if (!json.empty() && json != "-") {
		FILE* out = std::fopen(json.c_str(), "wb");
		if (!out)
			throw std::runtime_error("cannot write " + json);
		write_json(out, tl);
		std::fclose(out);
		std::fprintf(stderr, "[record] wrote %s\n", json.c_str());
	} else if (json == "-") {
		write_json(stdout, tl);
	}
	return 0;
} catch (const std::exception& e) {
	std::fprintf(stderr, "error: %s\n", e.what());
	return 1;
}
