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
		MODE1_LIGHT,
		MODE2_LIGHT,
		STATE1_POS_LIGHT, STATE1_NEG_LIGHT,
		STATE2_POS_LIGHT, STATE2_NEG_LIGHT,
		NUM_LIGHTS
	};

	dsp::SchmittTrigger gateTriggers[2];
	dsp::SchmittTrigger modeTriggers[2];
	bool modes[2] = {};
	bool outcomes[2] = {};

	Branches() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(THRESHOLD1_PARAM, 0.0, 1.0, 0.5, "Probability 1");
		configParam(MODE1_PARAM, 0.0, 1.0, 0.0, "Mode 1");
		configParam(THRESHOLD2_PARAM, 0.0, 1.0, 0.5, "Probability 2");
		configParam(MODE2_PARAM, 0.0, 1.0, 0.0, "Mode 2");
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

	void process(const ProcessArgs& args) override {
		float gate = 0.0;
		for (int i = 0; i < 2; i++) {
			// mode button
			if (modeTriggers[i].process(params[MODE1_PARAM + i].getValue()))
				modes[i] = !modes[i];

			if (inputs[IN1_INPUT + i].isConnected())
				gate = inputs[IN1_INPUT + i].getVoltage();

			if (gateTriggers[i].process(gate)) {
				// trigger
				float r = random::uniform();
				float threshold = clamp(params[THRESHOLD1_PARAM + i].getValue() + inputs[P1_INPUT + i].getVoltage() / 10.f, 0.f, 1.f);
				bool toss = (r < threshold);
				if (!modes[i]) {
					// direct modes
					outcomes[i] = toss;
				}
				else {
					// toggle modes
					outcomes[i] = (outcomes[i] != toss);
				}

				if (!outcomes[i])
					lights[STATE1_POS_LIGHT + 2 * i].value = 1.0;
				else
					lights[STATE1_NEG_LIGHT + 2 * i].value = 1.0;
			}

			lights[STATE1_POS_LIGHT + 2 * i].value *= 1.0 - args.sampleTime * 15.0;
			lights[STATE1_NEG_LIGHT + 2 * i].value *= 1.0 - args.sampleTime * 15.0;
			lights[MODE1_LIGHT + i].value = modes[i] ? 1.0 : 0.0;

			outputs[OUT1A_OUTPUT + i].setVoltage(outcomes[i] ? 0.0 : gate);
			outputs[OUT1B_OUTPUT + i].setVoltage(outcomes[i] ? gate : 0.0);
		}
	}

	void onReset() override {
		for (int i = 0; i < 2; i++) {
			modes[i] = false;
			outcomes[i] = false;
		}
	}
};


struct BranchesWidget : ModuleWidget {
	BranchesWidget(Branches* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Branches.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));

		addParam(createParam<Rogan1PSRed>(Vec(24, 64), module, Branches::THRESHOLD1_PARAM));
		addParam(createParam<TL1105>(Vec(69, 58), module, Branches::MODE1_PARAM));
		addInput(createInput<PJ301MPort>(Vec(9, 122), module, Branches::IN1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(55, 122), module, Branches::P1_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(9, 160), module, Branches::OUT1A_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(55, 160), module, Branches::OUT1B_OUTPUT));

		addParam(createParam<Rogan1PSGreen>(Vec(24, 220), module, Branches::THRESHOLD2_PARAM));
		addParam(createParam<TL1105>(Vec(69, 214), module, Branches::MODE2_PARAM));
		addInput(createInput<PJ301MPort>(Vec(9, 278), module, Branches::IN2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(55, 278), module, Branches::P2_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(9, 316), module, Branches::OUT2A_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(55, 316), module, Branches::OUT2B_OUTPUT));

		addChild(createLight<SmallLight<GreenRedLight>>(Vec(40, 169), module, Branches::STATE1_POS_LIGHT));
		addChild(createLight<SmallLight<GreenRedLight>>(Vec(40, 325), module, Branches::STATE2_POS_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		Branches* branches = dynamic_cast<Branches*>(module);
		assert(branches);

		struct BranchesModeItem : MenuItem {
			Branches* branches;
			int channel;
			void onAction(const event::Action& e) override {
				branches->modes[channel] ^= 1;
			}
			void step() override {
				rightText = branches->modes[channel] ? "Toggle" : "Latch";
				MenuItem::step();
			}
		};

		menu->addChild(new MenuSeparator);

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Channels"));
		menu->addChild(construct<BranchesModeItem>(&MenuItem::text, "Channel 1 modes", &BranchesModeItem::branches, branches, &BranchesModeItem::channel, 0));
		menu->addChild(construct<BranchesModeItem>(&MenuItem::text, "Channel 2 modes", &BranchesModeItem::branches, branches, &BranchesModeItem::channel, 1));
	}
};


Model* modelBranches = createModel<Branches, BranchesWidget>("Branches");
