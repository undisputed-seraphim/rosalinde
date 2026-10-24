#include "asset_loader.hpp"
#include "trace.hpp"

#include <cctype>
#include <cstdlib>
#include <eltolinde.hpp>
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
	: _cpkt(cpk_path) {
	for (const auto& item : _cpkt) {
		std::string d(item.dir);
		std::string f(item.name);
		_asset_index[lower(d + "/" + f)] = {d, f};
	}
}

std::optional<Job::dir_and_path> AssetLoader::resolve(std::string_view path) const {
	const Job::dir_and_path sp = split_path(path);
	if (_cpkt.find(sp.dir, sp.path) != _cpkt.end())
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

AssetLoader::PreparedLayer AssetLoader::prepare(const Job& job) const {
	PreparedLayer p;
	std::vector<char> ftx_raw;
	{
		viewer::trace::Scoped sc("prepare extract " + job.ftx.path);
		auto entry = _cpkt.find(job.ftx.dir, job.ftx.path);
		if (entry == _cpkt.end())
			throw std::runtime_error("FTX was not found: " + job.ftx.dir + "/" + job.ftx.path);
		_cpkt.extract(*entry, ftx_raw);
	}
	{
		viewer::trace::Scoped sc("prepare ftx parse " + job.ftx.path);
		p.ftx = FTX::parse(ftx_raw);
	}
	return p;
}

SpriteAssetPtr AssetLoader::make_asset(
	PreparedLayer&& p,
	mbs::v77 sections,
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
		asset->data = SpriteData::build(std::move(sections), std::move(p.ftx));
	}
	{
		viewer::trace::Scoped sc("make gpu upload " + class_name);
		asset->renderer.upload_textures(asset->data, ftx);
	}
	return asset;
}
