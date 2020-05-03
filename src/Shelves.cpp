#include "plugin.hpp"


struct Shelves : Module {
	enum ParamIds {
		HS_FREQ_PARAM,
		HS_GAIN_PARAM,
		P1_FREQ_PARAM,
		P1_GAIN_PARAM,
		P1_Q_PARAM,
		P2_FREQ_PARAM,
		P2_GAIN_PARAM,
		P2_Q_PARAM,
		LS_FREQ_PARAM,
		LS_GAIN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		HS_FREQ_INPUT,
		HS_GAIN_INPUT,
		P1_FREQ_INPUT,
		P1_GAIN_INPUT,
		P1_Q_INPUT,
		P2_FREQ_INPUT,
		P2_GAIN_INPUT,
		P2_Q_INPUT,
		LS_FREQ_INPUT,
		LS_GAIN_INPUT,
		FREQ_INPUT,
		GAIN_INPUT,
		IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		P1_HP_OUTPUT,
		P1_BP_OUTPUT,
		P1_LP_OUTPUT,
		P2_HP_OUTPUT,
		P2_BP_OUTPUT,
		P2_LP_OUTPUT,
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CLIP_LIGHT,
		NUM_LIGHTS
	};

	Shelves() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(HS_FREQ_PARAM, 0.f, 1.f, 0.f, "High-shelf frequency");
		configParam(HS_GAIN_PARAM, 0.f, 1.f, 0.f, "High-shelf gain");
		configParam(P1_FREQ_PARAM, 0.f, 1.f, 0.f, "Parametric 1 frequency");
		configParam(P1_GAIN_PARAM, 0.f, 1.f, 0.f, "Parametric 1 gain");
		configParam(P1_Q_PARAM, 0.f, 1.f, 0.f, "Parametric 1 quality");
		configParam(P2_FREQ_PARAM, 0.f, 1.f, 0.f, "Parametric 2 frequency");
		configParam(P2_GAIN_PARAM, 0.f, 1.f, 0.f, "Parametric 2 gain");
		configParam(P2_Q_PARAM, 0.f, 1.f, 0.f, "Parametric 2 quality");
		configParam(LS_FREQ_PARAM, 0.f, 1.f, 0.f, "Low-shelf frequency");
		configParam(LS_GAIN_PARAM, 0.f, 1.f, 0.f, "Low-shelf gain");
	}

	void process(const ProcessArgs& args) override {
	}
};


struct ShelvesWidget : ModuleWidget {
	ShelvesWidget(Shelves* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Shelves.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(41.582, 19.659)), module, Shelves::HS_FREQ_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(65.699, 19.659)), module, Shelves::HS_GAIN_PARAM));
		addParam(createParamCentered<Rogan1PSRed>(mm2px(Vec(41.582, 43.473)), module, Shelves::P1_FREQ_PARAM));
		addParam(createParamCentered<Rogan1PSRed>(mm2px(Vec(65.699, 43.473)), module, Shelves::P1_GAIN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.632, 48.111)), module, Shelves::P1_Q_PARAM));
		addParam(createParamCentered<Rogan1PSGreen>(mm2px(Vec(41.582, 67.286)), module, Shelves::P2_FREQ_PARAM));
		addParam(createParamCentered<Rogan1PSGreen>(mm2px(Vec(65.699, 67.286)), module, Shelves::P2_GAIN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.632, 63.447)), module, Shelves::P2_Q_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(41.582, 91.099)), module, Shelves::LS_FREQ_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(65.699, 91.099)), module, Shelves::LS_GAIN_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 17.329)), module, Shelves::HS_FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.619, 17.329)), module, Shelves::HS_GAIN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 33.824)), module, Shelves::P1_FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.619, 33.824)), module, Shelves::P1_GAIN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 48.111)), module, Shelves::P1_Q_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 77.733)), module, Shelves::P2_FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.619, 77.733)), module, Shelves::P2_GAIN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 63.447)), module, Shelves::P2_Q_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 94.228)), module, Shelves::LS_FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.619, 94.228)), module, Shelves::LS_GAIN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 109.475)), module, Shelves::FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.619, 109.475)), module, Shelves::GAIN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(41.565, 109.475)), module, Shelves::IN_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(84.418, 17.329)), module, Shelves::P1_HP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(84.418, 32.725)), module, Shelves::P1_BP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(84.431, 48.111)), module, Shelves::P1_LP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(84.431, 63.447)), module, Shelves::P2_HP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(84.418, 78.832)), module, Shelves::P2_BP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(84.418, 94.228)), module, Shelves::P2_LP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(65.682, 109.475)), module, Shelves::OUT_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(53.629, 109.475)), module, Shelves::CLIP_LIGHT));
	}
};


Model* modelShelves = createModel<Shelves, ShelvesWidget>("Shelves");