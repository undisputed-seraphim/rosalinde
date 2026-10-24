#pragma once

#include <scarlett/animation.hpp>
#include <scarlett/asb.hpp>
#include <scarlett/scene.hpp>
#include <scarlett/trace.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

class asIScriptEngine;
class asIScriptModule;
class asIScriptFunction;

namespace asb {

class RuntimeSession;

// Hosts a decoded module inside a stock AngelScript 2.34 engine: it registers
// the engine setup and the module's imports, loads the compiled bytecode and
// runs scene functions. The imported natives are generic stubs for now.
class Host {
public:
	friend class RuntimeSession;

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

	// Engine CharaSet appearance table, used to resolve world characters
	// (DrCreateChara/DrCreateNpc), whose call carries no asset. Not owned.
	void set_chara_set_table(const CharaSetTable& sets);

	// Optional animation-length provider (frames) for an asset + track index,
	// used to model `WaitSpriteMoneAnimationEnd` during run().
	void set_track_frames(std::function<long(const std::string&, int)> fn);
	void set_track_info(TrackInfoProvider fn);

	// Optional animation-name resolver: maps an MBS track name (as passed to
	// `DrSetIdleAnimation` / `DrAnimation*`) to its track index for the asset.
	// Returns < 0 when unknown, leaving the entity with no selected track.
	void set_resolve_track(std::function<int(const std::string&, const std::string&)> fn);

	// Optional facility display-name resolver (`GetStationFacilityName`): maps a
	// facility id/enum name to its localized name. Empty when unknown.
	void set_facility_name(std::function<std::string(const std::string&)> fn);

	// Stage base levels/cues and facility positions (engine `.data` dump).
	void set_stage_table(const StageTable& stages);

	// Drop tables (engine `.data` + DROPTABLE/ITEMID enums).
	void set_drop_tables(const DropTables& drops);

	// Optional sound cue length resolver (frames), used by `GetSoundLength`.
	void set_sound_length(std::function<int(int)> fn);

	// Run a function under a virtual clock: `Wait(n)` parks the script for n
	// frames, the scene-relevant natives populate a runtime scene, and a
	// keyframe is recorded whenever it changes. Returns the resulting timeline.
	Timeline run(asIScriptFunction* fn, std::string source, long max_frames = 200000);

	// Override the native call trace target; by default calls print to stdout.
	void set_call_log(std::function<void(const std::string&)> cb);

	// Optional structured runtime event sink shared by all sessions created
	// from this host.
	void set_trace(TraceCallback cb);

	// Optional complete-native-call sink shared by all sessions created from
	// this host. Emitted once per generic native invocation, after dispatch.
	void set_native_call(NativeCallCallback cb);

private:
	struct Impl;
	Impl* _impl;
};

// One-shot: parse `bytecode`, register + load it into `host`, pick the scene
// entry function (`func`, else void MAIN(), else scene00/…) and run it to a Timeline.
// Returns an empty timeline if the module fails to load or has no scene
// function. `host` should be freshly constructed.
Timeline run_module(
	Host& host,
	std::span<const uint8_t> bytecode,
	const std::string& source,
	const std::string& func = {},
	long max_frames = 200000);

} // namespace asb
