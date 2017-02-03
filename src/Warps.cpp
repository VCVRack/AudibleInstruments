#include <string.h>
#include "AudibleInstruments.hpp"
#include "warps/dsp/modulator.h"


struct Warps : Module {
	enum ParamIds {
		ALGORITHM_PARAM,
		TIMBRE_PARAM,
		STATE_PARAM,
		LEVEL1_PARAM,
		LEVEL2_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		LEVEL1_INPUT,
		LEVEL2_INPUT,
		ALGORITHM_INPUT,
		TIMBRE_INPUT,
		CARRIER_INPUT,
		MODULATOR_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MODULATOR_OUTPUT,
		AUX_OUTPUT,
		NUM_OUTPUTS
	};

	int frame = 0;
	warps::Modulator modulator;
	warps::ShortFrame inputFrames[60] = {};
	warps::ShortFrame outputFrames[60] = {};
	float lights[1] = {};

	Warps();
	void step();
};


Warps::Warps() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);

	memset(&modulator, 0, sizeof(modulator));
	modulator.Init(96000.0f);
}

void Warps::step() {
	if (++frame >= 60) {
		frame = 0;

		warps::Parameters *p = modulator.mutable_parameters();
		p->channel_drive[0] = clampf(params[LEVEL1_PARAM] + getf(inputs[LEVEL1_INPUT]) / 5.0, 0.0, 1.0);
		p->channel_drive[1] = clampf(params[LEVEL2_PARAM] + getf(inputs[LEVEL2_INPUT]) / 5.0, 0.0, 1.0);
		p->modulation_algorithm = clampf(params[ALGORITHM_PARAM] / 8.0 + getf(inputs[ALGORITHM_PARAM]) / 5.0, 0.0, 1.0);
		p->modulation_parameter = clampf(params[TIMBRE_PARAM] + getf(inputs[TIMBRE_INPUT]) / 5.0, 0.0, 1.0);

		p->frequency_shift_pot = params[ALGORITHM_PARAM] / 8.0;
		p->frequency_shift_cv = clampf(getf(inputs[ALGORITHM_INPUT]) / 5.0, -1.0, 1.0);
		p->phase_shift = p->modulation_algorithm;
		p->note = 60.0 * params[LEVEL1_PARAM] + 12.0 * getf(inputs[LEVEL1_INPUT], 2.0) + 12.0;
		p->note += log2f(96000.0 / gSampleRate) * 12.0;
		float state = roundf(params[STATE_PARAM]);
		p->carrier_shape = (int32_t)state;
		lights[0] = state - 1.0;

		modulator.Process(inputFrames, outputFrames, 60);
	}

	inputFrames[frame].l = clampf(getf(inputs[CARRIER_INPUT]) / 16.0 * 0x8000, -0x8000, 0x7fff);
	inputFrames[frame].r = clampf(getf(inputs[MODULATOR_INPUT]) / 16.0 * 0x8000, -0x8000, 0x7fff);
	setf(outputs[MODULATOR_OUTPUT], (float)outputFrames[frame].l / 0x8000 * 5.0);
	setf(outputs[AUX_OUTPUT], (float)outputFrames[frame].r / 0x8000 * 5.0);
}


WarpsWidget::WarpsWidget() : ModuleWidget(new Warps()) {
	box.size = Vec(15*10, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load("plugins/AudibleInstruments/res/Warps.png");
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew(Vec(15, 0)));
	addChild(createScrew(Vec(120, 0)));
	addChild(createScrew(Vec(15, 365)));
	addChild(createScrew(Vec(120, 365)));

	addParam(createParam<HugeGlowKnob>(Vec(30, 53), module, Warps::ALGORITHM_PARAM, 0.0, 8.0, 0.0));

	addParam(createParam<SmallWhiteKnob>(Vec(95, 173), module, Warps::TIMBRE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<MediumToggleSwitch>(Vec(17, 182), module, Warps::STATE_PARAM, 0.0, 3.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(15, 214), module, Warps::LEVEL1_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<TinyBlackKnob>(Vec(53, 214), module, Warps::LEVEL2_PARAM, 0.0, 1.0, 1.0));

	addInput(createInput<InputPortPJ3410>(Vec(5, 270), module, Warps::LEVEL1_INPUT));
	addInput(createInput<InputPortPJ3410>(Vec(41, 270), module, Warps::LEVEL2_INPUT));
	addInput(createInput<InputPortPJ3410>(Vec(77, 270), module, Warps::ALGORITHM_INPUT));
	addInput(createInput<InputPortPJ3410>(Vec(113, 270), module, Warps::TIMBRE_INPUT));

	addInput(createInput<InputPortPJ3410>(Vec(5, 313), module, Warps::CARRIER_INPUT));
	addInput(createInput<InputPortPJ3410>(Vec(41, 313), module, Warps::MODULATOR_INPUT));
	addOutput(createOutput<OutputPortPJ3410>(Vec(77, 313), module, Warps::MODULATOR_OUTPUT));
	addOutput(createOutput<OutputPortPJ3410>(Vec(113, 313), module, Warps::AUX_OUTPUT));

	Warps *warps = dynamic_cast<Warps*>(module);
	addChild(createValueLight<SmallModeLight>(Vec(21, 168), &warps->lights[0]));
}
