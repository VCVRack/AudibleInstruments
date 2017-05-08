#include "AudibleInstruments.hpp"
#include <string.h>


struct Shades : Module {
	enum ParamIds {
		GAIN1_PARAM,
		GAIN2_PARAM,
		GAIN3_PARAM,
		MODE1_PARAM,
		MODE2_PARAM,
		MODE3_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		IN1_INPUT,
		IN2_INPUT,
		IN3_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		OUT3_OUTPUT,
		NUM_OUTPUTS
	};

	float lights[3] = {};

	Shades();
	void step();
};


Shades::Shades() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);
}

static float getChannelOutput(const float *in, float gain, float mode) {
	float out = getf(in, 5.0);
	if ((int)roundf(mode) == 1) {
		// attenuverter
		out *= 2.0*gain - 1.0;
	}
	else {
		// attenuator
		out *= gain;
	}
	return out;
}

void Shades::step() {
	float out = 0.0;
	out += getChannelOutput(inputs[IN1_INPUT], params[GAIN1_PARAM], params[MODE1_PARAM]);
	lights[0] = out / 5.0;
	if (outputs[OUT1_OUTPUT]) {
		*outputs[OUT1_OUTPUT] = out;
		out = 0.0;
	}

	out += getChannelOutput(inputs[IN2_INPUT], params[GAIN2_PARAM], params[MODE2_PARAM]);
	lights[1] = out / 5.0;
	if (outputs[OUT2_OUTPUT]) {
		*outputs[OUT2_OUTPUT] = out;
		out = 0.0;
	}

	out += getChannelOutput(inputs[IN3_INPUT], params[GAIN3_PARAM], params[MODE3_PARAM]);
	lights[2] = out / 5.0;
	if (outputs[OUT3_OUTPUT]) {
		*outputs[OUT3_OUTPUT] = out;
	}
}


ShadesWidget::ShadesWidget() {
	Shades *module = new Shades();
	setModule(module);
	box.size = Vec(15*6, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load("plugins/AudibleInstruments/res/Shades.png");
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));

	addParam(createParam<Rogan1PSRed>(Vec(40, 41), module, Shades::GAIN1_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSWhite>(Vec(40, 107), module, Shades::GAIN2_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSGreen>(Vec(40, 173), module, Shades::GAIN3_PARAM, 0.0, 1.0, 0.5));

	addParam(createParam<CKSS>(Vec(10, 52), module, Shades::MODE1_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<CKSS>(Vec(10, 118), module, Shades::MODE2_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<CKSS>(Vec(10, 184), module, Shades::MODE3_PARAM, 0.0, 1.0, 1.0));

	addInput(createInput<PJ3410Port>(Vec(5, 242), module, Shades::IN1_INPUT));
	addInput(createInput<PJ3410Port>(Vec(5, 278), module, Shades::IN2_INPUT));
	addInput(createInput<PJ3410Port>(Vec(5, 314), module, Shades::IN3_INPUT));

	addOutput(createOutput<PJ3410Port>(Vec(52, 242), module, Shades::OUT1_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(52, 278), module, Shades::OUT2_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(52, 314), module, Shades::OUT3_OUTPUT));

	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(41, 254), &module->lights[0]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(41, 290), &module->lights[1]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(41, 326), &module->lights[2]));
}
