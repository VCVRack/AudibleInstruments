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
		float exponential = (powf(ex, linear) - 1.0) / (ex - 1.0);
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


VeilsWidget::VeilsWidget() : ModuleWidget(new Veils()) {
	box.size = Vec(15*12, 380);

	{
		AudiblePanel *panel = new AudiblePanel();
		panel->imageFilename = "plugins/AudibleInstruments/res/Veils.png";
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew(Vec(15, 0)));
	addChild(createScrew(Vec(150, 0)));
	addChild(createScrew(Vec(15, 365)));
	addChild(createScrew(Vec(150, 365)));

	addParam(createParam<SmallWhiteKnob>(Vec(8, 52), module, Veils::GAIN1_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<SmallWhiteKnob>(Vec(8, 131), module, Veils::GAIN2_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<SmallWhiteKnob>(Vec(8, 210), module, Veils::GAIN3_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<SmallWhiteKnob>(Vec(8, 288), module, Veils::GAIN4_PARAM, 0.0, 1.0, 0.0));

	addParam(createParam<TinyBlackKnob>(Vec(72, 56), module, Veils::RESPONSE1_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<TinyBlackKnob>(Vec(72, 135), module, Veils::RESPONSE2_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<TinyBlackKnob>(Vec(72, 214), module, Veils::RESPONSE3_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<TinyBlackKnob>(Vec(72, 292), module, Veils::RESPONSE4_PARAM, 0.0, 1.0, 1.0));

	addInput(createInput(Vec(112, 43), module, Veils::IN1_INPUT));
	addInput(createInput(Vec(112, 122), module, Veils::IN2_INPUT));
	addInput(createInput(Vec(112, 200), module, Veils::IN3_INPUT));
	addInput(createInput(Vec(112, 279), module, Veils::IN4_INPUT));

	addInput(createInput(Vec(112, 82), module, Veils::CV1_INPUT));
	addInput(createInput(Vec(112, 161), module, Veils::CV2_INPUT));
	addInput(createInput(Vec(112, 240), module, Veils::CV3_INPUT));
	addInput(createInput(Vec(112, 318), module, Veils::CV4_INPUT));

	addOutput(createOutput(Vec(146, 43), module, Veils::OUT1_OUTPUT));
	addOutput(createOutput(Vec(146, 122), module, Veils::OUT2_OUTPUT));
	addOutput(createOutput(Vec(146, 200), module, Veils::OUT3_OUTPUT));
	addOutput(createOutput(Vec(146, 279), module, Veils::OUT4_OUTPUT));

	Veils *veils = dynamic_cast<Veils*>(module);
	addChild(createValueLight<MediumValueLight>(Vec(149, 86), &veils->lights[0]));
	addChild(createValueLight<MediumValueLight>(Vec(149, 165), &veils->lights[1]));
	addChild(createValueLight<MediumValueLight>(Vec(149, 244), &veils->lights[2]));
	addChild(createValueLight<MediumValueLight>(Vec(149, 323), &veils->lights[3]));
}
