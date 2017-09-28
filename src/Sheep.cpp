#include "AudibleInstruments.hpp"
#include <string.h>

#define WAVETABLE_HACK
#include "tides/generator.h"


struct Sheep : Module {
	enum ParamIds {
		BANK_PARAM,
		RANGE_PARAM,

		FREQUENCY_PARAM,
		FM_PARAM,

		ROW_PARAM,
		COLUMN_PARAM,
		SMOOTHNESS_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ROW_INPUT,
		COLUMN_INPUT,
		SMOOTHNESS_INPUT,

		SYNC_INPUT,
		FREEZE_INPUT,
		PITCH_INPUT,
		FM_INPUT,
		LEVEL_INPUT,

		BANK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		BIT_OUTPUT,
		SUB_OUTPUT,
		UNI_OUTPUT,
		BI_OUTPUT,
		NUM_OUTPUTS
	};

	tides::Generator generator;
	float lights[3] = {};
	int frame = 0;
	uint8_t lastGate;
	SchmittTrigger modeTrigger;
	SchmittTrigger rangeTrigger;

	Sheep();
	void step();

	json_t *toJson() {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "bank", json_integer((int) generator.mode()));
		json_object_set_new(rootJ, "range", json_integer((int) generator.range()));

		return rootJ;
	}

	void fromJson(json_t *rootJ) {
		json_t *bankJ = json_object_get(rootJ, "bank");
		if (bankJ) {
			generator.set_mode((tides::GeneratorMode) json_integer_value(bankJ));
		}

		json_t *rangeJ = json_object_get(rootJ, "range");
		if (rangeJ) {
			generator.set_range((tides::GeneratorRange) json_integer_value(rangeJ));
		}
	}

	void initialize() {
		generator.set_range(tides::GENERATOR_RANGE_MEDIUM);
		generator.set_mode(tides::GENERATOR_MODE_LOOPING);
	}

	void randomize() {
		generator.set_range((tides::GeneratorRange) (randomu32() % 3));
		generator.set_mode((tides::GeneratorMode) (randomu32() % 3));
	}
};


Sheep::Sheep() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {
	memset(&generator, 0, sizeof(generator));
	generator.Init();
	generator.set_sync(false);
	initialize();
}

void Sheep::step() {
	tides::GeneratorMode mode = generator.mode();
	if (modeTrigger.process(params[BANK_PARAM].value)) {
		mode = (tides::GeneratorMode) (((int)mode - 1 + 3) % 3);
		generator.set_mode(mode);
	}
	lights[0] = (float)mode;

	tides::GeneratorRange range = generator.range();
	if (rangeTrigger.process(params[RANGE_PARAM].value)) {
		range = (tides::GeneratorRange) (((int)range - 1 + 3) % 3);
		generator.set_range(range);
	}
	lights[2] = (float)range;

	// Buffer loop
	if (++frame >= 16) {
		frame = 0;

		// Pitch
		float pitch = params[FREQUENCY_PARAM].value;
		pitch += 12.0 * inputs[PITCH_INPUT].value;
		pitch += params[FM_PARAM].value * inputs[FM_INPUT].normalize(0.1) / 5.0;
		pitch += 60.0;
		// Scale to the global sample rate
		pitch += log2f(48000.0 / gSampleRate) * 12.0;
		generator.set_pitch(clampf(pitch * 0x80, -0x8000, 0x7fff));

		// Slope, smoothness, pitch
		int16_t shape = clampf(params[ROW_PARAM].value + inputs[ROW_INPUT].value / 5.0, -1.0, 1.0) * 0x7fff;
		int16_t slope = clampf(params[COLUMN_PARAM].value + inputs[COLUMN_INPUT].value / 5.0, -1.0, 1.0) * 0x7fff;
		int16_t smoothness = clampf(params[SMOOTHNESS_PARAM].value + inputs[SMOOTHNESS_INPUT].value / 5.0, -1.0, 1.0) * 0x7fff;
		generator.set_shape(shape);
		generator.set_slope(slope);
		generator.set_smoothness(smoothness);

		// Sync
		// Slight deviation from spec here.
		// Instead of toggling sync by holding the range button, just enable it if the clock port is plugged in.
		generator.set_sync(inputs[BANK_INPUT].active);

		// Generator
		generator.Process();
	}

	// Level
	uint16_t level = clampf(inputs[LEVEL_INPUT].normalize(8.0) / 8.0, 0.0, 1.0) * 0xffff;
	if (level < 32)
		level = 0;

	uint8_t gate = 0;
	if (inputs[FREEZE_INPUT].value >= 0.7)
		gate |= tides::CONTROL_FREEZE;
	if (inputs[SYNC_INPUT].value >= 0.7)
		gate |= tides::CONTROL_GATE;
	if (inputs[BANK_INPUT].value >= 0.7)
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
	float unif = rescalef(uni, 0, 0xffff, 0.0, 8.0);
	float bif = rescalef(bi, -0x8000, 0x7fff, -5.0, 5.0);

	outputs[BIT_OUTPUT].value = sample.flags & tides::FLAG_END_OF_ATTACK ? 0.0 : 5.0;
	outputs[SUB_OUTPUT].value = sample.flags & tides::FLAG_END_OF_RELEASE ? 0.0 : 5.0;
	outputs[UNI_OUTPUT].value = unif;
	outputs[BI_OUTPUT].value = bif;

	lights[1] = (sample.flags & tides::FLAG_END_OF_ATTACK ? -unif : unif) / 8.0;
}


struct SheepModeLight : ModeValueLight {
	SheepModeLight() {
		addColor(COLOR_RED);
		addColor(COLOR_BLACK_TRANSPARENT);
		addColor(COLOR_CYAN);
	}
};


SheepWidget::SheepWidget() {
	Sheep *module = new Sheep();
	setModule(module);
	box.size = Vec(15 * 14, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load(assetPlugin(plugin, "res/Sheep.png"));
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(180, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(180, 365)));

	addParam(createParam<CKD6>(Vec(19, 52), module, Sheep::BANK_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<CKD6>(Vec(19, 93), module, Sheep::RANGE_PARAM, 0.0, 1.0, 0.0));

	addParam(createParam<Rogan3PSGreen>(Vec(79, 60), module, Sheep::FREQUENCY_PARAM, -48.0, 48.0, 0.0));
	addParam(createParam<Rogan1PSGreen>(Vec(156, 66), module, Sheep::FM_PARAM, -12.0, 12.0, 0.0));

	addParam(createParam<Rogan1PSWhite>(Vec(13, 155), module, Sheep::ROW_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(85, 155), module, Sheep::COLUMN_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(156, 155), module, Sheep::SMOOTHNESS_PARAM, -1.0, 1.0, 0.0));

	addInput(createInput<PJ3410Port>(Vec(18, 216), module, Sheep::ROW_INPUT));
	addInput(createInput<PJ3410Port>(Vec(90, 216), module, Sheep::COLUMN_INPUT));
	addInput(createInput<PJ3410Port>(Vec(161, 216), module, Sheep::SMOOTHNESS_INPUT));

	addInput(createInput<PJ3410Port>(Vec(18, 271), module, Sheep::SYNC_INPUT));
	addInput(createInput<PJ3410Port>(Vec(54, 271), module, Sheep::FREEZE_INPUT));
	addInput(createInput<PJ3410Port>(Vec(90, 271), module, Sheep::PITCH_INPUT));
	addInput(createInput<PJ3410Port>(Vec(125, 271), module, Sheep::FM_INPUT));
	addInput(createInput<PJ3410Port>(Vec(161, 271), module, Sheep::LEVEL_INPUT));

	addInput(createInput<PJ3410Port>(Vec(18, 313), module, Sheep::BANK_INPUT));
	addOutput(createOutput<PJ3410Port>(Vec(54, 313), module, Sheep::BIT_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(90, 313), module, Sheep::SUB_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(125, 313), module, Sheep::UNI_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(161, 313), module, Sheep::BI_OUTPUT));

	addChild(createValueLight<SmallLight<SheepModeLight>>(Vec(57, 62), &module->lights[0]));
	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(57, 83), &module->lights[1]));
	addChild(createValueLight<SmallLight<SheepModeLight>>(Vec(57, 103), &module->lights[2]));
}
