#include "plugin.hpp"


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

	Shades() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(GAIN1_PARAM, 0.0, 1.0, 0.5, "Gain 1");
		configParam(GAIN2_PARAM, 0.0, 1.0, 0.5, "Gain 2");
		configParam(GAIN3_PARAM, 0.0, 1.0, 0.5, "Gain 3");
		configParam(MODE1_PARAM, 0.0, 1.0, 1.0, "Attenuverter/Attenuator mode 1");
		configParam(MODE2_PARAM, 0.0, 1.0, 1.0, "Attenuverter/Attenuator mode 2");
		configParam(MODE3_PARAM, 0.0, 1.0, 1.0, "Attenuverter/Attenuator mode 3");
	}

	void process(const ProcessArgs &args) override {
		float out = 0.0;

		for (int i = 0; i < 3; i++) {
			float in = inputs[IN1_INPUT + i].getNormalVoltage(5.0);
			if ((int)params[MODE1_PARAM + i].getValue() == 1) {
				// attenuverter
				in *= 2.0 * params[GAIN1_PARAM + i].getValue() - 1.0;
			}
			else {
				// attenuator
				in *= params[GAIN1_PARAM + i].getValue();
			}
			out += in;
			lights[OUT1_POS_LIGHT + 2*i].setSmoothBrightness(fmaxf(0.0, out / 5.0), args.sampleTime);
			lights[OUT1_NEG_LIGHT + 2*i].setSmoothBrightness(fmaxf(0.0, -out / 5.0), args.sampleTime);
			if (outputs[OUT1_OUTPUT + i].isConnected()) {
				outputs[OUT1_OUTPUT + i].setVoltage(out);
				out = 0.0;
			}
		}
	}
};


struct ShadesWidget : ModuleWidget {
	ShadesWidget(Shades *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Shades.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));

		addParam(createParam<Rogan1PSRed>(Vec(40, 40), module, Shades::GAIN1_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(40, 106), module, Shades::GAIN2_PARAM));
		addParam(createParam<Rogan1PSGreen>(Vec(40, 172), module, Shades::GAIN3_PARAM));

		addParam(createParam<CKSS>(Vec(10, 51), module, Shades::MODE1_PARAM));
		addParam(createParam<CKSS>(Vec(10, 117), module, Shades::MODE2_PARAM));
		addParam(createParam<CKSS>(Vec(10, 183), module, Shades::MODE3_PARAM));

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
};


Model *modelShades = createModel<Shades, ShadesWidget>("Shades");
