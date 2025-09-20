#pragma once

#include <eltolinde.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "shader.hpp"

class State {
	CPK _cpk;
	uint32_t _tgt_fb;
	std::vector<char> _buffer;

public:
	State(const std::string& path);
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
