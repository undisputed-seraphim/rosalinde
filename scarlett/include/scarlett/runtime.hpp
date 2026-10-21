#pragma once

#include <scarlett/scene.hpp>

#include <memory>
#include <string>
#include <string_view>

class asIScriptFunction;

namespace asb {

class Host;

// Incremental execution of one module function. Each advance() moves the scene
// clock by one frame and resumes the VM when its next Wait expires. The scene
// is live: it is mutated in place and remains valid after the script finishes.
class RuntimeSession {
public:
	enum class StepResult {
		Advanced,
		Finished,
		Limit,
		Error,
	};

	explicit RuntimeSession(Host& host, std::string source = {});
	~RuntimeSession();

	RuntimeSession(const RuntimeSession&) = delete;
	RuntimeSession& operator=(const RuntimeSession&) = delete;

	bool start(asIScriptFunction* fn, asIScriptFunction* prelude = nullptr);

	StepResult advance();

	void set_max_frames(long frames) { _max_frames = frames; }
	void set_record_keyframes(bool on) { _record_keyframes = on; }

	const SceneState& scene() const;
	long frame() const { return _frame; }
	bool finished() const { return _finished; }
	bool limit_reached() const { return _limit; }
	const std::string& error() const { return _error; }
	asIScriptFunction* resolve_callback(std::string_view name) const;

	Timeline take_timeline();

private:
	struct Impl;

	void burst();
	bool run_prelude(asIScriptFunction* fn);

	Host& _host;
	std::string _source;
	std::unique_ptr<Impl> _impl;
	asIScriptFunction* _fn = nullptr;
	long _frame = 0;
	long _max_frames = 200000;
	bool _record_keyframes = false;
	bool _started = false;
	bool _finished = false;
	bool _limit = false;
	std::string _error;
};

} // namespace asb
