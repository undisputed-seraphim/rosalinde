#pragma once

#include <scarlett/asb.hpp>

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

	asIScriptModule* module() const;

	// First matching entry function by candidate name, or nullptr.
	asIScriptFunction* find_function(const std::vector<std::string>& names) const;

	// Execute a function to completion (aborts past a step limit). Returns the
	// AngelScript execution result.
	int execute(asIScriptFunction* fn);

	// Override the native call trace target; by default calls print to stdout.
	void set_call_log(std::function<void(const std::string&)> cb);

private:
	struct Impl;
	Impl* _impl;
};

} // namespace asb
