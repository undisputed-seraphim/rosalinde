#include <glad/glad.h>

#include "sprite.hpp"

static_assert(std::is_same_v<GLuint, uint32_t>);

Sprite::Sprite(Quad q)
	: _q(std::move(q)) {}

void Sprite::AddTexture(Texture2D&& texture) { _textures.emplace_back(std::move(texture)); }

void Sprite::Animate(int current_active_anim) const {
	static int timestep = 0;

	const auto& a = _q.animations().at(current_active_anim);
	const auto& tl = a.timelines.at(timestep++);
	timestep %= a.timelines.size();

	if (tl.attach.objt == Quad::ObjectType::KEYFRAME) {
		const auto& kf = _q.keyframes().at(tl.attach.id);
		for (const auto& layer : kf.layers) {
			const auto& texture = _textures.at(layer.texid);
			const auto& shader = GetKeyframeShader();
		}
	}
}