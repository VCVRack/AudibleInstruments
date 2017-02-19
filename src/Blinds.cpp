#include "AudibleInstruments.hpp"
#include <string.h>


struct Blinds : Module {
	enum ParamIds {
		GAIN1_PARAM,
		GAIN2_PARAM,
		GAIN3_PARAM,
		GAIN4_PARAM,
		MOD1_PARAM,
		MOD2_PARAM,
		MOD3_PARAM,
		MOD4_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		IN1_INPUT,
		IN2_INPUT,
		IN3_INPUT,
		IN4_INPUT,
		CV1_INPUT,
		CV2_INPUT,
		CV3_INPUT,
		CV4_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		OUT3_OUTPUT,
		OUT4_OUTPUT,
		NUM_OUTPUTS
	};

	float lights[4] = {};
	float gainLights[4] = {};

	Blinds();
	void step();
};


Blinds::Blinds() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);
}

static float getChannelOutput(const float *in, float gain, const float *cv, float mod, float *light) {
	gain += mod * getf(cv) / 5.0;
	*light = gain * 5.0;
	return gain * getf(in, 5.0);
}

void Blinds::step() {
	float out = 0.0;
	out += getChannelOutput(inputs[IN1_INPUT], params[GAIN1_PARAM], inputs[CV1_INPUT], params[MOD1_PARAM], &gainLights[0]);
	lights[0] = out;
	if (outputs[OUT1_OUTPUT]) {
		*outputs[OUT1_OUTPUT] = out;
		out = 0.0;
	}

	out += getChannelOutput(inputs[IN2_INPUT], params[GAIN2_PARAM], inputs[CV2_INPUT], params[MOD2_PARAM], &gainLights[1]);
	lights[1] = out;
	if (outputs[OUT2_OUTPUT]) {
		*outputs[OUT2_OUTPUT] = out;
		out = 0.0;
	}

	out += getChannelOutput(inputs[IN3_INPUT], params[GAIN3_PARAM], inputs[CV3_INPUT], params[MOD3_PARAM], &gainLights[2]);
	lights[2] = out;
	if (outputs[OUT3_OUTPUT]) {
		*outputs[OUT3_OUTPUT] = out;
		out = 0.0;
	}

	out += getChannelOutput(inputs[IN4_INPUT], params[GAIN4_PARAM], inputs[CV4_INPUT], params[MOD4_PARAM], &gainLights[3]);
	lights[3] = out;
	if (outputs[OUT4_OUTPUT]) {
		*outputs[OUT4_OUTPUT] = out;
	}
}


BlindsWidget::BlindsWidget() {
	Blinds *module = new Blinds();
	setModule(module);
	box.size = Vec(15*12, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load("plugins/AudibleInstruments/res/Blinds.png");
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<SilverScrew>(Vec(15, 0)));
	addChild(createScrew<SilverScrew>(Vec(150, 0)));
	addChild(createScrew<SilverScrew>(Vec(15, 365)));
	addChild(createScrew<SilverScrew>(Vec(150, 365)));

	addParam(createParam<Rogan1PSWhite>(Vec(8, 52), module, Blinds::GAIN1_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(8, 131), module, Blinds::GAIN2_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(8, 210), module, Blinds::GAIN3_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(8, 288), module, Blinds::GAIN4_PARAM, -1.0, 1.0, 0.0));

	addParam(createParam<Trimpot>(Vec(72, 63), module, Blinds::MOD1_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(72, 142), module, Blinds::MOD2_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(72, 221), module, Blinds::MOD3_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(72, 300), module, Blinds::MOD4_PARAM, -1.0, 1.0, 0.0));

	addInput(createInput<PJ3410Port>(Vec(107, 38), module, Blinds::IN1_INPUT));
	addInput(createInput<PJ3410Port>(Vec(107, 117), module, Blinds::IN2_INPUT));
	addInput(createInput<PJ3410Port>(Vec(107, 195), module, Blinds::IN3_INPUT));
	addInput(createInput<PJ3410Port>(Vec(107, 274), module, Blinds::IN4_INPUT));

	addInput(createInput<PJ3410Port>(Vec(107, 77), module, Blinds::CV1_INPUT));
	addInput(createInput<PJ3410Port>(Vec(107, 156), module, Blinds::CV2_INPUT));
	addInput(createInput<PJ3410Port>(Vec(107, 235), module, Blinds::CV3_INPUT));
	addInput(createInput<PJ3410Port>(Vec(107, 313), module, Blinds::CV4_INPUT));

	addOutput(createOutput<PJ3410Port>(Vec(141, 38), module, Blinds::OUT1_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(141, 117), module, Blinds::OUT2_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(141, 195), module, Blinds::OUT3_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(141, 274), module, Blinds::OUT4_OUTPUT));

	addChild(createValueLight<MediumLight<GreenRedPolarityLight>>(Vec(150, 87), &module->lights[0]));
	addChild(createValueLight<MediumLight<GreenRedPolarityLight>>(Vec(150, 166), &module->lights[1]));
	addChild(createValueLight<MediumLight<GreenRedPolarityLight>>(Vec(150, 245), &module->lights[2]));
	addChild(createValueLight<MediumLight<GreenRedPolarityLight>>(Vec(150, 324), &module->lights[3]));

	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(77, 96), &module->gainLights[0]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(77, 175), &module->gainLights[1]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(77, 254), &module->gainLights[2]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(77, 333), &module->gainLights[3]));
}
