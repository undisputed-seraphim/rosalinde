#pragma once

#include <scarlett/asb.hpp>
#include <scarlett/scene.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

class asIScriptEngine;
class asIScriptModule;
class asIScriptFunction;

namespace asb {

// Hosts a decoded module inside a stock AngelScript 2.34 engine: it registers
// the engine setup and the module's imports, loads the compiled bytecode and
// runs scene functions. The imported natives are generic stubs for now.
class Host {
public:
	Host();
	~Host();
	Host(const Host&) = delete;
	Host& operator=(const Host&) = delete;

	// Register the object types, global/object properties and native
	// functions/behaviours the module imports. Call before load().
	void register_module(const Module& mod);

	// Load the compiled module. Returns true on success (r == 0).
	bool load(std::span<const uint8_t> bytecode);

	// Compile AngelScript source into the host module (for tests/minimal repros).
	bool compile(const std::string& source);

	asIScriptModule* module() const;

	// First matching entry function by candidate name, or nullptr.
	asIScriptFunction* find_function(const std::vector<std::string>& names) const;

	// Execute a function to completion (aborts past a step limit). Returns the
	// AngelScript execution result. Does not model the scene or time.
	int execute(asIScriptFunction* fn);

	// Display-name -> asset table used to resolve chara creates while recording.
	void set_char_table(const CharTable& chars);

	// Run a function under a virtual clock: `Wait(n)` parks the script for n
	// frames, the scene-relevant natives populate a runtime scene, and a
	// keyframe is recorded whenever it changes. Returns the resulting timeline.
	Timeline run(asIScriptFunction* fn, std::string source, long max_frames = 200000);

	// Override the native call trace target; by default calls print to stdout.
	void set_call_log(std::function<void(const std::string&)> cb);

private:
	struct Impl;
	Impl* _impl;
};

// One-shot: parse `bytecode`, register + load it into `host`, pick the scene
// entry function (`func`, else scene00/MAIN/…) and run it to a Timeline.
// Returns an empty timeline if the module fails to load or has no scene
// function. `host` should be freshly constructed.
Timeline run_module(
	Host& host,
	std::span<const uint8_t> bytecode,
	const std::string& source,
	const std::string& func = {},
	long max_frames = 200000);

} // namespace asb
