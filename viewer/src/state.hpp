#pragma once

#include <eltolinde.hpp>
#include <filesystem>
#include <string>
#include <vector>

#include "shader.hpp"

class State {
	CPKTable _cpkt;
	uint32_t _tgt_fb;
	std::vector<char> _buffer;

public:
	State(std::filesystem::path);
	State(const State&) = delete;
	State(State&&) noexcept = default;
	~State() noexcept;

	struct Sprite {
		MBS mbs;
		std::vector<FTX::Entry> textures;
		uint32_t glTexHandle;
		uint32_t flags;
	};

	Sprite FetchCharacterSprite(const std::string& classname, const std::string& charaname);
	Sprite FetchBackgroundSprite(const std::string& name);
};
