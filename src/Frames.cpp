#include "AudibleInstruments.hpp"
#include <string.h>
#include "frames/keyframer.h"
#include "frames/poly_lfo.h"
#include "dsp/digital.hpp"


struct Frames : Module {
	enum ParamIds {
		GAIN1_PARAM,
		GAIN2_PARAM,
		GAIN3_PARAM,
		GAIN4_PARAM,
		ADD_PARAM,
		DEL_PARAM,
		FRAME_PARAM,
		MODULATION_PARAM,
		OFFSET_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ALL_INPUT,
		IN1_INPUT,
		IN2_INPUT,
		IN3_INPUT,
		IN4_INPUT,
		FRAME_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MIX_OUTPUT,
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		OUT3_OUTPUT,
		OUT4_OUTPUT,
		FRAME_STEP_OUTPUT,
		GAIN1_LIGHT,
		EDIT_LIGHT = GAIN1_LIGHT + 4,
		FRAME_LIGHT,
		NUM_OUTPUTS = FRAME_LIGHT + 3
	};

	frames::Keyframer keyframer;
	frames::PolyLfo poly_lfo;
	bool poly_lfo_mode = false;
	uint16_t lastControls[4] = {};

	SchmittTrigger addTrigger;
	SchmittTrigger delTrigger;
	bool clearKeyframes = false;

	Frames();
	void step() override;

	json_t *toJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "polyLfo", json_boolean(poly_lfo_mode));

		json_t *keyframesJ = json_array();
		for (int i = 0; i < keyframer.num_keyframes(); i++) {
			json_t *keyframeJ = json_array();
			frames::Keyframe *keyframe = keyframer.mutable_keyframe(i);
			json_array_append_new(keyframeJ, json_integer(keyframe->timestamp));
			for (int k = 0; k < 4; k++) {
				json_array_append_new(keyframeJ, json_integer(keyframe->values[k]));
			}
			json_array_append_new(keyframesJ, keyframeJ);
		}
		json_object_set_new(rootJ, "keyframes", keyframesJ);

		// TODO Channel settings

		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		json_t *polyLfoJ = json_object_get(rootJ, "polyLfo");
		if (polyLfoJ)
			poly_lfo_mode = json_boolean_value(polyLfoJ);

		json_t *keyframesJ = json_object_get(rootJ, "keyframes");
		if (keyframesJ) {
			json_t *keyframeJ;
			size_t i;
			json_array_foreach(keyframesJ, i, keyframeJ) {
				uint16_t timestamp = json_integer_value(json_array_get(keyframeJ, 0));
				uint16_t values[4];
				for (int k = 0; k < 4; k++) {
					values[k] = json_integer_value(json_array_get(keyframeJ, k + 1));
				}
				keyframer.AddKeyframe(timestamp, values);
			}
		}

		// TODO Channel settings
	}

	void initialize() override {
		poly_lfo_mode = false;
		keyframer.Clear();
	}
	void randomize() override {
		// TODO
		// Maybe something useful should go in here??
	}
};


Frames::Frames() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {
	memset(&keyframer, 0, sizeof(keyframer));
	keyframer.Init();
	memset(&poly_lfo, 0, sizeof(poly_lfo));
	poly_lfo.Init();

	for (int i = 0; i < 4; i++) {
		keyframer.mutable_settings(i)->easing_curve = frames::EASING_CURVE_LINEAR;
	}
}


void Frames::step() {
	// Set gain and timestamp knobs
	uint16_t controls[4];
	for (int i = 0; i < 4; i++) {
		controls[i] = params[GAIN1_PARAM + i].value * 65535.0;
	}

	int32_t timestamp = params[FRAME_PARAM].value * 65535.0;
	int32_t timestampMod = timestamp + params[MODULATION_PARAM].value * inputs[FRAME_INPUT].value / 10.0 * 65535.0;
	timestamp = clampi(timestamp, 0, 65535);
	timestampMod = clampi(timestampMod, 0, 65535);
	int16_t nearestIndex = -1;
	if (!poly_lfo_mode) {
		nearestIndex = keyframer.FindNearestKeyframe(timestamp, 2048);
	}

	// Render, handle buttons
	if (poly_lfo_mode) {
		if (controls[0] != lastControls[0])
			poly_lfo.set_shape(controls[0]);
		if (controls[1] != lastControls[1])
			poly_lfo.set_shape_spread(controls[1]);
		if (controls[2] != lastControls[2])
			poly_lfo.set_spread(controls[2]);
		if (controls[3] != lastControls[3])
			poly_lfo.set_coupling(controls[3]);
		poly_lfo.Render(timestampMod);
	}
	else {
		for (int i = 0; i < 4; i++) {
			if (controls[i] != lastControls[i]) {
				// Update recently moved control
				if (keyframer.num_keyframes() == 0) {
					keyframer.set_immediate(i, controls[i]);
				}
				if (nearestIndex >= 0) {
					frames::Keyframe *nearestKeyframe = keyframer.mutable_keyframe(nearestIndex);
					nearestKeyframe->values[i] = controls[i];
				}
			}
		}

		if (addTrigger.process(params[ADD_PARAM].value)) {
			if (nearestIndex < 0) {
				keyframer.AddKeyframe(timestamp, controls);
			}
		}
		if (delTrigger.process(params[DEL_PARAM].value)) {
			if (nearestIndex >= 0) {
				int32_t nearestTimestamp = keyframer.keyframe(nearestIndex).timestamp;
				keyframer.RemoveKeyframe(nearestTimestamp);
			}
		}
		keyframer.Evaluate(timestampMod);
	}

	// Get gains
	float gains[4];
	for (int i = 0; i < 4; i++) {
		if (poly_lfo_mode) {
			// gains[i] = poly_lfo.level(i) / 255.0;
			gains[i] = poly_lfo.level16(i) / 65535.0;
		}
		else {
			float lin = keyframer.level(i) / 65535.0;
			gains[i] = lin;
		}
	}

	// Update last controls
	for (int i = 0; i < 4; i++) {
		lastControls[i] = controls[i];
	}

	// Get inputs
	float all = ((int)params[OFFSET_PARAM].value == 1) ? 10.0 : 0.0;
	if (inputs[ALL_INPUT].active) {
		all = inputs[ALL_INPUT].value;
	}

	float ins[4];
	for (int i = 0; i < 4; i++) {
		ins[i] = inputs[IN1_INPUT + i].normalize(all) * gains[i];
	}

	// Set outputs
	float mix = 0.0;

	for (int i = 0; i < 4; i++) {
		if (outputs[OUT1_OUTPUT + i].active) {
			outputs[OUT1_OUTPUT + i].value = ins[i];
		}
		else {
			mix += ins[i];
		}
	}

	outputs[MIX_OUTPUT].value = clampf(mix / 2.0, -10.0, 10.0);

	// Set lights
	for (int i = 0; i < 4; i++) {
		outputs[GAIN1_LIGHT + i].value = gains[i];
	}

	if (poly_lfo_mode) {
		outputs[EDIT_LIGHT].value = (poly_lfo.level(0) > 128 ? 1.0 : 0.0);
	}
	else {
		// TODO
		outputs[EDIT_LIGHT].value = (nearestIndex >= 0 ? 1.0 : 0.0);
	}

	// Set frame light colors
	const uint8_t *colors;
	if (poly_lfo_mode) {
		colors = poly_lfo.color();
	}
	else {
		colors = keyframer.color();
	}
	for (int c = 0; c < 3; c++) {
		outputs[FRAME_LIGHT + c].value = colors[c] / 255.0;
	}
}


struct FramesLight : Light {
	float *colors[3];
	FramesLight() {
		box.size = Vec(67, 67);
	}
	void step() override {
		const NVGcolor red = COLOR_RED;
		const NVGcolor green = COLOR_GREEN;
		const NVGcolor blue = COLOR_BLUE;
		float r = *colors[0];
		float g = *colors[1];
		float b = *colors[2];
		color.r = red.r * r + green.r * g + blue.r * b;
		color.g = red.g * r + green.g * g + blue.g * b;
		color.b = red.b * r + green.b * g + blue.b * b;
		color.a = 1.0;
		// Lighten
		color = nvgLerpRGBA(color, COLOR_WHITE, 0.5);
	}
};

struct CKSSRot : SVGSwitch, ToggleSwitch {
	CKSSRot() {
		addFrame(SVG::load(assetPlugin(plugin, "res/CKSS_rot_0.svg")));
		addFrame(SVG::load(assetPlugin(plugin, "res/CKSS_rot_1.svg")));
		sw->wrap();
		box.size = sw->box.size;
	}
};


FramesWidget::FramesWidget() {
	Frames *module = new Frames();
	setModule(module);
	box.size = Vec(15*18, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load(assetPlugin(plugin, "res/Frames.png"));
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<Rogan1PSWhite>(Vec(14, 52), module, Frames::GAIN1_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(81, 52), module, Frames::GAIN2_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(149, 52), module, Frames::GAIN3_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(216, 52), module, Frames::GAIN4_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Rogan6PSWhite>(Vec(91, 117), module, Frames::FRAME_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSGreen>(Vec(208, 141), module, Frames::MODULATION_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<CKD6>(Vec(19, 123), module, Frames::ADD_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<CKD6>(Vec(19, 172), module, Frames::DEL_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<CKSSRot>(Vec(18, 239), module, Frames::OFFSET_PARAM, 0.0, 1.0, 0.0));

	addInput(createInput<PJ3410Port>(Vec(12, 271), module, Frames::ALL_INPUT));
	addInput(createInput<PJ3410Port>(Vec(55, 271), module, Frames::IN1_INPUT));
	addInput(createInput<PJ3410Port>(Vec(98, 271), module, Frames::IN2_INPUT));
	addInput(createInput<PJ3410Port>(Vec(141, 271), module, Frames::IN3_INPUT));
	addInput(createInput<PJ3410Port>(Vec(184, 271), module, Frames::IN4_INPUT));
	addInput(createInput<PJ3410Port>(Vec(227, 271), module, Frames::FRAME_INPUT));

	addOutput(createOutput<PJ3410Port>(Vec(12, 313), module, Frames::MIX_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(55, 313), module, Frames::OUT1_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(98, 313), module, Frames::OUT2_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(141, 313), module, Frames::OUT3_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(184, 313), module, Frames::OUT4_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(227, 313), module, Frames::FRAME_STEP_OUTPUT));

	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(30, 101), &module->outputs[Frames::GAIN1_LIGHT + 0].value));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(97, 101), &module->outputs[Frames::GAIN1_LIGHT + 1].value));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(165, 101), &module->outputs[Frames::GAIN1_LIGHT + 2].value));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(232, 101), &module->outputs[Frames::GAIN1_LIGHT + 3].value));
	addChild(createValueLight<MediumLight<GreenRedPolarityLight>>(Vec(61, 155), &module->outputs[Frames::EDIT_LIGHT].value));
	{
		FramesLight *framesLight = new FramesLight();
		framesLight->box.pos = Vec(102, 128);
		framesLight->colors[0] = &module->outputs[Frames::FRAME_LIGHT + 0].value;
		framesLight->colors[1] = &module->outputs[Frames::FRAME_LIGHT + 1].value;
		framesLight->colors[2] = &module->outputs[Frames::FRAME_LIGHT + 2].value;
		addChild(framesLight);
	}
}


struct FramesCurveItem : MenuItem {
	Frames *frames;
	uint8_t channel;
	frames::EasingCurve curve;
	void onAction() override {
		frames->keyframer.mutable_settings(channel)->easing_curve = curve;
	}
	void step() override {
		rightText = (frames->keyframer.mutable_settings(channel)->easing_curve == curve) ? "✔" : "";
	}
};

struct FramesResponseItem : MenuItem {
	Frames *frames;
	uint8_t channel;
	uint8_t response;
	void onAction() override {
		frames->keyframer.mutable_settings(channel)->response = response;
	}
	void step() override {
		rightText = (frames->keyframer.mutable_settings(channel)->response = response) ? "✔" : "";
	}
};

struct FramesChannelSettingsItem : MenuItem {
	Frames *frames;
	uint8_t channel;
	Menu *createChildMenu() override {
		Menu *menu = new Menu();

		// TODO
		menu->pushChild(construct<MenuLabel>(&MenuEntry::text, "Interpolation Curve"));
		menu->pushChild(construct<FramesCurveItem>(&MenuEntry::text, "Step", &FramesCurveItem::frames, frames, &FramesCurveItem::channel, channel, &FramesCurveItem::curve, frames::EASING_CURVE_STEP));
		menu->pushChild(construct<FramesCurveItem>(&MenuEntry::text, "Linear", &FramesCurveItem::frames, frames, &FramesCurveItem::channel, channel, &FramesCurveItem::curve, frames::EASING_CURVE_LINEAR));
		menu->pushChild(construct<FramesCurveItem>(&MenuEntry::text, "Accelerating", &FramesCurveItem::frames, frames, &FramesCurveItem::channel, channel, &FramesCurveItem::curve, frames::EASING_CURVE_IN_QUARTIC));
		menu->pushChild(construct<FramesCurveItem>(&MenuEntry::text, "Decelerating", &FramesCurveItem::frames, frames, &FramesCurveItem::channel, channel, &FramesCurveItem::curve, frames::EASING_CURVE_OUT_QUARTIC));
		menu->pushChild(construct<FramesCurveItem>(&MenuEntry::text, "Smooth Departure/Arrival", &FramesCurveItem::frames, frames, &FramesCurveItem::channel, channel, &FramesCurveItem::curve, frames::EASING_CURVE_SINE));
		menu->pushChild(construct<FramesCurveItem>(&MenuEntry::text, "Bouncing", &FramesCurveItem::frames, frames, &FramesCurveItem::channel, channel, &FramesCurveItem::curve, frames::EASING_CURVE_BOUNCE));
		menu->pushChild(construct<MenuLabel>());
		menu->pushChild(construct<MenuLabel>(&MenuEntry::text, "Response Curve"));
		menu->pushChild(construct<FramesResponseItem>(&MenuEntry::text, "Linear", &FramesResponseItem::frames, frames, &FramesResponseItem::channel, channel, &FramesResponseItem::response, 0));
		menu->pushChild(construct<FramesResponseItem>(&MenuEntry::text, "Exponential", &FramesResponseItem::frames, frames, &FramesResponseItem::channel, channel, &FramesResponseItem::response, 1));

		return menu;
	}
};

struct FramesClearItem : MenuItem {
	Frames *frames;
	void onAction() override {
		frames->keyframer.Clear();
	}
};

struct FramesModeItem : MenuItem {
	Frames *frames;
	bool poly_lfo_mode;
	void onAction() override {
		frames->poly_lfo_mode = poly_lfo_mode;
	}
	void step() override {
		rightText = (frames->poly_lfo_mode == poly_lfo_mode) ? "✔" : "";
	}
};


Menu *FramesWidget::createContextMenu() {
	Menu *menu = ModuleWidget::createContextMenu();

	Frames *frames = dynamic_cast<Frames*>(module);
	assert(frames);

	menu->pushChild(construct<MenuLabel>());
	menu->pushChild(construct<MenuLabel>(&MenuEntry::text, "Channel Settings"));
	for (int i = 0; i < 4; i++) {
		menu->pushChild(construct<FramesChannelSettingsItem>(&MenuItem::text, stringf("Channel %d", i + 1), &FramesChannelSettingsItem::frames, frames, &FramesChannelSettingsItem::channel, i));
	}
	menu->pushChild(construct<FramesClearItem>(&MenuItem::text, "Clear Keyframes", &FramesClearItem::frames, frames));

	menu->pushChild(construct<MenuLabel>());
	menu->pushChild(construct<MenuLabel>(&MenuEntry::text, "Mode"));
	menu->pushChild(construct<FramesModeItem>(&MenuItem::text, "Keyframer", &FramesModeItem::frames, frames, &FramesModeItem::poly_lfo_mode, false));
	menu->pushChild(construct<FramesModeItem>(&MenuItem::text, "Poly LFO", &FramesModeItem::frames, frames, &FramesModeItem::poly_lfo_mode, true));

	return menu;
}
