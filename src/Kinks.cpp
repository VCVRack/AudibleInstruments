#include "AudibleInstruments.hpp"


// If the trigger input is rising at a rate of at least DTRIG, the S&H will be triggered
#define DTRIG 5000.0


struct Kinks : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		SIGN_INPUT,
		LOGIC_A_INPUT,
		LOGIC_B_INPUT,
		SH_INPUT,
		TRIG_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		INVERT_OUTPUT,
		HALF_RECTIFY_OUTPUT,
		FULL_RECTIFY_OUTPUT,
		MAX_OUTPUT,
		MIN_OUTPUT,
		NOISE_OUTPUT,
		SH_OUTPUT,
		NUM_OUTPUTS
	};

	float lastTrig = 0.0;
	float sample = 0.0;
	float lights[3] = {};

	Kinks();
	void step();
};


Kinks::Kinks() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);
}

void Kinks::step() {
	// Gaussian noise generator
	float noise = 2.0 * randomNormal();

	// S&H
	float trig = getf(inputs[TRIG_INPUT]);
	float dtrig = (trig - lastTrig) * gSampleRate;
	if (dtrig > DTRIG) {
		sample = getf(inputs[SH_INPUT], noise);
	}
	lastTrig = trig;

	// lights
	lights[0] = getf(inputs[SIGN_INPUT]);
	lights[1] = getf(inputs[LOGIC_A_INPUT]) + getf(inputs[LOGIC_B_INPUT]);
	lights[2] = sample;

	// outputs
	if (outputs[INVERT_OUTPUT]) {
		*outputs[INVERT_OUTPUT] = -getf(inputs[SIGN_INPUT]);
	}
	if (outputs[HALF_RECTIFY_OUTPUT]) {
		*outputs[HALF_RECTIFY_OUTPUT] = fmaxf(0.0, getf(inputs[SIGN_INPUT]));
	}
	if (outputs[FULL_RECTIFY_OUTPUT]) {
		*outputs[FULL_RECTIFY_OUTPUT] = fabsf(getf(inputs[SIGN_INPUT]));
	}
	if (outputs[MAX_OUTPUT]) {
		*outputs[MAX_OUTPUT] = fmaxf(getf(inputs[LOGIC_A_INPUT]), getf(inputs[LOGIC_B_INPUT]));
	}
	if (outputs[MIN_OUTPUT]) {
		*outputs[MIN_OUTPUT] = fminf(getf(inputs[LOGIC_A_INPUT]), getf(inputs[LOGIC_B_INPUT]));
	}
	if (outputs[NOISE_OUTPUT]) {
		*outputs[NOISE_OUTPUT] = noise;
	}
	if (outputs[SH_OUTPUT]) {
		*outputs[SH_OUTPUT] = sample;
	}
}


KinksWidget::KinksWidget() {
	Kinks *module = new Kinks();
	setModule(module);
	box.size = Vec(15*4, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load("plugins/AudibleInstruments/res/Kinks.png");
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));

	addInput(createInput<PJ3410Port>(Vec(0, 72), module, Kinks::SIGN_INPUT));
	addOutput(createOutput<PJ3410Port>(Vec(29, 72), module, Kinks::INVERT_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(0, 110), module, Kinks::HALF_RECTIFY_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(29, 110), module, Kinks::FULL_RECTIFY_OUTPUT));

	addInput(createInput<PJ3410Port>(Vec(0, 174), module, Kinks::LOGIC_A_INPUT));
	addInput(createInput<PJ3410Port>(Vec(29, 174), module, Kinks::LOGIC_B_INPUT));
	addOutput(createOutput<PJ3410Port>(Vec(0, 211), module, Kinks::MAX_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(29, 211), module, Kinks::MIN_OUTPUT));

	addInput(createInput<PJ3410Port>(Vec(0, 275), module, Kinks::SH_INPUT));
	addInput(createInput<PJ3410Port>(Vec(29, 275), module, Kinks::TRIG_INPUT));
	addOutput(createOutput<PJ3410Port>(Vec(0, 313), module, Kinks::NOISE_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(29, 313), module, Kinks::SH_OUTPUT));

	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(11, 59), &module->lights[0]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(11, 161), &module->lights[1]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(11, 262), &module->lights[2]));
}
