#pragma once

#include <scarlett/asb.hpp>
#include <scarlett/scene.hpp>

#include <string>
#include <unordered_map>
#include <vector>

class asIScriptGeneric;

namespace asb {

// The recording shim: models the scene-relevant native calls (create / mutate /
// camera) so a hosted module populates a runtime scene instead of hitting a
// no-op stub. Also owns the `Wait` scheduler hook.
class Registry {
public:
	// Display-name -> asset table used to resolve chara creates. Not owned.
	void set_char_table(const CharTable* chars) { _chars = chars; }

	void reset(std::string source, std::string function);

	// Model a native call. Returns true if recognised (and thus not handled by
	// the fallback stub). `gen` is the AngelScript generic call context.
	bool dispatch(asIScriptGeneric* gen);

	// Snapshot the current state into a keyframe if it changed since the last.
	void flush_keyframe();

	long frame = 0;
	// Frames to sleep requested by the last `Wait`; -1 when unset.
	long pending_wait = -1;

	Timeline& timeline() { return _timeline; }

private:
	bool construct_value(asIScriptGeneric* gen);
	int ensure(const std::string& name);
	void alias(int idx, const std::string& name);
	void create_sprite(
		const std::string& kind, const std::string& id, std::string mbs, std::string ftx, const float* pos3, int layer,
		float scale);
	void create_chara(const std::string& kind, const std::vector<std::string>& names);

	const CharTable* _chars = nullptr;
	Timeline _timeline;
	std::vector<RuntimeEntity> _entities;
	std::unordered_map<std::string, int> _by_name;
	RuntimeCamera _camera;
	bool _dirty = false;
};

} // namespace asb
