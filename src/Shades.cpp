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
	if (mode < 0.5) {
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
	lights[0] = out;
	if (outputs[OUT1_OUTPUT]) {
		*outputs[OUT1_OUTPUT] = out;
		out = 0.0;
	}

	out += getChannelOutput(inputs[IN2_INPUT], params[GAIN2_PARAM], params[MODE2_PARAM]);
	lights[1] = out;
	if (outputs[OUT2_OUTPUT]) {
		*outputs[OUT2_OUTPUT] = out;
		out = 0.0;
	}

	out += getChannelOutput(inputs[IN3_INPUT], params[GAIN3_PARAM], params[MODE3_PARAM]);
	lights[2] = out;
	if (outputs[OUT3_OUTPUT]) {
		*outputs[OUT3_OUTPUT] = out;
	}
}


ShadesWidget::ShadesWidget() : ModuleWidget(new Shades()) {
	box.size = Vec(15*6, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load("plugins/AudibleInstruments/res/Shades.png");
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew(Vec(15, 0)));
	addChild(createScrew(Vec(15, 365)));

	addParam(createParam<SmallRedKnob>(Vec(40, 41), module, Shades::GAIN1_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallWhiteKnob>(Vec(40, 107), module, Shades::GAIN2_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallGreenKnob>(Vec(40, 173), module, Shades::GAIN3_PARAM, 0.0, 1.0, 0.5));

	addParam(createParam<SlideSwitch>(Vec(11, 52), module, Shades::MODE1_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<SlideSwitch>(Vec(11, 118), module, Shades::MODE2_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<SlideSwitch>(Vec(11, 184), module, Shades::MODE3_PARAM, 0.0, 1.0, 0.0));

	addInput(createInput<InputPortPJ3410>(Vec(5, 242), module, Shades::IN1_INPUT));
	addInput(createInput<InputPortPJ3410>(Vec(5, 278), module, Shades::IN2_INPUT));
	addInput(createInput<InputPortPJ3410>(Vec(5, 314), module, Shades::IN3_INPUT));

	addOutput(createOutput<OutputPortPJ3410>(Vec(52, 242), module, Shades::OUT1_OUTPUT));
	addOutput(createOutput<OutputPortPJ3410>(Vec(52, 278), module, Shades::OUT2_OUTPUT));
	addOutput(createOutput<OutputPortPJ3410>(Vec(52, 314), module, Shades::OUT3_OUTPUT));

	Shades *shades = dynamic_cast<Shades*>(module);
	addChild(createValueLight<SmallValueLight>(Vec(42, 256), &shades->lights[0]));
	addChild(createValueLight<SmallValueLight>(Vec(42, 292), &shades->lights[1]));
	addChild(createValueLight<SmallValueLight>(Vec(42, 328), &shades->lights[2]));
}
