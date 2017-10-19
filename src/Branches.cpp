#include "AudibleInstruments.hpp"


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
		P1_INPUT,
		IN2_INPUT,
		P2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT1A_OUTPUT,
		OUT1B_OUTPUT,
		OUT2A_OUTPUT,
		OUT2B_OUTPUT,
		NUM_OUTPUTS
	};

	bool lastGate[2] = {};
	bool outcome[2] = {};
	float light[2] = {};

	Branches() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step() override;
};


static void processChannel(Input &in, Input &p, Param &threshold, Param &mode, bool *lastGate, bool *outcome, Output &out1, Output &out2, float *light) {
	float out = in.value;
	bool gate = (out >= 1.0);
	if (gate && !*lastGate) {
		// trigger
		float r = randomf();
		bool toss = (r < threshold.value + p.value);
		if (mode.value < 0.5) {
			// direct mode
			*outcome = toss;
		}
		else {
			// toggle mode
			*outcome = *outcome != toss;
		}
	}
	*lastGate = gate;
	*light = *outcome ? out : -out;

	out1.value = *outcome ? 0.0 : out;
	out2.value = *outcome ? out : 0.0;
}

void Branches::step() {
	processChannel(inputs[IN1_INPUT], inputs[P1_INPUT], params[THRESHOLD1_PARAM], params[MODE1_PARAM], &lastGate[0], &outcome[0], outputs[OUT1A_OUTPUT], outputs[OUT1B_OUTPUT], &light[0]);
	processChannel(inputs[IN2_INPUT].active ? inputs[IN2_INPUT] : inputs[IN1_INPUT], inputs[P2_INPUT], params[THRESHOLD2_PARAM], params[MODE2_PARAM], &lastGate[1], &outcome[1], outputs[OUT2A_OUTPUT], outputs[OUT2B_OUTPUT], &light[1]);
}


BranchesWidget::BranchesWidget() {
	Branches *module = new Branches();
	setModule(module);
	box.size = Vec(15*6, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load(assetPlugin(plugin, "res/Branches.png"));
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));

	addParam(createParam<Rogan1PSRed>(Vec(24, 64), module, Branches::THRESHOLD1_PARAM, 0.0, 1.0, 0.5));
	// addParam(createParam<MediumToggleSwitch>(Vec(69, 58), module, Branches::MODE1_PARAM, 0.0, 1.0, 0.0));
	addInput(createInput<PJ301MPort>(Vec(9, 122), module, Branches::IN1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(55, 122), module, Branches::P1_INPUT));
	addOutput(createOutput<PJ301MPort>(Vec(9, 160), module, Branches::OUT1A_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(55, 160), module, Branches::OUT1B_OUTPUT));

	addParam(createParam<Rogan1PSGreen>(Vec(24, 220), module, Branches::THRESHOLD2_PARAM, 0.0, 1.0, 0.5));
	// addParam(createParam<MediumToggleSwitch>(Vec(69, 214), module, Branches::MODE2_PARAM, 0.0, 1.0, 0.0));
	addInput(createInput<PJ301MPort>(Vec(9, 278), module, Branches::IN2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(55, 278), module, Branches::P2_INPUT));
	addOutput(createOutput<PJ301MPort>(Vec(9, 316), module, Branches::OUT2A_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(55, 316), module, Branches::OUT2B_OUTPUT));

	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(40, 169), &module->light[0]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(40, 325), &module->light[1]));
}
