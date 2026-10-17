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

std::string json_escape(const std::string& s) {
	std::string o;
	o.reserve(s.size() + 8);
	for (unsigned char c : s) {
		switch (c) {
		case '"':
			o += "\\\"";
			break;
		case '\\':
			o += "\\\\";
			break;
		case '\n':
			o += "\\n";
			break;
		case '\r':
			o += "\\r";
			break;
		case '\t':
			o += "\\t";
			break;
		default:
			if (c < 0x20) {
				char b[8];
				std::snprintf(b, sizeof b, "\\u%04x", c);
				o += b;
			} else {
				o += static_cast<char>(c);
			}
		}
	}
	return o;
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
			"\"accel\": %g, \"decel\": %g, \"tebure\": %s, \"wave\": %s, \"demo_y\": %g, "
			"\"degree_offset\": %g},\n",
			f.camera.pos[0],
			f.camera.pos[1],
			f.camera.pos[2],
			f.camera.has_pos ? "true" : "false",
			f.camera.zoom,
			f.camera.has_zoom ? "true" : "false",
			f.camera.accel,
			f.camera.decel,
			f.camera.tebure ? "true" : "false",
			f.camera.wave ? "true" : "false",
			f.camera.demo_y,
			f.camera.degree_offset);
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
				"\"track_index\": %d, \"pos\": [%g, %g, %g], \"scale\": %g, \"dir_x\": %g, \"alpha\": %d, "
				"\"layer\": %d, \"visible\": %s, \"layer_mask\": %u, \"breath_off\": %s, \"lipsync\": %s, "
				"\"shadow_off\": %s, \"sort_id_offset\": %d, \"dlprio\": %d, \"window_id_offset\": %d, "
				"\"prerender_force\": %s, \"color\": [%d, %d, %d, %d], "
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
				e.dir_x,
				e.alpha,
				e.layer,
				e.visible ? "true" : "false",
				e.layer_mask,
				e.breath_off ? "true" : "false",
				e.lipsync ? "true" : "false",
				e.shadow_off ? "true" : "false",
				e.sort_id_offset,
				e.dlprio,
				e.window_id_offset,
				e.prerender_force ? "true" : "false",
				e.color[0], e.color[1], e.color[2], e.color[3],
				e.order,
				aliases.c_str(),
				i + 1 < f.entities.size() ? "," : "");
		}
		std::fprintf(out, "\t\t\t]\n\t\t}%s\n", k + 1 < tl.keyframes.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"fades\": [\n");
	for (size_t i = 0; i < tl.fades.size(); ++i) {
		const asb::ScreenFade& f = tl.fades[i];
		std::fprintf(
			out,
			"\t\t{\"start\": %ld, \"end\": %ld, \"from\": [%g, %g, %g, %g], \"to\": [%g, %g, %g, %g]}%s\n",
			f.start,
			f.end,
			f.from[0],
			f.from[1],
			f.from[2],
			f.from[3],
			f.to[0],
			f.to[1],
			f.to[2],
			f.to[3],
			i + 1 < tl.fades.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"stages\": [\n");
	for (size_t i = 0; i < tl.stages.size(); ++i) {
		const asb::StageConfig& s = tl.stages[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"stage_type\": %d, \"weather_type\": %d, \"coa_preset\": %d, \"flag_color\": "
			"%d}%s\n",
			s.frame,
			s.stage_type,
			s.weather_type,
			s.coa_preset,
			s.flag_color,
			i + 1 < tl.stages.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"texts\": [\n");
	for (size_t i = 0; i < tl.texts.size(); ++i) {
		const asb::RuntimeText& t = tl.texts[i];
		std::string ids;
		for (size_t j = 0; j < t.ids.size(); ++j) {
			if (j)
				ids += ", ";
			ids += std::to_string(t.ids[j]);
		}
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"end\": %ld, \"kind\": %d, \"speaker\": \"%s\", \"text\": \"%s\", "
			"\"pos\": [%g, %g], \"disp_type\": %d, \"disp_name\": %d, \"face_dir_x\": %d, \"flags\": %u, "
			"\"result\": %d, \"ids\": [%s]}%s\n",
			t.frame,
			t.end,
			static_cast<int>(t.kind),
			json_escape(t.speaker).c_str(),
			json_escape(t.text).c_str(),
			t.pos[0],
			t.pos[1],
			t.disp_type,
			t.disp_name,
			t.face_dir_x,
			t.flags,
			t.result,
			ids.c_str(),
			i + 1 < tl.texts.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"shakes\": [\n");
	for (size_t i = 0; i < tl.shakes.size(); ++i) {
		const asb::RuntimeShake& s = tl.shakes[i];
		std::fprintf(
			out,
			"\t\t{\"start\": %ld, \"end\": %ld, \"index\": %d, \"type\": %d, \"shake\": [%g, %g], "
			"\"angle\": %g, \"att\": %g, \"pow\": %g}%s\n",
			s.start,
			s.end,
			s.index,
			s.type,
			s.shake[0],
			s.shake[1],
			s.angle,
			s.att,
			s.pow,
			i + 1 < tl.shakes.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"bokashi\": [\n");
	for (size_t i = 0; i < tl.bokashi.size(); ++i) {
		const asb::RuntimeBokashi& b = tl.bokashi[i];
		std::fprintf(
			out,
			"\t\t{\"start\": %ld, \"end\": %ld, \"bokashi\": %s, \"freeze\": %s, \"max_rate\": %g}%s\n",
			b.start,
			b.end,
			b.bokashi ? "true" : "false",
			b.freeze ? "true" : "false",
			b.max_rate,
			i + 1 < tl.bokashi.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"variations\": [\n");
	for (size_t i = 0; i < tl.variations.size(); ++i) {
		const asb::VariationEvent& v = tl.variations[i];
		std::fprintf(
			out, "\t\t{\"frame\": %ld, \"mask\": %u}%s\n", v.frame, v.mask, i + 1 < tl.variations.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"effects\": [\n");
	for (size_t i = 0; i < tl.effects.size(); ++i) {
		const asb::RuntimeEffect& e = tl.effects[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"kind\": \"%s\", \"name\": \"%s\", \"mbs\": \"%s\", \"ftx\": \"%s\", "
			"\"attached\": \"%s\", \"anime\": %d, \"pos\": [%g, %g, %g], \"i0\": %d, \"i1\": %d, \"i2\": %d, "
			"\"f0\": %g, \"flag\": %s}%s\n",
			e.frame,
			json_escape(e.kind).c_str(),
			json_escape(e.name).c_str(),
			json_escape(e.mbs).c_str(),
			json_escape(e.ftx).c_str(),
			json_escape(e.attached).c_str(),
			e.anime,
			e.pos[0],
			e.pos[1],
			e.pos[2],
			e.i0,
			e.i1,
			e.i2,
			e.f0,
			e.flag ? "true" : "false",
			i + 1 < tl.effects.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"point_lights\": [\n");
	for (size_t i = 0; i < tl.point_lights.size(); ++i) {
		const asb::RuntimePointLight& l = tl.point_lights[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"remove\": %s, \"name\": \"%s\", \"pos\": [%g, %g, %g], "
			"\"light\": [%u, %u, %u, %u], \"shadow\": [%u, %u, %u, %u], \"godray\": [%u, %u, %u, %u], "
			"\"power\": %g, \"bloom\": %g, \"f2\": %g, \"f3\": %g, \"f4\": %g, \"f5\": %g, "
			"\"attached\": \"%s\", \"index\": %d}%s\n",
			l.frame,
			l.remove ? "true" : "false",
			json_escape(l.name).c_str(),
			l.pos[0],
			l.pos[1],
			l.pos[2],
			l.light[0],
			l.light[1],
			l.light[2],
			l.light[3],
			l.shadow[0],
			l.shadow[1],
			l.shadow[2],
			l.shadow[3],
			l.godray[0],
			l.godray[1],
			l.godray[2],
			l.godray[3],
			l.power,
			l.bloom,
			l.f2,
			l.f3,
			l.f4,
			l.f5,
			json_escape(l.attached).c_str(),
			l.index,
			i + 1 < tl.point_lights.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"audio\": [\n");
	for (size_t i = 0; i < tl.audio.size(); ++i) {
		const asb::RuntimeAudio& a = tl.audio[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"kind\": \"%s\", \"cue\": %d, \"target\": \"%s\", \"fade\": %d, "
			"\"wait\": %d, \"index\": %d, \"volume\": %g}%s\n",
			a.frame,
			json_escape(a.kind).c_str(),
			a.cue,
			json_escape(a.target).c_str(),
			a.fade,
			a.wait,
			a.index,
			a.volume,
			i + 1 < tl.audio.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"items\": [\n");
	for (size_t i = 0; i < tl.items.size(); ++i) {
		const asb::RuntimeItem& it = tl.items[i];
		std::string add;
		for (int k = 0; k < it.add_count; ++k) {
			if (k)
				add += ", ";
			add += std::to_string(it.add[k]);
		}
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"kind\": \"%s\", \"object\": \"%s\", \"item\": \"%s\", \"id\": %d, "
			"\"count\": %d, \"pos\": [%g, %g], \"dialog\": %s, \"callback\": \"%s\", \"add\": [%s]}%s\n",
			it.frame,
			json_escape(it.kind).c_str(),
			json_escape(it.object).c_str(),
			json_escape(it.item).c_str(),
			it.id,
			it.count,
			it.pos[0],
			it.pos[1],
			it.dialog ? "true" : "false",
			json_escape(it.callback).c_str(),
			add.c_str(),
			i + 1 < tl.items.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"fog\": [\n");
	for (size_t i = 0; i < tl.fog.size(); ++i) {
		const asb::RuntimeFog& f = tl.fog[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"name\": \"%s\", \"color\": [%u, %u, %u, %u]}%s\n",
			f.frame,
			json_escape(f.name).c_str(),
			f.color[0],
			f.color[1],
			f.color[2],
			f.color[3],
			i + 1 < tl.fog.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"gradients\": [\n");
	for (size_t i = 0; i < tl.gradients.size(); ++i) {
		const asb::RuntimeGradient& g = tl.gradients[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"index\": %d, \"rate\": %g, \"time\": %d}%s\n",
			g.frame,
			g.index,
			g.rate,
			g.time,
			i + 1 < tl.gradients.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"ui\": [\n");
	for (size_t i = 0; i < tl.ui.size(); ++i) {
		const asb::RuntimeUi& u = tl.ui[i];
		std::string ints;
		for (size_t k = 0; k < u.ints.size(); ++k) {
			if (k)
				ints += ", ";
			ints += std::to_string(u.ints[k]);
		}
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"kind\": \"%s\", \"text\": \"%s\", \"id\": %d, \"i0\": %d, \"i1\": %d, "
			"\"pos\": [%g, %g], \"rate\": %g, \"flag\": %s, \"ints\": [%s]}%s\n",
			u.frame,
			json_escape(u.kind).c_str(),
			json_escape(u.text).c_str(),
			u.id,
			u.i0,
			u.i1,
			u.pos[0],
			u.pos[1],
			u.rate,
			u.flag ? "true" : "false",
			ints.c_str(),
			i + 1 < tl.ui.size() ? "," : "");
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
	if (std::getenv("ASB_TEXT") || std::getenv("SCENE_TRACE"))
		host.set_call_log([](const std::string& s) { std::fprintf(stderr, "[call] %s\n", s.c_str()); });
	else
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
