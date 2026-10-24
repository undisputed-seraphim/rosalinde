#include "probe.hpp"

#include "mbs_info.hpp"

#include <scarlett/trace.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace {

std::string frame_name(long frame) {
	std::ostringstream out;
	out << std::setfill('0') << std::setw(6) << frame;
	return out.str();
}

std::string quote(std::string_view value) { return "\"" + asb::trace::quote(value) + "\""; }

std::string number(double value) {
	if (!std::isfinite(value))
		return "null";
	return asb::trace::number(value);
}

std::string vec2_json(const float* v) { return "[" + number(v[0]) + "," + number(v[1]) + "]"; }

std::string vec3_json(const float* v) { return "[" + number(v[0]) + "," + number(v[1]) + "," + number(v[2]) + "]"; }

std::string vec4_json(const float* v) {
	return "[" + number(v[0]) + "," + number(v[1]) + "," + number(v[2]) + "," + number(v[3]) + "]";
}

std::string color_json(const unsigned char* c) {
	return "[" + std::to_string(c[0]) + "," + std::to_string(c[1]) + "," + std::to_string(c[2]) + "," +
		   std::to_string(c[3]) + "]";
}

template <typename T, typename F>
std::string array_json(const std::vector<T>& values, F&& fn) {
	std::string out = "[";
	for (size_t i = 0; i < values.size(); ++i) {
		if (i)
			out += ',';
		out += fn(values[i]);
	}
	out += ']';
	return out;
}

struct EntityReport {
	int selected_track = -1;
	const MbsInfoCache::Track* selected = nullptr;
	int resolved_track = -1;
	const MbsInfoCache::Track* resolved = nullptr;
	bool mbs_found = false;
	bool ftx_found = false;
	bool drawable = false;
	std::string missing;
};

EntityReport report_entity(const asb::RuntimeEntity& e, const MbsInfoCache& cache) {
	EntityReport report;
	const MbsInfoCache::Info& info = cache.get(e.mbs);
	report.mbs_found = info.found;
	report.ftx_found = cache.ftx_exists(e.ftx, e.mbs);
	if (const MbsInfoCache::Track* by_index = info.track(e.track_index)) {
		report.selected_track = e.track_index;
		report.selected = by_index;
	} else if (!e.track.empty()) {
		const int by_name = info.resolve(e.track);
		if (const MbsInfoCache::Track* track = info.track(by_name)) {
			report.selected_track = by_name;
			report.selected = track;
		}
	}
	report.resolved_track = info.choose(e.track_index, e.track);
	report.resolved = info.track(report.resolved_track);
	if (!e.started)
		report.missing = "not_started";
	else if (!e.visible)
		report.missing = "invisible";
	else if (e.alpha <= 0)
		report.missing = "transparent";
	else if (!report.mbs_found)
		report.missing = "mbs";
	else if (!info.parsed)
		report.missing = "mbs_parse";
	else if (!report.ftx_found)
		report.missing = "ftx";
	else if (!report.resolved || !report.resolved->usable)
		report.missing = "no_track";
	report.drawable = report.missing.empty();
	return report;
}

std::string entity_json(const asb::RuntimeEntity& e, const SceneReportContext& context) {
	std::string out = "{\"name\":" + quote(e.name) + ",\"kind\":" + quote(e.kind) +
					  ",\"aliases\":" + array_json(e.aliases, [](const std::string& s) { return quote(s); }) +
					  ",\"mbs\":" + quote(e.mbs) + ",\"ftx\":" + quote(e.ftx) + ",\"track\":" + quote(e.track) +
					  ",\"track_index\":" + std::to_string(e.track_index) +
					  ",\"idle_track\":" + quote(e.idle_track) +
					  ",\"idle_track_index\":" + std::to_string(e.idle_track_index) +
					  ",\"action_active\":" + asb::trace::boolean(e.action_active) +
					  ",\"action_end_frame\":" + std::to_string(e.action_end_frame) +
					  ",\"started\":" + asb::trace::boolean(e.started) + ",\"pos\":" + vec3_json(e.pos) +
					  ",\"scale\":" + number(e.scale) + ",\"dir_x\":" + number(e.dir_x) +
					  ",\"alpha\":" + std::to_string(e.alpha) + ",\"layer\":" + std::to_string(e.layer) +
					  ",\"visible\":" + asb::trace::boolean(e.visible) +
					  ",\"reverse\":" + asb::trace::boolean(e.reverse) +
					  ",\"layer_mask\":" + std::to_string(e.layer_mask) +
					  ",\"breath_off\":" + asb::trace::boolean(e.breath_off) +
					  ",\"lipsync\":" + asb::trace::boolean(e.lipsync) +
					  ",\"shadow_off\":" + asb::trace::boolean(e.shadow_off) +
					  ",\"sort_id_offset\":" + std::to_string(e.sort_id_offset) +
					  ",\"dlprio\":" + std::to_string(e.dlprio) +
					  ",\"window_id_offset\":" + std::to_string(e.window_id_offset) +
					  ",\"prerender_force\":" + asb::trace::boolean(e.prerender_force) + ",\"color\":[" +
					  std::to_string(e.color[0]) + "," + std::to_string(e.color[1]) + "," +
					  std::to_string(e.color[2]) + "," + std::to_string(e.color[3]) + "]" +
					  ",\"state\":" + std::to_string(e.state) + ",\"state_icon\":" + std::to_string(e.state_icon) +
					  ",\"height\":" + number(e.height) + ",\"emote\":" + std::to_string(e.emote) +
					  ",\"search_flag\":" + asb::trace::boolean(e.search_flag) +
					  ",\"paused\":" + asb::trace::boolean(e.paused) +
					  ",\"anim_paused\":" + asb::trace::boolean(e.anim_paused) +
					  ",\"ai_paused\":" + asb::trace::boolean(e.ai_paused) +
					  ",\"auto_dir_off\":" + asb::trace::boolean(e.auto_dir_off) +
					  ",\"reverse_x\":" + asb::trace::boolean(e.reverse_x) + ",\"order\":" + std::to_string(e.order) +
					  ",\"space\":" + quote(e.space == asb::RuntimeEntity::Space::Ui ? "ui" : "world") +
					  ",\"parent\":" + quote(e.parent);
	if (context.mbs) {
		const EntityReport report = report_entity(e, *context.mbs);
		out += ",\"track_name\":" + quote(report.selected ? report.selected->name : e.track);
		out += ",\"track_runs\":" + std::to_string(report.selected ? report.selected->runs : 0);
		out += ",\"resolved_track\":" + std::to_string(report.resolved_track);
		out += ",\"resolved_track_name\":" + quote(report.resolved ? report.resolved->name : "");
		out += ",\"drawable\":" + asb::trace::boolean(report.drawable);
		out += ",\"mbs_found\":" + asb::trace::boolean(report.mbs_found);
		out += ",\"ftx_found\":" + asb::trace::boolean(report.ftx_found);
		out += ",\"missing\":" + quote(report.missing);
	}
	out += "}";
	return out;
}

std::string camera_json(const asb::RuntimeCamera& c) {
	const auto& t = c.tween;
	return "{\"pos\":" + vec3_json(c.pos) + ",\"has_pos\":" + asb::trace::boolean(c.has_pos) +
		   ",\"zoom\":" + number(c.zoom) + ",\"has_zoom\":" + asb::trace::boolean(c.has_zoom) +
		   ",\"accel\":" + number(c.accel) + ",\"decel\":" + number(c.decel) +
		   ",\"param_set\":" + asb::trace::boolean(c.param_set) + ",\"origin\":" + vec3_json(c.origin) +
		   ",\"tween\":{\"active\":" + asb::trace::boolean(t.active) +
		   ",\"zoom_active\":" + asb::trace::boolean(t.zoom_active) +
		   ",\"start_frame\":" + std::to_string(t.start_frame) + ",\"end_frame\":" + std::to_string(t.end_frame) +
		   ",\"start_pos\":" + vec3_json(t.start_pos) + ",\"target_pos\":" + vec3_json(t.target_pos) +
		   ",\"start_zoom\":" + number(t.start_zoom) + ",\"target_zoom\":" + number(t.target_zoom) +
		   ",\"ease_in\":" + number(t.ease_in) + ",\"ease_out\":" + number(t.ease_out) + "}" +
		   ",\"tebure\":" + asb::trace::boolean(c.tebure) + ",\"wave\":" + asb::trace::boolean(c.wave) +
		   ",\"demo_y\":" + number(c.demo_y) + ",\"demo_xz\":[" + number(c.demo_xz[0]) + "," + number(c.demo_xz[1]) +
		   "],\"degree_offset\":" + number(c.degree_offset) + "}";
}

std::string stage_json(const asb::StageConfig& s) {
	return "{\"frame\":" + std::to_string(s.frame) + ",\"stage_type\":" + std::to_string(s.stage_type) +
		   ",\"weather_type\":" + std::to_string(s.weather_type) + ",\"coa_preset\":" + std::to_string(s.coa_preset) +
		   ",\"flag_color\":" + std::to_string(s.flag_color) + "}";
}

} // namespace

std::string scene_state_json(const asb::SceneState& scene, const SceneReportContext& context) {
	return "{\"source\":" + quote(scene.source) + ",\"function\":" + quote(scene.function) +
		   ",\"frame\":" + std::to_string(scene.frame) + ",\"revision\":" + std::to_string(scene.revision) +
		   ",\"entities\":" +
		   array_json(scene.entities, [&](const asb::RuntimeEntity& e) { return entity_json(e, context); }) +
		   ",\"camera\":" + camera_json(scene.camera) + ",\"stage\":" + stage_json(scene.stage) +
		   ",\"stages\":" + array_json(scene.stages, [](const asb::StageConfig& s) { return stage_json(s); }) +
		   ",\"fades\":" +
		   array_json(
			   scene.fades,
			   [](const asb::ScreenFade& f) {
				   return "{\"start\":" + std::to_string(f.start) + ",\"end\":" + std::to_string(f.end) +
						  ",\"from\":" + vec4_json(f.from) + ",\"to\":" + vec4_json(f.to) + "}";
			   }) +
		   ",\"texts\":" +
		   array_json(
			   scene.texts,
			   [](const asb::RuntimeText& t) {
				   return "{\"frame\":" + std::to_string(t.frame) + ",\"end\":" + std::to_string(t.end) +
						  ",\"kind\":" + std::to_string(static_cast<int>(t.kind)) + ",\"speaker\":" + quote(t.speaker) +
						  ",\"text\":" + quote(t.text) +
						  ",\"ids\":" + array_json(t.ids, [](int id) { return std::to_string(id); }) + ",\"pos\":[" +
						  number(t.pos[0]) + "," + number(t.pos[1]) + "]" +
						  ",\"disp_type\":" + std::to_string(t.disp_type) +
						  ",\"disp_name\":" + std::to_string(t.disp_name) +
						  ",\"face_dir_x\":" + std::to_string(t.face_dir_x) + ",\"flags\":" + std::to_string(t.flags) +
						  ",\"result\":" + std::to_string(t.result) + "}";
			   }) +
		   ",\"shakes\":" +
		   array_json(
			   scene.shakes,
			   [](const asb::RuntimeShake& s) {
				   return "{\"start\":" + std::to_string(s.start) + ",\"end\":" + std::to_string(s.end) +
						  ",\"index\":" + std::to_string(s.index) + ",\"type\":" + std::to_string(s.type) +
						  ",\"shake\":[" + number(s.shake[0]) + "," + number(s.shake[1]) + "]" +
						  ",\"angle\":" + number(s.angle) + ",\"att\":" + number(s.att) + ",\"pow\":" + number(s.pow) +
						  "}";
			   }) +
		   ",\"bokashi\":" +
		   array_json(
			   scene.bokashi,
			   [](const asb::RuntimeBokashi& b) {
				   return "{\"start\":" + std::to_string(b.start) + ",\"end\":" + std::to_string(b.end) +
						  ",\"bokashi\":" + asb::trace::boolean(b.bokashi) +
						  ",\"freeze\":" + asb::trace::boolean(b.freeze) + ",\"max_rate\":" + number(b.max_rate) + "}";
			   }) +
		   ",\"variations\":" +
		   array_json(
			   scene.variations,
			   [](const asb::VariationEvent& v) {
				   return "{\"frame\":" + std::to_string(v.frame) + ",\"mask\":" + std::to_string(v.mask) + "}";
			   }) +
		   ",\"effects\":" +
		   array_json(
			   scene.effects,
			   [](const asb::RuntimeEffect& e) {
				   return "{\"frame\":" + std::to_string(e.frame) + ",\"kind\":" + quote(e.kind) +
						  ",\"name\":" + quote(e.name) + ",\"mbs\":" + quote(e.mbs) + ",\"ftx\":" + quote(e.ftx) +
						  ",\"attached\":" + quote(e.attached) + ",\"anime\":" + std::to_string(e.anime) +
						  ",\"pos\":" + vec3_json(e.pos) + ",\"i0\":" + std::to_string(e.i0) +
						  ",\"i1\":" + std::to_string(e.i1) + ",\"i2\":" + std::to_string(e.i2) +
						  ",\"f0\":" + number(e.f0) + ",\"flag\":" + asb::trace::boolean(e.flag) + "}";
			   }) +
		   ",\"point_lights\":" +
		   array_json(
			   scene.point_lights,
			   [](const asb::RuntimePointLight& l) {
				   return "{\"frame\":" + std::to_string(l.frame) + ",\"remove\":" + asb::trace::boolean(l.remove) +
						  ",\"name\":" + quote(l.name) + ",\"pos\":" + vec3_json(l.pos) +
						  ",\"light\":" + color_json(l.light) + ",\"shadow\":" + color_json(l.shadow) +
						  ",\"godray\":" + color_json(l.godray) + ",\"power\":" + number(l.power) +
						  ",\"bloom\":" + number(l.bloom) + ",\"f2\":" + number(l.f2) + ",\"f3\":" + number(l.f3) +
						  ",\"f4\":" + number(l.f4) + ",\"f5\":" + number(l.f5) + ",\"attached\":" + quote(l.attached) +
						  ",\"index\":" + std::to_string(l.index) + "}";
			   }) +
		   ",\"audio\":" +
		   array_json(
			   scene.audio,
			   [](const asb::RuntimeAudio& a) {
				   return "{\"frame\":" + std::to_string(a.frame) + ",\"kind\":" + quote(a.kind) +
						  ",\"cue\":" + std::to_string(a.cue) + ",\"target\":" + quote(a.target) +
						  ",\"fade\":" + std::to_string(a.fade) + ",\"wait\":" + std::to_string(a.wait) +
						  ",\"index\":" + std::to_string(a.index) + ",\"volume\":" + number(a.volume) + "}";
			   }) +
		   ",\"items\":" +
		   array_json(
			   scene.items,
			   [](const asb::RuntimeItem& i) {
				   return "{\"frame\":" + std::to_string(i.frame) + ",\"kind\":" + quote(i.kind) +
						  ",\"object\":" + quote(i.object) + ",\"item\":" + quote(i.item) +
						  ",\"id\":" + std::to_string(i.id) + ",\"count\":" + std::to_string(i.count) + ",\"pos\":[" +
						  number(i.pos[0]) + "," + number(i.pos[1]) + "]" +
						  ",\"dialog\":" + asb::trace::boolean(i.dialog) + ",\"callback\":" + quote(i.callback) +
						  ",\"add\":" +
						  array_json(
							  std::vector<int>(i.add, i.add + std::clamp(i.add_count, 0, 8)),
							  [](int value) { return std::to_string(value); }) +
						  ",\"add_count\":" + std::to_string(i.add_count) + "}";
			   }) +
		   ",\"callbacks\":" +
		   array_json(
			   scene.callbacks,
			   [](const asb::RuntimeCallback& c) {
				   return "{\"frame\":" + std::to_string(c.frame) + ",\"event\":" + quote(c.event) +
						  ",\"key\":" + quote(c.key) + ",\"slot\":" + quote(c.slot) +
						  ",\"function\":" + quote(c.function) + "}";
			   }) +
		   ",\"fog\":" +
		   array_json(
			   scene.fog,
			   [](const asb::RuntimeFog& f) {
				   return "{\"frame\":" + std::to_string(f.frame) + ",\"name\":" + quote(f.name) +
						  ",\"color\":" + color_json(f.color) + "}";
			   }) +
		   ",\"gradients\":" +
		   array_json(
			   scene.gradients,
			   [](const asb::RuntimeGradient& g) {
				   return "{\"frame\":" + std::to_string(g.frame) + ",\"index\":" + std::to_string(g.index) +
						  ",\"rate\":" + number(g.rate) + ",\"time\":" + std::to_string(g.time) + "}";
			   }) +
		   ",\"ui\":" +
		   array_json(
			   scene.ui,
			   [](const asb::RuntimeUi& u) {
				   return "{\"frame\":" + std::to_string(u.frame) + ",\"kind\":" + quote(u.kind) +
						  ",\"text\":" + quote(u.text) + ",\"id\":" + std::to_string(u.id) +
						  ",\"i0\":" + std::to_string(u.i0) + ",\"i1\":" + std::to_string(u.i1) + ",\"pos\":[" +
						  number(u.pos[0]) + "," + number(u.pos[1]) + "]" + ",\"rate\":" + number(u.rate) +
						  ",\"flag\":" + asb::trace::boolean(u.flag) +
						  ",\"ints\":" + array_json(u.ints, [](int value) { return std::to_string(value); }) + "}";
			   }) +
		   ",\"field\":" +
		   array_json(
			   scene.field,
			   [](const asb::RuntimeField& f) {
				   return "{\"frame\":" + std::to_string(f.frame) + ",\"kind\":" + quote(f.kind) +
						  ",\"name\":" + quote(f.name) + ",\"text\":" + quote(f.text) + ",\"text2\":" + quote(f.text2) +
						  ",\"i0\":" + std::to_string(f.i0) + ",\"i1\":" + std::to_string(f.i1) +
						  ",\"i2\":" + std::to_string(f.i2) + ",\"f0\":" + number(f.f0) + ",\"pos\":[" +
						  number(f.pos[0]) + "," + number(f.pos[1]) + "]" + ",\"flag\":" + asb::trace::boolean(f.flag) +
						  ",\"ints\":" + array_json(f.ints, [](int value) { return std::to_string(value); }) + "}";
			   }) +
		   ",\"opening_resources\":" +
		   array_json(scene.opening_resources, [](const std::string& s) { return quote(s); }) + "}";
}

std::string scene_state_text(const asb::SceneState& scene, const SceneReportContext& context) {
	std::ostringstream out;
	out << "source: " << scene.source << '\n';
	out << "function: " << scene.function << '\n';
	out << "frame: " << scene.frame << " revision: " << scene.revision << " entities: " << scene.entities.size()
		<< '\n';
	for (const auto& e : scene.entities) {
		out << e.name << " kind=" << e.kind << " mbs=" << e.mbs << " ftx=" << e.ftx << " track=" << e.track
			<< " track_index=" << e.track_index << " idle=" << e.idle_track
			<< " action=" << (e.action_active ? "true" : "false")
			<< " action_end=" << e.action_end_frame << " started=" << (e.started ? "true" : "false")
			<< " visible=" << (e.visible ? "true" : "false") << " alpha=" << e.alpha << " order=" << e.order
			<< " layer=" << e.layer << " layer_mask=" << e.layer_mask << " scale=" << number(e.scale) << " pos=("
			<< number(e.pos[0]) << ',' << number(e.pos[1]) << ',' << number(e.pos[2]) << ')'
			<< " paused=" << (e.paused ? "true" : "false")
			<< " anim_paused=" << (e.anim_paused ? "true" : "false")
			<< " space=" << (e.space == asb::RuntimeEntity::Space::Ui ? "ui" : "world");
		if (context.mbs) {
			const EntityReport report = report_entity(e, *context.mbs);
			out << " track_name=" << (report.selected ? report.selected->name : e.track)
				<< " track_runs=" << (report.selected ? report.selected->runs : 0)
				<< " resolved_track=" << report.resolved_track
				<< " resolved_track_name=" << (report.resolved ? report.resolved->name : "")
				<< " drawable=" << (report.drawable ? "true" : "false")
				<< " mbs_found=" << (report.mbs_found ? "true" : "false")
				<< " ftx_found=" << (report.ftx_found ? "true" : "false") << " missing=" << report.missing;
		}
		if (!e.parent.empty())
			out << " parent=" << e.parent;
		if (!e.aliases.empty()) {
			out << " aliases=[";
			for (size_t i = 0; i < e.aliases.size(); ++i) {
				if (i)
					out << ',';
				out << e.aliases[i];
			}
			out << ']';
		}
		out << '\n';
	}
	out << "camera: pos=(" << number(scene.camera.pos[0]) << ',' << number(scene.camera.pos[1]) << ','
		<< number(scene.camera.pos[2]) << ") zoom=" << number(scene.camera.zoom)
		<< " has_pos=" << asb::trace::boolean(scene.camera.has_pos)
		<< " has_zoom=" << asb::trace::boolean(scene.camera.has_zoom) << '\n';
	out << "stage: type=" << scene.stage.stage_type << " weather=" << scene.stage.weather_type
		<< " coa=" << scene.stage.coa_preset << " flag=" << scene.stage.flag_color << '\n';
	out << "channels: fades=" << scene.fades.size() << " texts=" << scene.texts.size()
		<< " shakes=" << scene.shakes.size() << " bokashi=" << scene.bokashi.size()
		<< " variations=" << scene.variations.size() << " effects=" << scene.effects.size()
		<< " point_lights=" << scene.point_lights.size() << " audio=" << scene.audio.size()
		<< " items=" << scene.items.size() << " callbacks=" << scene.callbacks.size() << " fog=" << scene.fog.size()
		<< " gradients=" << scene.gradients.size() << " ui=" << scene.ui.size() << " field=" << scene.field.size()
		<< " opening_resources=" << scene.opening_resources.size() << '\n';
	return out.str();
}

bool ProbeWriter::open(const std::string& dir, const std::vector<long>& frames) {
	_dir = dir;
	_frames.clear();
	_written.clear();
	for (long frame : frames)
		_frames.insert(frame);
	if (_dir.empty() || _frames.empty())
		return false;
	std::error_code ec;
	std::filesystem::create_directories(_dir, ec);
	if (ec) {
		spdlog::error("probe: cannot create {}: {}", _dir, ec.message());
		return false;
	}
	_enabled = true;
	return true;
}

std::string ProbeWriter::path(std::string_view prefix, long frame, std::string_view extension) const {
	return (std::filesystem::path(_dir) / (std::string(prefix) + "-" + frame_name(frame) + std::string(extension)))
		.string();
}

bool ProbeWriter::wants(long frame) const { return _enabled && _frames.contains(frame) && !_written.contains(frame); }

bool ProbeWriter::write_scene(long frame, std::string_view json, std::string_view summary) {
	if (!wants(frame))
		return false;
	write_state(frame, json);
	write_summary(frame, summary);
	_written.insert(frame);
	return true;
}

bool ProbeWriter::begin(long frame) {
	if (!wants(frame))
		return false;
	_frame = frame;
	_draw.open(path("draw", frame, ".ndjson"), std::ios::binary | std::ios::trunc);
	if (!_draw) {
		spdlog::error("probe: cannot write draw dump for frame {}", frame);
		_enabled = false;
		return false;
	}
	_draw << "{\"frame\":" << frame << ",\"kind\":\"draw\",\"event\":\"begin\"}\n";
	return true;
}

void ProbeWriter::end() {
	if (_frame < 0)
		return;
	if (_draw) {
		_draw << "{\"frame\":" << _frame << ",\"kind\":\"draw\",\"event\":\"end\"}\n";
		_draw.close();
	}
	_written.insert(_frame);
	_frame = -1;
}

void ProbeWriter::write_state(long frame, std::string_view json) {
	std::ofstream out(path("state", frame, ".json"), std::ios::binary | std::ios::trunc);
	if (!out) {
		spdlog::error("probe: cannot write state for frame {}", frame);
		return;
	}
	out << json << '\n';
}

void ProbeWriter::write_summary(long frame, std::string_view text) {
	std::ofstream out(path("summary", frame, ".txt"), std::ios::binary | std::ios::trunc);
	if (!out) {
		spdlog::error("probe: cannot write summary for frame {}", frame);
		return;
	}
	out << text;
}
