#include "asbrun_record.hpp"

#include "asbrun_common.hpp"

#include <cstdio>
#include <string>

namespace asbrun {

void write_timeline_json(FILE* out, const asb::Timeline& timeline) {
	std::fprintf(
		out,
		"{\n\t\"source\": \"%s\",\n\t\"function\": \"%s\",\n",
		json_escape(timeline.source).c_str(),
		json_escape(timeline.function).c_str());
	std::fprintf(out, "\t\"keyframes\": [\n");
	for (size_t k = 0; k < timeline.keyframes.size(); ++k) {
		const asb::Keyframe& frame = timeline.keyframes[k];
		std::fprintf(out, "\t\t{\n\t\t\t\"frame\": %ld,\n", frame.frame);
		std::fprintf(
			out,
			"\t\t\t\"camera\": {\"pos\": [%g, %g, %g], \"has_pos\": %s, \"zoom\": %g, \"has_zoom\": %s, "
			"\"accel\": %g, \"decel\": %g, \"tebure\": %s, \"wave\": %s, \"demo_y\": %g, "
			"\"degree_offset\": %g},\n",
			frame.camera.pos[0],
			frame.camera.pos[1],
			frame.camera.pos[2],
			frame.camera.has_pos ? "true" : "false",
			frame.camera.zoom,
			frame.camera.has_zoom ? "true" : "false",
			frame.camera.accel,
			frame.camera.decel,
			frame.camera.tebure ? "true" : "false",
			frame.camera.wave ? "true" : "false",
			frame.camera.demo_y,
			frame.camera.degree_offset);
		std::fprintf(out, "\t\t\t\"entities\": [\n");
		for (size_t i = 0; i < frame.entities.size(); ++i) {
			const asb::RuntimeEntity& entity = frame.entities[i];
			std::string aliases;
			for (size_t j = 0; j < entity.aliases.size(); ++j) {
				if (j)
					aliases += ", ";
				aliases += "\"" + json_escape(entity.aliases[j]) + "\"";
			}
			std::fprintf(
				out,
				"\t\t\t\t{\"name\": \"%s\", \"kind\": \"%s\", \"space\": \"%s\", \"mbs\": \"%s\", \"ftx\": \"%s\", "
				"\"track\": \"%s\", "
				"\"track_index\": %d, \"pos\": [%g, %g, %g], \"scale\": %g, \"dir_x\": %g, \"alpha\": %d, "
				"\"layer\": %d, \"visible\": %s, \"layer_mask\": %u, \"breath_off\": %s, \"lipsync\": %s, "
				"\"shadow_off\": %s, \"sort_id_offset\": %d, \"dlprio\": %d, \"window_id_offset\": %d, "
				"\"prerender_force\": %s, \"color\": [%d, %d, %d, %d], "
				"\"order\": %d, \"aliases\": [%s]}%s\n",
				json_escape(entity.name).c_str(),
				json_escape(entity.kind).c_str(),
				entity.space == asb::RuntimeEntity::Space::Ui ? "ui" : "world",
				json_escape(entity.mbs).c_str(),
				json_escape(entity.ftx).c_str(),
				json_escape(entity.track).c_str(),
				entity.track_index,
				entity.pos[0],
				entity.pos[1],
				entity.pos[2],
				entity.scale,
				entity.dir_x,
				entity.alpha,
				entity.layer,
				entity.visible ? "true" : "false",
				entity.layer_mask,
				entity.breath_off ? "true" : "false",
				entity.lipsync ? "true" : "false",
				entity.shadow_off ? "true" : "false",
				entity.sort_id_offset,
				entity.dlprio,
				entity.window_id_offset,
				entity.prerender_force ? "true" : "false",
				entity.color[0],
				entity.color[1],
				entity.color[2],
				entity.color[3],
				entity.order,
				aliases.c_str(),
				i + 1 < frame.entities.size() ? "," : "");
		}
		std::fprintf(out, "\t\t\t]\n\t\t}%s\n", k + 1 < timeline.keyframes.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"fades\": [\n");
	for (size_t i = 0; i < timeline.fades.size(); ++i) {
		const asb::ScreenFade& fade = timeline.fades[i];
		std::fprintf(
			out,
			"\t\t{\"start\": %ld, \"end\": %ld, \"from\": [%g, %g, %g, %g], \"to\": [%g, %g, %g, %g]}%s\n",
			fade.start,
			fade.end,
			fade.from[0],
			fade.from[1],
			fade.from[2],
			fade.from[3],
			fade.to[0],
			fade.to[1],
			fade.to[2],
			fade.to[3],
			i + 1 < timeline.fades.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"stages\": [\n");
	for (size_t i = 0; i < timeline.stages.size(); ++i) {
		const asb::StageConfig& stage = timeline.stages[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"stage_type\": %d, \"weather_type\": %d, \"coa_preset\": %d, \"flag_color\": "
			"%d}%s\n",
			stage.frame,
			stage.stage_type,
			stage.weather_type,
			stage.coa_preset,
			stage.flag_color,
			i + 1 < timeline.stages.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"texts\": [\n");
	for (size_t i = 0; i < timeline.texts.size(); ++i) {
		const asb::RuntimeText& text = timeline.texts[i];
		std::string ids;
		for (size_t j = 0; j < text.ids.size(); ++j) {
			if (j)
				ids += ", ";
			ids += std::to_string(text.ids[j]);
		}
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"end\": %ld, \"kind\": %d, \"speaker\": \"%s\", \"text\": \"%s\", "
			"\"pos\": [%g, %g], \"disp_type\": %d, \"disp_name\": %d, \"face_dir_x\": %d, \"flags\": %u, "
			"\"result\": %d, \"ids\": [%s]}%s\n",
			text.frame,
			text.end,
			static_cast<int>(text.kind),
			json_escape(text.speaker).c_str(),
			json_escape(text.text).c_str(),
			text.pos[0],
			text.pos[1],
			text.disp_type,
			text.disp_name,
			text.face_dir_x,
			text.flags,
			text.result,
			ids.c_str(),
			i + 1 < timeline.texts.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"shakes\": [\n");
	for (size_t i = 0; i < timeline.shakes.size(); ++i) {
		const asb::RuntimeShake& shake = timeline.shakes[i];
		std::fprintf(
			out,
			"\t\t{\"start\": %ld, \"end\": %ld, \"index\": %d, \"type\": %d, \"shake\": [%g, %g], "
			"\"angle\": %g, \"att\": %g, \"pow\": %g}%s\n",
			shake.start,
			shake.end,
			shake.index,
			shake.type,
			shake.shake[0],
			shake.shake[1],
			shake.angle,
			shake.att,
			shake.pow,
			i + 1 < timeline.shakes.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"bokashi\": [\n");
	for (size_t i = 0; i < timeline.bokashi.size(); ++i) {
		const asb::RuntimeBokashi& bokashi = timeline.bokashi[i];
		std::fprintf(
			out,
			"\t\t{\"start\": %ld, \"end\": %ld, \"bokashi\": %s, \"freeze\": %s, \"max_rate\": %g}%s\n",
			bokashi.start,
			bokashi.end,
			bokashi.bokashi ? "true" : "false",
			bokashi.freeze ? "true" : "false",
			bokashi.max_rate,
			i + 1 < timeline.bokashi.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"variations\": [\n");
	for (size_t i = 0; i < timeline.variations.size(); ++i) {
		const asb::VariationEvent& variation = timeline.variations[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"mask\": %u}%s\n",
			variation.frame,
			variation.mask,
			i + 1 < timeline.variations.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"effects\": [\n");
	for (size_t i = 0; i < timeline.effects.size(); ++i) {
		const asb::RuntimeEffect& effect = timeline.effects[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"kind\": \"%s\", \"name\": \"%s\", \"mbs\": \"%s\", \"ftx\": \"%s\", "
			"\"attached\": \"%s\", \"anime\": %d, \"pos\": [%g, %g, %g], \"i0\": %d, \"i1\": %d, \"i2\": %d, "
			"\"f0\": %g, \"flag\": %s}%s\n",
			effect.frame,
			json_escape(effect.kind).c_str(),
			json_escape(effect.name).c_str(),
			json_escape(effect.mbs).c_str(),
			json_escape(effect.ftx).c_str(),
			json_escape(effect.attached).c_str(),
			effect.anime,
			effect.pos[0],
			effect.pos[1],
			effect.pos[2],
			effect.i0,
			effect.i1,
			effect.i2,
			effect.f0,
			effect.flag ? "true" : "false",
			i + 1 < timeline.effects.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"point_lights\": [\n");
	for (size_t i = 0; i < timeline.point_lights.size(); ++i) {
		const asb::RuntimePointLight& light = timeline.point_lights[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"remove\": %s, \"name\": \"%s\", \"pos\": [%g, %g, %g], "
			"\"light\": [%u, %u, %u, %u], \"shadow\": [%u, %u, %u, %u], \"godray\": [%u, %u, %u, %u], "
			"\"power\": %g, \"bloom\": %g, \"f2\": %g, \"f3\": %g, \"f4\": %g, \"f5\": %g, "
			"\"attached\": \"%s\", \"index\": %d}%s\n",
			light.frame,
			light.remove ? "true" : "false",
			json_escape(light.name).c_str(),
			light.pos[0],
			light.pos[1],
			light.pos[2],
			light.light[0],
			light.light[1],
			light.light[2],
			light.light[3],
			light.shadow[0],
			light.shadow[1],
			light.shadow[2],
			light.shadow[3],
			light.godray[0],
			light.godray[1],
			light.godray[2],
			light.godray[3],
			light.power,
			light.bloom,
			light.f2,
			light.f3,
			light.f4,
			light.f5,
			json_escape(light.attached).c_str(),
			light.index,
			i + 1 < timeline.point_lights.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"audio\": [\n");
	for (size_t i = 0; i < timeline.audio.size(); ++i) {
		const asb::RuntimeAudio& audio = timeline.audio[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"kind\": \"%s\", \"cue\": %d, \"target\": \"%s\", \"fade\": %d, "
			"\"wait\": %d, \"index\": %d, \"volume\": %g}%s\n",
			audio.frame,
			json_escape(audio.kind).c_str(),
			audio.cue,
			json_escape(audio.target).c_str(),
			audio.fade,
			audio.wait,
			audio.index,
			audio.volume,
			i + 1 < timeline.audio.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"items\": [\n");
	for (size_t i = 0; i < timeline.items.size(); ++i) {
		const asb::RuntimeItem& item = timeline.items[i];
		std::string add;
		for (int k = 0; k < item.add_count; ++k) {
			if (k)
				add += ", ";
			add += std::to_string(item.add[k]);
		}
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"kind\": \"%s\", \"object\": \"%s\", \"item\": \"%s\", \"id\": %d, "
			"\"count\": %d, \"pos\": [%g, %g], \"dialog\": %s, \"callback\": \"%s\", \"add\": [%s]}%s\n",
			item.frame,
			json_escape(item.kind).c_str(),
			json_escape(item.object).c_str(),
			json_escape(item.item).c_str(),
			item.id,
			item.count,
			item.pos[0],
			item.pos[1],
			item.dialog ? "true" : "false",
			json_escape(item.callback).c_str(),
			add.c_str(),
			i + 1 < timeline.items.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"callbacks\": [\n");
	for (size_t i = 0; i < timeline.callbacks.size(); ++i) {
		const asb::RuntimeCallback& callback = timeline.callbacks[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"event\": \"%s\", \"key\": \"%s\", \"slot\": \"%s\", \"function\": \"%s\"}%s\n",
			callback.frame,
			json_escape(callback.event).c_str(),
			json_escape(callback.key).c_str(),
			json_escape(callback.slot).c_str(),
			json_escape(callback.function).c_str(),
			i + 1 < timeline.callbacks.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"fog\": [\n");
	for (size_t i = 0; i < timeline.fog.size(); ++i) {
		const asb::RuntimeFog& fog = timeline.fog[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"name\": \"%s\", \"color\": [%u, %u, %u, %u]}%s\n",
			fog.frame,
			json_escape(fog.name).c_str(),
			fog.color[0],
			fog.color[1],
			fog.color[2],
			fog.color[3],
			i + 1 < timeline.fog.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"gradients\": [\n");
	for (size_t i = 0; i < timeline.gradients.size(); ++i) {
		const asb::RuntimeGradient& gradient = timeline.gradients[i];
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"index\": %d, \"rate\": %g, \"time\": %d}%s\n",
			gradient.frame,
			gradient.index,
			gradient.rate,
			gradient.time,
			i + 1 < timeline.gradients.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"ui\": [\n");
	for (size_t i = 0; i < timeline.ui.size(); ++i) {
		const asb::RuntimeUi& ui = timeline.ui[i];
		std::string ints;
		for (size_t k = 0; k < ui.ints.size(); ++k) {
			if (k)
				ints += ", ";
			ints += std::to_string(ui.ints[k]);
		}
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"kind\": \"%s\", \"text\": \"%s\", \"id\": %d, \"i0\": %d, \"i1\": %d, "
			"\"pos\": [%g, %g], \"rate\": %g, \"flag\": %s, \"ints\": [%s]}%s\n",
			ui.frame,
			json_escape(ui.kind).c_str(),
			json_escape(ui.text).c_str(),
			ui.id,
			ui.i0,
			ui.i1,
			ui.pos[0],
			ui.pos[1],
			ui.rate,
			ui.flag ? "true" : "false",
			ints.c_str(),
			i + 1 < timeline.ui.size() ? "," : "");
	}
	std::fprintf(out, "\t],\n\t\"field\": [\n");
	for (size_t i = 0; i < timeline.field.size(); ++i) {
		const asb::RuntimeField& field = timeline.field[i];
		std::string ints;
		for (size_t k = 0; k < field.ints.size(); ++k) {
			if (k)
				ints += ", ";
			ints += std::to_string(field.ints[k]);
		}
		std::fprintf(
			out,
			"\t\t{\"frame\": %ld, \"kind\": \"%s\", \"name\": \"%s\", \"text\": \"%s\", "
			"\"text2\": \"%s\", \"i0\": %d, \"i1\": %d, \"i2\": %d, \"f0\": %g, "
			"\"pos\": [%g, %g], \"flag\": %s, \"ints\": [%s]}%s\n",
			field.frame,
			json_escape(field.kind).c_str(),
			json_escape(field.name).c_str(),
			json_escape(field.text).c_str(),
			json_escape(field.text2).c_str(),
			field.i0,
			field.i1,
			field.i2,
			field.f0,
			field.pos[0],
			field.pos[1],
			field.flag ? "true" : "false",
			ints.c_str(),
			i + 1 < timeline.field.size() ? "," : "");
	}
	std::fprintf(out, "\t]\n}\n");
}

} // namespace asbrun
