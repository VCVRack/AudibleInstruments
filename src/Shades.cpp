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
	enum LightIds {
		OUT1_POS_LIGHT, OUT1_NEG_LIGHT,
		OUT2_POS_LIGHT, OUT2_NEG_LIGHT,
		OUT3_POS_LIGHT, OUT3_NEG_LIGHT,
		NUM_LIGHTS
	};

	Shades() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;
};


void Shades::step() {
	float out = 0.0;

	for (int i = 0; i < 3; i++) {
		float in = inputs[IN1_INPUT + i].normalize(5.0);
		if ((int)params[MODE1_PARAM + i].value == 1) {
			// attenuverter
			in *= 2.0 * params[GAIN1_PARAM + i].value - 1.0;
		}
		else {
			// attenuator
			in *= params[GAIN1_PARAM + i].value;
		}
		out += in;
		lights[OUT1_POS_LIGHT + 2*i].setBrightnessSmooth(fmaxf(0.0, out / 5.0));
		lights[OUT1_NEG_LIGHT + 2*i].setBrightnessSmooth(fmaxf(0.0, -out / 5.0));
		if (outputs[OUT1_OUTPUT + i].active) {
			outputs[OUT1_OUTPUT + i].value = out;
			out = 0.0;
		}
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

	addParam(createParam<Rogan1PSRed>(Vec(40, 41), module, Shades::GAIN1_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSWhite>(Vec(40, 107), module, Shades::GAIN2_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSGreen>(Vec(40, 173), module, Shades::GAIN3_PARAM, 0.0, 1.0, 0.5));

	addParam(createParam<CKSS>(Vec(10, 52), module, Shades::MODE1_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<CKSS>(Vec(10, 118), module, Shades::MODE2_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<CKSS>(Vec(10, 184), module, Shades::MODE3_PARAM, 0.0, 1.0, 1.0));

	addInput(createInput<PJ301MPort>(Vec(9, 245), module, Shades::IN1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(9, 281), module, Shades::IN2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(9, 317), module, Shades::IN3_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(56, 245), module, Shades::OUT1_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(56, 281), module, Shades::OUT2_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(56, 317), module, Shades::OUT3_OUTPUT));

	addChild(createLight<SmallLight<GreenRedLight>>(Vec(41, 254), module, Shades::OUT1_POS_LIGHT));
	addChild(createLight<SmallLight<GreenRedLight>>(Vec(41, 290), module, Shades::OUT2_POS_LIGHT));
	addChild(createLight<SmallLight<GreenRedLight>>(Vec(41, 326), module, Shades::OUT3_POS_LIGHT));
}
