#include <scarlett/asb.hpp>
#include <scarlett/host.hpp>

#include <criware/cpk.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

asb::Scene make_scene(const std::vector<std::pair<std::string, int>>& fns) {
	asb::Scene s;
	for (const auto& [name, creates] : fns) {
		asb::SceneFunction f;
		f.name = name;
		for (int i = 0; i < creates; ++i) {
			asb::SceneEvent e;
			e.call = "CreateSpriteMone";
			f.events.push_back(std::move(e));
		}
		s.functions.push_back(std::move(f));
	}
	return s;
}

// Locate the test CPK: $UNICORN_CPK, else ./Unicorn.CPK or ../Unicorn.CPK.
std::filesystem::path find_cpk() {
	if (const char* e = std::getenv("UNICORN_CPK"); e && *e)
		return e;
	for (const char* p : {"Unicorn.CPK", "../Unicorn.CPK", "../../Unicorn.CPK"})
		if (std::filesystem::exists(p))
			return p;
	return {};
}

std::vector<uint8_t> extract_entry(const std::filesystem::path& cpk, const std::string& entry) {
	auto pos = entry.find_last_of('/');
	std::string dir = pos == std::string::npos ? "" : entry.substr(0, pos);
	std::string name = pos == std::string::npos ? entry : entry.substr(pos + 1);
	CPKTable table = TopLevelCpk(cpk).getTableOfContents();
	std::vector<char> buf;
	auto e = table.find_file(dir, name);
	if (e == table.end())
		return {};
	table.extract(*e, buf);
	return std::vector<uint8_t>(buf.begin(), buf.end());
}

} // namespace

TEST_CASE("call classification") {
	CHECK(asb::is_sprite_create_call("CreateSpriteMone"));
	CHECK(asb::is_sprite_create_call("CreateSpriteCoA"));
	CHECK(asb::is_chara_create_call("DrUiCreateChara"));
	CHECK(asb::is_entity_create_call("DrCreateChara"));
	CHECK_FALSE(asb::is_entity_create_call("DrUiSetPosV"));
	CHECK_FALSE(asb::is_sprite_create_call("Wait"));
}

TEST_CASE("pick_scene_function follows the heuristic") {
	// scene00 with no creates is skipped in favour of MAIN.
	auto s = make_scene({{"init", 0}, {"scene00", 0}, {"MAIN", 1}, {"scene02", 3}});
	REQUIRE(asb::pick_scene_function(s));
	CHECK(asb::pick_scene_function(s)->name == "MAIN");

	// scene00 wins whenever it creates entities.
	auto s2 = make_scene({{"scene00", 2}, {"MAIN", 5}});
	CHECK(asb::pick_scene_function(s2)->name == "scene00");

	// An explicit name is honoured verbatim.
	CHECK(asb::pick_scene_function(s2, "MAIN")->name == "MAIN");

	// Falls back to the busiest function then the first non-empty one.
	auto s3 = make_scene({{"a", 1}, {"b", 4}});
	CHECK(asb::pick_scene_function(s3)->name == "b");
}

TEST_CASE("decode, load and run a real script") {
	const std::filesystem::path cpk = find_cpk();
	if (cpk.empty()) {
		WARN("no Unicorn.CPK found (set UNICORN_CPK); skipping integration test");
		return;
	}

	const std::string entry = "Script/Dr_OW_0_Opening.asb";
	std::vector<uint8_t> bytes = extract_entry(cpk, entry);
	REQUIRE_FALSE(bytes.empty());

	asb::Module mod = asb::parse(bytes);
	CHECK(mod.error.empty());
	CHECK(mod.consumed == mod.total);

	asb::Host host;
	host.register_module(mod);
	host.set_call_log([](const std::string&) {});
	REQUIRE(host.load(bytes));

	asb::Scene scene = asb::extract_scene(mod);
	const asb::SceneFunction* picked = asb::pick_scene_function(scene);
	REQUIRE(picked);

	asIScriptFunction* fn = host.find_function({picked->name});
	REQUIRE(fn);

	asb::Timeline tl = host.run(fn, entry);
	CHECK(tl.keyframes.size() > 10);
	REQUIRE_FALSE(tl.keyframes.empty());
	CHECK(tl.keyframes.back().entities.size() > 40);

	// At least one entity reaches a non-zero position (the vector maths that was
	// previously left at the origin).
	bool placed = false;
	for (const auto& kf : tl.keyframes)
		for (const auto& e : kf.entities)
			if (std::abs(e.pos[0]) > 0.5f || std::abs(e.pos[1]) > 0.5f)
				placed = true;
	CHECK(placed);
}
