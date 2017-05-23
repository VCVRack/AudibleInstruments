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
	Trigger modeTrigger;
	Trigger rangeTrigger;

	Tides();
	void step();

	json_t *toJsonData() {
		json_t *root = json_object();

		json_object_set_new(root, "mode", json_integer((int)generator.mode()));
		json_object_set_new(root, "range", json_integer((int)generator.range()));

		return root;
	}
	void fromJsonData(json_t *root) {
		json_t *modeJ = json_object_get(root, "mode");
		if (modeJ) {
			generator.set_mode((tides::GeneratorMode)json_integer_value(modeJ));
		}

		json_t *rangeJ = json_object_get(root, "range");
		if (rangeJ) {
			generator.set_range((tides::GeneratorRange)json_integer_value(rangeJ));
		}
	}
};


Tides::Tides() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);

	memset(&generator, 0, sizeof(generator));
	generator.Init();
	generator.set_range(tides::GENERATOR_RANGE_MEDIUM);
	generator.set_mode(tides::GENERATOR_MODE_LOOPING);
	generator.set_sync(false);
}

void Tides::step() {
	// TODO Save / load the state of MODE and RANGE to JSON
	tides::GeneratorMode mode = generator.mode();
	if (modeTrigger.process(params[MODE_PARAM])) {
		mode = (tides::GeneratorMode) (((int)mode + 2) % 3);
		generator.set_mode(mode);
	}
	lights[0] = (float)mode;

	tides::GeneratorRange range = generator.range();
	if (rangeTrigger.process(params[RANGE_PARAM])) {
		range = (tides::GeneratorRange) (((int)range + 2) % 3);
		generator.set_range(range);
	}
	lights[2] = (float)range;

	// Buffer loop
	if (++frame >= 16) {
		frame = 0;

		// Pitch
		float pitch = params[FREQUENCY_PARAM];
		pitch += 12.0*getf(inputs[PITCH_INPUT]);
		pitch += params[FM_PARAM] * getf(inputs[FM_INPUT], 0.1) / 5.0;
		pitch += 60.0;
		// Scale to the global sample rate
		pitch += log2f(48000.0 / gSampleRate) * 12.0;
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

	lights[1] = (sample.flags & tides::FLAG_END_OF_ATTACK ? -unif : unif) / 8.0;
}


struct TidesModeLight : ModeValueLight {
	TidesModeLight() {
		addColor(COLOR_RED);
		addColor(COLOR_BLACK_TRANSPARENT);
		addColor(COLOR_CYAN);
	}
};


TidesWidget::TidesWidget() {
	Tides *module = new Tides();
	setModule(module);
	box.size = Vec(15*14, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load("plugins/AudibleInstruments/res/Tides.png");
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(180, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(180, 365)));

	addParam(createParam<CKD6>(Vec(19, 52), module, Tides::MODE_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<CKD6>(Vec(19, 93), module, Tides::RANGE_PARAM, 0.0, 1.0, 0.0));

	addParam(createParam<Rogan3PSGreen>(Vec(79, 60), module, Tides::FREQUENCY_PARAM, -48.0, 48.0, 0.0));
	addParam(createParam<Rogan1PSGreen>(Vec(156, 66), module, Tides::FM_PARAM, -12.0, 12.0, 0.0));

	addParam(createParam<Rogan1PSWhite>(Vec(13, 155), module, Tides::SHAPE_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(85, 155), module, Tides::SLOPE_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(156, 155), module, Tides::SMOOTHNESS_PARAM, -1.0, 1.0, 0.0));

	addInput(createInput<PJ3410Port>(Vec(18, 216), module, Tides::SHAPE_INPUT));
	addInput(createInput<PJ3410Port>(Vec(90, 216), module, Tides::SLOPE_INPUT));
	addInput(createInput<PJ3410Port>(Vec(161, 216), module, Tides::SMOOTHNESS_INPUT));

	addInput(createInput<PJ3410Port>(Vec(18, 271), module, Tides::TRIG_INPUT));
	addInput(createInput<PJ3410Port>(Vec(54, 271), module, Tides::FREEZE_INPUT));
	addInput(createInput<PJ3410Port>(Vec(90, 271), module, Tides::PITCH_INPUT));
	addInput(createInput<PJ3410Port>(Vec(125, 271), module, Tides::FM_INPUT));
	addInput(createInput<PJ3410Port>(Vec(161, 271), module, Tides::LEVEL_INPUT));

	addInput(createInput<PJ3410Port>(Vec(18, 313), module, Tides::CLOCK_INPUT));
	addOutput(createOutput<PJ3410Port>(Vec(54, 313), module, Tides::HIGH_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(90, 313), module, Tides::LOW_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(125, 313), module, Tides::UNI_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(161, 313), module, Tides::BI_OUTPUT));

	addChild(createValueLight<SmallLight<TidesModeLight>>(Vec(57, 62), &module->lights[0]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(57, 83), &module->lights[1]));
	addChild(createValueLight<SmallLight<TidesModeLight>>(Vec(57, 103), &module->lights[2]));
}
