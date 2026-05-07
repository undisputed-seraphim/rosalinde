#pragma once

#include <eltolinde.hpp>
#include <glm/glm.hpp>
#include <impl/mbs/sections.hpp>

#include <cstdint>
#include <string>
#include <vector>

#pragma pack(push, 1)
struct SpriteVertex {
	int16_t texid;
	glm::vec2 uv;
	glm::vec3 xyz;
	uint32_t color;
};
#pragma pack(pop)

struct CachedKeyframe {
	struct Layer {
		uint16_t tex_id;
		uint16_t _pad;
		glm::vec2 uv[4];
		glm::vec2 xy[4];
		uint32_t color[4];
		uint32_t attributes;
	};
	std::vector<Layer> layers;
	glm::vec4 bounds;
};

struct SpriteData {
	std::vector<FTX::Entry> textures;
	mbs::v77 v77;

	struct FrameRun {
		uint32_t s8_start;
		uint32_t s8_count;
		uint8_t s8_st;
		uint32_t loop_start;
	};
	struct Track {
		std::string name;
		std::vector<FrameRun> runs;
		glm::vec4 bounds;
	};
	std::vector<Track> tracks;
	std::vector<CachedKeyframe> keyframes;

	static SpriteData load(std::span<const uint8_t> mbs_raw, std::vector<FTX::Entry> textures);

	const Track* find_track(const std::string& name) const;

private:
	void preprocess();
};

struct SpriteInstance {
	const SpriteData* data = nullptr;
	uint32_t track_idx = 0;
	uint32_t variant_flags = 0;

	float _accum = 0.0f;
	std::vector<uint32_t> ticks;
	std::vector<uint32_t> offsets;
	uint32_t _frame_counter = 0;

	void play(uint32_t track_id);
	void play(const std::string& name);
	void update(float dt_seconds);
	void next_track();
	void prev_track();

	void build_vertices(uint32_t sa_idx, std::vector<SpriteVertex>& verts, std::vector<uint32_t>& indices) const;

	glm::mat4 transform_for_sa(uint32_t sa_idx, bool* out_flipx = nullptr, bool* out_flipy = nullptr) const;
	uint32_t sa_count() const;
	uint32_t frame_counter() const;
	glm::vec4 track_bounds() const;
};
