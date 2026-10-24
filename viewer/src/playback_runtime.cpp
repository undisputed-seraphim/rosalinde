#include "playback_runtime.hpp"

#include "asset_loader.hpp"
#include "runtime_data.hpp"
#include "tables.hpp"

#include <criware/cpk.hpp>
#include <scarlett/animation.hpp>
#include <scarlett/asb.hpp>

#include <spdlog/spdlog.h>

#include <span>
#include <stdexcept>

PlaybackRuntime::PlaybackRuntime(
	const AssetLoader& loader,
	const MbsCatalog& mbs,
	const ViewerConfig& config,
	TraceCallback trace,
	NativeCallback native_call)
	: _loader(loader)
	, _mbs(mbs) {
	const cpk::archive& cpk = _loader.cpk();
	const size_t slash = config.script.find_last_of('/');
	const std::string dir = slash == std::string::npos ? "" : config.script.substr(0, slash);
	const std::string file = slash == std::string::npos ? config.script : config.script.substr(slash + 1);
	auto entry = cpk.find(dir, file);
	if (entry == cpk.end())
		throw std::runtime_error("script not found in CPK: " + config.script);

	std::vector<char> bytes;
	cpk.extract(*entry, bytes);
	asb::Module mod =
		asb::parse(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()));
	if (!mod.error.empty())
		throw std::runtime_error("failed to decode " + config.script + ": " + mod.error);
	_module = std::make_shared<asb::Module>(std::move(mod));

	const std::string data_dir = config.data_dir.empty() ? viewer_data_dir() : config.data_dir;
	_runtime = load_viewer_runtime_data(_loader.cpk(), config.chara_table, data_dir);
	configure_host(_host, _runtime);
	_host.set_call_log({});
	_host.set_track_frames([this](const std::string& mbs, int index) { return _mbs.get(mbs).duration(index); });
	_host.set_track_info([this](const std::string& mbs, int index) {
		asb::TrackInfo info;
		const auto& parsed = _mbs.get(mbs);
		if (const auto* track = parsed.track(index)) {
			info.duration_frames = track->duration_frames;
			info.one_shot = track->one_shot;
			info.continuation_track = track->continuation_track;
			info.continuation_name = track->continuation_name;
			info.transition_mode = track->transition_mode;
		}
		return info;
	});
	_host.set_resolve_track([this](const std::string& mbs, const std::string& name) {
		if (name.empty())
			return -1;
		return _mbs.get(mbs).resolve(name);
	});
	_host.set_trace(std::move(trace));
	_host.set_native_call(std::move(native_call));
	_host.register_module(*_module);
	if (!_host.load(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size())))
		throw std::runtime_error("failed to load " + config.script);

	asb::Scene scene = asb::extract_scene(*_module);
	const asb::SceneFunction* picked = asb::pick_scene_function(scene, config.scene_func);
	if (picked) {
		_entry_name = picked->name;
	}
	asIScriptFunction* fn = picked ? _host.find_function({picked->name}) : nullptr;
	if (!fn && !config.scene_func.empty()) {
		fn = _host.find_function({config.scene_func});
		if (fn)
			_entry_name = config.scene_func;
	}
	if (!fn)
		throw std::runtime_error(
			picked ? "scene function not found: " + picked->name : "no scene function in " + config.script);
	asIScriptFunction* prelude = _host.find_function({"ON_TITLE_UI_START"});

	_session = std::make_unique<asb::RuntimeSession>(_host, config.script);
	if (!_session->start(fn, prelude))
		throw std::runtime_error("could not start " + config.script + ": " + _session->error());
	log_archive_events();
}

PlaybackRuntime::~PlaybackRuntime() = default;

void PlaybackRuntime::set_max_frames(long frames) {
	if (_session)
		_session->set_max_frames(frames);
}

asb::RuntimeSession::StepResult PlaybackRuntime::advance() {
	const auto result = _session->advance();
	log_archive_events();
	return result;
}

void PlaybackRuntime::log_archive_events() {
	const auto& ui = _session->scene().ui;
	for (; _seen_ui < ui.size(); ++_seen_ui) {
		const auto& event = ui[_seen_ui];
		if (event.kind != "archive_open" && event.kind != "archive_close")
			continue;
		std::string pages;
		for (size_t i = 0; i < event.ints.size(); ++i) {
			if (i)
				pages += ',';
			pages += std::to_string(event.ints[i]);
		}
		spdlog::info(
			"fevrith archive: {}(\"{}\") pages=[{}]",
			event.kind == "archive_open" ? "OpenArchive" : "CloseArchive",
			event.text,
			pages);
	}
}

const asb::SceneState& PlaybackRuntime::scene() const { return _session->scene(); }

long PlaybackRuntime::frame() const { return _session->frame(); }

bool PlaybackRuntime::finished() const { return _session->finished(); }

const std::string& PlaybackRuntime::error() const { return _session->error(); }
