#pragma once

#include <glm/glm.hpp>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

class Quad {
public:
	enum class ObjectType : int8_t {
		NONE = 0,
		KEYFRAME,
		HITBOX,
		SLOT,
		ANIMATION,
		SKELETON,
	};

	struct Attach {
		int32_t id;
		ObjectType objt;
	};

	struct Keyframe {
		struct Layer {
			glm::mat4x2 dst;
			glm::mat4x2 src;
			glm::vec4 fog;
			int16_t blendid;
			int16_t texid;
			// uint32_t attribute; // Always zero here
			// uint8_t color; // Unused?
		};
		int32_t id;
		std::vector<Layer> layers;
		std::vector<int32_t> order;
	};

	struct Hitbox {
		struct Layer {
			glm::mat4x2 hitbox;
		};
		int32_t id;
		std::vector<Layer> layers;
	};

	struct Animation {
		struct Timeline {
			int16_t time;
			glm::mat4 matrix;
			uint32_t color;
			Attach attach;
			bool matrix_mix;
			bool color_mix;
			bool keyframe_mix;
			bool hitbox_mix;
		};

		int32_t id;
		int32_t loop_id;
		std::vector<Timeline> timelines;
	};

	struct Skeleton {
		struct Bone {
			int32_t id;
			Attach attach;
		};
		std::string name;
		std::vector<Bone> bones;
	};

	struct Slot {
		std::vector<Attach> attachments;
	};

	struct Blend {
		enum class Mode : uint8_t {
			ADD,
			SUBTRACT,
			REVERSE_SUBTRACT,
			MIN,
			MAX,
		};
		enum class Factor : uint8_t {
			ZERO,
			ONE,
			SRC_COLOR,
			SRC_ALPHA,
			ONE_MINUS_SRC_COLOR,
			ONE_MINUS_SRC_ALPHA,
			DST_COLOR,
			DST_ALPHA,
			ONE_MINUS_DST_COLOR,
			ONE_MINUS_DST_ALPHA,
			CONSTANT_COLOR,
			CONSTANT_ALPHA,
			ONE_MINUS_CONSTANT_COLOR,
			ONE_MINUS_CONSTANT_ALPHA,
		};
		enum class LogicOp : uint8_t {
			CLEAR,
			SET,
			COPY,
			COPY_INVERT,
			NOOP,
			INVERT,
			AND,
			NAND,
			OR,
			NOR,
			XOR,
			EQUIV,
			AND_REVERSE,
			AND_INVERT,
			OR_REVERSE,
			OR_INVERT,
		};
		int32_t id;
		Mode rgb_mode;
		Factor rgb_src;
		Factor rgb_dst;
		Mode alpha_mode;
		Factor alpha_src;
		Factor alpha_dst;
		uint32_t color;
		LogicOp r, g, b, a;
	};

	struct Link {
		int32_t id;
	};

	virtual ~Quad() noexcept = default;

	const std::vector<Keyframe>& keyframes() const { return _keyframes; }
	const std::vector<Hitbox>& hitboxes() const { return _hitboxes; }
	const std::vector<Animation>& animations() const { return _animations; }
	const std::vector<Skeleton>& skeletons() const { return _skeletons; }
	const std::vector<Slot>& slots() const { return _slots; }
	const std::vector<Blend>& blends() const { return _blends; }
	const std::vector<Link>& links() const { return _links; }

	std::ostream& operator<<(std::ostream&) const;
	friend std::ostream& operator<<(std::ostream&, const Quad&);

protected:
	std::vector<Keyframe> _keyframes;
	std::vector<Hitbox> _hitboxes;
	std::vector<Animation> _animations;
	std::vector<Skeleton> _skeletons;
	std::vector<Slot> _slots;
	std::vector<Blend> _blends;
	std::vector<Link> _links;
};

std::ostream& operator<<(std::ostream&, const Quad&);

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
