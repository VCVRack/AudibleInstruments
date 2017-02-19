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
	Links();
	void step();
};


Links::Links() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);
}

void Links::step() {
	float in1 = getf(inputs[A1_INPUT]);
	float in2 = getf(inputs[B1_INPUT]) + getf(inputs[B2_INPUT]);
	float in3 = getf(inputs[C1_INPUT]) + getf(inputs[C2_INPUT]) + getf(inputs[C3_INPUT]);

	setf(outputs[A1_OUTPUT], in1);
	setf(outputs[A2_OUTPUT], in1);
	setf(outputs[A3_OUTPUT], in1);
	setf(outputs[B1_OUTPUT], in2);
	setf(outputs[B2_OUTPUT], in2);
	setf(outputs[C1_OUTPUT], in3);

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
		panel->backgroundImage = Image::load("plugins/AudibleInstruments/res/Links.png");
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<SilverScrew>(Vec(15, 0)));
	addChild(createScrew<SilverScrew>(Vec(15, 365)));

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
