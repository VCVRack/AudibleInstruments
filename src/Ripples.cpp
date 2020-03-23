#include "plugin.hpp"


struct Ripples : Module {
	enum ParamIds {
		RES_PARAM,
		FREQ_PARAM,
		FM_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		RES_INPUT,
		FREQ_INPUT,
		FM_INPUT,
		IN_INPUT,
		GAIN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		BP2_OUTPUT,
		LP2_OUTPUT,
		LP4_OUTPUT,
		LP4VCA_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	Ripples() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RES_PARAM, 0.f, 1.f, 0.5f, "Resonance");
		configParam(FREQ_PARAM, 0.f, 1.f, 0.5f, "Frequency");
		configParam(FM_PARAM, 0.f, 1.f, 0.5f, "Frequency modulation");
	}

	void process(const ProcessArgs& args) override {
	}
};


struct RipplesWidget : ModuleWidget {
	RipplesWidget(Ripples* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Ripples.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<Rogan2PSRed>(mm2px(Vec(8.872, 20.877)), module, Ripples::RES_PARAM));
		addParam(createParamCentered<Rogan3PSWhite>(mm2px(Vec(20.307, 42.468)), module, Ripples::FREQ_PARAM));
		addParam(createParamCentered<Rogan2PSGreen>(mm2px(Vec(31.732 + 0.1, 64.059)), module, Ripples::FM_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.227, 86.909)), module, Ripples::RES_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.297, 86.909)), module, Ripples::FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(32.367, 86.909)), module, Ripples::FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.227, 98.979)), module, Ripples::IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.227, 111.05)), module, Ripples::GAIN_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.297, 98.979)), module, Ripples::BP2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.367, 98.979)), module, Ripples::LP2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.297, 111.05)), module, Ripples::LP4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.367, 111.05)), module, Ripples::LP4VCA_OUTPUT));
	}
};


Model* modelRipples = createModel<Ripples, RipplesWidget>("Ripples");