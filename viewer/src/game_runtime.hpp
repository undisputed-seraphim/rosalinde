#pragma once

#include "drama_session.hpp"
#include "mbs_catalog.hpp"
#include "viewer_config.hpp"

#include <scarlett/save.hpp>
#include <scarlett/scene.hpp>

#include <memory>
#include <string>

class AssetLoader;

// Application-level runtime. Owns the persistent save and the active scene
// mode; drama playback is one mode backed by a DramaSession. Field, battle, and
// menu modes are explicit placeholders until their simulators exist.
class GameRuntime {
public:
	using TraceCallback = DramaSession::TraceCallback;
	using NativeCallback = DramaSession::NativeCallback;

	GameRuntime(
		const AssetLoader& loader,
		const MbsCatalog& mbs,
		const ViewerConfig& config,
		TraceCallback trace,
		NativeCallback native_call);
	~GameRuntime();

	bool start();

	void set_max_frames(long frames);
	asb::RuntimeSession::StepResult advance();

	const asb::SceneState& scene() const;
	long frame() const;
	bool finished() const;
	const std::string& error() const;
	const asb::Module& module() const;
	const std::string& entry_name() const;
	const MbsCatalog& mbs_catalog() const { return _mbs; }
	asb::SaveState& save() { return _save; }
	const asb::SaveState& save() const { return _save; }
	asb::SceneMode mode() const { return _mode; }

private:
	bool start_drama_session();

	const AssetLoader& _loader;
	const MbsCatalog& _mbs;
	const ViewerConfig& _config;
	asb::SaveState _save;
	asb::SceneMode _mode = asb::SceneMode::Drama;
	TraceCallback _trace;
	NativeCallback _native_call;
	std::unique_ptr<DramaSession> _drama_session;
	std::string _error;
};
