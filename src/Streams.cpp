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

	Streams() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step() override;
};


void Streams::step() {
}


StreamsWidget::StreamsWidget() {
	Streams *module = new Streams();
	setModule(module);
	box.size = Vec(15*12, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load(assetPlugin(plugin, "res/Streams.png"));
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(150, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
	addChild(Widget::create<ScrewSilver>(Vec(150, 365)));

	// addParam(ParamWidget::create<HugeGlowKnob>(Vec(30, 53), module, Streams::ALGORITHM_PARAM, 0.0, 8.0, 0.0));

	// addParam(ParamWidget::create<Rogan1PSWhite>(Vec(95, 173), module, Streams::TIMBRE_PARAM, 0.0, 1.0, 0.5));
	// addParam(ParamWidget::create<MediumToggleSwitch>(Vec(17, 182), module, Streams::STATE_PARAM, 0.0, 3.0, 0.0));
	// addParam(ParamWidget::create<Trimpot>(Vec(15, 214), module, Streams::LEVEL1_PARAM, 0.0, 1.0, 1.0));
	// addParam(ParamWidget::create<Trimpot>(Vec(53, 214), module, Streams::LEVEL2_PARAM, 0.0, 1.0, 1.0));

	// addInput(Port::create<PJ3410Port>(Vec(11, 275), Port::INPUT, module, Streams::LEVEL1_INPUT));
	// addInput(Port::create<PJ3410Port>(Vec(47, 275), Port::INPUT, module, Streams::LEVEL2_INPUT));
	// addInput(Port::create<PJ3410Port>(Vec(83, 275), Port::INPUT, module, Streams::ALGORITHM_INPUT));
	// addInput(Port::create<PJ3410Port>(Vec(119, 275), Port::INPUT, module, Streams::TIMBRE_INPUT));

	// addInput(Port::create<PJ3410Port>(Vec(11, 318), Port::INPUT, module, Streams::CARRIER_INPUT));
	// addInput(Port::create<PJ3410Port>(Vec(47, 318), Port::INPUT, module, Streams::MODULATOR_INPUT));
	// addOutput(Port::create<PJ3410Port>(Vec(83, 318), Port::OUTPUT, module, Streams::MODULATOR_OUTPUT));
	// addOutput(Port::create<PJ3410Port>(Vec(119, 318), Port::OUTPUT, module, Streams::AUX_OUTPUT));

	// Streams *streams = dynamic_cast<Streams*>(module);
	// addChild(createValueLight<SmallModeLight>(Vec(21, 168), &streams->lights[0]));
}
