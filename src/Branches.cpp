#include "plugin.hpp"


struct Branches : Module {
	enum ParamIds {
		THRESHOLD1_PARAM,
		THRESHOLD2_PARAM,
		MODE1_PARAM,
		MODE2_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		IN1_INPUT,
		IN2_INPUT,
		P1_INPUT,
		P2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT1A_OUTPUT,
		OUT2A_OUTPUT,
		OUT1B_OUTPUT,
		OUT2B_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STATE_LIGHTS, 2 * 2),
		NUM_LIGHTS
	};

	dsp::BooleanTrigger gateTriggers[2][16];
	dsp::BooleanTrigger modeTriggers[2];
	bool modes[2] = {};
	bool outcomes[2][16] = {};

	Branches() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(THRESHOLD1_PARAM, 0.0, 1.0, 0.5, "Channel 1 probability", "%", 0, 100);
		configParam(MODE1_PARAM, 0.0, 1.0, 0.0, "Channel 1 mode");
		configParam(THRESHOLD2_PARAM, 0.0, 1.0, 0.5, "Channel 2 probability", "%", 0, 100);
		configParam(MODE2_PARAM, 0.0, 1.0, 0.0, "Channel 2 mode");
	}

	void process(const ProcessArgs& args) override {
		for (int i = 0; i < 2; i++) {
			// Get input
			Input* input = &inputs[IN1_INPUT + i];
			// 2nd input is normalized to 1st.
			if (i == 1 && !input->isConnected())
				input = &inputs[IN1_INPUT + 0];
			int channels = std::max(input->getChannels(), 1);

			// mode button
			if (modeTriggers[i].process(params[MODE1_PARAM + i].getValue() > 0.f))
				modes[i] ^= true;

			bool lightA = false;
			bool lightB = false;

			// Process triggers
			for (int c = 0; c < channels; c++) {
				bool gate = input->getVoltage(c) >= 2.f;
				if (gateTriggers[i][c].process(gate)) {
					// trigger
					// We don't have to clamp here because the threshold comparison works without it.
					float threshold = params[THRESHOLD1_PARAM + i].getValue() + inputs[P1_INPUT + i].getPolyVoltage(c) / 10.f;
					bool toss = (random::uniform() < threshold);
					if (!modes[i]) {
						// direct modes
						outcomes[i][c] = toss;
					}
					else {
						// toggle modes
						if (toss)
							outcomes[i][c] ^= true;
					}
				}

				// Output gate logic
				bool gateA = !outcomes[i][c] && (modes[i] ? true : gate);
				bool gateB = outcomes[i][c] && (modes[i] ? true : gate);

				if (gateA)
					lightA = true;
				if (gateB)
					lightB = true;

				// Set output gates
				outputs[OUT1A_OUTPUT + i].setVoltage(gateA ? 10.f : 0.f, c);
				outputs[OUT1B_OUTPUT + i].setVoltage(gateB ? 10.f : 0.f, c);
			}

			outputs[OUT1A_OUTPUT + i].setChannels(channels);
			outputs[OUT1B_OUTPUT + i].setChannels(channels);

			lights[STATE_LIGHTS + i * 2 + 1].setSmoothBrightness(lightA, args.sampleTime);
			lights[STATE_LIGHTS + i * 2 + 0].setSmoothBrightness(lightB, args.sampleTime);
		}
	}

	void onReset() override {
		for (int i = 0; i < 2; i++) {
			modes[i] = false;
			for (int c = 0; c < 16; c++) {
				outcomes[i][c] = false;
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_t* modesJ = json_array();
		for (int i = 0; i < 2; i++) {
			json_array_insert_new(modesJ, i, json_boolean(modes[i]));
		}
		json_object_set_new(rootJ, "modes", modesJ);
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* modesJ = json_object_get(rootJ, "modes");
		if (modesJ) {
			for (int i = 0; i < 2; i++) {
				json_t* modeJ = json_array_get(modesJ, i);
				if (modeJ)
					modes[i] = json_boolean_value(modeJ);
			}
		}
	}
};


struct BranchesWidget : ModuleWidget {
	BranchesWidget(Branches* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Branches.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<TL1105>(mm2px(Vec(25.852, 22.24)), module, Branches::MODE1_PARAM));
		addParam(createParamCentered<Rogan1PSRed>(mm2px(Vec(15.057, 28.595)), module, Branches::THRESHOLD1_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(25.852, 74.95)), module, Branches::MODE2_PARAM));
		addParam(createParamCentered<Rogan1PSGreen>(mm2px(Vec(15.057, 81.296)), module, Branches::THRESHOLD2_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.112, 45.74)), module, Branches::IN1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.991, 45.74)), module, Branches::P1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.112, 98.44)), module, Branches::IN2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.991, 98.44)), module, Branches::P2_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.112, 58.44)), module, Branches::OUT1A_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.991, 58.44)), module, Branches::OUT1B_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.112, 111.14)), module, Branches::OUT2A_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.991, 111.14)), module, Branches::OUT2B_OUTPUT));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(15.052, 58.45)), module, Branches::STATE_LIGHTS + 0 * 2));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(15.052, 111.151)), module, Branches::STATE_LIGHTS + 1 * 2));
	}

	void appendContextMenu(Menu* menu) override {
		Branches* module = dynamic_cast<Branches*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexPtrSubmenuItem("Channel 1 mode", {
			"Latch",
			"Toggle",
		}, &module->modes[0]));

		menu->addChild(createIndexPtrSubmenuItem("Channel 2 mode", {
			"Latch",
			"Toggle",
		}, &module->modes[1]));
	}
};


Model* modelBranches = createModel<Branches, BranchesWidget>("Branches");
