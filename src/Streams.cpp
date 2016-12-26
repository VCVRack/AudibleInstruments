#include <string.h>
#include "AudibleInstruments.hpp"


struct Streams : Module {
	enum ParamIds {
		SHAPE1_PARAM,
		MOD1_PARAM,
		LEVEL1_PARAM,
		RESPONSE1_PARAM,

		SHAPE2_PARAM,
		MOD2_PARAM,
		LEVEL2_PARAM,
		RESPONSE2_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		EXCITE1_INPUT,
		IN1_INPUT,
		LEVEL1_INPUT,

		EXCITE2_INPUT,
		IN2_INPUT,
		LEVEL2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		NUM_OUTPUTS
	};

	Streams();
	void step();
};


Streams::Streams() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);
}

void Streams::step() {
}


StreamsWidget::StreamsWidget() : ModuleWidget(new Streams()) {
	box.size = Vec(15*12, 380);

	{
		AudiblePanel *panel = new AudiblePanel();
		panel->imageFilename = "plugins/AudibleInstruments/res/Streams.png";
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew(Vec(15, 0)));
	addChild(createScrew(Vec(150, 0)));
	addChild(createScrew(Vec(15, 365)));
	addChild(createScrew(Vec(150, 365)));

	// addParam(createParam<HugeGlowKnob>(Vec(30, 53), module, Streams::ALGORITHM_PARAM, 0.0, 8.0, 0.0));

	// addParam(createParam<SmallWhiteKnob>(Vec(95, 173), module, Streams::TIMBRE_PARAM, 0.0, 1.0, 0.5));
	// addParam(createParam<MediumToggleSwitch>(Vec(17, 182), module, Streams::STATE_PARAM, 0.0, 3.0, 0.0));
	// addParam(createParam<TinyBlackKnob>(Vec(15, 214), module, Streams::LEVEL1_PARAM, 0.0, 1.0, 1.0));
	// addParam(createParam<TinyBlackKnob>(Vec(53, 214), module, Streams::LEVEL2_PARAM, 0.0, 1.0, 1.0));

	// addInput(createInput(Vec(11, 275), module, Streams::LEVEL1_INPUT));
	// addInput(createInput(Vec(47, 275), module, Streams::LEVEL2_INPUT));
	// addInput(createInput(Vec(83, 275), module, Streams::ALGORITHM_INPUT));
	// addInput(createInput(Vec(119, 275), module, Streams::TIMBRE_INPUT));

	// addInput(createInput(Vec(11, 318), module, Streams::CARRIER_INPUT));
	// addInput(createInput(Vec(47, 318), module, Streams::MODULATOR_INPUT));
	// addOutput(createOutput(Vec(83, 318), module, Streams::MODULATOR_OUTPUT));
	// addOutput(createOutput(Vec(119, 318), module, Streams::AUX_OUTPUT));

	// Streams *streams = dynamic_cast<Streams*>(module);
	// addChild(createValueLight<SmallModeLight>(Vec(21, 168), &streams->lights[0]));
}
