#include "AudibleInstruments.hpp"
#include "dsp/digital.hpp"


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
	enum LightIds {
		SIGN_POS_LIGHT, SIGN_NEG_LIGHT,
		LOGIC_POS_LIGHT, LOGIC_NEG_LIGHT,
		SH_POS_LIGHT, SH_NEG_LIGHT,
		NUM_LIGHTS
	};

	SchmittTrigger trigger;
	float sample = 0.0;

	Kinks() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;
};


void Kinks::step() {
	// Gaussian noise generator
	float noise = 2.0 * randomNormal();

	// S&H
	if (trigger.process(inputs[TRIG_INPUT].value / 0.7)) {
		sample = inputs[SH_INPUT].normalize(noise);
	}

	// lights
	lights[SIGN_POS_LIGHT].setBrightnessSmooth(fmaxf(0.0, inputs[SIGN_INPUT].value / 5.0));
	lights[SIGN_NEG_LIGHT].setBrightnessSmooth(fmaxf(0.0, -inputs[SIGN_INPUT].value / 5.0));
	float logicSum = inputs[LOGIC_A_INPUT].value + inputs[LOGIC_B_INPUT].value;
	lights[LOGIC_POS_LIGHT].setBrightnessSmooth(fmaxf(0.0, logicSum / 5.0));
	lights[LOGIC_NEG_LIGHT].setBrightnessSmooth(fmaxf(0.0, -logicSum / 5.0));
	lights[SH_POS_LIGHT].setBrightness(fmaxf(0.0, sample / 5.0));
	lights[SH_NEG_LIGHT].setBrightness(fmaxf(0.0, -sample / 5.0));

	// outputs
	outputs[INVERT_OUTPUT].value = -inputs[SIGN_INPUT].value;
	outputs[HALF_RECTIFY_OUTPUT].value = fmaxf(0.0, inputs[SIGN_INPUT].value);
	outputs[FULL_RECTIFY_OUTPUT].value = fabsf(inputs[SIGN_INPUT].value);
	outputs[MAX_OUTPUT].value = fmaxf(inputs[LOGIC_A_INPUT].value, inputs[LOGIC_B_INPUT].value);
	outputs[MIN_OUTPUT].value = fminf(inputs[LOGIC_A_INPUT].value, inputs[LOGIC_B_INPUT].value);
	outputs[NOISE_OUTPUT].value = noise;
	outputs[SH_OUTPUT].value = sample;
}


KinksWidget::KinksWidget() {
	Kinks *module = new Kinks();
	setModule(module);
	box.size = Vec(15*4, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load(assetPlugin(plugin, "res/Kinks.png"));
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));

	addInput(createInput<PJ301MPort>(Vec(4, 75), module, Kinks::SIGN_INPUT));
	addOutput(createOutput<PJ301MPort>(Vec(31, 75), module, Kinks::INVERT_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(4, 113), module, Kinks::HALF_RECTIFY_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(31, 113), module, Kinks::FULL_RECTIFY_OUTPUT));

	addInput(createInput<PJ301MPort>(Vec(4, 177), module, Kinks::LOGIC_A_INPUT));
	addInput(createInput<PJ301MPort>(Vec(31, 177), module, Kinks::LOGIC_B_INPUT));
	addOutput(createOutput<PJ301MPort>(Vec(4, 214), module, Kinks::MAX_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(31, 214), module, Kinks::MIN_OUTPUT));

	addInput(createInput<PJ301MPort>(Vec(4, 278), module, Kinks::SH_INPUT));
	addInput(createInput<PJ301MPort>(Vec(31, 278), module, Kinks::TRIG_INPUT));
	addOutput(createOutput<PJ301MPort>(Vec(4, 316), module, Kinks::NOISE_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(31, 316), module, Kinks::SH_OUTPUT));

	addChild(createLight<SmallLight<GreenRedLight>>(Vec(11, 59), module, Kinks::SIGN_POS_LIGHT));
	addChild(createLight<SmallLight<GreenRedLight>>(Vec(11, 161), module, Kinks::LOGIC_POS_LIGHT));
	addChild(createLight<SmallLight<GreenRedLight>>(Vec(11, 262), module, Kinks::SH_POS_LIGHT));
}
