#include "asb.hpp"

#include <criware/cpk.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
using asb2::Function;
using asb2::Module;

static std::string json_escape(const std::string& s) {
	std::string out;
	out.reserve(s.size() + 8);
	for (unsigned char c : s) {
		switch (c) {
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20) {
				char buf[8];
				std::snprintf(buf, sizeof(buf), "\\u%04x", c);
				out += buf;
			} else
				out += char(c);
		}
	}
	return out;
}

static bool is_asset(const std::string& s) {
	if (s.find('/') == std::string::npos)
		return false;
	for (const char* ext : {".ftx", ".mbs", ".acb", ".hca", ".otm", ".nnd", ".awb", ".adx", ".vag", ".at3"}) {
		size_t n = std::strlen(ext);
		if (s.size() >= n && s.compare(s.size() - n, n, ext) == 0)
			return true;
	}
	return false;
}

static std::string int_list(const std::vector<uint64_t>& v) {
	std::string s = "[";
	for (size_t i = 0; i < v.size(); ++i)
		s += (i ? "," : "") + std::to_string(v[i]);
	return s + "]";
}

// Emit, for one function, the ordered string-constant refs and call refs as
// indices into the module pools (lossless, compact).
static void refs_for(const Module& mod, const Function& fn, std::vector<uint64_t>& str_refs, std::vector<uint64_t>& call_refs) {
	for (const auto& in : fn.code) {
		switch (in.op) {
		case 1:
		case 7:
		case 8:
		case 84:
		case 87:
		case 96:
		case 98:
		case 136: {
			uint64_t v = in.qarg;
			if (!(v & 1)) {
				uint64_t idx = v >> 1;
				if (idx < mod.used_strings.size())
					str_refs.push_back(idx);
			}
			break;
		}
		case 9:
		case 61:
		case 139:
		case 200: {
			if (!in.dargs.empty()) {
				uint64_t idx = in.dargs[0];
				if (idx < mod.used_functions.size())
					call_refs.push_back(idx);
			}
			break;
		}
		default:
			break;
		}
	}
}

static std::string harvest(const Module& mod, const std::string& name, size_t size, bool summary) {
	std::string json;
	json += "  {\"file\":\"" + json_escape(name) + "\",";
	json += "\"size\":" + std::to_string(size) + ",";
	json += "\"parse_ok\":" + std::string(mod.error.empty() ? "true" : "false") + ",";
	json += "\"consumed\":" + std::to_string(mod.consumed) + ",";
	json += "\"error\":\"" + json_escape(mod.error) + "\",";

	// section names
	json += "\"sections\":[";
	std::unordered_set<std::string> sec_seen;
	bool first = true;
	for (const auto& fn : mod.functions) {
		if (fn->script_section.empty() || !sec_seen.insert(fn->script_section).second)
			continue;
		json += (first ? "" : ",") + std::string("\"") + json_escape(fn->script_section) + "\"";
		first = false;
	}
	json += "],";

	// string pool
	json += "\"strings\":[";
	for (size_t i = 0; i < mod.used_strings.size(); ++i)
		json += (i ? "," : "") + std::string("\"") + json_escape(mod.used_strings[i]) + "\"";
	json += "],";

	// call-target pool used by functions
	json += "\"callpool\":[";
	for (size_t i = 0; i < mod.used_functions.size(); ++i) {
		json += (i ? "," : "");
		std::string kind = i < mod.used_function_kind.size() ? std::string(1, mod.used_function_kind[i]) : "?";
		std::string decl = mod.used_functions[i] ? mod.used_functions[i]->sig.decl() : std::string("null");
		json += "{\"kind\":\"" + json_escape(kind) + "\",\"decl\":\"" + json_escape(decl) + "\"}";
	}
	json += "],";

	json += "\"functions\":[";
	for (size_t i = 0; i < mod.functions.size(); ++i) {
		const auto& fn = *mod.functions[i];
		json += (i ? "," : "");
		json += "{\"name\":\"" + json_escape(fn.sig.name) + "\",";
		json += "\"decl\":\"" + json_escape(fn.sig.decl()) + "\",";
		json += "\"section\":\"" + json_escape(fn.script_section) + "\",";
		json += "\"shared\":" + std::string(fn.sig.shared ? "true" : "false");
		if (!summary) {
			std::vector<uint64_t> str_refs, call_refs;
			refs_for(mod, fn, str_refs, call_refs);
			json += ",\"str\":" + int_list(str_refs);
			json += ",\"calls\":" + int_list(call_refs);
		}
		json += "}";
	}
	json += "],";

	// app API surface
	json += "\"api\":[";
	first = true;
	for (size_t i = 0; i < mod.used_functions.size(); ++i) {
		if (i < mod.used_function_kind.size() && mod.used_function_kind[i] == 'a' && mod.used_functions[i]) {
			json += (first ? "" : ",") + std::string("\"") + json_escape(mod.used_functions[i]->sig.decl()) + "\"";
			first = false;
		}
	}
	json += "],";

	// asset strings (indices into strings pool)
	json += "\"assets\":[";
	first = true;
	for (size_t i = 0; i < mod.used_strings.size(); ++i) {
		if (!is_asset(mod.used_strings[i]))
			continue;
		json += (first ? "" : ",") + std::to_string(i);
		first = false;
	}
	json += "]}";
	return json;
}

static std::string dlist(const std::vector<double>& v) {
	std::string s = "[";
	char buf[40];
	for (size_t i = 0; i < v.size(); ++i) {
		if (!std::isfinite(v[i]))
			std::snprintf(buf, sizeof(buf), "null");
		else
			std::snprintf(buf, sizeof(buf), "%g", v[i]);
		s += (i ? "," : "") + std::string(buf);
	}
	return s + "]";
}
static std::string ilist(const std::vector<int64_t>& v) {
	std::string s = "[";
	for (size_t i = 0; i < v.size(); ++i)
		s += (i ? "," : "") + std::to_string(v[i]);
	return s + "]";
}
static std::string slist(const std::vector<std::string>& v) {
	std::string s = "[";
	for (size_t i = 0; i < v.size(); ++i)
		s += (i ? "," : "") + std::string("\"") + json_escape(v[i]) + "\"";
	return s + "]";
}

static std::string scene_json(const Module& mod, const std::string& name) {
	asb2::Scene sc = asb2::extract_scene(mod);
	std::string j = "  {\"file\":\"" + json_escape(name) + "\",\"functions\":[";
	for (size_t i = 0; i < sc.functions.size(); ++i) {
		const auto& f = sc.functions[i];
		j += (i ? "," : "");
		j += "{\"name\":\"" + json_escape(f.name) + "\",\"decl\":\"" + json_escape(f.decl) + "\",\"section\":\"" +
			 json_escape(f.section) + "\",\"events\":[";
		for (size_t e = 0; e < f.events.size(); ++e) {
			const auto& v = f.events[e];
			j += (e ? "," : "");
			j += "{\"call\":\"" + json_escape(v.call) + "\",\"kind\":\"" + std::string(1, v.kind) + "\"";
			if (!v.strings.empty())
				j += ",\"strings\":" + slist(v.strings);
			if (!v.ints.empty())
				j += ",\"ints\":" + ilist(v.ints);
			if (!v.floats.empty())
				j += ",\"floats\":" + dlist(v.floats);
			if (!v.vec2.empty())
				j += ",\"vec2\":" + dlist(v.vec2);
			if (!v.vec3.empty())
				j += ",\"vec3\":" + dlist(v.vec3);
			j += "}";
		}
		j += "]}";
	}
	return j + "]}";
}

static bool ends_with(const std::string& s, const char* suf) {
	size_t n = std::strlen(suf);
	return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

static std::string scene_index_json(const Module& mod, const std::string& name) {
	asb2::Scene sc = asb2::extract_scene(mod);
	size_t sprites = 0, chara = 0, stages = 0, cameras = 0;
	std::vector<std::string> funcs;
	std::unordered_set<std::string> aset;
	for (const auto& f : sc.functions) {
		size_t sf = 0;
		for (const auto& e : f.events) {
			const std::string& c = e.call;
			const bool is_sprite = c == "CreateSpriteMone" || c == "DrUiCreateChara" || c == "CreateSpriteCoA" ||
								   c == "CreateSpriteEffect" || c == "CreateSpriteMoneWorld" || c == "CreateSpriteMoneStageStart";
			if (is_sprite) {
				++sf;
				++sprites;
				for (const auto& s : e.strings)
					if (ends_with(s, ".mbs") || ends_with(s, ".ftx"))
						aset.insert(s);
			}
			if (c == "DrCreateChara" || c == "DrCreateNpc" || c == "DrUiCreateChara")
				++chara;
			if (c == "DrCreateStage" || c == "FlCreateStage" || c == "StSetBattleScene" || c == "StCreateUnit" ||
				c == "StCreateDemoUnit" || c == "FlCreateCharaSymbol")
				++stages;
			if (c == "DrSetCameraPosV" || c == "SetCameraPosV" || c == "DrSetCameraParam")
				++cameras;
		}
		if (sf > 0)
			funcs.push_back(f.name + ":" + std::to_string(sf));
	}

	std::vector<std::string> av(aset.begin(), aset.end());
	std::sort(av.begin(), av.end());

	std::string j = "  {\"file\":\"" + json_escape(name) + "\",\"parse_ok\":" +
					std::string(mod.error.empty() && mod.consumed == mod.total ? "true" : "false") +
					",\"sprites\":" + std::to_string(sprites) + ",\"chara\":" + std::to_string(chara) +
					",\"stages\":" + std::to_string(stages) + ",\"cameras\":" + std::to_string(cameras) +
					",\"funcs\":" + slist(funcs) + ",\"assets\":" + slist(av) + "}";
	return j;
}

static std::string num(double v) {
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%g", v);
	return buf;
}

static std::string scene_ir_json(const Module& mod, const std::string& name, const asb2::CharTable* chars) {
	asb2::Scene sc = asb2::extract_scene(mod);
	std::string j = "  {\"file\":\"" + json_escape(name) + "\",\"functions\":[";
	bool firstf = true;
	for (const auto& f : sc.functions) {
		asb2::SceneIR ir = asb2::build_scene_ir(f, name, chars);
		if (ir.entities.empty())
			continue;
		j += (firstf ? "" : ",");
		firstf = false;
		j += "{\"name\":\"" + json_escape(f.name) + "\",\"entities\":[";
		for (size_t i = 0; i < ir.entities.size(); ++i) {
			const auto& e = ir.entities[i];
			j += (i ? "," : "");
			j += "{\"id\":\"" + json_escape(e.id) + "\",\"kind\":\"" + json_escape(e.kind) + "\",\"mbs\":\"" +
				 json_escape(e.mbs) + "\",\"ftx\":\"" + json_escape(e.ftx) + "\",\"pos\":[" + num(e.pos[0]) + "," +
				 num(e.pos[1]) + "," + num(e.pos[2]) + "],\"scale\":" + num(e.scale) + ",\"track\":\"" +
				 json_escape(e.track) + "\",\"track_index\":" + std::to_string(e.track_index) +
				 ",\"visible\":" + (e.visible ? "true" : "false") + "}";
		}
		j += "],\"camera\":[";
		for (size_t i = 0; i < ir.camera.size(); ++i)
			j += (i ? "," : "") + std::string("[") + num(ir.camera[i].pos[0]) + "," + num(ir.camera[i].pos[1]) + "," +
				 num(ir.camera[i].pos[2]) + "]";
		j += "],\"stage\":" + ilist(ir.stage) + "}";
	}
	return j + "]}";
}

static void accumulate_chara(const Module& mod, std::map<std::string, asb2::CharaInfo>& out) {
	asb2::Scene sc = asb2::extract_scene(mod);
	for (const auto& f : sc.functions)
		for (const auto& e : f.events)
			if (e.call == "DrUiCreateChara" && e.strings.size() >= 4) {
				// (ftx, mbs, display name, character name)
				asb2::CharaInfo ci;
				ci.ftx = e.strings[0];
				ci.mbs = e.strings[1];
				ci.disp_name = e.strings[2];
				out.emplace(e.strings[3], ci);
			}
}

static void write_chara_table(const std::map<std::string, asb2::CharaInfo>& t, std::ostream& os) {
	os << "# character name -> display name, mbs, ftx (generated by asb_harvest --chara-table)\n";
	for (const auto& [name, ci] : t)
		os << name << '\t' << ci.disp_name << '\t' << ci.mbs << '\t' << ci.ftx << '\n';
}

static std::span<const uint8_t> as_bytes(const std::vector<char>& v) {
	return {reinterpret_cast<const uint8_t*>(v.data()), v.size()};
}

int main(int argc, char** argv) {
	std::string cpk_path, entry, input_path, json_path, disasm_dir, chara_file;
	bool all = false, want_disasm = false, summary = false, want_scene = false, scene_index = false, want_ir = false,
		 want_chara = false;

	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
		if (a == "--cpk") cpk_path = next();
		else if (a == "--asb") entry = next();
		else if (a == "--input") input_path = next();
		else if (a == "--json") json_path = next();
		else if (a == "--disasm-dir") disasm_dir = next();
		else if (a == "--all") all = true;
		else if (a == "--disasm") want_disasm = true;
		else if (a == "--summary") summary = true;
		else if (a == "--scene") want_scene = true;
		else if (a == "--scene-index") scene_index = true;
		else if (a == "--scene-ir") want_ir = true;
		else if (a == "--chara-table") want_chara = true;
		else if (a == "--chara-file") chara_file = next();
		else if (a == "-h" || a == "--help") {
			std::printf("usage: asb_harvest [--input FILE.asb | --cpk CPK --asb Script/X.asb | --cpk CPK --all]\n"
						"                    [--disasm] [--disasm-dir DIR] [--summary] [--scene] [--scene-index] [--json OUT.json|-]\n");
			return 0;
		} else {
			std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
			return 2;
		}
	}

	std::string json = "[\n";
	bool first = true;

	asb2::CharTable char_table;
	if (!chara_file.empty())
		char_table = asb2::load_char_table(chara_file);
	std::map<std::string, asb2::CharaInfo> chara_acc;

	auto process = [&](const std::string& name, std::span<const uint8_t> data) {
		Module mod = asb2::parse(data);
		bool ok = mod.error.empty() && mod.consumed == mod.total;
		std::fprintf(stderr, "%-42s %s  consumed=%zu/%zu  funcs=%zu  used=%zu  strings=%zu\n", name.c_str(),
					 ok ? "OK  " : (mod.error.empty() ? "PART" : "FAIL"), mod.consumed, mod.total, mod.functions.size(),
					 mod.used_functions.size(), mod.used_strings.size());
		if (!mod.error.empty())
			std::fprintf(stderr, "    error: %s\n", mod.error.c_str());

		if (want_disasm && !disasm_dir.empty()) {
			std::string out;
			for (const auto& fn : mod.functions)
				out += asb2::disassemble(mod, *fn) + "\n";
			std::ofstream(disasm_dir + "/" + fs::path(name).filename().string() + ".txt") << out;
		} else if (want_disasm) {
			for (const auto& fn : mod.functions)
				std::cout << asb2::disassemble(mod, *fn) << "\n";
		}

		if (want_chara) {
			accumulate_chara(mod, chara_acc);
			return;
		}

		if (!first)
			json += ",\n";
		json += want_scene ? scene_json(mod, name)
						   : (scene_index ? scene_index_json(mod, name)
										  : (want_ir ? scene_ir_json(mod, name, char_table.empty() ? nullptr : &char_table)
													 : harvest(mod, name, data.size(), summary)));
		first = false;
	};

	if (!input_path.empty()) {
		std::ifstream f(input_path, std::ios::binary);
		if (!f) {
			std::fprintf(stderr, "cannot open %s\n", input_path.c_str());
			return 1;
		}
		std::vector<char> buf((std::istreambuf_iterator<char>(f)), {});
		process(input_path, as_bytes(buf));
	} else if (!cpk_path.empty()) {
		fs::path p = fs::absolute(cpk_path);
		CPKTable table = TopLevelCpk(p).getTableOfContents();
		if (all) {
			for (const auto& [DirName, FileName] : table.rows<"DirName", "FileName">()) {
				if (DirName != "Script")
					continue;
				std::string fn = FileName;
				if (fn.size() < 4 || fn.compare(fn.size() - 4, 4, ".asb") != 0)
					continue;
				std::vector<char> buf;
				table.extract(DirName, FileName, buf);
				process("Script/" + fn, as_bytes(buf));
			}
		} else if (!entry.empty()) {
			auto pos = entry.find_last_of('/');
			std::string dir = pos == std::string::npos ? "" : entry.substr(0, pos);
			std::string nm = pos == std::string::npos ? entry : entry.substr(pos + 1);
			std::vector<char> buf;
			table.extract(dir, nm, buf);
			process(entry, as_bytes(buf));
		} else {
			std::fprintf(stderr, "specify --asb or --all\n");
			return 2;
		}
	} else {
		std::fprintf(stderr, "specify --input or --cpk\n");
		return 2;
	}

	if (want_chara) {
		std::ostringstream os;
		write_chara_table(chara_acc, os);
		if (!json_path.empty() && json_path != "-")
			std::ofstream(json_path) << os.str();
		else
			std::cout << os.str();
		return 0;
	}

	json += "\n]\n";
	if (!json_path.empty() && json_path != "-")
		std::ofstream(json_path) << json;
	else
		std::cout << json;
	return 0;
}
