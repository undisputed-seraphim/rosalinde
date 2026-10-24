#include "game_runtime.hpp"

#include <stdexcept>
#include <utility>

GameRuntime::GameRuntime(
	const AssetLoader& loader,
	const MbsCatalog& mbs,
	const ViewerConfig& config,
	TraceCallback trace,
	NativeCallback native_call)
	: _loader(loader)
	, _mbs(mbs)
	, _config(config)
	, _mode(config.scene_mode)
	, _trace(std::move(trace))
	, _native_call(std::move(native_call)) {}

GameRuntime::~GameRuntime() = default;

bool GameRuntime::start() {
	switch (_mode) {
	case asb::SceneMode::Drama:
		return start_drama_session();
	case asb::SceneMode::Field:
	case asb::SceneMode::Battle:
	case asb::SceneMode::Menu:
		_error = std::string(asb::scene_mode_name(_mode)) + " scene mode is not implemented";
		return false;
	}
	_error = "unknown scene mode";
	return false;
}

bool GameRuntime::start_drama_session() {
	if (_config.script.empty()) {
		_error = "--script is required for drama mode";
		return false;
	}
	try {
		_drama_session = std::make_unique<DramaSession>(_loader, _mbs, _save, _mode, _config, _trace, _native_call);
	} catch (const std::exception& e) {
		_error = e.what();
		_drama_session = nullptr;
		return false;
	}
	_error.clear();
	return true;
}

void GameRuntime::set_max_frames(long frames) {
	if (_drama_session)
		_drama_session->set_max_frames(frames);
}

asb::RuntimeSession::StepResult GameRuntime::advance() {
	if (!_drama_session) {
		_error = "runtime not started";
		return asb::RuntimeSession::StepResult::Error;
	}
	return _drama_session->advance();
}

const asb::SceneState& GameRuntime::scene() const {
	if (!_drama_session)
		throw std::runtime_error("GameRuntime::scene before start");
	return _drama_session->scene();
}

long GameRuntime::frame() const { return _drama_session ? _drama_session->frame() : 0; }

bool GameRuntime::finished() const { return _drama_session ? _drama_session->finished() : true; }

const std::string& GameRuntime::error() const { return _drama_session ? _drama_session->error() : _error; }

const asb::Module& GameRuntime::module() const {
	if (!_drama_session)
		throw std::runtime_error("GameRuntime::module before start");
	return _drama_session->module();
}

const std::string& GameRuntime::entry_name() const {
	if (!_drama_session)
		throw std::runtime_error("GameRuntime::entry_name before start");
	return _drama_session->entry_name();
}
