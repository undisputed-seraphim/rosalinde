#pragma once

#include <array>
#include <glm/glm.hpp>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

class Quad {
public:
	// s0, s1, s2
	struct Layer {
		glm::vec2 src[4];
		glm::vec2 dst[4];
		uint32_t fog[4];
		uint8_t colorid;
		uint8_t flags;
		uint8_t blendid;
		uint8_t texid;
		uint32_t attributes;
	};

	// s6
	struct Keyframe {
		std::vector<uint32_t> layers;   // s4
		std::vector<uint16_t> hitboxes; // s5, s3
		glm::vec4 bounds; // left right top bottom
	};

	// s8
	struct Animation {
		struct Timeline {
			int16_t frames;
			glm::mat4 matrix; // s7
			uint32_t color;
			uint32_t keyframe_id; // s6
			int16_t kf_interpolation;
		};

		int32_t loop_id;
		std::vector<Timeline> keyframes;
		glm::vec4 bounds; // left right top bottom
	};

	virtual ~Quad() noexcept = default;

	const std::vector<Keyframe>& keyframes() const { return _keyframes; }
	const std::vector<glm::mat4x2>& hitboxes() const { return _hitboxes; }
	const std::vector<Layer>& layers() const { return _layers; }

	using AnimationSets = std::vector<std::pair<std::string, std::vector<Animation>>>;
	const AnimationSets& animationsets() const noexcept { return _animations; }

protected:
	std::vector<Layer> _layers;
	std::vector<glm::mat4x2> _hitboxes;
	std::vector<Keyframe> _keyframes;
	AnimationSets _animations;

};

class MBS {
public:
	MBS(std::istream&);
	MBS(std::istream&&);
	MBS(const MBS&&) = delete;
	MBS(MBS&&) noexcept;
	~MBS() noexcept;

	const std::string& filename() const noexcept { return _filename; }

	Quad extract() const;

private:
	class data_t;
	std::unique_ptr<data_t> read(std::istream&);

	std::string _filename;
	std::unique_ptr<data_t> _dataptr;
};
