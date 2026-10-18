#include "dump/dump.hpp"
#include "line_editor.hpp"

#include <criware/cpk.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
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

constexpr size_t no_entry = static_cast<size_t>(-1);
constexpr int max_source_depth = 16;

std::string_view trim(std::string_view text) {
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
		text.remove_prefix(1);
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
		text.remove_suffix(1);
	return text;
}

bool has_trailing_separator(std::string_view text) {
	return !text.empty() && (text.back() == '/' || text.back() == '\\');
}

fs::path expand_real_path(std::string_view text) {
	if (text.empty() || text.front() != '~' || (text.size() > 1 && text[1] != '/' && text[1] != '\\'))
		return fs::path(std::string(text));

	const char* home = std::getenv("HOME");
#if defined(_WIN32)
	if (!home || !*home)
		home = std::getenv("USERPROFILE");
	if (!home || !*home) {
		const char* drive = std::getenv("HOMEDRIVE");
		const char* path = std::getenv("HOMEPATH");
		if (drive && *drive && path && *path) {
			std::string_view rest = text.substr(1);
			while (!rest.empty() && (rest.front() == '/' || rest.front() == '\\'))
				rest.remove_prefix(1);
			fs::path result = fs::path(std::string(drive) + std::string(path));
			if (!rest.empty())
				result /= fs::path(std::string(rest));
			return result;
		}
	}
#endif
	if (!home || !*home)
		throw std::runtime_error("cannot expand ~: home directory is not set");

	std::string_view rest = text.substr(1);
	while (!rest.empty() && (rest.front() == '/' || rest.front() == '\\'))
		rest.remove_prefix(1);

	fs::path result = fs::path(home);
	if (!rest.empty())
		result /= fs::path(std::string(rest));
	return result;
}

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

struct vnode {
	std::string name;
	bool directory = true;
	size_t entry = no_entry;
	vnode* parent = nullptr;
	std::map<std::string, std::unique_ptr<vnode>, std::less<>> children;
};

struct lookup_result {
	const vnode* node = nullptr;
	bool missing = false;
	bool not_directory = false;
};

class virtual_tree {
public:
	explicit virtual_tree(std::span<const cpk::entry> entries)
		: _root(std::make_unique<vnode>()) {
		for (size_t i = 0; i < entries.size(); ++i) {
			const auto& item = entries[i];
			vnode* current = _root.get();

			std::string_view dir = item.dir;
			size_t start = 0;
			while (start < dir.size()) {
				const size_t slash = dir.find('/', start);
				const std::string_view component =
					dir.substr(start, slash == std::string_view::npos ? std::string_view::npos : slash - start);
				if (!component.empty()) {
					if (component == "." || component == "..")
						throw std::runtime_error(
							"CPK contains an invalid directory component: " + std::string(component));
					current = add_directory(current, component);
				}
				if (slash == std::string_view::npos)
					break;
				start = slash + 1;
			}

			if (item.name.empty() || item.name == "." || item.name == ".." ||
				item.name.find('/') != std::string_view::npos)
				throw std::runtime_error("CPK contains an invalid file name: " + std::string(item.name));
			add_file(current, item.name, i);
		}
	}

	const vnode* root_node() const noexcept { return _root.get(); }

	lookup_result resolve(const vnode* base, std::string_view path) const {
		if (!base)
			return {nullptr, true, false};

		const vnode* current = (!path.empty() && path.front() == '/') ? _root.get() : base;
		const bool trailing_slash = !path.empty() && path.back() == '/';
		size_t start = 0;

		while (start < path.size()) {
			const size_t slash = path.find('/', start);
			const size_t end = slash == std::string_view::npos ? path.size() : slash;
			const std::string_view component = path.substr(start, end - start);

			if (!current->directory)
				return {nullptr, false, true};

			if (!component.empty() && component != ".") {
				if (component == "..") {
					if (current->parent)
						current = current->parent;
				} else {
					const auto it = current->children.find(component);
					if (it == current->children.end())
						return {nullptr, true, false};
					current = it->second.get();
				}
			}

			if (slash == std::string_view::npos)
				break;
			start = slash + 1;
		}

		if (trailing_slash && !current->directory)
			return {nullptr, false, true};
		return {current, false, false};
	}

	std::string path_of(const vnode* node) const {
		if (!node || node == _root.get())
			return "/";

		std::vector<std::string_view> parts;
		for (const vnode* current = node; current && current->parent; current = current->parent)
			parts.push_back(current->name);

		std::string result;
		for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
			result += '/';
			result += *it;
		}
		return result.empty() ? "/" : result;
	}

private:
	std::string child_path(const vnode* parent, std::string_view name) const {
		const std::string base = path_of(parent);
		return base == "/" ? "/" + std::string(name) : base + "/" + std::string(name);
	}

	vnode* add_directory(vnode* parent, std::string_view name) {
		const auto it = parent->children.find(name);
		if (it != parent->children.end()) {
			if (!it->second->directory)
				throw std::runtime_error("CPK has a file where a directory is required: " + child_path(parent, name));
			return it->second.get();
		}

		auto child = std::make_unique<vnode>();
		child->name = name;
		child->directory = true;
		child->parent = parent;
		auto* raw = child.get();
		parent->children.emplace(std::string(name), std::move(child));
		return raw;
	}

	void add_file(vnode* parent, std::string_view name, size_t entry_index) {
		const auto it = parent->children.find(name);
		if (it != parent->children.end())
			throw std::runtime_error("CPK contains a conflicting path: " + child_path(parent, name));

		auto child = std::make_unique<vnode>();
		child->name = name;
		child->directory = false;
		child->entry = entry_index;
		child->parent = parent;
		parent->children.emplace(std::string(name), std::move(child));
	}

	std::unique_ptr<vnode> _root;
};

class shell {
public:
	shell(cpk::archive& archive, bool interactive)
		: _archive(archive)
		, _tree(archive.entries())
		, _cwd(_tree.root_node())
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
		int (shell::*handler)(arg_list, int);
	};

	struct extraction_stats {
		size_t files = 0;
		uint64_t compressed = 0;
		uint64_t decompressed = 0;
	};

	struct subtree_stats {
		size_t files = 0;
		size_t directories = 0;
		uint64_t compressed = 0;
		uint64_t decompressed = 0;
	};

	static std::span<const command_spec> command_list();

	int execute_line(std::string_view line, int depth) {
		line = trim(line);
		if (line.empty() || line.front() == '#')
			return 0;

		std::string error;
		auto args = tokenize(line, error);
		if (!args) {
			std::cerr << "rosalinde: " << error << '\n';
			_had_error = true;
			return 2;
		}
		if (args->empty())
			return 0;

		int status = 0;
		try {
			status = dispatch(*args, depth);
		} catch (const std::exception& e) {
			std::cerr << "rosalinde: " << e.what() << '\n';
			status = 1;
		} catch (...) {
			std::cerr << "rosalinde: unknown error\n";
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
		std::cerr << "rosalinde: unknown command '" << args[0] << "' (type 'help')\n";
		return 1;
	}

	int cmd_ls(arg_list args, int) {
		bool long_format = false;
		bool path_set = false;
		std::string_view path;

		for (size_t i = 1; i < args.size(); ++i) {
			if (args[i] == "-l" || args[i] == "--long") {
				long_format = true;
			} else if (!path_set) {
				path = args[i];
				path_set = true;
			} else {
				std::cerr << "ls: too many arguments\n";
				return 2;
			}
		}

		const lookup_result found = _tree.resolve(_cwd, path);
		const std::string_view display = path.empty() ? std::string_view(".") : path;

		if (found.not_directory) {
			std::cerr << "ls: " << display << ": Not a directory\n";
			return 1;
		}
		if (found.missing || !found.node) {
			std::cerr << "ls: " << display << ": No such file or directory\n";
			return 1;
		}
		if (!found.node->directory) {
			if (long_format)
				print_file_long(found.node);
			else
				std::cout << found.node->name << '\n';
			return 0;
		}

		for (const auto& [name, child] : found.node->children) {
			if (long_format) {
				if (child->directory)
					print_directory_long(child.get());
				else
					print_file_long(child.get());
			} else {
				std::cout << name;
				if (child->directory)
					std::cout << '/';
				std::cout << '\n';
			}
		}
		return 0;
	}

	int cmd_stat(arg_list args, int) {
		if (args.size() != 2) {
			std::cerr << "stat: usage: stat PATH\n";
			return 2;
		}

		const std::string_view path = args[1];
		const lookup_result found = _tree.resolve(_cwd, path);
		const std::string_view display = path.empty() ? std::string_view(".") : path;
		if (found.not_directory) {
			std::cerr << "stat: " << display << ": Not a directory\n";
			return 1;
		}
		if (found.missing || !found.node) {
			std::cerr << "stat: " << display << ": No such file or directory\n";
			return 1;
		}

		std::cout << "Path: " << virtual_path(found.node) << '\n';
		if (!found.node->directory) {
			const cpk::entry& item = _archive.at(found.node->entry);
			std::cout << "Type: file\n";
			std::cout << "ID: " << item.id << '\n';
			std::cout << "Compressed: " << item.file_size << '\n';
			std::cout << "Decompressed: " << item.extract_size << '\n';
			std::cout << "Ratio: " << ratio_text(item.file_size, item.extract_size) << '\n';
			std::cout << "Offset: " << item.offset << '\n';
			return 0;
		}

		subtree_stats stats;
		collect_stats(found.node, stats);
		std::cout << "Type: directory\n";
		std::cout << "Entries: " << found.node->children.size() << '\n';
		std::cout << "Files: " << stats.files << '\n';
		std::cout << "Directories: " << stats.directories << '\n';
		std::cout << "Compressed: " << stats.compressed << '\n';
		std::cout << "Decompressed: " << stats.decompressed << '\n';
		std::cout << "Ratio: " << ratio_text(stats.compressed, stats.decompressed) << '\n';
		return 0;
	}

	int cmd_find(arg_list args, int) {
		bool long_format = false;
		bool path_set = false;
		std::string_view path;
		std::string_view text;

		for (size_t i = 1; i < args.size(); ++i) {
			if (args[i] == "-l" || args[i] == "--long") {
				long_format = true;
			} else if (!path_set) {
				path = args[i];
				path_set = true;
			} else if (text.empty()) {
				text = args[i];
			} else {
				std::cerr << "find: too many arguments\n";
				return 2;
			}
		}
		if (!path_set)
			path = ".";

		const lookup_result found = _tree.resolve(_cwd, path);
		const std::string_view display = path.empty() ? std::string_view(".") : path;
		if (found.not_directory) {
			std::cerr << "find: " << display << ": Not a directory\n";
			return 1;
		}
		if (found.missing || !found.node) {
			std::cerr << "find: " << display << ": No such file or directory\n";
			return 1;
		}

		if (!found.node->directory) {
			if (contains_case_insensitive(virtual_path(found.node), text)) {
				if (long_format)
					print_file_long(found.node);
				else
					std::cout << virtual_path(found.node) << '\n';
			}
			return 0;
		}

		find_files(found.node, text, long_format);
		return 0;
	}

	int cmd_d(arg_list args, int) {
		constexpr std::string_view usage =
			"d PATH [--json] [--full] [--limit N] [--section sN] [--track N] [--seq N] [--timeline] "
			"[--colors]";
		if (args.size() < 2) {
			std::cerr << "d: usage: " << usage << '\n';
			return 2;
		}

		dump::options options;
		auto parse_size = [&](size_t& index, std::string_view name) -> std::optional<size_t> {
			if (index + 1 >= args.size()) {
				std::cerr << "d: " << name << " requires a value\n";
				return std::nullopt;
			}
			const std::string& text = args[++index];
			size_t value = 0;
			const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
			if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
				std::cerr << "d: invalid " << name << " value '" << text << "'\n";
				return std::nullopt;
			}
			return value;
		};

		for (size_t i = 2; i < args.size(); ++i) {
			const std::string_view option = args[i];
			if (option == "--json") {
				options.json = true;
			} else if (option == "--full") {
				options.full = true;
			} else if (option == "--timeline") {
				options.timeline = true;
			} else if (option == "--colors") {
				options.colors = true;
			} else if (option == "--limit") {
				const auto value = parse_size(i, "--limit");
				if (!value)
					return 2;
				options.limit_set = true;
				options.limit = *value;
			} else if (option == "--section") {
				if (i + 1 >= args.size()) {
					std::cerr << "d: --section requires a value\n";
					return 2;
				}
				options.section = args[++i];
			} else if (option == "--track") {
				const auto value = parse_size(i, "--track");
				if (!value)
					return 2;
				options.track = *value;
			} else if (option == "--seq" || option == "--sequence") {
				const auto value = parse_size(i, "--seq");
				if (!value)
					return 2;
				options.sequence = *value;
			} else {
				std::cerr << "d: unknown option '" << option << "'\n";
				std::cerr << "d: usage: " << usage << '\n';
				return 2;
			}
		}

		const std::string_view path = args[1];
		const lookup_result found = _tree.resolve(_cwd, path);
		const std::string_view display = path.empty() ? std::string_view(".") : path;
		if (found.not_directory) {
			std::cerr << "d: " << display << ": Not a directory\n";
			return 1;
		}
		if (found.missing || !found.node) {
			std::cerr << "d: " << display << ": No such file or directory\n";
			return 1;
		}
		if (found.node->directory) {
			std::cerr << "d: " << display << ": Is a directory\n";
			return 1;
		}

		return dump::dispatch(_archive, _archive.at(found.node->entry), virtual_path(found.node), options);
	}

	int cmd_e(arg_list args, int) {
		if (args.size() < 2 || args.size() > 3) {
			std::cerr << "e: usage: e VIRTUAL_PATH [REAL_PATH]\n";
			return 2;
		}

		const std::string_view virtual_path = args[1];
		const std::string_view real_text = args.size() == 3 ? std::string_view(args[2]) : std::string_view(".");
		const std::string_view display = virtual_path.empty() ? std::string_view(".") : virtual_path;
		if (real_text.empty()) {
			std::cerr << "e: empty destination\n";
			return 2;
		}

		const lookup_result found = _tree.resolve(_cwd, virtual_path);
		if (found.not_directory) {
			std::cerr << "e: " << display << ": Not a directory\n";
			return 1;
		}
		if (found.missing || !found.node) {
			std::cerr << "e: " << display << ": No such file or directory\n";
			return 1;
		}

		fs::path destination;
		try {
			destination = expand_real_path(real_text);
		} catch (const std::exception& e) {
			std::cerr << "e: " << e.what() << '\n';
			return 1;
		}

		fs::path check_path = destination;
		if (check_path.filename().empty() && check_path.has_parent_path()) {
			const fs::path parent = check_path.parent_path();
			if (!parent.empty() && parent != check_path)
				check_path = parent;
		}
		bool destination_exists = false;
		bool destination_is_directory = false;
		try {
			destination_exists = fs::exists(check_path);
			destination_is_directory = fs::is_directory(check_path);
		} catch (const std::exception& e) {
			std::cerr << "e: " << real_text << ": " << e.what() << '\n';
			return 1;
		}

		if (found.node->directory) {
			if (destination_exists && !destination_is_directory) {
				std::cerr << "e: " << real_text << ": Not a directory\n";
				return 1;
			}

			extraction_stats stats;
			if (!extract_directory(found.node, destination, stats))
				return 1;
			std::cout << "Extracted " << stats.files << " files (compressed=" << stats.compressed
					  << ", decompressed=" << stats.decompressed << ") -> " << destination.string() << '\n';
			return 0;
		}

		if (destination_is_directory || has_trailing_separator(real_text)) {
			if (destination_exists && !destination_is_directory) {
				std::cerr << "e: " << real_text << ": Not a directory\n";
				return 1;
			}
			destination /= fs::path(found.node->name);
		}

		extraction_stats stats;
		return extract_file(found.node, destination, stats) ? 0 : 1;
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

	int cmd_source(arg_list args, int depth) {
		if (args.size() != 2) {
			std::cerr << "source: usage: source FILE\n";
			return 2;
		}
		if (depth >= max_source_depth) {
			std::cerr << "source: maximum nesting depth exceeded\n";
			return 1;
		}

		std::ifstream in{std::string(args[1])};
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

	int cmd_quit(arg_list args, int) {
		if (args.size() != 1) {
			std::cerr << "quit: too many arguments\n";
			return 2;
		}
		_running = false;
		return 0;
	}

	void remember(std::string_view line) {
		const std::string_view trimmed = trim(line);
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

	std::string virtual_path(const vnode* node) const {
		std::string path = _tree.path_of(node);
		if (path.size() > 1 && path.front() == '/')
			path.erase(0, 1);
		return path;
	}

	static std::string ratio_text(uint64_t compressed, uint64_t decompressed) {
		if (compressed == 0)
			return "0.00";
		return std::format("{:.2f}", static_cast<double>(decompressed) / static_cast<double>(compressed));
	}

	void print_file_long(const vnode* node) const {
		const cpk::entry& item = _archive.at(node->entry);
		std::cout << item.id << ": " << virtual_path(node) << "  compressed=" << item.file_size
				  << "  decompressed=" << item.extract_size
				  << "  ratio=" << ratio_text(item.file_size, item.extract_size) << '\n';
	}

	void print_directory_long(const vnode* node) const {
		std::cout << "-: " << virtual_path(node) << "/  entries=" << node->children.size() << '\n';
	}

	void collect_stats(const vnode* node, subtree_stats& stats) const {
		for (const auto& [name, child] : node->children) {
			if (child->directory) {
				++stats.directories;
				collect_stats(child.get(), stats);
			} else {
				++stats.files;
				const cpk::entry& item = _archive.at(child->entry);
				stats.compressed += item.file_size;
				stats.decompressed += item.extract_size;
			}
		}
	}

	static bool contains_case_insensitive(std::string_view text, std::string_view needle) {
		if (needle.empty())
			return true;
		if (needle.size() > text.size())
			return false;
		for (size_t i = 0; i + needle.size() <= text.size(); ++i) {
			bool match = true;
			for (size_t j = 0; j < needle.size(); ++j) {
				const auto left = static_cast<unsigned char>(text[i + j]);
				const auto right = static_cast<unsigned char>(needle[j]);
				if (std::tolower(left) != std::tolower(right)) {
					match = false;
					break;
				}
			}
			if (match)
				return true;
		}
		return false;
	}

	void find_files(const vnode* node, std::string_view text, bool long_format) const {
		for (const auto& [name, child] : node->children) {
			if (child->directory) {
				find_files(child.get(), text, long_format);
				continue;
			}
			const std::string path = virtual_path(child.get());
			if (!contains_case_insensitive(path, text))
				continue;
			if (long_format)
				print_file_long(child.get());
			else
				std::cout << path << '\n';
		}
	}

	bool extract_file(const vnode* node, const fs::path& target, extraction_stats& stats) {
		const cpk::entry& item = _archive.at(node->entry);
		try {
			const fs::path parent = target.parent_path();
			if (!parent.empty())
				fs::create_directories(parent);
			_archive.extract_to_file(item, target);
		} catch (const std::exception& e) {
			std::cerr << "e: " << virtual_path(node) << ": " << e.what() << '\n';
			return false;
		}

		std::cout << "Extracted " << virtual_path(node) << " -> " << target.string()
				  << " (compressed=" << item.file_size << ", decompressed=" << item.extract_size << ")\n";
		++stats.files;
		stats.compressed += item.file_size;
		stats.decompressed += item.extract_size;
		return true;
	}

	bool extract_directory(const vnode* node, const fs::path& target, extraction_stats& stats) {
		try {
			fs::create_directories(target);
		} catch (const std::exception& e) {
			std::cerr << "e: " << target.string() << ": " << e.what() << '\n';
			return false;
		}

		for (const auto& [name, child] : node->children) {
			const fs::path child_target = target / fs::path(name);
			const bool ok = child->directory ? extract_directory(child.get(), child_target, stats)
											 : extract_file(child.get(), child_target, stats);
			if (!ok)
				return false;
		}
		return true;
	}

	std::string prompt() const { return "rosalinde:" + _tree.path_of(_cwd) + "> "; }

	cpk::archive& _archive;
	virtual_tree _tree;
	const vnode* _cwd;
	bool _interactive;
	bool _running = true;
	bool _had_error = false;
	std::deque<std::string> _history;
};

std::span<const shell::command_spec> shell::command_list() {
	static constexpr command_spec list[] = {
		{"ls", "ls [-l] [PATH]", "list virtual directory contents", &shell::cmd_ls},
		{"find", "find [PATH] [TEXT] [--long]", "recursively find files", &shell::cmd_find},
		{"stat", "stat PATH", "show file or directory metadata", &shell::cmd_stat},
		{"d",
		 "d PATH [--json] [--full] [--limit N] [--section sN] [--track N] [--seq N] [--timeline] "
		 "[--colors]",
		 "inspect a CPK file in memory",
		 &shell::cmd_d},
		{"e",
		 "e VIRTUAL_PATH [REAL_PATH]",
		 "extract a file or directory contents (default destination: .)",
		 &shell::cmd_e},
		{"help", "help [COMMAND]", "show command help", &shell::cmd_help},
		{"source", "source FILE", "run commands from a host file", &shell::cmd_source},
		{"quit", "quit", "leave the shell", &shell::cmd_quit},
		{"exit", "exit", "leave the shell", &shell::cmd_quit},
		{"q", "q", "leave the shell", &shell::cmd_quit},
	};
	return std::span<const command_spec>(list);
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

void print_usage(std::ostream& out) { out << "usage: rosalinde --cpk PATH\n"; }

} // namespace

int main(int argc, char** argv) {
	std::string cpk_path;
	bool cpk_set = false;

	for (int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		if (arg == "--cpk") {
			if (cpk_set) {
				std::cerr << "rosalinde: duplicate --cpk\n";
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
				std::cerr << "rosalinde: duplicate --cpk\n";
				return 2;
			}
			cpk_set = true;
			cpk_path = std::string(arg.substr(6));
		} else {
			std::cerr << "rosalinde: unknown argument '" << arg << "'\n";
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
			std::cerr << "rosalinde: not a regular file: " << path.string() << '\n';
			return 1;
		}

		cpk::archive archive(path);
		const bool interactive = stdin_is_tty();
		if (interactive) {
			std::cerr << "rosalinde: " << path.string() << " (" << archive.size() << " entries)\n"
					  << "Type 'help' for commands.\n";
		}

		shell sh(archive, interactive);
		return sh.run(std::cin);
	} catch (const std::exception& e) {
		std::cerr << "rosalinde: " << e.what() << '\n';
		return 1;
	} catch (...) {
		std::cerr << "rosalinde: unknown error\n";
		return 1;
	}
}
