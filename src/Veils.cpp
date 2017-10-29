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
	enum LightIds {
		OUT1_POS_LIGHT, OUT1_NEG_LIGHT,
		OUT2_POS_LIGHT, OUT2_NEG_LIGHT,
		OUT3_POS_LIGHT, OUT3_NEG_LIGHT,
		OUT4_POS_LIGHT, OUT4_NEG_LIGHT,
		NUM_LIGHTS
	};

	Veils() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;
};


void Veils::step() {
	float out = 0.0;

	for (int i = 0; i < 4; i++) {
		float in = inputs[IN1_INPUT + i].value * params[GAIN1_PARAM + i].value;
		if (inputs[CV1_INPUT + i].active) {
			float linear = fmaxf(inputs[CV1_INPUT + i].value / 5.0, 0.0);
			linear = clampf(linear, 0.0, 2.0);
			const float base = 200.0;
			float exponential = rescalef(powf(base, linear / 2.0), 1.0, base, 0.0, 10.0);
			in *= crossf(exponential, linear, params[RESPONSE1_PARAM + i].value);
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

	addInput(createInput<PJ301MPort>(Vec(110, 41), module, Veils::IN1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(110, 120), module, Veils::IN2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(110, 198), module, Veils::IN3_INPUT));
	addInput(createInput<PJ301MPort>(Vec(110, 277), module, Veils::IN4_INPUT));

	addInput(createInput<PJ301MPort>(Vec(110, 80), module, Veils::CV1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(110, 159), module, Veils::CV2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(110, 238), module, Veils::CV3_INPUT));
	addInput(createInput<PJ301MPort>(Vec(110, 316), module, Veils::CV4_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(144, 41), module, Veils::OUT1_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(144, 120), module, Veils::OUT2_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(144, 198), module, Veils::OUT3_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(144, 277), module, Veils::OUT4_OUTPUT));

	addChild(createLight<MediumLight<GreenRedLight>>(Vec(150, 87), module, Veils::OUT1_POS_LIGHT));
	addChild(createLight<MediumLight<GreenRedLight>>(Vec(150, 166), module, Veils::OUT2_POS_LIGHT));
	addChild(createLight<MediumLight<GreenRedLight>>(Vec(150, 245), module, Veils::OUT3_POS_LIGHT));
	addChild(createLight<MediumLight<GreenRedLight>>(Vec(150, 324), module, Veils::OUT4_POS_LIGHT));
}
