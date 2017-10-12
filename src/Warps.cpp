#include <string.h>
#include "AudibleInstruments.hpp"
#include "dsp/digital.hpp"
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
	float lights[2] = {};
	SchmittTrigger stateTrigger;

	Warps();
	void step();

	json_t *toJson() {
		json_t *rootJ = json_object();
		warps::Parameters *p = modulator.mutable_parameters();
		json_object_set_new(rootJ, "shape", json_integer(p->carrier_shape));
		return rootJ;
	}

	void fromJson(json_t *rootJ) {
		json_t *shapeJ = json_object_get(rootJ, "shape");
		warps::Parameters *p = modulator.mutable_parameters();
		if (shapeJ) {
			p->carrier_shape = json_integer_value(shapeJ);
		}
	}

	void initialize() {
		warps::Parameters *p = modulator.mutable_parameters();
		p->carrier_shape = 0;
	}

	void randomize() {
		warps::Parameters *p = modulator.mutable_parameters();
		p->carrier_shape = randomu32() % 4;
	}
};


Warps::Warps() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {
	memset(&modulator, 0, sizeof(modulator));
	modulator.Init(96000.0f);

	stateTrigger.setThresholds(0.0, 1.0);
}

void Warps::step() {
	// State trigger
	warps::Parameters *p = modulator.mutable_parameters();
	if (stateTrigger.process(params[STATE_PARAM].value)) {
		p->carrier_shape = (p->carrier_shape + 1) % 4;
	}
	lights[0] = p->carrier_shape;

	// Buffer loop
	if (++frame >= 60) {
		frame = 0;

		p->channel_drive[0] = clampf(params[LEVEL1_PARAM].value + inputs[LEVEL1_INPUT].value / 5.0, 0.0, 1.0);
		p->channel_drive[1] = clampf(params[LEVEL2_PARAM].value + inputs[LEVEL2_INPUT].value / 5.0, 0.0, 1.0);
		p->modulation_algorithm = clampf(params[ALGORITHM_PARAM].value / 8.0 + inputs[ALGORITHM_INPUT].value / 5.0, 0.0, 1.0);
		lights[1] = p->modulation_algorithm * 8.0;
		p->modulation_parameter = clampf(params[TIMBRE_PARAM].value + inputs[TIMBRE_INPUT].value / 5.0, 0.0, 1.0);

		p->frequency_shift_pot = params[ALGORITHM_PARAM].value / 8.0;
		p->frequency_shift_cv = clampf(inputs[ALGORITHM_INPUT].value / 5.0, -1.0, 1.0);
		p->phase_shift = p->modulation_algorithm;
		p->note = 60.0 * params[LEVEL1_PARAM].value + 12.0 * inputs[LEVEL1_INPUT].normalize(2.0) + 12.0;
		p->note += log2f(96000.0 / gSampleRate) * 12.0;

		modulator.Process(inputFrames, outputFrames, 60);
	}

	inputFrames[frame].l = clampf(inputs[CARRIER_INPUT].value / 16.0 * 0x8000, -0x8000, 0x7fff);
	inputFrames[frame].r = clampf(inputs[MODULATOR_INPUT].value / 16.0 * 0x8000, -0x8000, 0x7fff);
	outputs[MODULATOR_OUTPUT].value = (float)outputFrames[frame].l / 0x8000 * 5.0;
	outputs[AUX_OUTPUT].value = (float)outputFrames[frame].r / 0x8000 * 5.0;
}


struct WarpsModeLight : ModeValueLight {
	WarpsModeLight() {
		addColor(COLOR_BLACK_TRANSPARENT);
		addColor(COLOR_GREEN);
		addColor(COLOR_YELLOW);
		addColor(COLOR_RED);
	}
};

struct WarpsAlgoLight : ValueLight {
	WarpsAlgoLight() {
		box.size = Vec(67, 67);
	}

	void step() {
		// TODO Set these to Warps' actual colors
		static NVGcolor colors[9] = {
			nvgHSL(0.5, 0.3, 0.85),
			nvgHSL(0.6, 0.3, 0.85),
			nvgHSL(0.7, 0.3, 0.85),
			nvgHSL(0.8, 0.3, 0.85),
			nvgHSL(0.9, 0.3, 0.85),
			nvgHSL(0.0, 0.3, 0.85),
			nvgHSL(0.1, 0.3, 0.85),
			nvgHSL(0.2, 0.3, 0.85),
			nvgHSL(0.3, 0.3, 0.85),
		};
		int i = clampi((int) *value, 0, 7);
		NVGcolor color0 = colors[i];
		NVGcolor color1 = colors[i + 1];
		float p = fmodf(*value, 1.0);
		color = nvgLerpRGBA(color0, color1, p);
	}
};


WarpsWidget::WarpsWidget() {
	Warps *module = new Warps();
	setModule(module);
	box.size = Vec(15*10, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load(assetPlugin(plugin, "res/Warps.png"));
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(120, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(120, 365)));

	addParam(createParam<Rogan6PSWhite>(Vec(30, 53), module, Warps::ALGORITHM_PARAM, 0.0, 8.0, 0.0));

	addParam(createParam<Rogan1PSWhite>(Vec(95, 173), module, Warps::TIMBRE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<TL1105>(Vec(16, 182), module, Warps::STATE_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(15, 214), module, Warps::LEVEL1_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<Trimpot>(Vec(54, 214), module, Warps::LEVEL2_PARAM, 0.0, 1.0, 1.0));

	addInput(createInput<PJ3410Port>(Vec(5, 270), module, Warps::LEVEL1_INPUT));
	addInput(createInput<PJ3410Port>(Vec(41, 270), module, Warps::LEVEL2_INPUT));
	addInput(createInput<PJ3410Port>(Vec(77, 270), module, Warps::ALGORITHM_INPUT));
	addInput(createInput<PJ3410Port>(Vec(113, 270), module, Warps::TIMBRE_INPUT));

	addInput(createInput<PJ3410Port>(Vec(5, 313), module, Warps::CARRIER_INPUT));
	addInput(createInput<PJ3410Port>(Vec(41, 313), module, Warps::MODULATOR_INPUT));
	addOutput(createOutput<PJ3410Port>(Vec(77, 313), module, Warps::MODULATOR_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(113, 313), module, Warps::AUX_OUTPUT));

	addChild(createValueLight<SmallLight<WarpsModeLight>>(Vec(20, 167), &module->lights[0]));
	addChild(createValueLight<WarpsAlgoLight>(Vec(41, 64), &module->lights[1]));
}
