#include "plugin.hpp"


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
	enum LightIds {
		CV1_POS_LIGHT, CV1_NEG_LIGHT,
		CV2_POS_LIGHT, CV2_NEG_LIGHT,
		CV3_POS_LIGHT, CV3_NEG_LIGHT,
		CV4_POS_LIGHT, CV4_NEG_LIGHT,
		OUT1_POS_LIGHT, OUT1_NEG_LIGHT,
		OUT2_POS_LIGHT, OUT2_NEG_LIGHT,
		OUT3_POS_LIGHT, OUT3_NEG_LIGHT,
		OUT4_POS_LIGHT, OUT4_NEG_LIGHT,
		NUM_LIGHTS
	};

	Blinds() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(GAIN1_PARAM, -1.0, 1.0, 0.0, "Polarity and gain 1");
		configParam(GAIN2_PARAM, -1.0, 1.0, 0.0, "Polarity and gain 2");
		configParam(GAIN3_PARAM, -1.0, 1.0, 0.0, "Polarity and gain 3");
		configParam(GAIN4_PARAM, -1.0, 1.0, 0.0, "Polarity and gain 4");
		configParam(MOD1_PARAM, -1.0, 1.0, 0.0, "Modulation 1");
		configParam(MOD2_PARAM, -1.0, 1.0, 0.0, "Modulation 2");
		configParam(MOD3_PARAM, -1.0, 1.0, 0.0, "Modulation 3");
		configParam(MOD4_PARAM, -1.0, 1.0, 0.0, "Modulation 4");
	}

	void process(const ProcessArgs& args) override {
		float out = 0.0;

		for (int i = 0; i < 4; i++) {
			float g = params[GAIN1_PARAM + i].getValue();
			g += params[MOD1_PARAM + i].getValue() * inputs[CV1_INPUT + i].getVoltage() / 5.0;
			g = clamp(g, -2.0f, 2.0f);
			lights[CV1_POS_LIGHT + 2 * i].setSmoothBrightness(fmaxf(0.0, g), args.sampleTime);
			lights[CV1_NEG_LIGHT + 2 * i].setSmoothBrightness(fmaxf(0.0, -g), args.sampleTime);
			out += g * inputs[IN1_INPUT + i].getNormalVoltage(5.0);
			lights[OUT1_POS_LIGHT + 2 * i].setSmoothBrightness(fmaxf(0.0, out / 5.0), args.sampleTime);
			lights[OUT1_NEG_LIGHT + 2 * i].setSmoothBrightness(fmaxf(0.0, -out / 5.0), args.sampleTime);
			if (outputs[OUT1_OUTPUT + i].isConnected()) {
				outputs[OUT1_OUTPUT + i].setVoltage(out);
				out = 0.0;
			}
		}
	}
};


struct BlindsWidget : ModuleWidget {
	BlindsWidget(Blinds* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Blinds.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(150, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(150, 365)));

		addParam(createParam<Rogan1PSWhite>(Vec(8, 52), module, Blinds::GAIN1_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(8, 131), module, Blinds::GAIN2_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(8, 210), module, Blinds::GAIN3_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(8, 288), module, Blinds::GAIN4_PARAM));

		addParam(createParam<Trimpot>(Vec(72, 63), module, Blinds::MOD1_PARAM));
		addParam(createParam<Trimpot>(Vec(72, 142), module, Blinds::MOD2_PARAM));
		addParam(createParam<Trimpot>(Vec(72, 221), module, Blinds::MOD3_PARAM));
		addParam(createParam<Trimpot>(Vec(72, 300), module, Blinds::MOD4_PARAM));

		addInput(createInput<PJ301MPort>(Vec(110, 41), module, Blinds::IN1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(110, 120), module, Blinds::IN2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(110, 198), module, Blinds::IN3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(110, 277), module, Blinds::IN4_INPUT));

		addInput(createInput<PJ301MPort>(Vec(110, 80), module, Blinds::CV1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(110, 159), module, Blinds::CV2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(110, 238), module, Blinds::CV3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(110, 316), module, Blinds::CV4_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(144, 41), module, Blinds::OUT1_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(144, 120), module, Blinds::OUT2_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(144, 198), module, Blinds::OUT3_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(144, 277), module, Blinds::OUT4_OUTPUT));

		addChild(createLight<SmallLight<GreenRedLight>>(Vec(78, 96), module, Blinds::CV1_POS_LIGHT));
		addChild(createLight<SmallLight<GreenRedLight>>(Vec(78, 175), module, Blinds::CV2_POS_LIGHT));
		addChild(createLight<SmallLight<GreenRedLight>>(Vec(78, 254), module, Blinds::CV3_POS_LIGHT));
		addChild(createLight<SmallLight<GreenRedLight>>(Vec(78, 333), module, Blinds::CV4_POS_LIGHT));

		addChild(createLight<MediumLight<GreenRedLight>>(Vec(152, 87), module, Blinds::OUT1_POS_LIGHT));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(152, 166), module, Blinds::OUT2_POS_LIGHT));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(152, 245), module, Blinds::OUT3_POS_LIGHT));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(152, 324), module, Blinds::OUT4_POS_LIGHT));
	}
};


Model* modelBlinds = createModel<Blinds, BlindsWidget>("Blinds");
