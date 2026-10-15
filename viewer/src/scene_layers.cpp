#include "scene_layers.hpp"
#include "trace.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

std::vector<std::unique_ptr<SpriteLayer>>
build_layers(const AssetLoader& loader, const std::vector<LayerSpec>& specs, int limit) {
	std::vector<std::unique_ptr<SpriteLayer>> layers(specs.size());

	// Phase 0 (main thread): resolve every spec to a Job. Cheap.
	std::vector<Job> jobs(specs.size());
	std::vector<char> ready(specs.size(), 0);
	int pending = 0;
	for (size_t i = 0; i < specs.size(); ++i) {
		const LayerSpec& s = specs[i];
		if (limit >= 0 && pending >= limit)
			break;
		if (!s.visible || s.mbs.empty() || s.ftx.empty())
			continue;
		auto m = loader.resolve(s.mbs);
		auto fx = loader.resolve_ftx(s.ftx, s.mbs);
		if (!m || !fx) {
			std::fprintf(stderr, "[scene] skip %s (not in archive)\n", s.mbs.c_str());
			continue;
		}
		jobs[i].mbs = *m;
		jobs[i].ftx = *fx;
		ready[i] = 1;
		++pending;
	}

	// Phase 1 (threads): extract + FTX decode (CPU-heavy, independent).
	std::vector<AssetLoader::PreparedLayer> prepared(specs.size());
	{
		viewer::trace::Scoped sc("build_layers prepare");
		const unsigned n = std::max(1u, std::thread::hardware_concurrency());
		std::atomic<size_t> next{0};
		auto worker = [&] {
			for (size_t i = next++; i < specs.size(); i = next++) {
				if (!ready[i])
					continue;
				try {
					prepared[i] = loader.prepare(jobs[i]);
				} catch (const std::exception& ex) {
					std::fprintf(stderr, "[scene] skip %s: %s\n", specs[i].id.c_str(), ex.what());
				}
			}
		};
		std::vector<std::thread> pool;
		pool.reserve(n);
		for (unsigned t = 0; t < n; ++t)
			pool.emplace_back(worker);
		for (auto& th : pool)
			th.join();
	}

	// Phase 2 (main/GL thread): parse + upload + place.
	{
		viewer::trace::Scoped sc("build_layers make+upload");
		for (size_t i = 0; i < specs.size(); ++i) {
			if (!ready[i] || prepared[i].mbs.empty())
				continue;
			std::unique_ptr<SpriteLayer> layer;
			try {
				layer = loader.make_layer(std::move(prepared[i]), 0, specs[i].id, specs[i].kind);
			} catch (const std::exception& ex) {
				std::fprintf(stderr, "[scene] skip %s: %s\n", specs[i].id.c_str(), ex.what());
				continue;
			}
			if (specs[i].track_index >= 0 && specs[i].track_index < static_cast<int>(layer->data.tracks.size()))
				layer->instance.play(static_cast<uint32_t>(specs[i].track_index));
			layer->position = specs[i].pos;
			layers[i] = std::move(layer);
		}
	}
	return layers;
}
