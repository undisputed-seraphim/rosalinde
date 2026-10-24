#pragma once

#include "sprite_playback_state.hpp"

#include <eltolinde.hpp>
#include <glm/glm.hpp>
#include <impl/mbs/sections.hpp>

#include <cstdint>
#include <functional>
#include <map>
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
		uint32_t s4_layer_start = 0;
		uint32_t s4_source_tile_id = 0;
		uint8_t s4_flags = 0;
		uint16_t s1_uv_id = 0;
		uint16_t s2_vertex_id = 0;
		uint16_t s0_color_row_id = 0;
		uint8_t color_id = 0;
		uint32_t alt_color[4] = {};
		glm::vec2 uv[4];
		glm::vec2 s1_center{};
		glm::vec2 s1_topleft{};
		glm::vec2 xy[4];
		uint32_t color[4];
		uint32_t attributes;
		uint8_t blend;
		bool notex = false;
	};
	std::vector<Layer> layers;
	glm::vec4 bounds;
};

struct SpriteData {
	std::vector<FTX::Entry> textures;
	mbs::v77 v77;

	uint32_t attributes_mask = 0;
	std::vector<uint32_t> attribute_bits;

	struct FrameRun {
		uint32_t s8_start;
		uint32_t s8_count;
		uint8_t s8_start_offset;
		uint32_t loop_start;
		bool once; // s8_intro_ticks < 0: play through once, then hold the last frame
		uint16_t sa_track_slot;
	};
	struct Track {
		std::string name;
		std::vector<FrameRun> runs;
		glm::vec4 bounds;
		uint8_t sa_main_sequence = 0;
		uint8_t disabled = 0;
		bool one_shot = false;
		int next_track = -1;
		uint8_t transition_mode = 0;
	};
	std::vector<Track> tracks;
	std::vector<CachedKeyframe> keyframes;

	// Parse and preprocess raw FMBS bytes. `build` takes already-parsed sections
	// so the runtime catalog and the renderer can share one binary parse.
	static SpriteData load(std::span<const uint8_t> mbs_raw, std::vector<FTX::Entry> textures);
	static SpriteData build(mbs::v77 sections, std::vector<FTX::Entry> textures);

	const Track* find_track(const std::string& name) const;

private:
	void preprocess();
};

// Parse the FMBS header and versioned sections without building keyframes.
mbs::v77 parse_mbs_sections(std::span<const uint8_t> mbs_raw);

// Build track metadata from parsed sections. Used by both SpriteData and the
// runtime MBS catalog so timing and rendering see the same tracks.
std::vector<SpriteData::Track> analyze_tracks(const mbs::v77& sections);

// Longest sequence in a track, converted from MBS 120 Hz ticks to scene frames.
long track_duration_frames(const mbs::v77& sections, const SpriteData::Track& track);
long track_duration_frames(const SpriteData& data, int track);

struct LayerVisit {
	uint32_t run = 0;
	int32_t layer = -1;
	uint32_t s4_layer_start = 0;
	uint32_t s4_source_tile_id = 0;
	uint8_t s4_flags = 0;
	uint16_t s1_uv_id = 0;
	uint16_t s2_vertex_id = 0;
	uint16_t s0_color_row_id = 0;
	uint8_t color_id = 0;
	uint32_t alt_color[4] = {};
	uint32_t s6_keyframe_id = 0;
	uint32_t s7_transform_id = 0;
	uint32_t s8_frame_start = 0;
	uint32_t s8_flags = 0;
	uint16_t s8_frames = 0;
	uint8_t s6_keyframe_interpolation = 0;
	uint8_t s7_transform_interpolation = 0;
	uint16_t sa_track_slot = 0;
	uint32_t offset = 0;
	bool done = false;
	bool hidden = false;
	bool drawn = false;
	bool notex = false;
	bool flip_v = false;
	uint16_t tex_id = 0;
	uint16_t tex_width = 0;
	uint16_t tex_height = 0;
	uint32_t attributes = 0;
	uint8_t blend = 0;
	const char* skip = "";
	float depth = 0.0f;
	float transition_t = 1.0f;
	glm::vec4 s6_bounds{};
	glm::vec3 s7_move{};
	glm::vec3 s7_rotate{};
	glm::vec2 s7_scale{1.0f};
	uint32_t s7_fog = 0;
	glm::vec2 uv[4] = {};
	glm::vec2 s1_center{};
	glm::vec2 s1_topleft{};
	glm::vec2 xy[4] = {};
	uint32_t raw_color[4] = {};
	uint32_t color[4] = {};
};

using LayerVisitor = std::function<void(const LayerVisit&)>;

struct SpriteInstance {
	SpriteInstance() = default;
	SpriteInstance(const SpriteData* data, uint32_t track, uint32_t variant)
		: data(data)
		, variant_flags(variant) {
		playback.track_idx = track;
	}

	const SpriteData* data = nullptr;
	uint32_t variant_flags = 0;
	SpritePlaybackOptions options = sprite_playback_options();

	uint32_t current_sequence = 0; // Mone: current sequence drawn even when inactive
	bool background = false;
	bool character = false;
	bool mone = false;
	bool flip_v = false;

	SpritePlaybackState playback;

	using RunState = SpriteRunState;

	void play(uint32_t track_id);
	void play(const std::string& name);
	void advance(float dt_seconds);
	void next_track();
	void prev_track();

	void build_vertices(
		uint32_t sa_idx,
		std::vector<SpriteVertex>& verts,
		std::vector<uint32_t>& indices,
		const std::map<uint32_t, glm::vec4>* tints = nullptr,
		bool apply_fog = false,
		int blend_filter = -1,
		uint32_t layer_mask = 0xFFFFFFFFu) const;

	void visit_layers(
		uint32_t sa_idx,
		const std::map<uint32_t, glm::vec4>* tints,
		bool apply_fog,
		uint32_t layer_mask,
		const LayerVisitor& visitor) const;

	glm::mat4 transform_for_sa(uint32_t sa_idx, bool* out_flipx = nullptr, bool* out_flipy = nullptr) const;
	uint32_t sa_count() const;
	uint32_t frame_counter() const;
	std::vector<RunState> run_states() const;
	glm::vec4 track_bounds() const;
	glm::vec4 content_bounds() const;
	glm::vec4 render_bounds() const;

private:
	template <typename F>
	void for_each_layer(
		uint32_t sa_idx,
		const std::map<uint32_t, glm::vec4>* tints,
		bool apply_fog,
		int blend_filter,
		uint32_t layer_mask,
		F&& visit) const;
};
