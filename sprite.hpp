#pragma once

#include "src/impl/mbs/sections.hpp"
#include <array>
#include <cstdint>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sprite {

//=============================================================================
// Configuration
//=============================================================================

struct Config {
    uint32_t max_layers = 64;          // Max layers per keyframe
    uint32_t max_tracks = 512;         // Max animation tracks
    float default_fps = 60.0f;         // Default playback speed
    bool enable_interpolation = true;  // Enable frame interpolation
};

//=============================================================================
// GPU Resource Handles (API-agnostic)
//=============================================================================

struct TextureHandle {
    uint32_t id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t layers = 0;
    bool is_array = true;
};

struct BufferHandle {
    uint32_t id = 0;
    size_t size = 0;
    void* mapped = nullptr;
};

struct DescriptorSet {
    void* handle = nullptr;  // VkDescriptorSet or GL-like binding
};

//=============================================================================
// Vertex Data for GPU
//=============================================================================

#pragma pack(push, 1)
struct Vertex {
    uint16_t texid;      // Texture array index
    glm::vec2 uv;        // UV coordinates
    glm::vec3 xyz;       // Position (includes depth for layering)
    uint32_t color;      // RGBA fog/tint color
};
#pragma pack(pop)

static_assert(sizeof(Vertex) == 24);

//=============================================================================
// Animation State
//=============================================================================

enum class PlayState {
    Stopped,
    Playing,
    Paused,
    Transitioning,
};

struct FrameState {
    uint32_t s8_index = 0;       // Current section_8 frame index
    uint32_t s6_index = 0;       // Resolved keyframe index
    uint32_t s7_index = 0;       // Resolved transform index
    uint32_t tick_remaining = 0; // Ticks until next frame
    uint32_t tick_total = 0;     // Total ticks for this frame
};

struct TrackState {
    uint32_t track_id = 0;       // Section_9 track index
    uint32_t sa_index = 0;       // Current section_a sequence
    uint32_t sa_offset = 0;      // Offset within sequence
    FrameState frame;
    PlayState state = PlayState::Stopped;
    float blend_weight = 1.0f;   // For transitions
};

//=============================================================================
// Transition Blend State
//=============================================================================

struct BlendTrack {
    TrackState src;
    TrackState dst;
    float blend_progress = 0.0f; // 0.0 = src only, 1.0 = dst only
    float blend_duration = 0.0f; // Seconds for full blend
};

//=============================================================================
// Preprocessed Layer Data (GPU-friendly)
//=============================================================================

struct LayerData {
    uint16_t tex_id;
    uint16_t pad0;
    std::array<glm::vec2, 4> uv;     // Source UVs (pre-scaled)
    std::array<glm::vec2, 4> vertex; // Destination vertices
    std::array<uint32_t, 4> color;   // Fog colors
    float depth = 0.0f;
    uint32_t attributes = 0;
    uint32_t flags = 0;
};

struct KeyframeData {
    std::vector<LayerData> layers;
    glm::vec4 bounds; // left, top, right, bottom
    uint32_t layer_count = 0;
};

//=============================================================================
// Transform Data (for GPU uniform)
//=============================================================================

struct TransformUniform {
    glm::mat4 matrix;      // Combined model matrix
    glm::vec4 flip;        // flip.x = flipx, flip.y = flipy
    glm::vec4 bounds;      // Keyframe bounding box
    uint32_t layer_count;
    float depth_rate;
    uint32_t pad0;
    uint32_t pad1;
};

static_assert(sizeof(TransformUniform) == 80);

//=============================================================================
// Cached Animation Data (CPU-side, preprocessed)
//=============================================================================

struct CachedFrame {
    uint32_t s6_index;
    uint32_t s7_index;
    uint32_t tick_duration;
    uint32_t flags;
    std::optional<KeyframeData> keyframe; // Lazily populated
};

struct CachedSequence {
    uint32_t s8_start;
    uint32_t s8_count;
    uint32_t total_ticks;
    std::vector<CachedFrame> frames;
};

struct CachedTrack {
    std::string name;
    uint32_t s9_index;
    uint32_t sa_start;
    uint32_t sa_count;
    uint32_t sa_set_main;
    uint32_t sa_sb_set_id;
    uint32_t sa_sb_set_no;
    std::vector<CachedSequence> sequences;
    bool enabled = true;
};

//=============================================================================
// Main Sprite Class
//=============================================================================

class Sprite {
public:
    Sprite() = default;
    ~Sprite();

    // Non-copyable, movable
    Sprite(const Sprite&) = delete;
    Sprite& operator=(const Sprite&) = delete;
    Sprite(Sprite&&) noexcept;
    Sprite& operator=(Sprite&&) noexcept;

    //=========================================================================
    // Initialization
    //=========================================================================

    /// Load v77 data from file or memory
    bool load(const std::string& path);
    bool loadFromMemory(const uint8_t* data, size_t size);

    /// Initialize with texture array (must be called after load)
    bool setTextureArray(TextureHandle tex);

    /// Preprocess all tracks for efficient playback
    void preprocess(const Config& config = Config{});

    //=========================================================================
    // Animation Control
    //=========================================================================

    /// Play a track by name or index
    bool play(const std::string& track_name, float blend_time = 0.0f);
    bool play(uint32_t track_id, float blend_time = 0.0f);

    /// Stop current animation
    void stop();

    /// Pause/resume playback
    void pause(bool paused);

    /// Set playback speed multiplier
    void setSpeed(float multiplier);

    /// Seek to specific time within current track
    void seek(float seconds);

    //=========================================================================
    // Update & Render
    //=========================================================================

    /// Update animation state (call once per frame)
    void update(float delta_seconds);

    /// Get vertex data for rendering (OpenGL path)
    const std::vector<Vertex>& getVertices() const { return _vertices; }
    const std::vector<uint16_t>& getIndices() const { return _indices; }

    /// Get transform uniform for shader
    const TransformUniform& getTransform() const { return _transform; }

    /// Build GPU buffers (OpenGL)
    void buildGLBuffers(BufferHandle& vertex_buf, BufferHandle& index_buf);

    /// Build GPU buffers (Vulkan path - returns staging data)
    void getStagingData(
        const void*& vertices, size_t& vertex_size,
        const void*& indices, size_t& index_size);

    //=========================================================================
    // Query Methods
    //=========================================================================

    /// Get current track name
    const std::string& getCurrentTrack() const;

    /// Get current playback state
    PlayState getPlayState() const { return _play_state; }

    /// Get track count
    uint32_t getTrackCount() const { return static_cast<uint32_t>(_tracks.size()); }

    /// Get track by name (returns npos if not found)
    uint32_t findTrack(const std::string& name) const;

    /// Check if track is enabled
    bool isTrackEnabled(uint32_t track_id) const;

    /// Enable/disable track
    void setTrackEnabled(uint32_t track_id, bool enabled);

    //=========================================================================
    // Raw Data Access (for debugging/inspection)
    //=========================================================================

    const mbs::v77& getRawData() const { return _raw; }
    const std::vector<CachedTrack>& getCachedTracks() const { return _tracks; }

private:
    //=========================================================================
    // Internal Methods
    //=========================================================================

    void preprocessTrack(uint32_t track_id, const Config& config);
    void preprocessKeyframe(uint32_t s6_id, KeyframeData& out);
    void updateFrame(TrackState& track, float delta);
    void buildVertices(const TrackState& track);
    void blendTransforms(const TrackState& src, const TrackState& dst, float t);

    glm::mat4 computeMatrix(
        const mbs::section_7& s7, bool flipx, bool flipy) const;

    //=========================================================================
    // Data Members
    //=========================================================================

    // Raw section data from file
    mbs::v77 _raw;

    // Preprocessed track data
    std::vector<CachedTrack> _tracks;

    // Texture handle (GPU resource)
    TextureHandle _texture;

    // Current animation state
    TrackState _current_track;
    std::optional<BlendTrack> _blend;

    // Playback settings
    PlayState _play_state = PlayState::Stopped;
    float _speed_multiplier = 1.0f;
    float _current_time = 0.0f;

    // Vertex/index buffers (CPU mirror)
    std::vector<Vertex> _vertices;
    std::vector<uint16_t> _indices;

    // Transform uniform for current frame
    TransformUniform _transform;

    // Name lookup cache
    mutable std::unordered_map<std::string, uint32_t> _track_name_cache;
};

//=============================================================================
// Vulkan-Specific Structures
//=============================================================================

#ifdef SPRITE_VULKAN_SUPPORT

namespace vk {

struct PipelineLayout {
    void* handle = nullptr;
};

struct DescriptorSetLayout {
    void* handle = nullptr;
};

struct ShaderModule {
    void* handle = nullptr;
    enum class Stage { Vertex, Fragment };
    Stage stage;
};

struct RenderPass {
    void* handle = nullptr;
};

struct Framebuffer {
    void* handle = nullptr;
};

/// Vulkan-specific sprite renderer
class SpriteRenderer {
public:
    bool initialize(
        void* device, void* physical_device,
        uint32_t queue_family);

    void destroy(void* device);

    /// Create pipeline for sprite rendering
    bool createPipeline(
        void* device,
        const ShaderModule& vert_shader,
        const ShaderModule& frag_shader,
        const DescriptorSetLayout& desc_layout,
        const RenderPass& render_pass);

    /// Record sprite draw commands to command buffer
    void recordDraw(
        void* command_buffer,
        const Sprite& sprite,
        const DescriptorSet& desc_set,
        uint32_t width,
        uint32_t height);

private:
    void* _device = nullptr;
    void* _pipeline = nullptr;
    void* _pipeline_layout = nullptr;
    PipelineLayout _layout;
};

} // namespace vk

#endif // SPRITE_VULKAN_SUPPORT

//=============================================================================
// OpenGL-Specific Structures
//=============================================================================

#ifdef SPRITE_OPENGL_SUPPORT

namespace gl {

/// OpenGL-specific sprite renderer
class SpriteRenderer {
public:
    bool initialize();
    void destroy();

    /// Create shader program for sprite rendering
    GLuint createProgram(const char* vert_src, const char* frag_src);

    /// Render sprite with given shader program
    void draw(
        GLuint program,
        const Sprite& sprite,
        const glm::mat4& mvp,
        GLuint texture_unit = 0);

private:
    GLuint _vao = 0;
};

} // namespace gl

#endif // SPRITE_OPENGL_SUPPORT

//=============================================================================
// Utility Functions
//=============================================================================

/// Interpolate between two values with easing
float easeInOut(float t);

/// Interpolate between two transforms
TransformUniform lerpTransform(
    const TransformUniform& a,
    const TransformUniform& b,
    float t);

/// Convert section_8 flags to human-readable string
std::string flagsToString(uint32_t flags);

} // namespace sprite
