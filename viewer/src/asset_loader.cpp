#include "asset_loader.hpp"
#include "screenshot.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <eltolinde.hpp>
#include <glad/glad.h>
#include <map>
#include <stdexcept>
#include <utility>

namespace {

// Decide which s9 tracks of a background MBS are "live".
//
// Landscape backgrounds are a set of independent elements that all animate at
// once (clouds, grass, ...) and should all render. Cutscene backgrounds (e.g.
// the Classpedia class-change) instead store mutually-exclusive states as
// separate tracks (ENKE far-view vs KNKE near-view silhouette, BFR/AFTR,
// IDLE/OPEN/CLOSE); only one should render. Day/night variants also exist,
// with either a "*_yoru" suffix or a "yoru*" prefix.
std::vector<uint32_t> select_background_tracks(const mbs::v77& v77) {
	std::vector<uint32_t> enabled;
	for (uint32_t i = 0; i < v77.s9.size(); ++i) {
		const auto& s9 = v77.s9[i];
		if (s9.disabled || v77.sa[s9.sa_set_id].s8_no == 0)
			continue;
		enabled.push_back(i);
	}

	std::vector<uint32_t> day;
	for (uint32_t i : enabled) {
		const std::string n = v77.s9[i].name;
		if (n.find("_yoru") != std::string::npos || n.rfind("yoru", 0) == 0)
			continue;
		day.push_back(i);
	}
	if (!day.empty())
		enabled = day;

	const bool is_state_machine = std::any_of(enabled.begin(), enabled.end(), [&](uint32_t i) {
		const std::string n = v77.s9[i].name;
		return n.find("ENKE") != std::string::npos || n.find("KNKE") != std::string::npos;
	});
	if (is_state_machine) {
		auto pick = [&](const char* needle) -> int {
			for (uint32_t i : enabled) {
				if (std::string(v77.s9[i].name).find(needle) != std::string::npos)
					return static_cast<int>(i);
			}
			return -1;
		};
		int chosen = pick("ENKE_IDLE");
		if (chosen < 0)
			chosen = pick("ENKE");
		if (chosen < 0)
			chosen = pick("VWR_IDLE");
		if (chosen < 0)
			chosen = pick("IDLE");
		if (chosen >= 0)
			return {static_cast<uint32_t>(chosen)};
	}
	return enabled;
}

} // namespace

AssetLoader::AssetLoader(std::filesystem::path cpk_path)
	: _cpkt(TopLevelCpk(cpk_path).getTableOfContents()) {
	for (const auto& [name, _] : Characters) {
		_class_names.push_back(name);
	}
}

std::unique_ptr<SpriteLayer> AssetLoader::load_layer(
	const Job& job,
	uint32_t trackid,
	const std::string& class_name,
	const std::string& variant_name) const {

	std::vector<char> mbs_buf, ftx_buf;
	if (auto entry = _cpkt.find_file(job.mbs.dir, job.mbs.path); entry == _cpkt.end()) {
		throw std::runtime_error("MBS was not found: " + job.mbs.dir + "/" + job.mbs.path);
	} else {
		_cpkt.extract(*entry, mbs_buf);
	}
	if (auto entry = _cpkt.find_file(job.ftx.dir, job.ftx.path); entry == _cpkt.end()) {
		throw std::runtime_error("FTX was not found: " + job.ftx.dir + "/" + job.ftx.path);
	} else {
		_cpkt.extract(*entry, ftx_buf);
	}

	auto ftx_entries = FTX::parse(ftx_buf);
	for (auto& t : ftx_entries) {
		FTX::decompress(t);
		FTX::deswizzle(t);
	}

	auto layer = std::make_unique<SpriteLayer>();
	layer->name = class_name + ":" + variant_name;
	layer->class_name = class_name;
	layer->variant_name = variant_name;
	layer->default_flags = 0xFFFFFFFF;
	layer->data = SpriteData::load(
		std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(mbs_buf.data()), mbs_buf.size()),
		std::move(ftx_entries));
	for (const auto& s4 : layer->data.v77.s4)
		layer->layer_tints.try_emplace(s4.attributes, 1.0f, 1.0f, 1.0f, 1.0f);
	layer->renderer.upload_textures(layer->data);
	layer->instance = SpriteInstance{&layer->data, trackid, layer->default_flags};
	layer->instance.play(trackid);
	return layer;
}

BackgroundScene AssetLoader::load_background(const Job& job) const {
	std::vector<char> mbs_buf, ftx_buf;
	if (auto entry = _cpkt.find_file(job.mbs.dir, job.mbs.path); entry == _cpkt.end()) {
		throw std::runtime_error("BG MBS was not found: " + job.mbs.dir + "/" + job.mbs.path);
	} else {
		_cpkt.extract(*entry, mbs_buf);
	}
	if (auto entry = _cpkt.find_file(job.ftx.dir, job.ftx.path); entry == _cpkt.end()) {
		throw std::runtime_error("BG FTX was not found: " + job.ftx.dir + "/" + job.ftx.path);
	} else {
		_cpkt.extract(*entry, ftx_buf);
	}

	auto ftx_entries = FTX::parse(ftx_buf);
	for (auto& t : ftx_entries) {
		FTX::decompress(t);
		FTX::deswizzle(t);
	}

	BackgroundScene bg;
	bg.data = SpriteData::load(
		std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(mbs_buf.data()), mbs_buf.size()),
		std::move(ftx_entries));
	bg.renderer.upload_textures(bg.data);

	if (std::getenv("FTX_DUMP")) {
		for (size_t k = 0; k < bg.data.textures.size(); ++k) {
			const auto& t = bg.data.textures[k];
			char path[160];
			std::snprintf(path, sizeof(path), "/tmp/opencode/ftx_%02zu.png", k);
			screenshot::write(path, t.width, t.height, t.rgba);
		}
	}

	if (std::getenv("FTX_GPU_DUMP")) {
		glBindTexture(GL_TEXTURE_2D_ARRAY, bg.renderer.texture_array());
		GLint w = 0, h = 0, d = 0;
		glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_WIDTH, &w);
		glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_HEIGHT, &h);
		glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_DEPTH, &d);
		fprintf(stderr, "[gpu tex] %dx%dx%d\n", w, h, d);
		std::vector<uint8_t> pix(static_cast<size_t>(w) * h * d * 4);
		glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix.data());
		screenshot::write("/tmp/opencode/gpu_tex.png", w, h * d, pix);
	}

	const auto& v77 = bg.data.v77;
	static const bool dump = std::getenv("BG_DUMP") != nullptr;
	static const int layers_idx = std::getenv("BG_LAYERS") ? std::atoi(std::getenv("BG_LAYERS")) : -1;
	if (dump) {
		fprintf(stderr, "=== all s9 tracks (%zu) ===\n", v77.s9.size());
		for (uint32_t i = 0; i < v77.s9.size(); ++i) {
			const auto& s9 = v77.s9[i];
			fprintf(
				stderr,
				"[s9 %2u] %-26s disabled=%u sa_set_id=%u sa_set_no=%u\n",
				i,
				s9.name,
				s9.disabled,
				s9.sa_set_id,
				s9.sa_set_no);
		}
		std::map<std::pair<uint32_t, uint32_t>, int> hist;
		for (const auto& s4 : v77.s4)
			hist[{s4.flags, s4.blend_id}]++;
		for (const auto& [k, v] : hist) {
			fprintf(stderr, "s4 flags=0x%02X blend=%u count=%d\n", k.first, k.second, v);
		}
	}
	for (uint32_t i : select_background_tracks(v77)) {
		const auto& s9 = v77.s9[i];
		const auto& sa = v77.sa[s9.sa_set_id];
		const auto& s8 = v77.s8[sa.s8_id + sa.s8_st];
		const auto& s7 = v77.s7[s8.s7_id];
		bool is_far = ((s7.fog >> 24) & 0xFF) == 0xFF;

		if (dump)
			fprintf(
				stderr,
				"[bg %2u] %-16s far=%d s9=(%.0f,%.0f,%.0f,%.0f) move=(%.1f,%.1f,%.1f) scale=(%.2f,%.2f) "
				"rot=(%.2f,%.2f,%.2f) fog=%08X\n",
				i,
				s9.name,
				is_far,
				s9.left,
				s9.top,
				s9.right,
				s9.bottom,
				s7.move.x,
				s7.move.y,
				s7.move.z,
				s7.scale.x,
				s7.scale.y,
				s7.rotate.x,
				s7.rotate.y,
				s7.rotate.z,
				s7.fog);

		auto& el = bg.elements.emplace_back(i, is_far);
		el.instance = SpriteInstance{&bg.data, i, 0xFFFFFFFF};
		el.instance.play(i);

		if (dump) {
			const glm::vec4 cb = el.instance.content_bounds();
			fprintf(stderr, "        content=(%.0f,%.0f,%.0f,%.0f)\n", cb.x, cb.y, cb.z, cb.w);

			const auto& tr = bg.data.tracks[i];
			for (size_t r = 0; r < tr.runs.size(); ++r) {
				const auto& run = tr.runs[r];
				if (run.s8_count == 0)
					continue;
				const auto& s8b = bg.data.v77.s8[run.s8_start + el.instance.offsets[r]];
				const auto& s7b = bg.data.v77.s7[s8b.s7_id];
				fprintf(
					stderr,
					"          run %zu/%zu s8_st=%u count=%u s6=%u s7.move=(%.1f,%.1f,%.1f) scale=(%.2f,%.2f) "
					"fog=%08X\n",
					r,
					tr.runs.size(),
					run.s8_st,
					run.s8_count,
					s8b.s6_id,
					s7b.move.x,
					s7b.move.y,
					s7b.move.z,
					s7b.scale.x,
					s7b.scale.y,
					s7b.fog);

				if (static_cast<int>(i) == layers_idx) {
					const auto& ck = bg.data.keyframes[s8b.s6_id];
					for (size_t li = 0; li < ck.layers.size(); ++li) {
						const auto& L = ck.layers[li];
						fprintf(
							stderr,
							"              layer %zu tex=%u blend=%u col0=%08X uv0=(%.1f,%.1f) uv2=(%.1f,%.1f)\n",
							li,
							L.tex_id,
							L.blend,
							L.color[0],
							L.uv[0].x,
							L.uv[0].y,
							L.uv[2].x,
							L.uv[2].y);
					}
				}
			}

			if (static_cast<int>(i) == layers_idx) {
				const auto& ck = bg.data.keyframes[s8.s6_id];
				for (size_t li = 0; li < ck.layers.size(); ++li) {
					const auto& L = ck.layers[li];
					fprintf(
						stderr,
						"            layer %zu tex=%u blend=%u attr=%08X col0=%08X uv0=(%.1f,%.1f) uv2=(%.1f,%.1f) xy0=(%.1f,%.1f) xy2=(%.1f,%.1f)\n",
						li,
						L.tex_id,
						L.blend,
						L.attributes,
						L.color[0],
						L.uv[0].x,
						L.uv[0].y,
						L.uv[2].x,
						L.uv[2].y,
						L.xy[0].x,
						L.xy[0].y,
						L.xy[2].x,
						L.xy[2].y);
				}
			}
		}
	}

	return bg;
}
