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
	enum LightIds {
		CARRIER_GREEN_LIGHT, CARRIER_RED_LIGHT,
		ALGORITHM_LIGHT,
		NUM_LIGHTS = ALGORITHM_LIGHT + 3
	};


	int frame = 0;
	warps::Modulator modulator;
	warps::ShortFrame inputFrames[60] = {};
	warps::ShortFrame outputFrames[60] = {};
	SchmittTrigger stateTrigger;

	Warps();
	void step() override;

	json_t *toJson() override {
		json_t *rootJ = json_object();
		warps::Parameters *p = modulator.mutable_parameters();
		json_object_set_new(rootJ, "shape", json_integer(p->carrier_shape));
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		json_t *shapeJ = json_object_get(rootJ, "shape");
		warps::Parameters *p = modulator.mutable_parameters();
		if (shapeJ) {
			p->carrier_shape = json_integer_value(shapeJ);
		}
	}

	void reset() override {
		warps::Parameters *p = modulator.mutable_parameters();
		p->carrier_shape = 0;
	}

	void randomize() override {
		warps::Parameters *p = modulator.mutable_parameters();
		p->carrier_shape = randomu32() % 4;
	}
};


Warps::Warps() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
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
	lights[CARRIER_GREEN_LIGHT].value = (p->carrier_shape == 1 || p->carrier_shape == 2) ? 1.0 : 0.0;
	lights[CARRIER_RED_LIGHT].value = (p->carrier_shape == 2 || p->carrier_shape == 3) ? 1.0 : 0.0;

	// Buffer loop
	if (++frame >= 60) {
		frame = 0;

		p->channel_drive[0] = clampf(params[LEVEL1_PARAM].value + inputs[LEVEL1_INPUT].value / 5.0, 0.0, 1.0);
		p->channel_drive[1] = clampf(params[LEVEL2_PARAM].value + inputs[LEVEL2_INPUT].value / 5.0, 0.0, 1.0);
		p->modulation_algorithm = clampf(params[ALGORITHM_PARAM].value / 8.0 + inputs[ALGORITHM_INPUT].value / 5.0, 0.0, 1.0);

		{
			// TODO
			// Use the correct light color
			NVGcolor algorithmColor = nvgHSL(p->modulation_algorithm, 0.3, 0.4);
			lights[ALGORITHM_LIGHT + 0].setBrightness(algorithmColor.r);
			lights[ALGORITHM_LIGHT + 1].setBrightness(algorithmColor.g);
			lights[ALGORITHM_LIGHT + 2].setBrightness(algorithmColor.b);
		}

		p->modulation_parameter = clampf(params[TIMBRE_PARAM].value + inputs[TIMBRE_INPUT].value / 5.0, 0.0, 1.0);

		p->frequency_shift_pot = params[ALGORITHM_PARAM].value / 8.0;
		p->frequency_shift_cv = clampf(inputs[ALGORITHM_INPUT].value / 5.0, -1.0, 1.0);
		p->phase_shift = p->modulation_algorithm;
		p->note = 60.0 * params[LEVEL1_PARAM].value + 12.0 * inputs[LEVEL1_INPUT].normalize(2.0) + 12.0;
		p->note += log2f(96000.0 / engineGetSampleRate()) * 12.0;

		modulator.Process(inputFrames, outputFrames, 60);
	}

	inputFrames[frame].l = clampf(inputs[CARRIER_INPUT].value / 16.0 * 0x8000, -0x8000, 0x7fff);
	inputFrames[frame].r = clampf(inputs[MODULATOR_INPUT].value / 16.0 * 0x8000, -0x8000, 0x7fff);
	outputs[MODULATOR_OUTPUT].value = (float)outputFrames[frame].l / 0x8000 * 5.0;
	outputs[AUX_OUTPUT].value = (float)outputFrames[frame].r / 0x8000 * 5.0;
}


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

	addParam(createParam<Rogan6PSWhite>(Vec(29, 52), module, Warps::ALGORITHM_PARAM, 0.0, 8.0, 0.0));

	addParam(createParam<Rogan1PSWhite>(Vec(94, 173), module, Warps::TIMBRE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<TL1105>(Vec(16, 182), module, Warps::STATE_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(14, 213), module, Warps::LEVEL1_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<Trimpot>(Vec(53, 213), module, Warps::LEVEL2_PARAM, 0.0, 1.0, 1.0));

	addInput(createInput<PJ301MPort>(Vec(8, 273), module, Warps::LEVEL1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(44, 273), module, Warps::LEVEL2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(80, 273), module, Warps::ALGORITHM_INPUT));
	addInput(createInput<PJ301MPort>(Vec(116, 273), module, Warps::TIMBRE_INPUT));

	addInput(createInput<PJ301MPort>(Vec(8, 316), module, Warps::CARRIER_INPUT));
	addInput(createInput<PJ301MPort>(Vec(44, 316), module, Warps::MODULATOR_INPUT));
	addOutput(createOutput<PJ301MPort>(Vec(80, 316), module, Warps::MODULATOR_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(116, 316), module, Warps::AUX_OUTPUT));

	addChild(createLight<SmallLight<GreenRedLight>>(Vec(20, 167), module, Warps::CARRIER_GREEN_LIGHT));
	{
		RedGreenBlueLight *algorithmLight = new RedGreenBlueLight();
		algorithmLight->box.pos = Vec(40, 63);
		algorithmLight->box.size = Vec(71, 71);
		algorithmLight->module = module;
		algorithmLight->lightId = Warps::ALGORITHM_LIGHT;
		addChild(algorithmLight);
	}
}
