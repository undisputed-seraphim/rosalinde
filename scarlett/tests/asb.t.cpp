#include <scarlett/animation.hpp>
#include <scarlett/asb.hpp>
#include <scarlett/host.hpp>
#include <scarlett/runtime.hpp>

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

std::filesystem::path find_viewer_data(const std::string& name) {
	for (const char* dir : {"viewer/data/", "../viewer/data/", "../../viewer/data/"}) {
		const std::filesystem::path p = std::filesystem::path(dir) / name;
		if (std::filesystem::exists(p))
			return p;
	}
	return {};
}

std::vector<uint8_t> extract_entry(const std::filesystem::path& cpk, const std::string& entry) {
	auto pos = entry.find_last_of('/');
	std::string dir = pos == std::string::npos ? "" : entry.substr(0, pos);
	std::string name = pos == std::string::npos ? entry : entry.substr(pos + 1);
	cpk::archive table(cpk);
	std::vector<char> buf;
	auto e = table.find(dir, name);
	if (e == table.end())
		return {};
	table.extract(*e, buf);
	return std::vector<uint8_t>(buf.begin(), buf.end());
}

} // namespace

TEST_CASE("animation controller tracks motion ownership") {
	asb::RuntimeEntity entity;
	entity.name = "actor";
	asb::AnimationController animations;

	animations.play(entity, "D_IDLE_A", 12, 10);
	CHECK(entity.track == "D_IDLE_A");
	CHECK(entity.track_index == 12);
	CHECK(entity.animation_start_frame == 10);

	const uint64_t token = animations.begin_motion(entity, "D_WALK_A1", 40, 20);
	REQUIRE(token != 0);
	CHECK(entity.track == "D_WALK_A1");
	CHECK(entity.track_index == 40);

	animations.play(entity, "D_LOOK_OVER_A", 50, 25);
	CHECK(entity.track == "D_LOOK_OVER_A");
	animations.finish_motion(entity, token, "D_IDLE_A", 12, 30);
	CHECK(entity.track == "D_LOOK_OVER_A");

	const uint64_t next = animations.begin_motion(entity, "D_WALK_A2", 41, 40);
	animations.finish_motion(entity, next, "D_IDLE_A", 12, 50);
	CHECK(entity.track == "D_IDLE_A");
	CHECK(entity.track_index == 12);
	CHECK(entity.animation_start_frame == 50);
}

TEST_CASE("call classification") {
	CHECK(asb::is_sprite_create_call("CreateSpriteMone"));
	CHECK(asb::is_sprite_create_call("CreateSpriteCoA"));
	CHECK(asb::is_chara_create_call("DrUiCreateChara"));
	CHECK(asb::is_entity_create_call("DrCreateChara"));
	CHECK_FALSE(asb::is_entity_create_call("DrUiSetPosV"));
	CHECK_FALSE(asb::is_sprite_create_call("Wait"));
}

TEST_CASE("pick_scene_function follows the heuristic") {
	// void MAIN() is the engine entry for drama modules.
	auto s = make_scene({{"init", 0}, {"scene00", 0}, {"MAIN", 1}, {"scene02", 3}});
	s.functions[2].decl = "void MAIN()";
	REQUIRE(asb::pick_scene_function(s));
	CHECK(asb::pick_scene_function(s)->name == "MAIN");

	// MAIN wins over an entity-creating scene00.
	auto s2 = make_scene({{"scene00", 2}, {"MAIN", 5}});
	s2.functions[1].decl = "void MAIN()";
	CHECK(asb::pick_scene_function(s2)->name == "MAIN");

	// MAIN is the module entry even when it only delegates to a helper.
	auto s4 = make_scene({{"helper", 3}, {"MAIN", 0}});
	s4.functions[1].decl = "void MAIN()";
	CHECK(asb::pick_scene_function(s4)->name == "MAIN");

	// A stage-style int MAIN(int) is not the drama entry.
	auto s5 = make_scene({{"scene00", 2}, {"MAIN", 5}});
	s5.functions[1].decl = "int MAIN(int)";
	CHECK(asb::pick_scene_function(s5)->name == "scene00");

	// An explicit name is honoured verbatim.
	CHECK(asb::pick_scene_function(s5, "MAIN")->name == "MAIN");

	// Falls back to the busiest function then the first non-empty one.
	auto s3 = make_scene({{"a", 1}, {"b", 4}});
	CHECK(asb::pick_scene_function(s3)->name == "b");
}

TEST_CASE("vector call arguments survive scene extraction") {
	const std::filesystem::path cpk = find_cpk();
	if (cpk.empty()) {
		WARN("no Unicorn.CPK found (set UNICORN_CPK); skipping vector argument test");
		return;
	}

	const std::vector<uint8_t> bytes = extract_entry(cpk, "Script/Dr_OW_C_0_Fl_Main_0.asb");
	REQUIRE_FALSE(bytes.empty());
	const asb::Module mod = asb::parse(bytes);
	REQUIRE(mod.error.empty());
	const asb::Scene scene = asb::extract_scene(mod);

	const asb::SceneEvent* move = nullptr;
	for (const auto& function : scene.functions) {
		if (function.name != "scene00")
			continue;
		for (const auto& event : function.events)
			if (event.call == "DrMoveAsync")
				move = &event;
	}
	REQUIRE(move);
	REQUIRE(move->strings.size() >= 2);
	CHECK(move->strings[move->strings.size() - 2] == "D_WALK_A1");
	CHECK(move->strings.back() == "スカーレット");
	REQUIRE(move->vec3.size() == 3);
	CHECK(move->vec3[2] == 1000.0);
	REQUIRE_FALSE(move->floats.empty());
	CHECK(move->floats.back() == 3.0);
}

TEST_CASE("async animation actions defer their idle track") {
	const std::filesystem::path cpk = find_cpk();
	if (cpk.empty()) {
		WARN("no Unicorn.CPK found (set UNICORN_CPK); skipping animation action test");
		return;
	}

	const std::string entry = "Script/Dr_OW_C_0_Fl_Main_0.asb";
	const std::vector<uint8_t> bytes = extract_entry(cpk, entry);
	REQUIRE_FALSE(bytes.empty());
	const asb::Module mod = asb::parse(bytes);
	REQUIRE(mod.error.empty());

	asb::Host host;
	host.register_module(mod);
	host.set_call_log([](const std::string&) {});
	host.set_resolve_track([](const std::string&, const std::string& name) {
		if (name == "D_SIT_A2") return 166;
		if (name == "D_SIT_A3") return 167;
		if (name == "D_IDLE_A") return 85;
		if (name == "D_THINK_A1") return 144;
		if (name == "D_THINK_A2") return 145;
		if (name == "D_THINK_A3") return 146;
		return -1;
	});
	host.set_track_frames([](const std::string&, int) { return 10L; });
	REQUIRE(host.load(bytes));

	asIScriptFunction* fn = host.find_function({"scene00"});
	REQUIRE(fn);
	asb::RuntimeSession session(host, entry);
	REQUIRE(session.start(fn));

	auto advance_to = [&](long target) {
		while (session.frame() < target && !session.finished())
			if (session.advance() == asb::RuntimeSession::StepResult::Error)
				break;
	};
	advance_to(170);
	const asb::RuntimeEntity* entity = session.scene().find_entity("高貴プリーステス");
	REQUIRE(entity);
	CHECK(entity->track == "D_SIT_A3");

	advance_to(180);
	entity = session.scene().find_entity("高貴プリーステス");
	REQUIRE(entity);
	CHECK(entity->track == "D_IDLE_A");
}

TEST_CASE("drama scripts pick their void MAIN entry") {
	const std::filesystem::path cpk = find_cpk();
	if (cpk.empty()) {
		WARN("no Unicorn.CPK found (set UNICORN_CPK); skipping drama entry test");
		return;
	}

	for (const std::string entry : {"Script/Dr_OW_Z_1_St_Before_1_SCR.asb",
			 "Script/Dr_OW_0_Opening.asb"}) {
		const std::vector<uint8_t> bytes = extract_entry(cpk, entry);
		REQUIRE_FALSE(bytes.empty());
		const asb::Module mod = asb::parse(bytes);
		REQUIRE(mod.error.empty());
		const asb::Scene scene = asb::extract_scene(mod);
		const asb::SceneFunction* picked = asb::pick_scene_function(scene);
		REQUIRE(picked);
		CHECK(picked->name == "MAIN");
	}
}

TEST_CASE("field chara symbols resolve their assets") {
	const std::filesystem::path cpk = find_cpk();
	const auto charaset = find_viewer_data("charaset.tsv");
	const auto classes = find_viewer_data("classes.tsv");
	if (cpk.empty() || charaset.empty() || classes.empty()) {
		WARN("missing Unicorn.CPK or viewer/data; skipping field-symbol test");
		return;
	}

	const std::string entry = "Script/Fl_OW.asb";
	std::vector<uint8_t> bytes = extract_entry(cpk, entry);
	REQUIRE_FALSE(bytes.empty());

	asb::Module mod = asb::parse(bytes);
	REQUIRE(mod.error.empty());
	asb::Host host;
	host.register_module(mod);
	host.set_call_log([](const std::string&) {});
	asb::CharaSetTable sets =
		asb::load_chara_set(charaset.string(), classes.string(), find_viewer_data("charanpc_assets.tsv").string());
	host.set_chara_set_table(sets);
	REQUIRE(host.load(bytes));
	asIScriptFunction* fn = host.find_function({"START"});
	REQUIRE(fn);

	asb::Timeline tl = host.run(fn, entry);
	size_t symbols = 0;
	size_t resolved = 0;
	for (const auto& kf : tl.keyframes)
		for (const auto& e : kf.entities)
			if (e.kind == "FlCreateCharaSymbol") {
				++symbols;
				if (!e.mbs.empty() && !e.ftx.empty())
					++resolved;
			}
	CHECK(symbols > 0);
	CHECK(symbols == resolved);
}

TEST_CASE("effect position and offset events are recorded") {
	const std::filesystem::path cpk = find_cpk();
	if (cpk.empty()) {
		WARN("no Unicorn.CPK found (set UNICORN_CPK); skipping effect test");
		return;
	}

	const std::string entry = "Script/Dr_OW_A_2_Fl_Main_0.asb";
	std::vector<uint8_t> bytes = extract_entry(cpk, entry);
	REQUIRE_FALSE(bytes.empty());

	asb::Module mod = asb::parse(bytes);
	REQUIRE(mod.error.empty());
	asb::Host host;
	host.register_module(mod);
	host.set_call_log([](const std::string&) {});
	REQUIRE(host.load(bytes));
	asIScriptFunction* fn = host.find_function({"scene00"});
	REQUIRE(fn);

	asb::Timeline tl = host.run(fn, entry);
	bool saw_offset = false;
	for (const auto& e : tl.effects)
		if (e.kind == "offset")
			saw_offset = true;
	CHECK(saw_offset);
}

TEST_CASE("native callback registrations are retained") {
	const std::filesystem::path cpk = find_cpk();
	if (cpk.empty()) {
		WARN("no Unicorn.CPK found (set UNICORN_CPK); skipping callback test");
		return;
	}

	const std::string entry = "Script/St_OW_A_1.asb";
	std::vector<uint8_t> bytes = extract_entry(cpk, entry);
	REQUIRE_FALSE(bytes.empty());

	asb::Module mod = asb::parse(bytes);
	REQUIRE(mod.error.empty());
	REQUIRE(mod.consumed == mod.total);

	asb::Host host;
	host.register_module(mod);
	host.set_call_log([](const std::string&) {});
	REQUIRE(host.load(bytes));

	asIScriptFunction* fn = host.find_function({"Stage_Setup"});
	REQUIRE(fn);

	asb::Timeline tl = host.run(fn, entry);
	REQUIRE_FALSE(tl.callbacks.empty());

	auto has = [&](const std::string& event, const std::string& function) {
		for (const auto& cb : tl.callbacks)
			if (cb.event == event && cb.function == function)
				return true;
		return false;
	};
	CHECK(has("battle_before", "BattleBefore_Boss"));
	CHECK(has("battle_die", "BattleDie_Boss"));
	CHECK(has("encounter_after", "StEncountAfter_FarLevel"));
	CHECK(has("facility", "Facility_Citizen01"));
	CHECK(has("gate_break_after", "StGataBreakeAfter"));
}

TEST_CASE("opening prelude registers opening resources") {
	const std::filesystem::path cpk = find_cpk();
	if (cpk.empty()) {
		WARN("no Unicorn.CPK found (set UNICORN_CPK); skipping opening prelude test");
		return;
	}

	const std::string entry = "Script/Dr_OW_0_Opening.asb";
	std::vector<uint8_t> bytes = extract_entry(cpk, entry);
	REQUIRE_FALSE(bytes.empty());

	asb::Module mod = asb::parse(bytes);
	REQUIRE(mod.error.empty());
	asb::Host host;
	host.register_module(mod);
	host.set_call_log([](const std::string&) {});
	REQUIRE(host.load(bytes));

	asb::Scene scene = asb::extract_scene(mod);
	const asb::SceneFunction* picked = asb::pick_scene_function(scene);
	REQUIRE(picked);
	asIScriptFunction* fn = host.find_function({picked->name});
	asIScriptFunction* prelude = host.find_function({"ON_TITLE_UI_START"});
	REQUIRE(fn);
	REQUIRE(prelude);

	asb::RuntimeSession session(host, entry);
	REQUIRE(session.start(fn, prelude));
	CHECK(session.scene().opening_resources.size() >= 40);
	CHECK(session.scene().opening_resources.front() == "GUI/Opening010.mbs");
}

TEST_CASE("2D UI positions do not read past their declared components") {
	const std::filesystem::path cpk = find_cpk();
	if (cpk.empty()) {
		WARN("no Unicorn.CPK found (set UNICORN_CPK); skipping 2D position test");
		return;
	}

	const std::string entry = "Script/Dr_OW_0_Opening.asb";
	std::vector<uint8_t> bytes = extract_entry(cpk, entry);
	REQUIRE_FALSE(bytes.empty());

	asb::Module mod = asb::parse(bytes);
	REQUIRE(mod.error.empty());
	asb::Host host;
	host.register_module(mod);
	host.set_call_log([](const std::string&) {});
	REQUIRE(host.load(bytes));

	asIScriptFunction* fn = host.find_function({"scene00"});
	REQUIRE(fn);

	asb::RuntimeSession session(host, entry);
	session.set_max_frames(3400);
	REQUIRE(session.start(fn));
	while (session.frame() < 3400 && !session.finished()) {
		if (session.advance() == asb::RuntimeSession::StepResult::Error)
			break;
	}
	REQUIRE(session.error().empty());

	// DrUiSetPosV takes FnVector2; the third component must stay zero.
	const asb::RuntimeEntity* hero = session.scene().find_entity("主人公");
	REQUIRE(hero);
	CHECK(hero->pos[0] == 1200.0f);
	CHECK(hero->pos[1] == 900.0f);
	CHECK(hero->pos[2] == 0.0f);
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

TEST_CASE("non-ASB input is rejected without a huge string allocation") {
	const std::filesystem::path cpk = find_cpk();
	if (cpk.empty()) {
		WARN("no Unicorn.CPK found (set UNICORN_CPK); skipping invalid input test");
		return;
	}

	const std::string entry = "GUI/Logo.mbs";
	std::vector<uint8_t> bytes = extract_entry(cpk, entry);
	REQUIRE_FALSE(bytes.empty());

	const asb::Module mod = asb::parse(bytes);
	CHECK_FALSE(mod.error.empty());
	CHECK(mod.consumed <= mod.total);
}

TEST_CASE("native call callback carries call, caller and handled") {
	const std::filesystem::path cpk = find_cpk();
	if (cpk.empty()) {
		WARN("no Unicorn.CPK found (set UNICORN_CPK); skipping native call test");
		return;
	}

	const std::string entry = "Script/Dr_OW_0_Opening.asb";
	std::vector<uint8_t> bytes = extract_entry(cpk, entry);
	REQUIRE_FALSE(bytes.empty());

	asb::Module mod = asb::parse(bytes);
	REQUIRE(mod.error.empty());

	asb::Host host;
	host.set_call_log([](const std::string&) {});
	std::vector<asb::NativeCall> calls;
	host.set_native_call([&](const asb::NativeCall& call) { calls.push_back(call); });
	host.register_module(mod);
	REQUIRE(host.load(bytes));

	asIScriptFunction* fn = host.find_function({"scene00"});
	REQUIRE(fn);

	asb::RuntimeSession session(host, entry);
	REQUIRE(session.start(fn));
	REQUIRE_FALSE(calls.empty());

	bool found = false;
	for (const auto& call : calls) {
		if (call.name == "DrUiCreateChara") {
			found = true;
			CHECK(call.caller == "scene00");
			CHECK(call.handled);
			CHECK(call.call.find("DrUiCreateChara(") == 0);
			break;
		}
	}
	CHECK(found);
}

TEST_CASE("runtime session advances a live scene") {
	const std::filesystem::path cpk = find_cpk();
	if (cpk.empty()) {
		WARN("no Unicorn.CPK found (set UNICORN_CPK); skipping live runtime test");
		return;
	}

	const std::string entry = "Script/Dr_OW_0_Opening.asb";
	std::vector<uint8_t> bytes = extract_entry(cpk, entry);
	REQUIRE_FALSE(bytes.empty());

	asb::Module mod = asb::parse(bytes);
	REQUIRE(mod.error.empty());

	asb::Host host;
	host.register_module(mod);
	host.set_call_log([](const std::string&) {});
	REQUIRE(host.load(bytes));

	asIScriptFunction* fn = host.find_function({"scene00"});
	REQUIRE(fn);

	asb::RuntimeSession session(host, entry);
	session.set_max_frames(5);
	REQUIRE(session.start(fn));
	CHECK(session.frame() == 0);
	CHECK(session.scene().frame == 0);
	CHECK_FALSE(session.scene().entities.empty());
	CHECK(session.scene().find_entity(session.scene().entities.front().name));

	const size_t initial = session.scene().entities.size();
	for (int i = 1; i <= 3; ++i) {
		const auto r = session.advance();
		CHECK(r != asb::RuntimeSession::StepResult::Error);
		CHECK(session.frame() == i);
		CHECK(session.scene().frame == i);
	}
	CHECK(session.scene().entities.size() >= initial);
	CHECK_FALSE(session.finished());

	bool finished = session.finished();
	while (!finished && !session.limit_reached()) {
		const auto r = session.advance();
		finished = r == asb::RuntimeSession::StepResult::Finished;
		if (r == asb::RuntimeSession::StepResult::Error)
			break;
	}
	CHECK(session.error().empty());
	CHECK((session.finished() || session.limit_reached()));
}
