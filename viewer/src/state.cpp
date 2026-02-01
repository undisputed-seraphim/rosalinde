#include "state.hpp"
#include "tables.hpp"

#include <glad/glad.h>
#include <spanstream>

State::State(std::filesystem::path path)
	: _cpkt(TopLevelCpk(path).getTableOfContents())
	, _camera(2.5) {
	// glGenFramebuffers(1, &_tgt_fb);
	// glBindFramebuffer(GL_FRAMEBUFFER, _tgt_fb);
}

State::~State() noexcept { /*glDeleteFramebuffers(1, &_tgt_fb);*/ }

void State::loadSprite(const std::string& classname, const std::string& charaname, uint32_t trackid) {
	const auto iter = Characters.find(classname);
	if (iter == Characters.end()) {
		throw std::runtime_error("Entry for character class " + classname + " was not found.");
	}

	const auto& job = iter->second;
	std::cout << job.mbs.dir << '\t' << job.mbs.path << '\n';
	if (auto entry = _cpkt.find_file(job.mbs.dir, job.mbs.path); entry == _cpkt.end()) {
		throw std::runtime_error("MBS for character class " + classname + " was not found.");
	} else {
		_cpkt.extract(*entry, _buffer);
	}

	auto mbs = MBS::From(_buffer);
	auto ftx = std::vector<FTX::Entry>();
	auto flags = iter->second.variants.at(charaname);
	if (auto entry = _cpkt.find_file(job.ftx.dir, job.ftx.path); entry == _cpkt.end()) {
		throw std::runtime_error("FTX for character class " + classname + " was not found.");
	} else {
		_cpkt.extract(*entry, _buffer);
		auto txt = FTX::parse(_buffer);
		std::move(txt.begin(), txt.end(), std::back_inserter(ftx));
	}
	_sprites.emplace_back(Sprite(std::move(mbs), std::move(ftx), flags, trackid));
}

void State::handleEvent(const SDL_Event& event) { _camera.handleInput(event); }

void State::render(const glm::mat4& projection) {
	for (auto& sprite : _sprites) {
		sprite.render(_camera, projection);
	}
}