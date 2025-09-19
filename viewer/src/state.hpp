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

	struct Job {
		std::vector<std::string> mbs, ftx;					// mbs and ftx filenames. Most will have just one each
		std::unordered_map<std::string, uint32_t> variants; // Flags for each variant of the character
	};

	struct Sprite {
		MBS mbs;
		std::vector<FTX::Entry> textures;
		uint32_t glTexHandle;
		uint32_t flags;
	};

	Sprite FetchSprite(const std::string& classname, const std::string& charaname);
};

extern const std::unordered_map<std::string, State::Job> Characters;