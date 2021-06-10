#include "plugin.hpp"


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

	dsp::SchmittTrigger trigger;
	float sample = 0.0;

	Kinks() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configInput(SIGN_INPUT, "Sign");
		configInput(LOGIC_A_INPUT, "Logic A");
		configInput(LOGIC_B_INPUT, "Logic B");
		configInput(SH_INPUT, "Sample & hold");
		configInput(TRIG_INPUT, "S&H trigger");

		configOutput(INVERT_OUTPUT, "Invert");
		configOutput(HALF_RECTIFY_OUTPUT, "Half rectified");
		configOutput(FULL_RECTIFY_OUTPUT, "Full rectified");
		configOutput(MAX_OUTPUT, "Maximum");
		configOutput(MIN_OUTPUT, "Minimum");
		configOutput(NOISE_OUTPUT, "Noise");
		configOutput(SH_OUTPUT, "Sample & hold");
	}

	void process(const ProcessArgs& args) override {
		// Gaussian noise generator
		float noise = 2.0 * random::normal();

		// S&H
		if (trigger.process(inputs[TRIG_INPUT].getVoltage() / 0.7)) {
			sample = inputs[SH_INPUT].getNormalVoltage(noise);
		}

		// lights
		lights[SIGN_POS_LIGHT].setSmoothBrightness(fmaxf(0.0, inputs[SIGN_INPUT].getVoltage() / 5.0), args.sampleTime);
		lights[SIGN_NEG_LIGHT].setSmoothBrightness(fmaxf(0.0, -inputs[SIGN_INPUT].getVoltage() / 5.0), args.sampleTime);
		float logicSum = inputs[LOGIC_A_INPUT].getVoltage() + inputs[LOGIC_B_INPUT].getVoltage();
		lights[LOGIC_POS_LIGHT].setSmoothBrightness(fmaxf(0.0, logicSum / 5.0), args.sampleTime);
		lights[LOGIC_NEG_LIGHT].setSmoothBrightness(fmaxf(0.0, -logicSum / 5.0), args.sampleTime);
		lights[SH_POS_LIGHT].setBrightness(fmaxf(0.0, sample / 5.0));
		lights[SH_NEG_LIGHT].setBrightness(fmaxf(0.0, -sample / 5.0));

		// outputs
		outputs[INVERT_OUTPUT].setVoltage(-inputs[SIGN_INPUT].getVoltage());
		outputs[HALF_RECTIFY_OUTPUT].setVoltage(fmaxf(0.0, inputs[SIGN_INPUT].getVoltage()));
		outputs[FULL_RECTIFY_OUTPUT].setVoltage(fabsf(inputs[SIGN_INPUT].getVoltage()));
		outputs[MAX_OUTPUT].setVoltage(fmaxf(inputs[LOGIC_A_INPUT].getVoltage(), inputs[LOGIC_B_INPUT].getVoltage()));
		outputs[MIN_OUTPUT].setVoltage(fminf(inputs[LOGIC_A_INPUT].getVoltage(), inputs[LOGIC_B_INPUT].getVoltage()));
		outputs[NOISE_OUTPUT].setVoltage(noise);
		outputs[SH_OUTPUT].setVoltage(sample);
	}
};


struct KinksWidget : ModuleWidget {
	KinksWidget(Kinks* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Kinks.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));

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
};


Model* modelKinks = createModel<Kinks, KinksWidget>("Kinks");
