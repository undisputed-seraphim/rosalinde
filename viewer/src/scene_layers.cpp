#include "scene_layers.hpp"
#include "trace.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
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
			spdlog::debug("scene: miss {} {}", s.id, s.mbs);
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
					spdlog::warn("scene: skip {}: {}", specs[i].id, ex.what());
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
				spdlog::warn("scene: skip {}: {}", specs[i].id, ex.what());
				continue;
			}
			if (specs[i].track_index >= 0 && specs[i].track_index < static_cast<int>(layer->data.tracks.size()))
				layer->instance.play(static_cast<uint32_t>(specs[i].track_index));
			else if (!specs[i].track.empty())
				layer->instance.play(specs[i].track);
			layer->position = specs[i].pos;
			layer->scale = specs[i].scale;
			layer->is_character = specs[i].mbs.rfind("Chara", 0) == 0;

			// Backgrounds (non-character assets) are a set of independent tracks
			// that all animate at once, each with its own clock. Instantiate them
			// all instead of only the script-selected track.
			const bool landscape = specs[i].mbs.rfind("BattleBG/", 0) == 0 || specs[i].mbs.rfind("FieldBG/", 0) == 0;
			if (landscape) {
				const auto& v77 = layer->data.v77;
				for (uint32_t t = 0; t < v77.s9.size(); ++t) {
					const auto& s9 = v77.s9[t];
					if (s9.disabled || s9.sa_set_no == 0)
						continue;
					if (s9.sa_set_id >= v77.sa.size() || v77.sa[s9.sa_set_id].s8_no == 0)
						continue;
					SpriteInstance inst{&layer->data, t, 0xFFFFFFFF};
					inst.play(t);
					layer->instances.push_back(std::move(inst));
				}
			}
			layers[i] = std::move(layer);
			spdlog::debug("scene: load {} {} ({} tracks)", specs[i].id, specs[i].mbs, layers[i]->data.tracks.size());
		}
	}
	return layers;
}
