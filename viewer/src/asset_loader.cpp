#include "asset_loader.hpp"

#include <eltolinde.hpp>
#include <stdexcept>

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

	const auto& v77 = bg.data.v77;
	for (uint32_t i = 0; i < v77.s9.size(); ++i) {
		const auto& s9 = v77.s9[i];
		if (s9.disabled)
			continue;

		const auto& sa = v77.sa[s9.sa_set_id];
		if (sa.s8_no == 0)
			continue;
		const auto& s8 = v77.s8[sa.s8_id + sa.s8_st];
		const auto& s7 = v77.s7[s8.s7_id];
		bool is_far = ((s7.fog >> 24) & 0xFF) == 0xFF;

		auto& el = bg.elements.emplace_back(i, is_far);
		el.instance = SpriteInstance{&bg.data, i, 0xFFFFFFFF};
		el.instance.play(i);
	}

	return bg;
}
