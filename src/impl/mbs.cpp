#include "mbs.hpp"
#include "mbs/sections.hpp"
#include "utils.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

// Faulty struct, do not use
struct v77_1 {
	uint32_t longvals[5];
	uint16_t shortvals[7];
	uint64_t _unknown1[2];
	char name[32];
	uint64_t _unknown2[2];
	uint64_t offsets[12];
};

#pragma pack(push, 1)
struct mbs_header {
	char magic[4];
	uint32_t file_size;
	uint32_t header_size;
	uint32_t _pad0;
	uint16_t _unk0;
	uint16_t _unk1;
	uint16_t version;
	uint16_t _unk2;
	uint32_t _unk3;
	uint32_t _pad1;

	static constexpr std::string_view FMBS = "FMBS";
};
#pragma pack(pop)

struct MBS::data_t : public std::variant<MBS_::v77, std::monostate> {
	~data_t() noexcept {}
};

MBS::MBS(std::istream& is)
	: MBS(std::move(is)) {}

MBS::MBS(std::istream&& is)
	: _filename(2 * 16, char(0))
	, _dataptr(read(is)) {}

MBS::MBS(MBS&&) noexcept = default;

MBS::~MBS() noexcept { _dataptr.reset(); }

Quad MBS::extract() const {
	auto makequad_v77 = [](const MBS_::v77& v77) -> Quad { return v77.to_quad(); };
	auto monostate = [](const std::monostate&) -> Quad { return Quad{}; };
	return std::visit(overloads{makequad_v77, monostate}, *_dataptr);
}

std::unique_ptr<MBS::data_t> MBS::read(std::istream& is) {
	is.seekg(0, std::ios::beg);
	const mbs_header h = read_value<mbs_header>(is);
	if (std::string_view(h.magic, sizeof(h.magic)) != mbs_header::FMBS) {
		std::cout << "Not an FMBS file." << std::endl;
		return std::make_unique<MBS::data_t>(std::monostate{});
	}

	is.seekg(0x80, std::ios::beg);
	is.read(_filename.data(), _filename.size());
	trim_string(_filename);

	switch (h.version) {
	case 0x77: {
		return std::make_unique<data_t>(MBS_::v77::read(is));
		break;
	}
	case 0x66:
	case 0x6b:
	case 0x6d:
	case 0x6e:
	case 0x72:
	case 0x76:
	default: {
		std::cout << "Unsupported FMBS version " << (uint16_t)h.version << std::endl;
	}
	}
	return std::make_unique<MBS::data_t>(std::monostate{});
}

std::ostream& operator<<(std::ostream& os, const Quad::ObjectType o) {
	using ObjectType = Quad::ObjectType;
	switch (o) {
	case ObjectType::KEYFRAME:
		return os << "keyframe";
	case ObjectType::HITBOX:
		return os << "hitbox";
	case ObjectType::SLOT:
		return os << "slot";
	case ObjectType::ANIMATION:
		return os << "animation";
	case ObjectType::SKELETON:
		return os << "skeleton";
	}
	return os << "none";
}

std::ostream& operator<<(std::ostream& os, const Quad::Attach& a) { return os << a.objt << ' ' << a.id; }

std::ostream& Quad::operator<<(std::ostream& os) const {
	static constexpr std::string_view fmt4f = "[{} {} {} {}]\n";
	static constexpr std::string_view fmt8f = "[{} {} {} {} {} {} {} {}]\n";
	os << "Keyframes\n";
	for (const auto& keyframe : _keyframes) {
		os << "\tID: " << keyframe.id << '\n';
		for (const auto& layer : keyframe.layers) {
			// TODO
			//const auto& sq = layer.srcquad;
			//const auto& dq = layer.dstquad;
			//os << "\t\tSrc: " << std::format(fmt8f, sq[0], sq[1], sq[2], sq[3], sq[4], sq[5], sq[6], sq[7]);
			//os << "\t\tDst: " << std::format(fmt8f, dq[0], dq[1], dq[2], dq[3], dq[4], dq[5], dq[6], dq[7]);
			//os << '\n';
		}
		os << "\t\tcount: " << keyframe.layers.size() << "\n\n";
	}
	os << '\n';
	os << "Animations\n";
	for (const auto& animation : _animations) {
		os << "\tID: " << animation.id << '\n';
		os << "\tLoop ID: " << animation.loop_id << '\n';
		for (const auto& timeline : animation.timelines) {
			const auto& m = timeline.matrix;
			os << "\t\tTime: " << timeline.time << '\n';
			os << "\t\tAttach: " << timeline.attach << '\n';
			//os << "\t\tMatrix: " << std::format(fmt4f, m[0], m[1], m[2], m[3]);
			//os << "\t\t        " << std::format(fmt4f, m[4], m[5], m[6], m[7]);
			//os << "\t\t        " << std::format(fmt4f, m[8], m[9], m[10], m[11]);
			//os << "\t\t        " << std::format(fmt4f, m[12], m[13], m[14], m[15]);
		}
	}
	os << '\n';
	os << "Skeletons\n";
	for (const auto& skeleton : _skeletons) {
		os << "\tName: " << skeleton.name << '\n';
		for (const auto& bone : skeleton.bones) {
			os << "\t\tID: " << bone.id << '\n';
			os << "\t\tAttach: " << bone.attach << '\n';
		}
	}
	return os;
}

std::ostream& operator<<(std::ostream& os, const Quad& q) { return (q.operator<<(os)); }
