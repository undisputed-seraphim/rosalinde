#include "playback_runtime.hpp"

#include "asset_loader.hpp"
#include "runtime_data.hpp"
#include "sprite.hpp"
#include "tables.hpp"

#include <criware/cpk.hpp>
#include <scarlett/asb.hpp>

#include <algorithm>
#include <span>
#include <stdexcept>
#include <unordered_map>

namespace {

long mbs_track_frames(const AssetLoader& loader, const std::string& mbs, int track_index) {
	if (mbs.empty() || track_index < 0)
		return 0;
	static std::unordered_map<std::string, long> cache;
	const std::string key = mbs + '\x1f' + std::to_string(track_index);
	if (auto it = cache.find(key); it != cache.end())
		return it->second;

	long n = 0;
	try {
		if (auto p = loader.resolve(mbs)) {
			const cpk::archive& cpk = loader.cpk();
			if (auto e = cpk.find(p->dir, p->path); e != cpk.end()) {
				std::vector<char> buf;
				cpk.extract(*e, buf);
				SpriteData data = SpriteData::load(
					std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buf.data()), buf.size()), {});
				n = track_duration_frames(data, track_index);
			}
		}
	} catch (...) {
		n = 0;
	}
	cache[key] = n;
	return n;
}

int mbs_resolve_track(const AssetLoader& loader, const std::string& mbs, const std::string& name) {
	if (mbs.empty() || name.empty())
		return -1;
	static std::unordered_map<std::string, int> cache;
	const std::string key = mbs + '\x1f' + name;
	if (auto it = cache.find(key); it != cache.end())
		return it->second;

	int index = -1;
	try {
		if (auto p = loader.resolve(mbs)) {
			const cpk::archive& cpk = loader.cpk();
			if (auto e = cpk.find(p->dir, p->path); e != cpk.end()) {
				std::vector<char> buf;
				cpk.extract(*e, buf);
				SpriteData data = SpriteData::load(
					std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buf.data()), buf.size()), {});
				if (const auto* track = data.find_track(name))
					index = static_cast<int>(track - data.tracks.data());
			}
		}
	} catch (...) {
		index = -1;
	}
	cache[key] = index;
	return index;
}

} // namespace

PlaybackRuntime::PlaybackRuntime(
	const AssetLoader& loader,
	const ViewerConfig& config,
	TraceCallback trace,
	NativeCallback native_call)
	: _loader(loader) {
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
	_host.set_call_log([](const std::string&) {});
	_host.set_track_frames([this](const std::string& mbs, int index) {
		return mbs_track_frames(_loader, mbs, index);
	});
	_host.set_resolve_track([this](const std::string& mbs, const std::string& name) {
		return mbs_resolve_track(_loader, mbs, name);
	});
	_host.set_trace(std::move(trace));
	_host.set_native_call(std::move(native_call));
	_host.register_module(*_module);
	if (!_host.load(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size())))
		throw std::runtime_error("failed to load " + config.script);

	asb::Scene scene = asb::extract_scene(*_module);
	const asb::SceneFunction* picked = asb::pick_scene_function(scene, config.scene_func);
	if (!picked)
		throw std::runtime_error("no scene function in " + config.script);
	_entry_name = picked->name;
	asIScriptFunction* fn = _host.find_function({picked->name});
	if (!fn)
		throw std::runtime_error("scene function not found: " + picked->name);
	asIScriptFunction* prelude = _host.find_function({"ON_TITLE_UI_START"});

	_session = std::make_unique<asb::RuntimeSession>(_host, config.script);
	if (!_session->start(fn, prelude))
		throw std::runtime_error("could not start " + config.script + ": " + _session->error());
}

PlaybackRuntime::~PlaybackRuntime() = default;

asb::RuntimeSession::StepResult PlaybackRuntime::advance() {
	return _session->advance();
}

const asb::SceneState& PlaybackRuntime::scene() const {
	return _session->scene();
}

long PlaybackRuntime::frame() const {
	return _session->frame();
}

bool PlaybackRuntime::finished() const {
	return _session->finished();
}

const std::string& PlaybackRuntime::error() const {
	return _session->error();
}
