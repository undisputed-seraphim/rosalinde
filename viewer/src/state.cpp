#include "state.hpp"
#include "tables.hpp"

#include <glad/glad.h>
#include <spanstream>

static GLuint make_texture_array(std::vector<FTX::Entry> textures) {
	unsigned int max_x = 0, max_y = 0;
	for (auto& t : textures) {
		FTX::decompress(t);
		FTX::deswizzle(t);
		max_x = std::max(max_x, t.width);
		max_y = std::max(max_y, t.height);
		std::cout << t.name << '\t' << t.width << 'x' << t.height << '\n';
	}
	GLuint id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D_ARRAY, id);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_ALWAYS);

	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, max_x, max_y, textures.size());
	for (int i = 0; i < textures.size(); ++i) {
		const auto& t = textures[i];
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, t.width, t.height, 1, GL_RGBA, GL_UNSIGNED_BYTE, t.rgba.data());
	}
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	return id;
}

State::State(const std::string& path)
	: _cpk(path) {
	//glGenFramebuffers(1, &_tgt_fb);
	//glBindFramebuffer(GL_FRAMEBUFFER, _tgt_fb);
}

State::~State() noexcept { /*glDeleteFramebuffers(1, &_tgt_fb);*/ }

State::Sprite State::FetchCharacterSprite(const std::string& classname, const std::string& charaname) {
	const auto iter = Characters.find(classname);
	if (iter == Characters.end()) {
		throw std::runtime_error("Entry for character class " + classname + " was not found.");
	}

	if (auto entry = _cpk.by_name(iter->second.mbs[0]); entry == _cpk.end()) {
		throw std::runtime_error("MBS for character class " + classname + " was not found.");
	} else {
		_cpk.extract(*entry, _buffer);
	}

	Sprite sprite { MBS::From(_buffer) };
	sprite.flags = iter->second.variants.at(charaname);
	for (const auto& ftx : iter->second.ftx) {
		auto entry = _cpk.by_name(ftx);
		if (entry == _cpk.end()) {
			continue;
		}
		_cpk.extract(*entry, _buffer);
		auto txt = FTX::parse(_buffer);
		std::move(txt.begin(), txt.end(), std::back_inserter(sprite.textures));
	}
	sprite.glTexHandle = make_texture_array(sprite.textures);
	return sprite;
}

State::Sprite State::FetchBackgroundSprite(const std::string& name) {
	const auto iter = BattleBGs.find(name);
	if (iter == BattleBGs.end()) {
		throw std::runtime_error("Entry for background" + name + " was not found.");
	}

	if (auto entry = _cpk.by_name(iter->second.mbs[0]); entry == _cpk.end()) {
		throw std::runtime_error("MBS for background" + name + " was not found.");
	} else {
		_cpk.extract(*entry, _buffer);
	}

	Sprite sprite { MBS::From(_buffer) };
	for (const auto& ftx : iter->second.ftx) {
		auto entry = _cpk.by_name(ftx);
		if (entry == _cpk.end()) {
			continue;
		}
		_cpk.extract(*entry, _buffer);
		auto txt = FTX::parse(_buffer);
		std::move(txt.begin(), txt.end(), std::back_inserter(sprite.textures));
	}
	sprite.glTexHandle = make_texture_array(sprite.textures);
	return sprite;
}