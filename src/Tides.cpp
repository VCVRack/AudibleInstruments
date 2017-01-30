#include "AudibleInstruments.hpp"
#include <string.h>
#include "tides/generator.h"


struct Tides : Module {
	enum ParamIds {
		MODE_PARAM,
		RANGE_PARAM,

		FREQUENCY_PARAM,
		FM_PARAM,

		SHAPE_PARAM,
		SLOPE_PARAM,
		SMOOTHNESS_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		SHAPE_INPUT,
		SLOPE_INPUT,
		SMOOTHNESS_INPUT,

		TRIG_INPUT,
		FREEZE_INPUT,
		PITCH_INPUT,
		FM_INPUT,
		LEVEL_INPUT,

		CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		HIGH_OUTPUT,
		LOW_OUTPUT,
		UNI_OUTPUT,
		BI_OUTPUT,
		NUM_OUTPUTS
	};

	tides::Generator generator;
	float lights[3] = {};
	int frame = 0;
	uint8_t lastGate;

	Tides();
	void step();
};


Tides::Tides() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);

	memset(&generator, 0, sizeof(generator));
	generator.Init();
	generator.set_range(tides::GENERATOR_RANGE_LOW);
	generator.set_mode(tides::GENERATOR_MODE_AD);
	generator.set_sync(false);
}

void Tides::step() {
	if (++frame >= 16) {
		frame = 0;

		// Mode and range
		tides::GeneratorMode mode = (tides::GeneratorMode) roundf(1.0 - params[MODE_PARAM]);
		if (mode != generator.mode()) {
			generator.set_mode(mode);
		}
		lights[0] = -params[MODE_PARAM];

		tides::GeneratorRange range = (tides::GeneratorRange) roundf(1.0 - params[RANGE_PARAM]);
		if (range != generator.range()) {
			generator.set_range(range);
		}
		lights[2] = -params[RANGE_PARAM];

		// Pitch
		float pitch = params[FREQUENCY_PARAM];
		pitch += 12.0*getf(inputs[PITCH_INPUT]);
		pitch += params[FM_PARAM] * getf(inputs[FM_INPUT], 0.1) / 5.0;
		pitch += 60.0;
		// Scale to the global sample rate
		pitch += log2f(48000.0 / gRack->sampleRate) * 12.0;
		generator.set_pitch(clampf(pitch*0x80, -0x8000, 0x7fff));

		// Slope, smoothness, pitch
		int16_t shape = clampf(params[SHAPE_PARAM] + getf(inputs[SHAPE_INPUT]) / 5.0, -1.0, 1.0) * 0x7fff;
		int16_t slope = clampf(params[SLOPE_PARAM] + getf(inputs[SLOPE_INPUT]) / 5.0, -1.0, 1.0) * 0x7fff;
		int16_t smoothness = clampf(params[SMOOTHNESS_PARAM] + getf(inputs[SMOOTHNESS_INPUT]) / 5.0, -1.0, 1.0) * 0x7fff;
		generator.set_shape(shape);
		generator.set_slope(slope);
		generator.set_smoothness(smoothness);

		// Sync
		// Slight deviation from spec here.
		// Instead of toggling sync by holding the range button, just enable it if the clock port is plugged in.
		generator.set_sync(inputs[CLOCK_INPUT]);

		// Generator
		generator.Process();
	}

	// Level
	uint16_t level = clampf(getf(inputs[LEVEL_INPUT], 8.0) / 8.0, 0.0, 1.0) * 0xffff;
	if (level < 32)
		level = 0;

	uint8_t gate = 0;
	if (getf(inputs[FREEZE_INPUT]) >= 0.7)
		gate |= tides::CONTROL_FREEZE;
	if (getf(inputs[TRIG_INPUT]) >= 0.7)
		gate |= tides::CONTROL_GATE;
	if (getf(inputs[CLOCK_INPUT]) >= 0.7)
		gate |= tides::CONTROL_CLOCK;
	if (!(lastGate & tides::CONTROL_CLOCK) && (gate & tides::CONTROL_CLOCK))
		gate |= tides::CONTROL_GATE_RISING;
	if (!(lastGate & tides::CONTROL_GATE) && (gate & tides::CONTROL_GATE))
		gate |= tides::CONTROL_GATE_RISING;
	if ((lastGate & tides::CONTROL_GATE) && !(gate & tides::CONTROL_GATE))
		gate |= tides::CONTROL_GATE_FALLING;
	lastGate = gate;

	const tides::GeneratorSample& sample = generator.Process(gate);
	uint32_t uni = sample.unipolar;
	int32_t bi = sample.bipolar;

	uni = uni * level >> 16;
	bi = -bi * level >> 16;
	float unif = mapf(uni, 0, 0xffff, 0.0, 8.0);
	float bif = mapf(bi, -0x8000, 0x7fff, -5.0, 5.0);

	setf(outputs[HIGH_OUTPUT], sample.flags & tides::FLAG_END_OF_ATTACK ? 0.0 : 5.0);
	setf(outputs[LOW_OUTPUT], sample.flags & tides::FLAG_END_OF_RELEASE ? 0.0 : 5.0);
	setf(outputs[UNI_OUTPUT], unif);
	setf(outputs[BI_OUTPUT], bif);

	lights[1] = sample.flags & tides::FLAG_END_OF_ATTACK ? -unif : unif;
}


TidesWidget::TidesWidget() : ModuleWidget(new Tides()) {
	box.size = Vec(15*14, 380);

	{
		AudiblePanel *panel = new AudiblePanel();
		panel->imageFilename = "plugins/AudibleInstruments/res/Tides.png";
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew(Vec(15, 0)));
	addChild(createScrew(Vec(180, 0)));
	addChild(createScrew(Vec(15, 365)));
	addChild(createScrew(Vec(180, 365)));

	addParam(createParam<LargeToggleSwitch>(Vec(19, 52), module, Tides::MODE_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<LargeToggleSwitch>(Vec(19, 93), module, Tides::RANGE_PARAM, -1.0, 1.0, 0.0));

	addParam(createParam<LargeGreenKnob>(Vec(79, 60), module, Tides::FREQUENCY_PARAM, -48.0, 48.0, 0.0));
	addParam(createParam<SmallGreenKnob>(Vec(156, 66), module, Tides::FM_PARAM, -12.0, 12.0, 0.0));

	addParam(createParam<SmallWhiteKnob>(Vec(13, 155), module, Tides::SHAPE_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<SmallWhiteKnob>(Vec(85, 155), module, Tides::SLOPE_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<SmallWhiteKnob>(Vec(156, 155), module, Tides::SMOOTHNESS_PARAM, -1.0, 1.0, 0.0));

	addInput(createInput(Vec(23, 221), module, Tides::SHAPE_INPUT));
	addInput(createInput(Vec(95, 221), module, Tides::SLOPE_INPUT));
	addInput(createInput(Vec(166, 221), module, Tides::SMOOTHNESS_INPUT));

	addInput(createInput(Vec(23, 275), module, Tides::TRIG_INPUT));
	addInput(createInput(Vec(59, 275), module, Tides::FREEZE_INPUT));
	addInput(createInput(Vec(95, 275), module, Tides::PITCH_INPUT));
	addInput(createInput(Vec(130, 275), module, Tides::FM_INPUT));
	addInput(createInput(Vec(166, 275), module, Tides::LEVEL_INPUT));

	addInput(createInput(Vec(23, 318), module, Tides::CLOCK_INPUT));
	addOutput(createOutput(Vec(59, 318), module, Tides::HIGH_OUTPUT));
	addOutput(createOutput(Vec(95, 318), module, Tides::LOW_OUTPUT));
	addOutput(createOutput(Vec(130, 318), module, Tides::UNI_OUTPUT));
	addOutput(createOutput(Vec(166, 318), module, Tides::BI_OUTPUT));

	Tides *tides = dynamic_cast<Tides*>(module);
	addChild(createValueLight<SmallValueLight>(Vec(58, 63), &tides->lights[0]));
	addChild(createValueLight<SmallValueLight>(Vec(58, 84), &tides->lights[1]));
	addChild(createValueLight<SmallValueLight>(Vec(58, 105), &tides->lights[2]));
}
