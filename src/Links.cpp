#include "AudibleInstruments.hpp"


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

	float lights[3] = {};
	Links() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step();
};


void Links::step() {
	float in1 = inputs[A1_INPUT].value;
	float in2 = inputs[B1_INPUT].value + inputs[B2_INPUT].value;
	float in3 = inputs[C1_INPUT].value + inputs[C2_INPUT].value + inputs[C3_INPUT].value;

	outputs[A1_OUTPUT].value = in1;
	outputs[A2_OUTPUT].value = in1;
	outputs[A3_OUTPUT].value = in1;
	outputs[B1_OUTPUT].value = in2;
	outputs[B2_OUTPUT].value = in2;
	outputs[C1_OUTPUT].value = in3;

	lights[0] = in1 / 5.0;
	lights[1] = in2 / 5.0;
	lights[2] = in3 / 5.0;
}


LinksWidget::LinksWidget() {
	Links *module = new Links();
	setModule(module);
	box.size = Vec(15*4, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load(assetPlugin(plugin, "res/Links.png"));
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));

	addInput(createInput<PJ3410Port>(Vec(0, 72), module, Links::A1_INPUT));
	addOutput(createOutput<PJ3410Port>(Vec(29, 72), module, Links::A1_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(0, 110), module, Links::A2_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(29, 110), module, Links::A3_OUTPUT));

	addInput(createInput<PJ3410Port>(Vec(0, 174), module, Links::B1_INPUT));
	addInput(createInput<PJ3410Port>(Vec(29, 174), module, Links::B2_INPUT));
	addOutput(createOutput<PJ3410Port>(Vec(0, 211), module, Links::B1_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(29, 211), module, Links::B2_OUTPUT));

	addInput(createInput<PJ3410Port>(Vec(0, 275), module, Links::C1_INPUT));
	addInput(createInput<PJ3410Port>(Vec(29, 275), module, Links::C2_INPUT));
	addInput(createInput<PJ3410Port>(Vec(0, 313), module, Links::C3_INPUT));
	addOutput(createOutput<PJ3410Port>(Vec(29, 313), module, Links::C1_OUTPUT));

	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(26, 59), &module->lights[0]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(26, 161), &module->lights[1]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(26, 262), &module->lights[2]));
}
