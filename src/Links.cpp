#include "AudibleInstruments.hpp"


struct Links : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		A1_INPUT,
		B1_INPUT,
		B2_INPUT,
		C1_INPUT,
		C2_INPUT,
		C3_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		A1_OUTPUT,
		A2_OUTPUT,
		A3_OUTPUT,
		B1_OUTPUT,
		B2_OUTPUT,
		C1_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		A_POS_LIGHT, A_NEG_LIGHT,
		B_POS_LIGHT, B_NEG_LIGHT,
		C_POS_LIGHT, C_NEG_LIGHT,
		NUM_LIGHTS
	};

	Links() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}

	void process(const ProcessArgs &args) {
		float inA = inputs[A1_INPUT].getVoltage();
		float inB = inputs[B1_INPUT].getVoltage() + inputs[B2_INPUT].getVoltage();
		float inC = inputs[C1_INPUT].getVoltage() + inputs[C2_INPUT].getVoltage() + inputs[C3_INPUT].getVoltage();

		outputs[A1_OUTPUT].setVoltage(inA);
		outputs[A2_OUTPUT].setVoltage(inA);
		outputs[A3_OUTPUT].setVoltage(inA);
		outputs[B1_OUTPUT].setVoltage(inB);
		outputs[B2_OUTPUT].setVoltage(inB);
		outputs[C1_OUTPUT].setVoltage(inC);

		lights[A_POS_LIGHT].setSmoothBrightness(fmaxf(0.0, inA / 5.0), args.sampleTime);
		lights[A_NEG_LIGHT].setSmoothBrightness(fmaxf(0.0, -inA / 5.0), args.sampleTime);
		lights[B_POS_LIGHT].setSmoothBrightness(fmaxf(0.0, inB / 5.0), args.sampleTime);
		lights[B_NEG_LIGHT].setSmoothBrightness(fmaxf(0.0, -inB / 5.0), args.sampleTime);
		lights[C_POS_LIGHT].setSmoothBrightness(fmaxf(0.0, inC / 5.0), args.sampleTime);
		lights[C_NEG_LIGHT].setSmoothBrightness(fmaxf(0.0, -inC / 5.0), args.sampleTime);
	}
};


struct LinksWidget : ModuleWidget {
	LinksWidget(Links *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Links.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));

		addInput(createInput<PJ301MPort>(Vec(4, 75), module, Links::A1_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(31, 75), module, Links::A1_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(4, 113), module, Links::A2_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(31, 113), module, Links::A3_OUTPUT));

		addInput(createInput<PJ301MPort>(Vec(4, 177), module, Links::B1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(31, 177), module, Links::B2_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(4, 214), module, Links::B1_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(31, 214), module, Links::B2_OUTPUT));

		addInput(createInput<PJ301MPort>(Vec(4, 278), module, Links::C1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(31, 278), module, Links::C2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(4, 316), module, Links::C3_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(31, 316), module, Links::C1_OUTPUT));

		addChild(createLight<SmallLight<GreenRedLight>>(Vec(26, 59), module, Links::A_POS_LIGHT));
		addChild(createLight<SmallLight<GreenRedLight>>(Vec(26, 161), module, Links::B_POS_LIGHT));
		addChild(createLight<SmallLight<GreenRedLight>>(Vec(26, 262), module, Links::C_POS_LIGHT));
	}
};


Model *modelLinks = createModel<Links, LinksWidget>("Links");
