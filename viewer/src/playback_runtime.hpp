#pragma once

#include "mbs_info.hpp"
#include "runtime_data.hpp"
#include "viewer_config.hpp"

#include <scarlett/host.hpp>
#include <scarlett/scene.hpp>
#include <scarlett/runtime.hpp>

#include <functional>
#include <memory>
#include <string>

class AssetLoader;

class PlaybackRuntime {
public:
	using TraceCallback = std::function<void(const asb::TraceEvent&)>;
	using NativeCallback = std::function<void(const asb::NativeCall&)>;

	PlaybackRuntime(
		const AssetLoader& loader,
		const ViewerConfig& config,
		TraceCallback trace,
		NativeCallback native_call);
	~PlaybackRuntime();

	void set_max_frames(long frames);
	asb::RuntimeSession::StepResult advance();
	const asb::SceneState& scene() const;
	long frame() const;
	bool finished() const;
	const std::string& error() const;
	const asb::Module& module() const { return *_module; }
	const std::string& entry_name() const { return _entry_name; }
	const MbsInfoCache& mbs_info() const { return _mbs; }

private:
	void log_archive_events();

	const AssetLoader& _loader;
	MbsInfoCache _mbs;
	ViewerRuntimeData _runtime;
	asb::Host _host;
	std::shared_ptr<asb::Module> _module;
	std::string _entry_name;
	std::unique_ptr<asb::RuntimeSession> _session;
	size_t _seen_ui = 0;
};
