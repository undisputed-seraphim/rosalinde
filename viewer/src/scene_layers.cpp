#include "scene_layers.hpp"

#include <cstdio>
#include <stdexcept>

std::vector<std::unique_ptr<SpriteLayer>>
build_layers(const AssetLoader& loader, const std::vector<LayerSpec>& specs, int limit) {
	std::vector<std::unique_ptr<SpriteLayer>> layers(specs.size());
	int loaded = 0;
	for (size_t i = 0; i < specs.size(); ++i) {
		const LayerSpec& s = specs[i];
		if (limit >= 0 && loaded >= limit)
			break;
		if (!s.visible || s.mbs.empty() || s.ftx.empty())
			continue;

		auto m = loader.resolve(s.mbs);
		auto fx = loader.resolve_ftx(s.ftx, s.mbs);
		if (!m || !fx) {
			std::fprintf(stderr, "[scene] skip %s (not in archive)\n", s.mbs.c_str());
			continue;
		}

		Job job;
		job.mbs = *m;
		job.ftx = *fx;

		std::unique_ptr<SpriteLayer> layer;
		try {
			layer = loader.load_layer(job, 0, s.id, s.kind);
		} catch (const std::exception& ex) {
			std::fprintf(stderr, "[scene] skip %s: %s\n", s.id.c_str(), ex.what());
			continue;
		}
		if (s.track_index >= 0 && s.track_index < static_cast<int>(layer->data.tracks.size()))
			layer->instance.play(static_cast<uint32_t>(s.track_index));
		layer->position = s.pos;
		layers[i] = std::move(layer);
		++loaded;
	}
	return layers;
}
