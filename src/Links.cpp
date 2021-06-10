#include "plugin.hpp"


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
		ENUMS(A_LIGHT, 2),
		ENUMS(B_LIGHT, 2),
		ENUMS(C_LIGHT, 2),
		NUM_LIGHTS
	};

	Links() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configInput(A1_INPUT, "A1");
		configInput(B1_INPUT, "B1");
		configInput(B2_INPUT, "B2");
		configInput(C1_INPUT, "C1");
		configInput(C2_INPUT, "C2");
		configInput(C3_INPUT, "C3");
		configOutput(A1_OUTPUT, "A1");
		configOutput(A2_OUTPUT, "A2");
		configOutput(A3_OUTPUT, "A3");
		configOutput(B1_OUTPUT, "B1");
		configOutput(B2_OUTPUT, "B2");
		configOutput(C1_OUTPUT, "C1");
	}

	void process(const ProcessArgs& args) override {
		// Section A
		{
			int channels = std::max(inputs[A1_INPUT].getChannels(), 1);
			float in[16];
			for (int c = 0; c < channels; c++) {
				in[c] = inputs[A1_INPUT].getVoltage(c);
				outputs[A1_OUTPUT].setVoltage(in[c], c);
				outputs[A2_OUTPUT].setVoltage(in[c], c);
				outputs[A3_OUTPUT].setVoltage(in[c], c);
			}
			outputs[A1_OUTPUT].setChannels(channels);
			outputs[A2_OUTPUT].setChannels(channels);
			outputs[A3_OUTPUT].setChannels(channels);
			lights[A_LIGHT + 0].setSmoothBrightness(in[0] / 5.f, args.sampleTime);
			lights[A_LIGHT + 1].setSmoothBrightness(-in[0] / 5.f, args.sampleTime);
		}

		// Section B
		{
			int channels = std::max(std::max(inputs[B1_INPUT].getChannels(), inputs[B2_INPUT].getChannels()), 1);
			float in[16];
			for (int c = 0; c < channels; c++) {
				in[c] = inputs[B1_INPUT].getPolyVoltage(c) + inputs[B2_INPUT].getPolyVoltage(c);
				outputs[B1_OUTPUT].setVoltage(in[c], c);
				outputs[B2_OUTPUT].setVoltage(in[c], c);
			}
			outputs[B1_OUTPUT].setChannels(channels);
			outputs[B2_OUTPUT].setChannels(channels);
			lights[B_LIGHT + 0].setSmoothBrightness(in[0] / 5.f, args.sampleTime);
			lights[B_LIGHT + 1].setSmoothBrightness(-in[0] / 5.f, args.sampleTime);
		}

		// Section C
		{
			int channels = std::max(std::max(std::max(inputs[C1_INPUT].getChannels(), inputs[C2_INPUT].getChannels()), inputs[C3_INPUT].getChannels()), 1);
			float in[16];
			for (int c = 0; c < channels; c++) {
				in[c] = inputs[C1_INPUT].getPolyVoltage(c) + inputs[C2_INPUT].getPolyVoltage(c) + inputs[C3_INPUT].getPolyVoltage(c);
				outputs[C1_OUTPUT].setVoltage(in[c], c);
			}
			outputs[C1_OUTPUT].setChannels(channels);
			lights[C_LIGHT + 0].setSmoothBrightness(in[0] / 5.f, args.sampleTime);
			lights[C_LIGHT + 1].setSmoothBrightness(-in[0] / 5.f, args.sampleTime);
		}
	}
};


struct LinksWidget : ModuleWidget {
	LinksWidget(Links* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Links.svg")));

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

		addChild(createLight<SmallLight<GreenRedLight>>(Vec(26, 59), module, Links::A_LIGHT));
		addChild(createLight<SmallLight<GreenRedLight>>(Vec(26, 161), module, Links::B_LIGHT));
		addChild(createLight<SmallLight<GreenRedLight>>(Vec(26, 262), module, Links::C_LIGHT));
	}
};


Model* modelLinks = createModel<Links, LinksWidget>("Links");
