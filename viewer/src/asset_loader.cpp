#include "asset_loader.hpp"
#include "background_tracks.hpp"
#include "screenshot.hpp"
#include "trace.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <eltolinde.hpp>
#include <glad/glad.h>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

std::string lower(std::string s) {
	for (char& c : s)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

} // namespace

AssetLoader::AssetLoader(std::filesystem::path cpk_path)
	: _cpkt(TopLevelCpk(cpk_path).getTableOfContents()) {
	for (const auto& [name, _] : Characters) {
		_class_names.push_back(name);
	}
	for (const auto& [d, f] : _cpkt.rows<"DirName", "FileName">())
		_asset_index[lower(d + "/" + f)] = {d, f};
}

std::optional<Job::dir_and_path> AssetLoader::resolve(std::string_view path) const {
	const Job::dir_and_path sp = split_path(path);
	if (_cpkt.find_file(sp.dir, sp.path) != _cpkt.end())
		return sp;
	auto it = _asset_index.find(lower(std::string(path)));
	if (it != _asset_index.end())
		return it->second;
	return std::nullopt;
}

std::optional<Job::dir_and_path> AssetLoader::resolve_ftx(std::string_view ftx, std::string_view mbs) const {
	std::vector<std::string> cands = {std::string(ftx)};
	const Job::dir_and_path sp = split_path(ftx);
	std::string n = sp.path;
	auto ext = n.rfind('.');
	if (ext != std::string::npos) {
		size_t e = ext;
		while (e > 0 && std::isdigit(static_cast<unsigned char>(n[e - 1])))
			--e;
		if (e < ext && ext - e == 1)
			cands.push_back(
				sp.dir.empty() ? (n.substr(0, e) + "0" + n.substr(e))
							   : (sp.dir + "/" + n.substr(0, e) + "0" + n.substr(e)));
	}
	std::string stem(mbs);
	if (stem.size() >= 4 && stem.compare(stem.size() - 4, 4, ".mbs") == 0)
		stem.resize(stem.size() - 4);
	for (const char* suf : {".ftx", "0.ftx", "00.ftx"})
		cands.push_back(stem + suf);
	for (const auto& c : cands)
		if (auto r = resolve(c))
			return r;
	return std::nullopt;
}

std::unique_ptr<SpriteLayer> AssetLoader::load_layer(
	const Job& job,
	uint32_t trackid,
	const std::string& class_name,
	const std::string& variant_name) const {
	return make_layer(prepare(job), trackid, class_name, variant_name);
}

AssetLoader::PreparedLayer AssetLoader::prepare(const Job& job) const {
	PreparedLayer p;
	std::vector<char> ftx_raw;
	{
		viewer::trace::Scoped sc("prepare extract " + job.mbs.path);
		if (auto entry = _cpkt.find_file(job.mbs.dir, job.mbs.path); entry == _cpkt.end()) {
			throw std::runtime_error("MBS was not found: " + job.mbs.dir + "/" + job.mbs.path);
		} else {
			_cpkt.extract(*entry, p.mbs);
		}
		if (auto entry = _cpkt.find_file(job.ftx.dir, job.ftx.path); entry == _cpkt.end()) {
			throw std::runtime_error("FTX was not found: " + job.ftx.dir + "/" + job.ftx.path);
		} else {
			_cpkt.extract(*entry, ftx_raw);
		}
	}
	{
		viewer::trace::Scoped sc("prepare ftx parse " + job.ftx.path);
		p.ftx = FTX::parse(ftx_raw);
	}
	return p;
}

SpriteAssetPtr AssetLoader::make_asset(
	PreparedLayer&& p,
	const std::string& mbs,
	const std::string& ftx,
	const std::string& class_name,
	const std::string& variant_name) const {
	(void)variant_name;
	auto asset = std::make_shared<SpriteAsset>();
	asset->mbs = mbs;
	asset->ftx = ftx;
	asset->is_character = mbs.rfind("Chara", 0) == 0;
	asset->is_background = mbs.rfind("BattleBG/", 0) == 0 || mbs.rfind("FieldBG/", 0) == 0;
	{
		viewer::trace::Scoped sc("make mbs parse " + class_name);
		asset->data = SpriteData::load(
			std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(p.mbs.data()), p.mbs.size()), std::move(p.ftx));
	}
	{
		viewer::trace::Scoped sc("make gpu upload " + class_name);
		asset->renderer.upload_textures(asset->data);
	}
	return asset;
}

std::unique_ptr<SpriteLayer> AssetLoader::make_layer(
	PreparedLayer&& p,
	uint32_t trackid,
	const std::string& class_name,
	const std::string& variant_name) const {
	auto layer = std::make_unique<SpriteLayer>();
	layer->name = class_name + ":" + variant_name;
	layer->class_name = class_name;
	layer->variant_name = variant_name;
	layer->default_flags = 0xFFFFFFFF;
	{
		viewer::trace::Scoped sc("make mbs parse " + class_name);
		layer->data = SpriteData::load(
			std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(p.mbs.data()), p.mbs.size()), std::move(p.ftx));
	}
	for (const auto& s4 : layer->data.v77.s4)
		layer->layer_tints.try_emplace(s4.attributes, 1.0f, 1.0f, 1.0f, 1.0f);
	{
		viewer::trace::Scoped sc("make gpu upload " + class_name);
		layer->renderer.upload_textures(layer->data);
	}
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

	BackgroundScene bg;
	bg.data = SpriteData::load(
		std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(mbs_buf.data()), mbs_buf.size()),
		std::move(ftx_entries));
	bg.renderer.upload_textures(bg.data);

	if (std::getenv("FTX_GPU_DUMP")) {
		glBindTexture(GL_TEXTURE_2D_ARRAY, bg.renderer.texture_array());
		GLint w = 0, h = 0, d = 0;
		glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_WIDTH, &w);
		glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_HEIGHT, &h);
		glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_DEPTH, &d);
		spdlog::info("gpu tex: {}x{}x{}", w, h, d);
		std::vector<uint8_t> pix(static_cast<size_t>(w) * h * d * 4);
		glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix.data());
		screenshot::write("/tmp/opencode/gpu_tex.png", w, h * d, pix);
	}

	const auto& v77 = bg.data.v77;
	static const bool dump = std::getenv("BG_DUMP") != nullptr;
	static const int layers_idx = std::getenv("BG_LAYERS") ? std::atoi(std::getenv("BG_LAYERS")) : -1;
	if (dump) {
		spdlog::info("all s9 tracks ({})", v77.s9.size());
		for (uint32_t i = 0; i < v77.s9.size(); ++i) {
			const auto& s9 = v77.s9[i];
			spdlog::info(
				"[s9 {:>2}] {:<26} disabled={} sa_set_id={} sa_set_no={}",
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
			spdlog::info("s4 flags=0x{:02X} blend={} count={}", k.first, k.second, v);
		}
	}
	for (uint32_t i : select_background_tracks(v77)) {
		const auto& s9 = v77.s9[i];
		const auto& sa = v77.sa[s9.sa_set_id];
		const auto& s8 = v77.s8[sa.s8_id + sa.s8_st];
		const auto& s7 = v77.s7[s8.s7_id];
		bool is_far = ((s7.fog >> 24) & 0xFF) == 0xFF;

		if (dump)
			spdlog::info(
				"[bg {:>2}] {:<16} far={} s9=({:.0f},{:.0f},{:.0f},{:.0f}) move=({:.1f},{:.1f},{:.1f}) "
				"scale=({:.2f},{:.2f}) rot=({:.2f},{:.2f},{:.2f}) fog=0x{:08X}",
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
			spdlog::info("        content=({:.0f},{:.0f},{:.0f},{:.0f})", cb.x, cb.y, cb.z, cb.w);

			const auto& tr = bg.data.tracks[i];
			for (size_t r = 0; r < tr.runs.size(); ++r) {
				const auto& run = tr.runs[r];
				if (run.s8_count == 0)
					continue;
				const auto& s8b = bg.data.v77.s8[run.s8_start + el.instance.offsets[r]];
				const auto& s7b = bg.data.v77.s7[s8b.s7_id];
				spdlog::info(
					"          run {}/{} s8_st={} count={} s6={} s7.move=({:.1f},{:.1f},{:.1f}) "
					"scale=({:.2f},{:.2f}) fog=0x{:08X}",
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
						spdlog::info(
							"              layer {} tex={} blend={} col0=0x{:08X} uv0=({:.1f},{:.1f}) "
							"uv2=({:.1f},{:.1f}) xy0=({:.0f},{:.0f}) xy2=({:.0f},{:.0f})",
							li,
							L.tex_id,
							L.blend,
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

			if (static_cast<int>(i) == layers_idx) {
				const auto& ck = bg.data.keyframes[s8.s6_id];
				for (size_t li = 0; li < ck.layers.size(); ++li) {
					const auto& L = ck.layers[li];
					spdlog::info(
						"            layer {} tex={} blend={} attr=0x{:08X} col0=0x{:08X} uv0=({:.1f},{:.1f}) "
						"uv2=({:.1f},{:.1f}) xy0=({:.1f},{:.1f}) xy2=({:.1f},{:.1f})",
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
