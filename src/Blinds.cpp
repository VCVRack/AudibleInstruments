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


BlindsWidget::BlindsWidget() : ModuleWidget(new Blinds()) {
	box.size = Vec(15*12, 380);

	{
		AudiblePanel *panel = new AudiblePanel();
		panel->imageFilename = "plugins/AudibleInstruments/res/Blinds.png";
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew(Vec(15, 0)));
	addChild(createScrew(Vec(150, 0)));
	addChild(createScrew(Vec(15, 365)));
	addChild(createScrew(Vec(150, 365)));

	addParam(createParam<SmallWhiteKnob>(Vec(8, 52), module, Blinds::GAIN1_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<SmallWhiteKnob>(Vec(8, 131), module, Blinds::GAIN2_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<SmallWhiteKnob>(Vec(8, 210), module, Blinds::GAIN3_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<SmallWhiteKnob>(Vec(8, 288), module, Blinds::GAIN4_PARAM, -1.0, 1.0, 0.0));

	addParam(createParam<TinyBlackKnob>(Vec(72, 63), module, Blinds::MOD1_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(72, 142), module, Blinds::MOD2_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(72, 221), module, Blinds::MOD3_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(72, 300), module, Blinds::MOD4_PARAM, -1.0, 1.0, 0.0));

	addInput(createInput(Vec(112, 43), module, Blinds::IN1_INPUT));
	addInput(createInput(Vec(112, 122), module, Blinds::IN2_INPUT));
	addInput(createInput(Vec(112, 200), module, Blinds::IN3_INPUT));
	addInput(createInput(Vec(112, 279), module, Blinds::IN4_INPUT));

	addInput(createInput(Vec(112, 82), module, Blinds::CV1_INPUT));
	addInput(createInput(Vec(112, 161), module, Blinds::CV2_INPUT));
	addInput(createInput(Vec(112, 240), module, Blinds::CV3_INPUT));
	addInput(createInput(Vec(112, 318), module, Blinds::CV4_INPUT));

	addOutput(createOutput(Vec(146, 43), module, Blinds::OUT1_OUTPUT));
	addOutput(createOutput(Vec(146, 122), module, Blinds::OUT2_OUTPUT));
	addOutput(createOutput(Vec(146, 200), module, Blinds::OUT3_OUTPUT));
	addOutput(createOutput(Vec(146, 279), module, Blinds::OUT4_OUTPUT));

	Blinds *blinds = dynamic_cast<Blinds*>(module);
	addChild(createValueLight<MediumValueLight>(Vec(149, 86), &blinds->lights[0]));
	addChild(createValueLight<MediumValueLight>(Vec(149, 165), &blinds->lights[1]));
	addChild(createValueLight<MediumValueLight>(Vec(149, 244), &blinds->lights[2]));
	addChild(createValueLight<MediumValueLight>(Vec(149, 323), &blinds->lights[3]));

	addChild(createValueLight<SmallValueLight>(Vec(78, 97), &blinds->gainLights[0]));
	addChild(createValueLight<SmallValueLight>(Vec(78, 176), &blinds->gainLights[1]));
	addChild(createValueLight<SmallValueLight>(Vec(78, 255), &blinds->gainLights[2]));
	addChild(createValueLight<SmallValueLight>(Vec(78, 334), &blinds->gainLights[3]));
}
