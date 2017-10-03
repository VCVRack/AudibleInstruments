#include "AudibleInstruments.hpp"
#include <string.h>
#include "frames/keyframer.h"


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
		NUM_OUTPUTS
	};

	float lights[6] = {};
	float color[3] = {};
	frames::Keyframer keyframer;
	SchmittTrigger addTrigger;
	SchmittTrigger delTrigger;
	bool clearKeyframes = false;

	Frames();
	void step();
};


Frames::Frames() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {
	memset(&keyframer, 0, sizeof(keyframer));
	keyframer.Init();
}


void Frames::step() {
	if (clearKeyframes) {
		keyframer.Clear();
		clearKeyframes = false;
	}

	for (int i = 0; i < 4; i++) {
		keyframer.set_immediate(i, params[GAIN1_PARAM + i].value * 65535.0);
	}

	int32_t frame = (params[FRAME_PARAM].value + inputs[FRAME_INPUT].value) * 65535.0;
	frame = clampi(frame, 0, 65535);
	int16_t nearestFrame = keyframer.FindNearestKeyframe(frame, 2048);
	printf("%d\n", nearestFrame);

	if (addTrigger.process(params[ADD_PARAM].value)) {
		uint16_t f[4] = {65000, 30000, 20000, 10000};
		keyframer.AddKeyframe(frame, f);
	}
	if (delTrigger.process(params[DEL_PARAM].value)) {
		keyframer.RemoveKeyframe(frame);
	}
	keyframer.Evaluate(frame);

	float gains[4];
	for (int i = 0; i < 4; i++) {
		float lin = keyframer.level(i) / 65535.0;
		gains[i] = lin;
	}
	printf("%f %f %f %f\n", gains[0], gains[1], gains[2], gains[3]);

	// Get inputs
	float all = ((int)params[OFFSET_PARAM].value == 1) ? 10.0 : 0.0;
	if (inputs[ALL_INPUT].active)
		all = inputs[ALL_INPUT].value;

	// TODO multiply by correct gains
	float ins[4];
	for (int i = 0; i < 4; i++)
		ins[i] = inputs[IN1_INPUT + i].normalize(all) * gains[i];

	// Set outputs
	float mix = 0.0;

	for (int i = 0; i < 4; i++) {
		if (outputs[OUT1_OUTPUT + i].active)
			outputs[OUT1_OUTPUT + i].value = ins[i];
		else
			mix += ins[i];
	}

	outputs[MIX_OUTPUT].value = clampf(mix / 2.0, -10.0, 10.0);

	for (int i = 0; i < 4; i++)
		lights[i] = gains[i];

	for (int c = 0; c < 3; c++)
		color[c] = keyframer.color(c) / 255.0;
}


struct FramesLight : ValueLight {
	FramesLight() {
		box.size = Vec(67, 67);
	}
	void step() {
		color = nvgRGBf(
			clampf(value[0] * 4, 0.0, 1.0),
			clampf(value[1] * 4, 0.0, 1.0),
			clampf(value[2] * 4, 0.0, 1.0));
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

	addParam(createParam<Rogan1PSWhite>(Vec(14, 52), module, Frames::GAIN1_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSWhite>(Vec(81, 52), module, Frames::GAIN2_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSWhite>(Vec(149, 52), module, Frames::GAIN3_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSWhite>(Vec(216, 52), module, Frames::GAIN4_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan6PSWhite>(Vec(91, 117), module, Frames::FRAME_PARAM, 0.0, 63.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(208, 141), module, Frames::MODULATION_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<CKD6>(Vec(19, 123), module, Frames::ADD_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<CKD6>(Vec(19, 172), module, Frames::DEL_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<CKSS>(Vec(21, 235), module, Frames::OFFSET_PARAM, 0.0, 1.0, 0.0));

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

	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(30, 101), &module->lights[0]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(97, 101), &module->lights[1]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(165, 101), &module->lights[2]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(232, 101), &module->lights[3]));
	addChild(createValueLight<MediumLight<GreenRedPolarityLight>>(Vec(61, 155), &module->lights[4]));
	addChild(createValueLight<FramesLight>(Vec(102, 128), module->color));
}


struct FramesChannelSettingsItem : MenuItem {
	Frames *frames;
	int channel;
	Menu *createChildMenu() {
		Menu *menu = new Menu();

		// TODO
		menu->pushChild(construct<MenuLabel>(&MenuEntry::text, "Interpolation Curve"));
		menu->pushChild(construct<MenuItem>(&MenuEntry::text, "Step"));
		menu->pushChild(construct<MenuItem>(&MenuEntry::text, "Linear"));
		menu->pushChild(construct<MenuItem>(&MenuEntry::text, "Accelerating"));
		menu->pushChild(construct<MenuItem>(&MenuEntry::text, "Decelerating"));
		menu->pushChild(construct<MenuItem>(&MenuEntry::text, "Smooth Departure/Arrival"));
		menu->pushChild(construct<MenuItem>(&MenuEntry::text, "Bouncing"));
		menu->pushChild(construct<MenuLabel>());
		menu->pushChild(construct<MenuLabel>(&MenuEntry::text, "Response Curve"));
		menu->pushChild(construct<MenuItem>(&MenuEntry::text, "Linear"));
		menu->pushChild(construct<MenuItem>(&MenuEntry::text, "Exponential"));

		return menu;
	}
};

struct FramesClearItem : MenuItem {
	Frames *frames;
	void onAction() {
		frames->clearKeyframes = true;
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

	return menu;
}
