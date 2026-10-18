#include "asbrun_common.hpp"
#include "asbrun_inspect.hpp"
#include "asbrun_record.hpp"
#include "asbrun_runtime.hpp"
#include "asbrun_trace.hpp"
#include "line_editor.hpp"

#include <scarlett/asb.hpp>
#include <scarlett/host.hpp>
#include <scarlett/runtime.hpp>
#include <scarlett/scene.hpp>

#include <angelscript.h>
#include <criware/cpk.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
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

std::string scene_event_text(const asb::SceneEvent& event) {
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
	if (first && !event.floats.empty()) {
		char buffer[40];
		for (double value : event.floats) {
			std::snprintf(buffer, sizeof(buffer), "%g", value);
			append(buffer);
		}
	}
	if (first && !event.vec3.empty()) {
		char buffer[64];
		std::snprintf(buffer, sizeof(buffer), "(%g,%g,%g)", event.vec3[0], event.vec3[1], event.vec3[2]);
		append(buffer);
	} else if (first && !event.vec2.empty()) {
		char buffer[48];
		std::snprintf(buffer, sizeof(buffer), "(%g,%g)", event.vec2[0], event.vec2[1]);
		append(buffer);
	}
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

	enum class PlayMode { Play, Trace, Record };

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
		const std::string path = resolve_entry(args[1]);
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
		_runtime_loaded = false;

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
		for (size_t i = 1; i < args.size(); ++i) {
			if (args[i] == "--json") {
				if (i + 1 >= args.size()) {
					std::cerr << "summary: --json requires a path\n";
					return 2;
				}
				json_path = args[++i];
			} else {
				std::cerr << "summary: unknown option '" << args[i] << "'\n";
				return 2;
			}
		}
		require_module();

		if (!json_path.empty()) {
			write_json_document(asbrun::harvest_json(_module, _path, _bytes.size(), false), json_path);
			return 0;
		}

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

		std::cout << "file: " << _path << '\n';
		std::cout << "size: " << _bytes.size() << '\n';
		std::cout << "parse: " << (_module.error.empty() ? "OK" : "FAIL") << " consumed=" << _module.consumed << '/'
				  << _module.total << '\n';
		std::cout << "functions: " << _module.functions.size() << '\n';
		std::cout << "natives: " << natives << '\n';
		std::cout << "assets: " << assets << '\n';
		std::cout << "sections: " << sections.size() << '\n';
		std::cout << "scene functions: " << scene().functions.size() << '\n';
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
		std::string json_path;
		std::string function_name;
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
		require_module();

		if (!json_path.empty()) {
			if (index) {
				write_json_document(asbrun::scene_index_json(_module, _path), json_path);
			} else if (ir) {
				asb::CharTable chars;
				const fs::path chars_path = fs::path(_data_dir) / "chara_table.tsv";
				if (fs::exists(chars_path))
					chars = asb::load_char_table(chars_path.string());
				write_json_document(asbrun::scene_ir_json(_module, _path, chars.empty() ? nullptr : &chars), json_path);
			} else {
				write_json_document(asbrun::scene_json(_module, _path), json_path);
			}
			return 0;
		}

		if (index) {
			size_t sprites = 0;
			size_t chara = 0;
			size_t stages = 0;
			size_t cameras = 0;
			for (const auto& function : scene().functions) {
				for (const auto& event : function.events) {
					if (asb::is_sprite_create_call(event.call))
						++sprites;
					if (asb::is_chara_create_call(event.call))
						++chara;
					if (event.call == "DrCreateStage" || event.call == "FlCreateStage" ||
						event.call == "StSetBattleScene" || event.call == "StCreateUnit" ||
						event.call == "StCreateDemoUnit" || event.call == "FlCreateCharaSymbol")
						++stages;
					if (event.call == "DrSetCameraPosV" || event.call == "SetCameraPosV" ||
						event.call == "DrSetCameraParam")
						++cameras;
				}
			}
			std::cout << "parse: " << (_module.error.empty() && _module.consumed == _module.total ? "OK" : "FAIL")
					  << '\n';
			std::cout << "sprites: " << sprites << '\n';
			std::cout << "chara: " << chara << '\n';
			std::cout << "stages: " << stages << '\n';
			std::cout << "cameras: " << cameras << '\n';
			return 0;
		}

		if (ir) {
			asb::CharTable chars;
			const fs::path chars_path = fs::path(_data_dir) / "chara_table.tsv";
			if (fs::exists(chars_path))
				chars = asb::load_char_table(chars_path.string());
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

		bool found = false;
		for (const auto& function : scene().functions) {
			if (!function_name.empty() && function.name != function_name && function.decl != function_name)
				continue;
			found = true;
			std::cout << function.name << " events=" << function.events.size();
			if (!function.section.empty())
				std::cout << " section=" << function.section;
			std::cout << '\n';
			for (size_t i = 0; i < function.events.size(); ++i)
				std::cout << "  [" << i << "] " << scene_event_text(function.events[i]) << '\n';
		}
		if (!found) {
			std::cerr << "scene: no such function: " << function_name << '\n';
			return 1;
		}
		return 0;
	}

	int cmd_play_entry(arg_list args, int) { return cmd_play(args, PlayMode::Play); }
	int cmd_trace(arg_list args, int) { return cmd_play(args, PlayMode::Trace); }
	int cmd_record(arg_list args, int) { return cmd_play(args, PlayMode::Record); }

	int cmd_play(arg_list args, PlayMode mode) {
		long frames = 200000;
		std::string function_name;
		std::string trace_path;
		std::string record_path;
		std::string categories = "all";
		std::string chara_table;
		std::string data_dir;

		for (size_t i = 1; i < args.size(); ++i) {
			const std::string& option = args[i];
			auto value = [&](std::string_view name) -> std::optional<std::string> {
				if (i + 1 >= args.size()) {
					std::cerr << "play: " << name << " requires a value\n";
					return std::nullopt;
				}
				return args[++i];
			};

			if (option == "--frames") {
				auto text = value("--frames");
				if (!text)
					return 2;
				try {
					frames = std::stol(*text);
				} catch (...) {
					std::cerr << "play: invalid --frames value '" << *text << "'\n";
					return 2;
				}
			} else if (option == "--func") {
				auto text = value("--func");
				if (!text)
					return 2;
				function_name = *text;
			} else if (option == "--trace" || (option == "--out" && mode == PlayMode::Trace)) {
				auto text = value(option);
				if (!text)
					return 2;
				trace_path = *text;
			} else if (option == "--record" || (option == "--json" && mode == PlayMode::Record)) {
				auto text = value(option);
				if (!text)
					return 2;
				record_path = *text;
			} else if (option == "--categories") {
				auto text = value("--categories");
				if (!text)
					return 2;
				categories = *text;
			} else if (option == "--chara-table") {
				auto text = value("--chara-table");
				if (!text)
					return 2;
				chara_table = *text;
			} else if (option == "--data-dir") {
				auto text = value("--data-dir");
				if (!text)
					return 2;
				data_dir = *text;
			} else if (function_name.empty()) {
				function_name = option;
			} else {
				std::cerr << "play: unknown option '" << option << "'\n";
				return 2;
			}
		}

		require_module();

		if (mode == PlayMode::Trace && trace_path.empty())
			trace_path = "-";
		if (mode == PlayMode::Record && record_path.empty())
			record_path = "-";
		if (trace_path == "-" && record_path == "-") {
			std::cerr << "play: --trace and --record cannot both write to stdout\n";
			return 2;
		}

		if (!chara_table.empty())
			_chara_table = chara_table;
		if (!data_dir.empty())
			_data_dir = data_dir;
		if (!_runtime_loaded || !chara_table.empty() || !data_dir.empty()) {
			asbrun::load_runtime_data(_runtime, _archive, _chara_table, _data_dir);
			_runtime_loaded = true;
		}

		asb::Host host;
		host.set_call_log([](const std::string&) {});
		host.register_module(_module);
		asbrun::configure_host(host, _runtime);
		if (!host.load(std::span<const uint8_t>(_bytes.data(), _bytes.size())))
			return 3;

		asIScriptFunction* target = find_entry_function(host, function_name);
		if (!target) {
			std::cerr << "play: no entry function found\n";
			return 4;
		}

		std::ofstream trace_file;
		std::ostream* trace_out = &std::cout;
		if (!trace_path.empty() && trace_path != "-") {
			create_parent_directories(trace_path);
			trace_file.open(trace_path, std::ios::binary | std::ios::trunc);
			if (!trace_file)
				throw std::runtime_error("cannot write " + trace_path);
			trace_out = &trace_file;
		}
		asbrun::TraceFilter filter(categories);
		if (!trace_path.empty()) {
			host.set_trace([&](const asb::TraceEvent& event) {
				if (filter.contains(event.kind))
					asbrun::write_trace_event(*trace_out, event);
			});
		}

		asb::RuntimeSession session(host, _path);
		session.set_max_frames(frames);
		session.set_record_keyframes(!record_path.empty());
		if (!session.start(target)) {
			std::cerr << "play: " << session.error() << '\n';
			return 5;
		}
		while (!session.finished() && !session.limit_reached()) {
			if (session.advance() == asb::RuntimeSession::StepResult::Error) {
				std::cerr << "play: " << session.error() << '\n';
				return 5;
			}
		}

		if (!record_path.empty()) {
			asb::Timeline timeline = session.take_timeline();
			FILE* out = stdout;
			if (record_path != "-") {
				create_parent_directories(record_path);
				out = std::fopen(record_path.c_str(), "wb");
				if (!out)
					throw std::runtime_error("cannot write " + record_path);
			}
			asbrun::write_timeline_json(out, timeline);
			if (out != stdout)
				std::fclose(out);
		}
		if (!trace_path.empty() && trace_path != "-")
			trace_file.close();
		std::cerr << "asbrun: " << target->GetName() << ": frame " << session.frame()
				  << (session.finished() ? " (finished)" : " (limit)") << '\n';
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
		host.set_call_log([](const std::string&) {});
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
			std::cerr << "help: unknown command '" << args[1] << "'\n";
			return 1;
		}
		for (const auto& command : command_list())
			std::cout << command.usage << " - " << command.summary << '\n';
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
	bool _runtime_loaded = false;
	std::string _path;
	std::string _data_dir = "viewer/data";
	std::string _chara_table;
	std::vector<uint8_t> _bytes;
	asb::Module _module;
	std::optional<asb::Scene> _scene;
	asbrun::RuntimeData _runtime;
	std::deque<std::string> _history;
};

std::span<const Shell::command_spec> Shell::command_list() {
	static constexpr command_spec list[] = {
		{"open", "open PATH", "load an ASB from the CPK", &Shell::cmd_open},
		{"close", "close", "unload the current module", &Shell::cmd_close},
		{"status", "status", "show the current CPK/module", &Shell::cmd_status},
		{"summary", "summary [--json FILE|-]", "show module metadata", &Shell::cmd_summary},
		{"funcs", "funcs", "list module functions", &Shell::cmd_funcs},
		{"disasm", "disasm [FUNC] [--all] [--dir DIR]", "disassemble functions", &Shell::cmd_disasm},
		{"scene", "scene [FUNC] [--json FILE|-] [--ir] [--index]", "inspect extracted scene events", &Shell::cmd_scene},
		{"play",
		 "play [FUNC] [--frames N] [--trace FILE|-] [--record FILE|-] [--categories LIST] "
		 "[--chara-table FILE] [--data-dir DIR]",
		 "run a module and optionally trace/record it",
		 &Shell::cmd_play_entry},
		{"trace",
		 "trace [FUNC] [--out FILE|-] [--frames N] [--categories LIST] [--chara-table FILE]",
		 "alias for play with an NDJSON trace",
		 &Shell::cmd_trace},
		{"record",
		 "record [FUNC] [--json FILE|-] [--frames N] [--chara-table FILE]",
		 "alias for play with a timeline JSON",
		 &Shell::cmd_record},
		{"exec", "exec [FUNC]", "execute a function without the scene scheduler", &Shell::cmd_exec},
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
