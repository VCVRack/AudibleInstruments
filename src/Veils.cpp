#include "AudibleInstruments.hpp"
#include <string.h>


struct Veils : Module {
	enum ParamIds {
		GAIN1_PARAM,
		GAIN2_PARAM,
		GAIN3_PARAM,
		GAIN4_PARAM,
		RESPONSE1_PARAM,
		RESPONSE2_PARAM,
		RESPONSE3_PARAM,
		RESPONSE4_PARAM,
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

	Veils();
	void step();
};


Veils::Veils() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);
}

static float getChannelOutput(float *in, float gain, float *cv, float response) {
	float out = getf(in) * gain;
	if (out == 0.0)
		return 0.0;

	if (cv) {
		float linear = fmaxf(getf(cv) / 5.0, 0.0);
		if (linear == 0.0)
			return 0.0;
		const float ex = 200.0;
		float exponential = rescalef(powf(ex, linear), 1.0, ex, 0.0, 1.0);
		out *= crossf(exponential, linear, response);
	}
	return out;
}

void Veils::step() {
	float out = 0.0;
	out += getChannelOutput(inputs[IN1_INPUT], params[GAIN1_PARAM], inputs[CV1_INPUT], params[RESPONSE1_PARAM]);
	lights[0] = out;
	if (outputs[OUT1_OUTPUT]) {
		*outputs[OUT1_OUTPUT] = out;
		out = 0.0;
	}

	out += getChannelOutput(inputs[IN2_INPUT], params[GAIN2_PARAM], inputs[CV2_INPUT], params[RESPONSE2_PARAM]);
	lights[1] = out;
	if (outputs[OUT2_OUTPUT]) {
		*outputs[OUT2_OUTPUT] = out;
		out = 0.0;
	}

	out += getChannelOutput(inputs[IN3_INPUT], params[GAIN3_PARAM], inputs[CV3_INPUT], params[RESPONSE3_PARAM]);
	lights[2] = out;
	if (outputs[OUT3_OUTPUT]) {
		*outputs[OUT3_OUTPUT] = out;
		out = 0.0;
	}

	out += getChannelOutput(inputs[IN4_INPUT], params[GAIN4_PARAM], inputs[CV4_INPUT], params[RESPONSE4_PARAM]);
	lights[3] = out;
	if (outputs[OUT4_OUTPUT]) {
		*outputs[OUT4_OUTPUT] = out;
	}
}


VeilsWidget::VeilsWidget() {
	Veils *module = new Veils();
	setModule(module);
	box.size = Vec(15*12, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load(assetPlugin(plugin, "res/Veils.png"));
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(150, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(150, 365)));

	addParam(createParam<Rogan1PSWhite>(Vec(8, 52), module, Veils::GAIN1_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(8, 131), module, Veils::GAIN2_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(8, 210), module, Veils::GAIN3_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(8, 288), module, Veils::GAIN4_PARAM, 0.0, 1.0, 0.0));

	addParam(createParam<Trimpot>(Vec(72, 56), module, Veils::RESPONSE1_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<Trimpot>(Vec(72, 135), module, Veils::RESPONSE2_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<Trimpot>(Vec(72, 214), module, Veils::RESPONSE3_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<Trimpot>(Vec(72, 292), module, Veils::RESPONSE4_PARAM, 0.0, 1.0, 1.0));

	addInput(createInput<PJ3410Port>(Vec(107, 38), module, Veils::IN1_INPUT));
	addInput(createInput<PJ3410Port>(Vec(107, 117), module, Veils::IN2_INPUT));
	addInput(createInput<PJ3410Port>(Vec(107, 195), module, Veils::IN3_INPUT));
	addInput(createInput<PJ3410Port>(Vec(107, 274), module, Veils::IN4_INPUT));

	addInput(createInput<PJ3410Port>(Vec(107, 77), module, Veils::CV1_INPUT));
	addInput(createInput<PJ3410Port>(Vec(107, 156), module, Veils::CV2_INPUT));
	addInput(createInput<PJ3410Port>(Vec(107, 235), module, Veils::CV3_INPUT));
	addInput(createInput<PJ3410Port>(Vec(107, 313), module, Veils::CV4_INPUT));

	addOutput(createOutput<PJ3410Port>(Vec(141, 38), module, Veils::OUT1_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(141, 117), module, Veils::OUT2_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(141, 195), module, Veils::OUT3_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(141, 274), module, Veils::OUT4_OUTPUT));

	addChild(createValueLight<MediumLight<GreenRedPolarityLight>>(Vec(150, 87), &module->lights[0]));
	addChild(createValueLight<MediumLight<GreenRedPolarityLight>>(Vec(150, 166), &module->lights[1]));
	addChild(createValueLight<MediumLight<GreenRedPolarityLight>>(Vec(150, 245), &module->lights[2]));
	addChild(createValueLight<MediumLight<GreenRedPolarityLight>>(Vec(150, 324), &module->lights[3]));
}
