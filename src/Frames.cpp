#include "AudibleInstruments.hpp"
#include <string.h>


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

	float lights[1] = {};

	Frames();
	void step();
};


Frames::Frames() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);
}

void Frames::step() {
}


struct FramesLight : ValueLight {
	FramesLight() {
		box.size = Vec(67, 67);
	}

	void step() {
		// TODO Set these to Frames' actual colors
		static NVGcolor colors[9] = {
			nvgHSL(0.5, 0.3, 0.85),
			nvgHSL(0.6, 0.3, 0.85),
			nvgHSL(0.7, 0.3, 0.85),
			nvgHSL(0.8, 0.3, 0.85),
			nvgHSL(0.9, 0.3, 0.85),
			nvgHSL(0.0, 0.3, 0.85),
			nvgHSL(0.1, 0.3, 0.85),
			nvgHSL(0.2, 0.3, 0.85),
			nvgHSL(0.3, 0.3, 0.85),
		};
		int i = clampi((int) *value, 0, 7);
		NVGcolor color0 = colors[i];
		NVGcolor color1 = colors[i + 1];
		float p = fmodf(*value, 1.0);
		color = nvgLerpRGBA(color0, color1, p);
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
	addParam(createParam<Rogan6PSWhite>(Vec(90, 117), module, Frames::FRAME_PARAM, 0.0, 63.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(208, 141), module, Frames::MODULATION_PARAM, 0.0, 1.0, 0.5));

	addInput(createInput<PJ3410Port>(Vec(12, 269), module, Frames::ALL_INPUT));
	addInput(createInput<PJ3410Port>(Vec(55, 269), module, Frames::IN1_INPUT));
	addInput(createInput<PJ3410Port>(Vec(98, 269), module, Frames::IN2_INPUT));
	addInput(createInput<PJ3410Port>(Vec(141, 269), module, Frames::IN3_INPUT));
	addInput(createInput<PJ3410Port>(Vec(184, 269), module, Frames::IN4_INPUT));
	addInput(createInput<PJ3410Port>(Vec(227, 269), module, Frames::FRAME_INPUT));

	addOutput(createOutput<PJ3410Port>(Vec(12, 312), module, Frames::MIX_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(55, 312), module, Frames::OUT1_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(98, 312), module, Frames::OUT2_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(141, 312), module, Frames::OUT3_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(184, 312), module, Frames::OUT4_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(227, 312), module, Frames::FRAME_STEP_OUTPUT));

	addChild(createValueLight<FramesLight>(Vec(101, 128), &module->lights[0]));
}
