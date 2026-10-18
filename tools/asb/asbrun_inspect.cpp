#include "asbrun_inspect.hpp"

#include "asbrun_common.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace asbrun {

bool is_asset(std::string_view value) {
	if (value.find('/') == std::string_view::npos)
		return false;
	for (std::string_view ext : {".ftx", ".mbs", ".acb", ".hca", ".otm", ".nnd", ".awb", ".adx", ".vag", ".at3"}) {
		if (ends_with(value, ext))
			return true;
	}
	return false;
}

namespace {

std::string int_list(const std::vector<uint64_t>& values) {
	std::string out = "[";
	for (size_t i = 0; i < values.size(); ++i)
		out += (i ? "," : "") + std::to_string(values[i]);
	return out + "]";
}

void refs_for(
	const asb::Module& module,
	const asb::Function& function,
	std::vector<uint64_t>& string_refs,
	std::vector<uint64_t>& call_refs) {
	for (const auto& instruction : function.code) {
		switch (instruction.op) {
		case 1:
		case 7:
		case 8:
		case 84:
		case 87:
		case 96:
		case 98:
		case 136: {
			const uint64_t value = instruction.qarg;
			if (!(value & 1)) {
				const uint64_t index = value >> 1;
				if (index < module.used_strings.size())
					string_refs.push_back(index);
			}
			break;
		}
		case 9:
		case 61:
		case 139:
		case 200: {
			if (!instruction.dargs.empty()) {
				const uint64_t index = instruction.dargs[0];
				if (index < module.used_functions.size())
					call_refs.push_back(index);
			}
			break;
		}
		default:
			break;
		}
	}
}

std::string dlist(const std::vector<double>& values) {
	std::string out = "[";
	char buffer[40];
	for (size_t i = 0; i < values.size(); ++i) {
		if (!std::isfinite(values[i]))
			std::snprintf(buffer, sizeof(buffer), "null");
		else
			std::snprintf(buffer, sizeof(buffer), "%g", values[i]);
		out += (i ? "," : "") + std::string(buffer);
	}
	return out + "]";
}

std::string ilist(const std::vector<int64_t>& values) {
	std::string out = "[";
	for (size_t i = 0; i < values.size(); ++i)
		out += (i ? "," : "") + std::to_string(values[i]);
	return out + "]";
}

std::string slist(const std::vector<std::string>& values) {
	std::string out = "[";
	for (size_t i = 0; i < values.size(); ++i)
		out += (i ? "," : "") + std::string("\"") + json_escape(values[i]) + "\"";
	return out + "]";
}

std::string number(double value) {
	char buffer[32];
	std::snprintf(buffer, sizeof(buffer), "%g", value);
	return buffer;
}

} // namespace

std::string harvest_json(const asb::Module& module, const std::string& name, size_t size, bool summary) {
	std::string json;
	json += "  {\"file\":\"" + json_escape(name) + "\",";
	json += "\"size\":" + std::to_string(size) + ",";
	json += "\"parse_ok\":" + std::string(module.error.empty() ? "true" : "false") + ",";
	json += "\"consumed\":" + std::to_string(module.consumed) + ",";
	json += "\"error\":\"" + json_escape(module.error) + "\",";

	json += "\"sections\":[";
	std::unordered_set<std::string> seen_sections;
	bool first = true;
	for (const auto& function : module.functions) {
		if (function->script_section.empty() || !seen_sections.insert(function->script_section).second)
			continue;
		json += (first ? "" : ",") + std::string("\"") + json_escape(function->script_section) + "\"";
		first = false;
	}
	json += "],";

	json += "\"strings\":[";
	for (size_t i = 0; i < module.used_strings.size(); ++i)
		json += (i ? "," : "") + std::string("\"") + json_escape(module.used_strings[i]) + "\"";
	json += "],";

	json += "\"callpool\":[";
	for (size_t i = 0; i < module.used_functions.size(); ++i) {
		json += (i ? "," : "");
		const std::string kind =
			i < module.used_function_kind.size() ? std::string(1, module.used_function_kind[i]) : "?";
		const std::string declaration =
			module.used_functions[i] ? module.used_functions[i]->sig.decl() : std::string("null");
		json += "{\"kind\":\"" + json_escape(kind) + "\",\"decl\":\"" + json_escape(declaration) + "\"}";
	}
	json += "],";

	json += "\"functions\":[";
	for (size_t i = 0; i < module.functions.size(); ++i) {
		const auto& function = *module.functions[i];
		json += (i ? "," : "");
		json += "{\"name\":\"" + json_escape(function.sig.name) + "\",";
		json += "\"decl\":\"" + json_escape(function.sig.decl()) + "\",";
		json += "\"section\":\"" + json_escape(function.script_section) + "\",";
		json += "\"shared\":" + std::string(function.sig.shared ? "true" : "false");
		if (!summary) {
			std::vector<uint64_t> string_refs;
			std::vector<uint64_t> call_refs;
			refs_for(module, function, string_refs, call_refs);
			json += ",\"str\":" + int_list(string_refs);
			json += ",\"calls\":" + int_list(call_refs);
		}
		json += "}";
	}
	json += "],";

	json += "\"api\":[";
	first = true;
	for (size_t i = 0; i < module.used_functions.size(); ++i) {
		if (i < module.used_function_kind.size() && module.used_function_kind[i] == 'a' && module.used_functions[i]) {
			json += (first ? "" : ",") + std::string("\"") + json_escape(module.used_functions[i]->sig.decl()) + "\"";
			first = false;
		}
	}
	json += "],";

	json += "\"assets\":[";
	first = true;
	for (size_t i = 0; i < module.used_strings.size(); ++i) {
		if (!is_asset(module.used_strings[i]))
			continue;
		json += (first ? "" : ",") + std::to_string(i);
		first = false;
	}
	json += "]}";
	return json;
}

std::string scene_json(
	const asb::Module& module,
	const std::string& name,
	std::string_view function_filter,
	std::string_view call_filter,
	std::optional<size_t> limit) {
	const asb::Scene scene = asb::extract_scene(module);
	const std::string function_needle = lower(std::string(function_filter));
	const std::string call_needle = lower(std::string(call_filter));
	size_t emitted = 0;
	bool first_function = true;

	std::string json = "  {\"file\":\"" + json_escape(name) + "\",\"functions\":[";
	for (const auto& function : scene.functions) {
		if (!function_needle.empty() && lower(function.name) != function_needle &&
			lower(function.decl) != function_needle)
			continue;

		std::string events;
		size_t function_events = 0;
		for (const auto& event : function.events) {
			if (limit && emitted >= *limit)
				break;
			if (!call_needle.empty() && lower(event.call).find(call_needle) == std::string::npos &&
				lower(event.decl).find(call_needle) == std::string::npos)
				continue;
			events += (function_events ? "," : "");
			events += "{\"call\":\"" + json_escape(event.call) + "\",\"kind\":\"" + std::string(1, event.kind) + "\"";
			if (!event.strings.empty())
				events += ",\"strings\":" + slist(event.strings);
			if (!event.ints.empty())
				events += ",\"ints\":" + ilist(event.ints);
			if (!event.floats.empty())
				events += ",\"floats\":" + dlist(event.floats);
			if (!event.vec2.empty())
				events += ",\"vec2\":" + dlist(event.vec2);
			if (!event.vec3.empty())
				events += ",\"vec3\":" + dlist(event.vec3);
			events += "}";
			++function_events;
			++emitted;
		}
		if (function_events == 0)
			continue;
		json += (first_function ? "" : ",");
		first_function = false;
		json += "{\"name\":\"" + json_escape(function.name) + "\",\"decl\":\"" + json_escape(function.decl) +
				"\",\"section\":\"" + json_escape(function.section) + "\",\"events\":[" + events + "]}";
		if (limit && emitted >= *limit)
			break;
	}
	return json + "]}";
}

std::string scene_index_json(const asb::Module& module, const std::string& name) {
	const asb::Scene scene = asb::extract_scene(module);
	size_t sprite_calls = 0;
	size_t chara_calls = 0;
	size_t stage_calls = 0;
	size_t camera_calls = 0;
	std::vector<std::string> functions;
	std::unordered_set<std::string> assets;
	for (const auto& function : scene.functions) {
		size_t function_entities = 0;
		for (const auto& event : function.events) {
			const std::string& call = event.call;
			const bool chara = asb::is_chara_create_call(call);
			if (asb::is_sprite_create_call(call) && !chara) {
				++sprite_calls;
				++function_entities;
				for (const auto& string : event.strings)
					if (ends_with(string, ".mbs") || ends_with(string, ".ftx"))
						assets.insert(string);
			}
			if (chara) {
				++chara_calls;
				++function_entities;
			}
			if (call == "DrCreateStage" || call == "FlCreateStage" || call == "StSetBattleScene" ||
				call == "StCreateUnit" || call == "StCreateDemoUnit" || call == "FlCreateCharaSymbol")
				++stage_calls;
			if (call == "DrSetCameraPosV" || call == "SetCameraPosV" || call == "DrSetCameraParam")
				++camera_calls;
		}
		if (function_entities > 0)
			functions.push_back(function.name + ":" + std::to_string(function_entities));
	}

	std::vector<std::string> asset_list(assets.begin(), assets.end());
	std::sort(asset_list.begin(), asset_list.end());

	return "  {\"file\":\"" + json_escape(name) + "\",\"parse_ok\":" +
		   std::string(module.error.empty() && module.consumed == module.total ? "true" : "false") +
		   ",\"static\":true,\"sprite_calls\":" + std::to_string(sprite_calls) +
		   ",\"chara_calls\":" + std::to_string(chara_calls) +
		   ",\"entity_calls\":" + std::to_string(sprite_calls + chara_calls) +
		   ",\"stage_calls\":" + std::to_string(stage_calls) + ",\"camera_calls\":" + std::to_string(camera_calls) +
		   ",\"funcs\":" + slist(functions) + ",\"assets\":" + slist(asset_list) + "}";
}

std::string scene_ir_json(const asb::Module& module, const std::string& name, const asb::CharTable* chars) {
	const asb::Scene scene = asb::extract_scene(module);
	std::string json = "  {\"file\":\"" + json_escape(name) + "\",\"functions\":[";
	bool first_function = true;
	for (const auto& function : scene.functions) {
		const asb::SceneIR ir = asb::build_scene_ir(function, name, chars);
		if (ir.entities.empty())
			continue;
		json += (first_function ? "" : ",");
		first_function = false;
		json += "{\"name\":\"" + json_escape(function.name) + "\",\"entities\":[";
		for (size_t i = 0; i < ir.entities.size(); ++i) {
			const auto& entity = ir.entities[i];
			json += (i ? "," : "");
			json += "{\"id\":\"" + json_escape(entity.id) + "\",\"kind\":\"" + json_escape(entity.kind) +
					"\",\"mbs\":\"" + json_escape(entity.mbs) + "\",\"ftx\":\"" + json_escape(entity.ftx) +
					"\",\"pos\":[" + number(entity.pos[0]) + "," + number(entity.pos[1]) + "," + number(entity.pos[2]) +
					"],\"scale\":" + number(entity.scale) + ",\"track\":\"" + json_escape(entity.track) +
					"\",\"track_index\":" + std::to_string(entity.track_index) +
					",\"visible\":" + (entity.visible ? "true" : "false") + "}";
		}
		json += "],\"camera\":[";
		for (size_t i = 0; i < ir.camera.size(); ++i)
			json += (i ? "," : "") + std::string("[") + number(ir.camera[i].pos[0]) + "," +
					number(ir.camera[i].pos[1]) + "," + number(ir.camera[i].pos[2]) + "]";
		json += "],\"stage\":" + ilist(ir.stage) + "}";
	}
	return json + "]}";
}

} // namespace asbrun
