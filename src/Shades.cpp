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

	Shades() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step() override;
};


static float getChannelOutput(Input &in, Param &gain, Param &mode) {
	float out = in.normalize(5.0);
	if ((int)roundf(mode.value) == 1) {
		// attenuverter
		out *= 2.0*gain.value - 1.0;
	}
	else {
		// attenuator
		out *= gain.value;
	}
	return out;
}

void Shades::step() {
	float out = 0.0;
	out += getChannelOutput(inputs[IN1_INPUT], params[GAIN1_PARAM], params[MODE1_PARAM]);
	lights[0] = out / 5.0;
	if (outputs[OUT1_OUTPUT].active) {
		outputs[OUT1_OUTPUT].value = out;
		out = 0.0;
	}

	out += getChannelOutput(inputs[IN2_INPUT], params[GAIN2_PARAM], params[MODE2_PARAM]);
	lights[1] = out / 5.0;
	if (outputs[OUT2_OUTPUT].active) {
		outputs[OUT2_OUTPUT].value = out;
		out = 0.0;
	}

	out += getChannelOutput(inputs[IN3_INPUT], params[GAIN3_PARAM], params[MODE3_PARAM]);
	lights[2] = out / 5.0;
	if (outputs[OUT3_OUTPUT].active) {
		outputs[OUT3_OUTPUT].value = out;
	}
}


ShadesWidget::ShadesWidget() {
	Shades *module = new Shades();
	setModule(module);
	box.size = Vec(15*6, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load(assetPlugin(plugin, "res/Shades.png"));
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));

	addParam(createParam<Rogan1PSRed>(Vec(40, 41), module, Shades::GAIN1_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(40, 107), module, Shades::GAIN2_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSGreen>(Vec(40, 173), module, Shades::GAIN3_PARAM, 0.0, 1.0, 0.0));

	addParam(createParam<CKSS>(Vec(10, 52), module, Shades::MODE1_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<CKSS>(Vec(10, 118), module, Shades::MODE2_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<CKSS>(Vec(10, 184), module, Shades::MODE3_PARAM, 0.0, 1.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(9, 245), module, Shades::IN1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(9, 281), module, Shades::IN2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(9, 317), module, Shades::IN3_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(56, 245), module, Shades::OUT1_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(56, 281), module, Shades::OUT2_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(56, 317), module, Shades::OUT3_OUTPUT));

	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(41, 254), &module->lights[0]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(41, 290), &module->lights[1]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(41, 326), &module->lights[2]));
}
