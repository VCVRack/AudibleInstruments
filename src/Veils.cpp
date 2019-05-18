#include "AudibleInstruments.hpp"


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

	Veils() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(Veils::GAIN1_PARAM, 0.0, 1.0, 0.0);
		configParam(Veils::GAIN2_PARAM, 0.0, 1.0, 0.0);
		configParam(Veils::GAIN3_PARAM, 0.0, 1.0, 0.0);
		configParam(Veils::GAIN4_PARAM, 0.0, 1.0, 0.0);
		configParam(Veils::RESPONSE1_PARAM, 0.0, 1.0, 1.0);
		configParam(Veils::RESPONSE2_PARAM, 0.0, 1.0, 1.0);
		configParam(Veils::RESPONSE3_PARAM, 0.0, 1.0, 1.0);
		configParam(Veils::RESPONSE4_PARAM, 0.0, 1.0, 1.0);
	}

	void process(const ProcessArgs &args) {
		float out = 0.0;

		for (int i = 0; i < 4; i++) {
			float in = inputs[IN1_INPUT + i].getVoltage() * params[GAIN1_PARAM + i].getValue();
			if (inputs[CV1_INPUT + i].isConnected()) {
				float linear = fmaxf(inputs[CV1_INPUT + i].getVoltage() / 5.0, 0.0);
				linear = clamp(linear, 0.0f, 2.0f);
				const float base = 200.0;
				float exponential = rescale(powf(base, linear / 2.0f), 1.0f, base, 0.0f, 10.0f);
				in *= crossfade(exponential, linear, params[RESPONSE1_PARAM + i].getValue());
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


struct VeilsWidget : ModuleWidget {
	VeilsWidget(Veils *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Veils.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(150, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(150, 365)));

		addParam(createParam<Rogan1PSWhite>(Vec(8, 52), module, Veils::GAIN1_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(8, 131), module, Veils::GAIN2_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(8, 210), module, Veils::GAIN3_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(8, 288), module, Veils::GAIN4_PARAM));

		addParam(createParam<Trimpot>(Vec(72, 56), module, Veils::RESPONSE1_PARAM));
		addParam(createParam<Trimpot>(Vec(72, 135), module, Veils::RESPONSE2_PARAM));
		addParam(createParam<Trimpot>(Vec(72, 214), module, Veils::RESPONSE3_PARAM));
		addParam(createParam<Trimpot>(Vec(72, 292), module, Veils::RESPONSE4_PARAM));

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

		addChild(createLight<MediumLight<GreenRedLight>>(Vec(152, 87), module, Veils::OUT1_POS_LIGHT));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(152, 166), module, Veils::OUT2_POS_LIGHT));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(152, 245), module, Veils::OUT3_POS_LIGHT));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(152, 324), module, Veils::OUT4_POS_LIGHT));
	}
};


Model *modelVeils = createModel<Veils, VeilsWidget>("Veils");
