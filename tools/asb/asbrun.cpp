#include "asbrun_bulk.hpp"
#include "asbrun_common.hpp"
#include "asbrun_inspect.hpp"
#include "line_editor.hpp"

#include <scarlett/asb.hpp>
#include <scarlett/host.hpp>
#include <scarlett/scene.hpp>

#include <angelscript.h>
#include <criware/cpk.hpp>

#include <data_path.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(__EMSCRIPTEN__) || defined(__wasm__)
#elif defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr int max_source_depth = 16;

std::optional<std::vector<std::string>> tokenize(std::string_view line, std::string& error) {
	std::vector<std::string> args;
	std::string token;
	bool in_token = false;
	char quote = 0;

	for (size_t i = 0; i < line.size(); ++i) {
		const char c = line[i];
		if (quote != 0) {
			if (c == quote) {
				quote = 0;
			} else if (
				c == '\\' && quote == '"' && i + 1 < line.size() && (line[i + 1] == '"' || line[i + 1] == '\\')) {
				token += line[++i];
			} else {
				token += c;
			}
			in_token = true;
			continue;
		}

		if (std::isspace(static_cast<unsigned char>(c))) {
			if (in_token) {
				args.push_back(std::move(token));
				token.clear();
				in_token = false;
			}
			continue;
		}

		if (c == '\'' || c == '"') {
			quote = c;
			in_token = true;
			continue;
		}

		if (c == '\\' && i + 1 < line.size() &&
			(std::isspace(static_cast<unsigned char>(line[i + 1])) || line[i + 1] == '\'' || line[i + 1] == '"' ||
			 line[i + 1] == '\\')) {
			token += line[++i];
			in_token = true;
			continue;
		}

		token += c;
		in_token = true;
	}

	if (quote != 0) {
		error = "unterminated quote";
		return std::nullopt;
	}
	if (in_token)
		args.push_back(std::move(token));
	return args;
}

std::string path_of(const cpk::entry& entry) {
	if (entry.dir.empty() || entry.dir == ".")
		return std::string(entry.name);
	return std::string(entry.dir) + "/" + std::string(entry.name);
}

void create_parent_directories(const std::string& path) {
	const fs::path parent = fs::path(path).parent_path();
	if (!parent.empty())
		fs::create_directories(parent);
}

void write_json_document(const std::string& object, const std::string& path) {
	std::ofstream file;
	std::ostream* out = &std::cout;
	if (path != "-") {
		create_parent_directories(path);
		file.open(path, std::ios::binary | std::ios::trunc);
		if (!file)
			throw std::runtime_error("cannot write " + path);
		out = &file;
	}
	*out << "[\n" << object << "\n]\n";
}

void write_json_object(const std::string& object, const std::string& path) {
	std::ofstream file;
	std::ostream* out = &std::cout;
	if (path != "-") {
		create_parent_directories(path);
		file.open(path, std::ios::binary | std::ios::trunc);
		if (!file)
			throw std::runtime_error("cannot write " + path);
		out = &file;
	}
	*out << object << '\n';
}

std::string number_text(double value) {
	char buffer[40];
	std::snprintf(buffer, sizeof(buffer), "%g", value);
	return buffer;
}

std::string vector_text(const asb::SceneEvent& event, bool reverse = false) {
	if (event.vec3.size() >= 3) {
		const auto& v = event.vec3;
		return reverse ? "(" + number_text(v[2]) + "," + number_text(v[1]) + "," + number_text(v[0]) + ")"
					   : "(" + number_text(v[0]) + "," + number_text(v[1]) + "," + number_text(v[2]) + ")";
	}
	if (event.vec2.size() >= 2)
		return "(" + number_text(event.vec2[0]) + "," + number_text(event.vec2[1]) + ")";
	return {};
}

bool is_vector_move(std::string_view call) {
	return call == "DrMoveAsync" || call == "DrMoveAbsAsync" || call == "DrMoveAbs" || call == "DrMove" ||
		   call == "DrShortMoveAsync" || call == "DrShortMoveAbsAsync" || call == "DrShortMove";
}

std::string movement_event_text(const asb::SceneEvent& event) {
	const std::vector<std::string>& strings = event.strings;
	const std::string& object = !strings.empty() ? strings.back() : std::string();
	const std::string& animation = strings.size() >= 2 ? strings[strings.size() - 2] : std::string();
	const std::string& follow = strings.size() >= 3 ? strings.front() : std::string();

	const double speed = !event.floats.empty() ? event.floats.back() : 0.0;
	const double acc = event.floats.size() >= 3 ? event.floats[0] : 0.0;
	const double dec = event.floats.size() >= 3 ? event.floats[1] : 0.0;

	std::string out = event.call + "(";
	out += "obj=\"" + asbrun::json_escape(object) + "\", to=" + vector_text(event, true);
	out += ", anim=\"" + asbrun::json_escape(animation) + "\"";
	out += ", speed=" + number_text(speed) + ", acc=" + number_text(acc) + ", dec=" + number_text(dec);
	if (strings.size() >= 3)
		out += ", follow=\"" + asbrun::json_escape(follow) + "\"";
	out += ")";
	return out;
}

std::string scene_event_text(const asb::SceneEvent& event) {
	if (is_vector_move(event.call))
		return movement_event_text(event);

	std::string out = event.call;
	bool first = true;
	auto append = [&](const std::string& value) {
		out += first ? "(" : ", ";
		out += value;
		first = false;
	};
	for (const auto& value : event.strings)
		append("\"" + asbrun::json_escape(value) + "\"");
	for (const auto& value : event.ints)
		append(std::to_string(value));
	if (event.ints.empty() && !event.floats.empty())
		for (double value : event.floats)
			append(number_text(value));
	if (const std::string vec = vector_text(event, !event.vec3.empty()); !vec.empty())
		append(vec);
	if (!first)
		out += ")";
	return out;
}

bool stdin_is_tty() {
#if defined(__EMSCRIPTEN__) || defined(__wasm__)
	return false;
#elif defined(_WIN32)
	return _isatty(_fileno(stdin)) != 0;
#else
	return ::isatty(STDIN_FILENO) != 0;
#endif
}

void print_usage(std::ostream& out) { out << "usage: asbrun --cpk PATH\n"; }

std::string_view retired_hint(std::string_view name) {
	if (name == "play")
		return "--run-to N [--trace FILE] [--record FILE] [--calls FILE] [--state FILE]";
	if (name == "trace")
		return "--trace FILE --trace-categories vm,stage,camera,sprite";
	if (name == "record")
		return "--record FILE [--record-every N]";
	if (name == "calls")
		return "--calls FILE --calls-json FILE";
	if (name == "state")
		return "--run-to N --state FILE --state-json FILE";
	return {};
}

class Shell {
public:
	Shell(cpk::archive& archive, bool interactive)
		: _archive(archive)
		, _interactive(interactive) {}

	int run(std::istream& in) {
		if (_interactive) {
			cli::line_editor editor;
			while (_running) {
				std::cout.flush();
				const auto line = editor.read_line(prompt(), _history);
				if (!line)
					break;
				remember(*line);
				execute_line(*line, 0);
			}
		} else {
			std::string line;
			while (_running && std::getline(in, line))
				execute_line(line, 0);
		}
		std::cout.flush();
		return _interactive ? 0 : (_had_error ? 1 : 0);
	}

private:
	using arg_list = std::span<const std::string>;

	struct command_spec {
		std::string_view name;
		std::string_view usage;
		std::string_view summary;
		int (Shell::*handler)(arg_list, int);
	};

	static std::span<const command_spec> command_list();

	int execute_line(std::string_view line, int depth) {
		line = asbrun::trim(line);
		if (line.empty() || line.front() == '#')
			return 0;

		std::string error;
		auto args = tokenize(line, error);
		if (!args) {
			std::cerr << "asbrun: " << error << '\n';
			_had_error = true;
			return 2;
		}
		if (args->empty())
			return 0;

		int status = 0;
		try {
			status = dispatch(*args, depth);
		} catch (const std::exception& e) {
			std::cerr << "asbrun: " << e.what() << '\n';
			status = 1;
		} catch (...) {
			std::cerr << "asbrun: unknown error\n";
			status = 1;
		}
		if (status != 0)
			_had_error = true;
		return status;
	}

	int dispatch(arg_list args, int depth) {
		for (const auto& command : command_list()) {
			if (command.name == args[0])
				return (this->*(command.handler))(args, depth);
		}
		if (const std::string_view hint = retired_hint(args[0]); !hint.empty()) {
			std::cerr << "asbrun: '" << args[0] << "' moved to the viewer:\n"
					  << "  viewer --cpk <CPK> --script <SCRIPT> --headless " << hint << "\n";
			return 2;
		}
		std::cerr << "asbrun: unknown command '" << args[0] << "' (type 'help')\n";
		return 1;
	}

	const asb::Scene& scene() {
		if (!_scene)
			_scene = asb::extract_scene(_module);
		return *_scene;
	}

	std::string resolve_entry(std::string_view path) const {
		while (!path.empty() && path.front() == '/')
			path.remove_prefix(1);
		if (path.empty())
			throw std::runtime_error("empty CPK path");

		if (_archive.find(path) != _archive.end())
			return std::string(path);

		if (path.find('/') == std::string_view::npos) {
			std::vector<std::string> matches;
			for (const auto& entry : _archive) {
				if (entry.name == path)
					matches.push_back(path_of(entry));
			}
			if (matches.size() == 1)
				return matches.front();
			if (matches.size() > 1) {
				std::string text = "ambiguous filename '" + std::string(path) + "':";
				for (const auto& match : matches)
					text += "\n  " + match;
				throw std::runtime_error(text);
			}
		}
		throw std::runtime_error("not found in CPK: " + std::string(path));
	}

	const asb::Function* find_module_function(std::string_view name) const {
		for (const auto& function : _module.functions) {
			if (function->sig.name == name || function->sig.decl() == name)
				return function.get();
		}
		return nullptr;
	}

	asIScriptFunction* find_entry_function(asb::Host& host, const std::string& requested) const {
		std::vector<std::string> candidates;
		if (!requested.empty())
			candidates.push_back(requested);
		candidates.push_back("scene00");
		candidates.push_back("MAIN");
		return host.find_function(candidates);
	}

	void require_module() const {
		if (!_loaded)
			throw std::runtime_error("no module loaded (use 'open PATH')");
	}

	void remember(std::string_view line) {
		const std::string_view trimmed = asbrun::trim(line);
		if (trimmed.empty() || trimmed.front() == '#')
			return;
		std::string command(trimmed);
		const auto existing = std::find(_history.begin(), _history.end(), command);
		if (existing != _history.end())
			_history.erase(existing);
		_history.push_front(std::move(command));
		if (_history.size() > 5)
			_history.pop_back();
	}

	int cmd_open(arg_list args, int) {
		if (args.size() != 2) {
			std::cerr << "open: usage: open PATH\n";
			return 2;
		}
		std::string path;
		if (fs::path(args[1]).extension().empty()) {
			try {
				path = resolve_entry(args[1] + ".asb");
			} catch (...) {
				path = resolve_entry(args[1]);
			}
		} else {
			path = resolve_entry(args[1]);
		}
		if (asbrun::lower(fs::path(path).extension().string()) != ".asb")
			throw std::runtime_error("not an ASB file (expected .asb): " + path);
		std::vector<uint8_t> bytes = asbrun::extract_entry(_archive, path);
		if (bytes.empty()) {
			std::cerr << "open: empty file: " << path << '\n';
			return 1;
		}
		asb::Module module = asb::parse(std::span<const uint8_t>(bytes.data(), bytes.size()));
		if (!module.error.empty()) {
			std::cerr << "open: " << path << ": " << module.error << '\n';
			return 1;
		}

		_path = path;
		_bytes = std::move(bytes);
		_module = std::move(module);
		_scene.reset();
		_loaded = true;

		if (_module.consumed != _module.total)
			std::cerr << "open: warning: consumed " << _module.consumed << "/" << _module.total << " bytes\n";
		std::cerr << "opened " << _path << " (" << _bytes.size() << " bytes, " << _module.functions.size()
				  << " functions)\n";
		return 0;
	}

	int cmd_close(arg_list args, int) {
		if (args.size() != 1) {
			std::cerr << "close: usage: close\n";
			return 2;
		}
		_loaded = false;
		_path.clear();
		_bytes.clear();
		_module = asb::Module{};
		_scene.reset();
		return 0;
	}

	int cmd_status(arg_list args, int) {
		if (args.size() != 1) {
			std::cerr << "status: usage: status\n";
			return 2;
		}
		std::cout << "cpk: " << _archive.path().string() << '\n';
		std::cout << "entries: " << _archive.size() << '\n';
		if (_loaded) {
			std::cout << "module: " << _path << '\n';
			std::cout << "bytes: " << _bytes.size() << '\n';
			std::cout << "consumed: " << _module.consumed << '/' << _module.total << '\n';
		} else {
			std::cout << "module: (none)\n";
		}
		return 0;
	}

	int cmd_summary(arg_list args, int) {
		std::string json_path;
		bool compact = false;
		for (size_t i = 1; i < args.size(); ++i) {
			if (args[i] == "--json") {
				if (i + 1 >= args.size()) {
					std::cerr << "summary: --json requires a path\n";
					return 2;
				}
				json_path = args[++i];
			} else if (args[i] == "--compact") {
				compact = true;
			} else {
				std::cerr << "summary: unknown option '" << args[i] << "'\n";
				return 2;
			}
		}
		require_module();

		size_t natives = 0;
		for (size_t i = 0; i < _module.used_functions.size(); ++i) {
			if (i < _module.used_function_kind.size() && _module.used_function_kind[i] == 'a' &&
				_module.used_functions[i])
				++natives;
		}
		std::unordered_set<std::string> sections;
		for (const auto& function : _module.functions) {
			if (!function->script_section.empty())
				sections.insert(function->script_section);
		}
		size_t assets = 0;
		for (const auto& text : _module.used_strings)
			if (asbrun::is_asset(text))
				++assets;
		const size_t scene_functions = scene().functions.size();

		if (!json_path.empty()) {
			if (!compact) {
				write_json_document(asbrun::harvest_json(_module, _path, _bytes.size(), false), json_path);
				return 0;
			}
			std::string object =
				"  {\"file\":\"" + asbrun::json_escape(_path) + "\",\"size\":" + std::to_string(_bytes.size()) +
				",\"parse_ok\":" + (_module.error.empty() ? "true" : "false") +
				",\"consumed\":" + std::to_string(_module.consumed) + ",\"total\":" + std::to_string(_module.total) +
				",\"error\":\"" + asbrun::json_escape(_module.error) +
				"\",\"functions\":" + std::to_string(_module.functions.size()) +
				",\"natives\":" + std::to_string(natives) + ",\"assets\":" + std::to_string(assets) +
				",\"sections\":" + std::to_string(sections.size()) +
				",\"scene_functions\":" + std::to_string(scene_functions) + "}";
			write_json_document(object, json_path);
			return 0;
		}

		std::cout << "file: " << _path << '\n';
		std::cout << "size: " << _bytes.size() << '\n';
		std::cout << "parse: " << (_module.error.empty() ? "OK" : "FAIL") << " consumed=" << _module.consumed << '/'
				  << _module.total << '\n';
		std::cout << "functions: " << _module.functions.size() << '\n';
		std::cout << "natives: " << natives << '\n';
		std::cout << "assets: " << assets << '\n';
		std::cout << "sections: " << sections.size() << '\n';
		std::cout << "scene functions: " << scene_functions << '\n';
		return 0;
	}

	int cmd_funcs(arg_list args, int) {
		if (args.size() != 1) {
			std::cerr << "funcs: usage: funcs\n";
			return 2;
		}
		require_module();
		for (size_t i = 0; i < _module.functions.size(); ++i) {
			const auto& function = *_module.functions[i];
			std::cout << '[' << i << "] " << function.sig.decl();
			if (!function.script_section.empty())
				std::cout << "  section=" << function.script_section;
			std::cout << '\n';
		}
		return 0;
	}

	int cmd_disasm(arg_list args, int) {
		bool all = false;
		std::string directory;
		std::string function_name;
		for (size_t i = 1; i < args.size(); ++i) {
			if (args[i] == "--all") {
				all = true;
			} else if (args[i] == "--dir") {
				if (i + 1 >= args.size()) {
					std::cerr << "disasm: --dir requires a path\n";
					return 2;
				}
				directory = args[++i];
			} else if (function_name.empty()) {
				function_name = args[i];
			} else {
				std::cerr << "disasm: too many arguments\n";
				return 2;
			}
		}
		require_module();

		if (!function_name.empty() && !all && directory.empty()) {
			const asb::Function* function = find_module_function(function_name);
			if (!function) {
				std::cerr << "disasm: no such function: " << function_name << '\n';
				return 1;
			}
			std::cout << asb::disassemble(_module, *function);
			return 0;
		}

		if (!all && directory.empty()) {
			std::cerr << "disasm: specify FUNC, --all, or --dir DIR\n";
			return 2;
		}

		std::string text;
		for (const auto& function : _module.functions)
			text += asb::disassemble(_module, *function) + "\n";
		if (!directory.empty()) {
			fs::create_directories(directory);
			const fs::path path = fs::path(directory) / (fs::path(_path).filename().string() + ".txt");
			std::ofstream out(path, std::ios::binary | std::ios::trunc);
			if (!out)
				throw std::runtime_error("cannot write " + path.string());
			out << text;
			std::cerr << "wrote " << path.string() << '\n';
		} else {
			std::cout << text;
		}
		return 0;
	}

	int cmd_scene(arg_list args, int) {
		bool ir = false;
		bool index = false;
		bool limit_set = false;
		size_t limit = 0;
		std::string json_path;
		std::string function_name;
		std::string filter;
		std::string chara_file;
		for (size_t i = 1; i < args.size(); ++i) {
			if (args[i] == "--ir") {
				ir = true;
			} else if (args[i] == "--index") {
				index = true;
			} else if (args[i] == "--json") {
				if (i + 1 >= args.size()) {
					std::cerr << "scene: --json requires a path\n";
					return 2;
				}
				json_path = args[++i];
			} else if (args[i] == "--limit") {
				if (i + 1 >= args.size()) {
					std::cerr << "scene: --limit requires a value\n";
					return 2;
				}
				try {
					limit = static_cast<size_t>(std::stoull(args[++i]));
				} catch (...) {
					std::cerr << "scene: invalid --limit value\n";
					return 2;
				}
				limit_set = true;
			} else if (args[i] == "--filter") {
				if (i + 1 >= args.size()) {
					std::cerr << "scene: --filter requires a value\n";
					return 2;
				}
				filter = args[++i];
			} else if (args[i] == "--chara-file") {
				if (i + 1 >= args.size()) {
					std::cerr << "scene: --chara-file requires a path\n";
					return 2;
				}
				chara_file = args[++i];
			} else if (function_name.empty()) {
				function_name = args[i];
			} else {
				std::cerr << "scene: too many arguments\n";
				return 2;
			}
		}
		if (ir && index) {
			std::cerr << "scene: --ir and --index are mutually exclusive\n";
			return 2;
		}
		if ((ir || index) && (limit_set || !filter.empty())) {
			std::cerr << "scene: --limit/--filter apply to event output, not --ir/--index\n";
			return 2;
		}
		require_module();
		auto load_chars = [&]() {
			asb::CharTable chars;
			if (!chara_file.empty()) {
				chars = asb::load_char_table(chara_file);
			} else {
				const fs::path chars_path = fs::path(rosalinde_data_dir()) / "chara_table.tsv";
				if (fs::exists(chars_path))
					chars = asb::load_char_table(chars_path.string());
			}
			return chars;
		};

		const std::optional<size_t> json_limit = limit_set && limit > 0 ? std::optional<size_t>(limit) : std::nullopt;
		if (!json_path.empty()) {
			if (index) {
				write_json_document(asbrun::scene_index_json(_module, _path), json_path);
			} else if (ir) {
				asb::CharTable chars = load_chars();
				write_json_document(asbrun::scene_ir_json(_module, _path, chars.empty() ? nullptr : &chars), json_path);
			} else {
				write_json_document(asbrun::scene_json(_module, _path, function_name, filter, json_limit), json_path);
			}
			return 0;
		}

		if (index) {
			size_t sprite_calls = 0;
			size_t chara_calls = 0;
			size_t stage_calls = 0;
			size_t camera_calls = 0;
			for (const auto& function : scene().functions) {
				for (const auto& event : function.events) {
					const bool chara = asb::is_chara_create_call(event.call);
					if (asb::is_sprite_create_call(event.call) && !chara)
						++sprite_calls;
					if (chara)
						++chara_calls;
					if (event.call == "DrCreateStage" || event.call == "FlCreateStage" ||
						event.call == "StSetBattleScene" || event.call == "StCreateUnit" ||
						event.call == "StCreateDemoUnit" || event.call == "FlCreateCharaSymbol")
						++stage_calls;
					if (event.call == "DrSetCameraPosV" || event.call == "SetCameraPosV" ||
						event.call == "DrSetCameraParam")
						++camera_calls;
				}
			}
			std::cout << "counts: static event stream (not runtime)\n";
			std::cout << "parse: " << (_module.error.empty() && _module.consumed == _module.total ? "OK" : "FAIL")
					  << '\n';
			std::cout << "sprite calls: " << sprite_calls << '\n';
			std::cout << "chara calls: " << chara_calls << '\n';
			std::cout << "entity calls: " << sprite_calls + chara_calls << '\n';
			std::cout << "stage calls: " << stage_calls << '\n';
			std::cout << "camera calls: " << camera_calls << '\n';
			return 0;
		}

		if (ir) {
			asb::CharTable chars = load_chars();
			bool found = false;
			for (const auto& function : scene().functions) {
				if (!function_name.empty() && function.name != function_name && function.decl != function_name)
					continue;
				const asb::SceneIR scene_ir = asb::build_scene_ir(function, _path, chars.empty() ? nullptr : &chars);
				if (scene_ir.entities.empty())
					continue;
				found = true;
				std::cout << "function " << function.name << " entities=" << scene_ir.entities.size() << '\n';
				for (const auto& entity : scene_ir.entities) {
					std::cout << "  " << entity.id << " kind=" << entity.kind << " mbs=" << entity.mbs
							  << " ftx=" << entity.ftx << " track=" << entity.track << " index=" << entity.track_index
							  << " pos=(" << entity.pos[0] << ',' << entity.pos[1] << ',' << entity.pos[2]
							  << ") scale=" << entity.scale << " visible=" << (entity.visible ? "true" : "false")
							  << '\n';
				}
			}
			if (!found) {
				std::cerr << "scene: no entities\n";
				return 1;
			}
			return 0;
		}

		const std::string filter_needle = asbrun::lower(filter);
		const size_t text_limit = limit_set ? limit : 50;
		size_t emitted = 0;
		size_t matched = 0;
		bool found = false;
		for (const auto& function : scene().functions) {
			if (!function_name.empty() && function.name != function_name && function.decl != function_name)
				continue;
			bool header = false;
			for (size_t i = 0; i < function.events.size(); ++i) {
				const auto& event = function.events[i];
				if (!filter_needle.empty() && asbrun::lower(event.call).find(filter_needle) == std::string::npos &&
					asbrun::lower(event.decl).find(filter_needle) == std::string::npos) {
					bool string_match = false;
					for (const auto& value : event.strings) {
						if (asbrun::lower(value).find(filter_needle) != std::string::npos) {
							string_match = true;
							break;
						}
					}
					if (!string_match)
						continue;
				}
				++matched;
				if (text_limit > 0 && emitted >= text_limit)
					continue;
				if (!header) {
					header = true;
					found = true;
					std::cout << function.name << " events=" << function.events.size();
					if (!function.section.empty())
						std::cout << " section=" << function.section;
					std::cout << '\n';
				}
				std::cout << "  [" << i << "] " << scene_event_text(event) << '\n';
				++emitted;
			}
		}
		if (matched == 0) {
			std::cerr << "scene: no matching events\n";
			return 1;
		}
		if (text_limit > 0 && matched > emitted)
			std::cout << "... " << (matched - emitted) << " more event(s); use --limit 0 for all\n";
		return found ? 0 : 1;
	}

	int cmd_scan(arg_list args, int) {
		asbrun::ScanOptions options;
		for (size_t i = 1; i < args.size(); ++i) {
			if (args[i] == "--all") {
				continue;
			} else if (args[i] == "--summary") {
				options.summary = true;
			} else if (args[i] == "--scene") {
				options.scene = true;
			} else if (args[i] == "--scene-index") {
				options.scene_index = true;
			} else if (args[i] == "--scene-ir") {
				options.scene_ir = true;
			} else if (args[i] == "--disasm") {
				options.disasm = true;
			} else if (args[i] == "--disasm-dir") {
				if (i + 1 >= args.size()) {
					std::cerr << "scan: --disasm-dir requires a path\n";
					return 2;
				}
				options.disasm_dir = args[++i];
			} else if (args[i] == "--json") {
				if (i + 1 >= args.size()) {
					std::cerr << "scan: --json requires a path\n";
					return 2;
				}
				options.json_path = args[++i];
			} else if (args[i] == "--filter") {
				if (i + 1 >= args.size()) {
					std::cerr << "scan: --filter requires a value\n";
					return 2;
				}
				options.filter = args[++i];
			} else if (args[i] == "--chara-file") {
				if (i + 1 >= args.size()) {
					std::cerr << "scan: --chara-file requires a path\n";
					return 2;
				}
				options.chara_file = args[++i];
			} else if (args[i] == "--limit") {
				if (i + 1 >= args.size()) {
					std::cerr << "scan: --limit requires a value\n";
					return 2;
				}
				try {
					options.limit = static_cast<size_t>(std::stoull(args[++i]));
				} catch (...) {
					std::cerr << "scan: invalid --limit value\n";
					return 2;
				}
			} else {
				std::cerr << "scan: unknown option '" << args[i] << "'\n";
				return 2;
			}
		}
		const int modes = int(options.summary) + int(options.scene) + int(options.scene_index) + int(options.scene_ir);
		if (modes > 1) {
			std::cerr << "scan: choose one of --summary, --scene, --scene-index, --scene-ir\n";
			return 2;
		}
		return asbrun::scan(_archive, options);
	}

	int cmd_export(arg_list args, int) {
		if (args.size() < 2) {
			std::cerr << "export: usage: export chara|enums|bgs [--out FILE|-]\n";
			return 2;
		}
		const std::string kind = args[1];
		std::string out_path = "-";
		for (size_t i = 2; i < args.size(); ++i) {
			if (args[i] == "--out") {
				if (i + 1 >= args.size()) {
					std::cerr << "export: --out requires a path\n";
					return 2;
				}
				out_path = args[++i];
			} else {
				std::cerr << "export: unknown option '" << args[i] << "'\n";
				return 2;
			}
		}
		if (kind == "chara")
			return asbrun::export_chara(_archive, out_path);
		if (kind == "enums")
			return asbrun::export_enums(_archive, out_path);
		if (kind == "bgs")
			return asbrun::export_bgs(_archive, out_path);
		std::cerr << "export: unknown table '" << kind << "'\n";
		return 2;
	}

	int cmd_timeline(arg_list args, int) {
		bool expand = false;
		bool limit_set = false;
		size_t limit = 0;
		std::string json_path;
		std::string function_name;

		for (size_t i = 1; i < args.size(); ++i) {
			if (args[i] == "--expand-calls") {
				expand = true;
			} else if (args[i] == "--json") {
				if (i + 1 >= args.size()) {
					std::cerr << "timeline: --json requires a path\n";
					return 2;
				}
				json_path = args[++i];
			} else if (args[i] == "--limit") {
				if (i + 1 >= args.size()) {
					std::cerr << "timeline: --limit requires a value\n";
					return 2;
				}
				try {
					limit = static_cast<size_t>(std::stoull(args[++i]));
				} catch (...) {
					std::cerr << "timeline: invalid --limit value\n";
					return 2;
				}
				limit_set = true;
			} else if (function_name.empty()) {
				function_name = args[i];
			} else {
				std::cerr << "timeline: too many arguments\n";
				return 2;
			}
		}
		if (function_name.empty()) {
			std::cerr << "timeline: usage: timeline FUNC [--expand-calls] [--json FILE|-] [--limit N]\n";
			return 2;
		}
		require_module();

		const asb::SceneFunction* root = nullptr;
		for (const auto& function : scene().functions) {
			if (function.name == function_name || function.decl == function_name) {
				root = &function;
				break;
			}
		}
		if (!root) {
			std::cerr << "timeline: no such function: " << function_name << '\n';
			return 1;
		}

		struct Row {
			long frame = 0;
			std::string call;
			std::string kind;
			std::string import_from;
			int depth = 0;
		};
		std::vector<Row> rows;
		long frame = 0;
		const size_t row_limit = limit_set ? limit : 0;

		auto source_kind = [](const asb::SceneEvent& event) -> std::string {
			if (!event.import_from.empty())
				return "module";
			switch (event.kind) {
			case 'a':
				return "native";
			case 'm':
				return "module";
			case 's':
				return "system";
			default:
				return "unknown";
			}
		};

		std::function<void(const asb::SceneFunction&, int)> walk;
		walk = [&](const asb::SceneFunction& function, int depth) {
			for (const auto& event : function.events) {
				if (row_limit > 0 && rows.size() >= row_limit)
					return;
				rows.push_back({frame, scene_event_text(event), source_kind(event), event.import_from, depth});
				if (event.call == "Wait") {
					const long wait = event.ints.empty() ? 0 : static_cast<long>(event.ints.front());
					frame += std::max(1L, wait);
					continue;
				}
				if (expand && depth == 0 && event.kind == 'm') {
					for (const auto& candidate : scene().functions) {
						if (candidate.name == event.call || candidate.decl == event.decl) {
							walk(candidate, depth + 1);
							break;
						}
					}
				}
			}
		};
		walk(*root, 0);

		if (json_path.empty()) {
			for (const auto& row : rows) {
				std::cout << std::setw(6) << row.frame << "  " << std::string(row.depth * 2, ' ') << row.call << "  "
						  << row.kind;
				if (!row.import_from.empty())
					std::cout << " from=" << row.import_from;
				std::cout << '\n';
			}
			return 0;
		}

		std::string object = "  {\"file\":\"" + asbrun::json_escape(_path) + "\",\"function\":\"" +
							 asbrun::json_escape(root->name) + "\",\"expand_calls\":" + (expand ? "true" : "false") +
							 ",\"rows\":[";
		bool first = true;
		for (const auto& row : rows) {
			object += first ? "" : ",";
			first = false;
			object += "{\"frame\":" + std::to_string(row.frame) + ",\"call\":\"" + asbrun::json_escape(row.call) +
					  "\",\"kind\":\"" + asbrun::json_escape(row.kind) + "\",\"depth\":" + std::to_string(row.depth);
			if (!row.import_from.empty())
				object += ",\"import_from\":\"" + asbrun::json_escape(row.import_from) + "\"";
			object += "}";
		}
		object += "]}";
		write_json_object(object, json_path);
		return 0;
	}

	int cmd_exec(arg_list args, int) {
		std::string function_name;
		for (size_t i = 1; i < args.size(); ++i) {
			if (args[i] == "--func") {
				if (i + 1 >= args.size()) {
					std::cerr << "exec: --func requires a value\n";
					return 2;
				}
				function_name = args[++i];
			} else if (function_name.empty()) {
				function_name = args[i];
			} else {
				std::cerr << "exec: too many arguments\n";
				return 2;
			}
		}
		require_module();

		asb::Host host;
		host.set_call_log({});
		host.register_module(_module);
		if (!host.load(std::span<const uint8_t>(_bytes.data(), _bytes.size())))
			return 3;
		asIScriptFunction* target = find_entry_function(host, function_name);
		if (!target) {
			std::cerr << "exec: no entry function found\n";
			return 4;
		}
		const int result = host.execute(target);
		return result == asEXECUTION_FINISHED ? 0 : 1;
	}

	int cmd_eval(arg_list args, int) {
		if (args.size() < 2) {
			std::cerr << "eval: usage: eval SOURCE\n";
			return 2;
		}
		std::string source;
		for (size_t i = 1; i < args.size(); ++i) {
			if (i > 1)
				source += ' ';
			source += args[i];
		}

		asb::Host host;
		if (!host.compile(source))
			return 3;
		asIScriptFunction* target = host.find_function({"main", "scene00", "MAIN"});
		if (!target) {
			std::cerr << "eval: no entry function found\n";
			return 4;
		}
		const int result = host.execute(target);
		return result == asEXECUTION_FINISHED ? 0 : 1;
	}

	int cmd_source(arg_list args, int depth) {
		if (args.size() != 2) {
			std::cerr << "source: usage: source FILE\n";
			return 2;
		}
		if (depth >= max_source_depth) {
			std::cerr << "source: maximum nesting depth exceeded\n";
			return 1;
		}
		std::ifstream in(args[1]);
		if (!in) {
			std::cerr << "source: " << args[1] << ": cannot open\n";
			return 1;
		}
		int status = 0;
		std::string line;
		while (_running && std::getline(in, line)) {
			const int current = execute_line(line, depth + 1);
			if (current != 0 && status == 0)
				status = current;
		}
		if (in.bad()) {
			std::cerr << "source: " << args[1] << ": read error\n";
			status = 1;
		}
		return status;
	}

	int cmd_help(arg_list args, int) {
		if (args.size() > 2) {
			std::cerr << "help: too many arguments\n";
			return 2;
		}
		if (args.size() == 2) {
			for (const auto& command : command_list()) {
				if (command.name == args[1]) {
					std::cout << command.usage << " - " << command.summary << '\n';
					return 0;
				}
			}
			if (const std::string_view hint = retired_hint(args[1]); !hint.empty()) {
				std::cout << args[1] << " moved to: viewer --cpk <CPK> --script <SCRIPT> --headless " << hint
						  << '\n';
				return 0;
			}
			std::cerr << "help: unknown command '" << args[1] << "'\n";
			return 1;
		}
		for (const auto& command : command_list())
			std::cout << command.usage << " - " << command.summary << '\n';
		std::cout << "\nScene execution moved to: viewer --cpk <CPK> --script <SCRIPT> --headless ...\n";
		return 0;
	}

	int cmd_quit(arg_list args, int) {
		if (args.size() != 1) {
			std::cerr << "quit: too many arguments\n";
			return 2;
		}
		_running = false;
		return 0;
	}

	std::string prompt() const {
		if (_loaded)
			return "asbrun:" + _path + "> ";
		return "asbrun:/> ";
	}

	cpk::archive& _archive;
	bool _interactive = false;
	bool _running = true;
	bool _had_error = false;
	bool _loaded = false;
	std::string _path;
	std::vector<uint8_t> _bytes;
	asb::Module _module;
	std::optional<asb::Scene> _scene;
	std::deque<std::string> _history;
};

std::span<const Shell::command_spec> Shell::command_list() {
	static constexpr command_spec list[] = {
		{"open", "open PATH", "load an ASB from the CPK", &Shell::cmd_open},
		{"close", "close", "unload the current module", &Shell::cmd_close},
		{"status", "status", "show the current CPK/module", &Shell::cmd_status},
		{"summary", "summary [--json FILE|-] [--compact]", "show module metadata", &Shell::cmd_summary},
		{"scan",
		 "scan [--summary|--scene|--scene-index|--scene-ir] [--json FILE|-] [--filter TEXT] "
		 "[--limit N] [--chara-file FILE] [--disasm|--disasm-dir DIR]",
		 "scan all Script/*.asb entries",
		 &Shell::cmd_scan},
		{"export",
		 "export chara|enums|bgs [--out FILE|-]",
		 "generate asset/table TSVs from the CPK",
		 &Shell::cmd_export},
		{"funcs", "funcs", "list module functions", &Shell::cmd_funcs},
		{"disasm", "disasm [FUNC] [--all] [--dir DIR]", "disassemble functions", &Shell::cmd_disasm},
		{"scene",
		 "scene [FUNC] [--json FILE|-] [--ir] [--index] [--limit N] [--filter TEXT] [--chara-file FILE]",
		 "inspect extracted scene events",
		 &Shell::cmd_scene},
		{"timeline",
		 "timeline FUNC [--expand-calls] [--json FILE|-] [--limit N]",
		 "build a static frame-annotated call timeline",
		 &Shell::cmd_timeline},
		{"exec", "exec [FUNC]", "run a VM function without scene modeling", &Shell::cmd_exec},
		{"eval", "eval SOURCE", "compile and execute AngelScript source", &Shell::cmd_eval},
		{"source", "source FILE", "run commands from a file", &Shell::cmd_source},
		{"help", "help [COMMAND]", "show command help", &Shell::cmd_help},
		{"quit", "quit", "leave the shell", &Shell::cmd_quit},
		{"exit", "exit", "leave the shell", &Shell::cmd_quit},
		{"q", "q", "leave the shell", &Shell::cmd_quit},
	};
	return std::span<const command_spec>(list);
}

} // namespace

int main(int argc, char** argv) {
	std::string cpk_path;
	bool cpk_set = false;

	for (int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		if (arg == "--cpk") {
			if (cpk_set) {
				std::cerr << "asbrun: duplicate --cpk\n";
				return 2;
			}
			if (i + 1 >= argc) {
				print_usage(std::cerr);
				return 2;
			}
			cpk_set = true;
			cpk_path = argv[++i];
		} else if (arg.starts_with("--cpk=")) {
			if (cpk_set) {
				std::cerr << "asbrun: duplicate --cpk\n";
				return 2;
			}
			cpk_set = true;
			cpk_path = std::string(arg.substr(6));
		} else if (arg == "-h" || arg == "--help") {
			print_usage(std::cout);
			return 0;
		} else {
			std::cerr << "asbrun: unknown argument '" << arg << "'\n";
			print_usage(std::cerr);
			return 2;
		}
	}

	if (!cpk_set || cpk_path.empty()) {
		print_usage(std::cerr);
		return 2;
	}

	try {
		const fs::path path = fs::absolute(fs::path(cpk_path));
		if (!fs::is_regular_file(path)) {
			std::cerr << "asbrun: not a regular file: " << path.string() << '\n';
			return 1;
		}

		cpk::archive archive(path);
		const bool interactive = stdin_is_tty();
		if (interactive) {
			std::cerr << "asbrun: " << path.string() << " (" << archive.size() << " entries)\n"
					  << "Type 'help' for commands.\n";
		}
		Shell shell(archive, interactive);
		return shell.run(std::cin);
	} catch (const std::exception& e) {
		std::cerr << "asbrun: " << e.what() << '\n';
		return 1;
	} catch (...) {
		std::cerr << "asbrun: unknown error\n";
		return 1;
	}
}
