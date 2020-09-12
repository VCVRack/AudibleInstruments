#include "plugin.hpp"


struct Streams : Module {
	enum ParamIds {
		BUTTON_1_PARAM,
		BUTTON_2_PARAM,
		SHAPE_1_PARAM,
		SHAPE_2_PARAM,
		MOD_1_PARAM,
		MOD_2_PARAM,
		METER_PARAM,
		KNOB_1_PARAM,
		LEVEL_MOD_1_PARAM,
		LEVEL_MOD_2_PARAM,
		KNOB_2_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		EXCITE_1_INPUT,
		IN_1_INPUT,
		IN_2_INPUT,
		EXCITE_2_INPUT,
		LEVEL_1_INPUT,
		LEVEL_2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_1_OUTPUT,
		OUT_2_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		METER_1_LIGHT,
		CIRCLE2329_LIGHT,
		CIRCLE2323_LIGHT,
		CIRCLE2331_LIGHT,
		CIRCLE2325_LIGHT,
		CIRCLE2333_LIGHT,
		CIRCLE2327_LIGHT,
		CIRCLE2335_LIGHT,
		NUM_LIGHTS
	};

	Streams() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(BUTTON_1_PARAM, 0.f, 1.f, 0.f, "");
		configParam(BUTTON_2_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SHAPE_1_PARAM, 0.f, 1.f, 0.5f, "");
		configParam(SHAPE_2_PARAM, 0.f, 1.f, 0.5f, "");
		configParam(MOD_1_PARAM, 0.f, 1.f, 0.5f, "");
		configParam(MOD_2_PARAM, 0.f, 1.f, 0.5f, "");
		configParam(METER_PARAM, 0.f, 1.f, 0.f, "");
		configParam(KNOB_1_PARAM, 0.f, 1.f, 0.5f, "");
		configParam(LEVEL_MOD_1_PARAM, 0.f, 1.f, 0.5f, "");
		configParam(LEVEL_MOD_2_PARAM, 0.f, 1.f, 0.5f, "");
		configParam(KNOB_2_PARAM, 0.f, 1.f, 0.5f, "");
	}

	void process(const ProcessArgs& args) override {
		lights[METER_1_LIGHT].setBrightness(1);
		lights[CIRCLE2329_LIGHT].setBrightness(1);
		lights[CIRCLE2323_LIGHT].setBrightness(1);
		lights[CIRCLE2331_LIGHT].setBrightness(1);
		lights[CIRCLE2325_LIGHT].setBrightness(1);
		lights[CIRCLE2333_LIGHT].setBrightness(1);
		lights[CIRCLE2327_LIGHT].setBrightness(1);
		lights[CIRCLE2335_LIGHT].setBrightness(1);
	}
};


struct StreamsWidget : ModuleWidget {
	StreamsWidget(Streams* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Streams.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<TL1105>(mm2px(Vec(24.715, 15.025)), module, Streams::BUTTON_1_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(36.135, 15.025)), module, Streams::BUTTON_2_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(11.065, 21.055)), module, Streams::SHAPE_1_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(49.785, 21.055)), module, Streams::SHAPE_2_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(11.065, 44.555)), module, Streams::MOD_1_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(49.785, 44.555)), module, Streams::MOD_2_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(30.425, 46.775)), module, Streams::METER_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(30.425, 60.745)), module, Streams::KNOB_1_PARAM));
		addParam(createParamCentered<Rogan1PSRed>(mm2px(Vec(11.065, 68.045)), module, Streams::LEVEL_MOD_1_PARAM));
		addParam(createParamCentered<Rogan1PSGreen>(mm2px(Vec(49.785, 68.045)), module, Streams::LEVEL_MOD_2_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(30.425, 75.345)), module, Streams::KNOB_2_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.506, 96.615)), module, Streams::EXCITE_1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(23.116, 96.615)), module, Streams::IN_1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(37.726, 96.615)), module, Streams::IN_2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(52.335, 96.615)), module, Streams::EXCITE_2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.506, 111.225)), module, Streams::LEVEL_1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(52.335, 111.225)), module, Streams::LEVEL_2_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(23.116, 111.225)), module, Streams::OUT_1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(37.726, 111.225)), module, Streams::OUT_2_OUTPUT));

		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(24.715, 22.005)), module, Streams::METER_1_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(36.135, 22.005)), module, Streams::CIRCLE2329_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(24.715, 27.725)), module, Streams::CIRCLE2323_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(36.135, 27.725)), module, Streams::CIRCLE2331_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(24.715, 33.445)), module, Streams::CIRCLE2325_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(36.135, 33.445)), module, Streams::CIRCLE2333_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(24.715, 39.166)), module, Streams::CIRCLE2327_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(36.135, 39.166)), module, Streams::CIRCLE2335_LIGHT));
	}
};


Model* modelStreams = createModel<Streams, StreamsWidget>("Streams");