#include <string.h>
#include "AudibleInstruments.hpp"
#include "dsp.hpp"
#include "elements/dsp/part.h"


struct Elements : Module {
	enum ParamIds {
		CONTOUR_PARAM,
		BOW_PARAM,
		BLOW_PARAM,
		STRIKE_PARAM,
		COARSE_PARAM,
		FINE_PARAM,
		FM_PARAM,

		FLOW_PARAM,
		MALLET_PARAM,
		GEOMETRY_PARAM,
		BRIGHTNESS_PARAM,

		BOW_TIMBRE_PARAM,
		BLOW_TIMBRE_PARAM,
		STRIKE_TIMBRE_PARAM,
		DAMPING_PARAM,
		POSITION_PARAM,
		SPACE_PARAM,

		BOW_TIMBRE_MOD_PARAM,
		FLOW_MOD_PARAM,
		BLOW_TIMBRE_MOD_PARAM,
		MALLET_MOD_PARAM,
		STRIKE_TIMBRE_MOD_PARAM,
		DAMPING_MOD_PARAM,
		GEOMETRY_MOD_PARAM,
		POSITION_MOD_PARAM,
		BRIGHTNESS_MOD_PARAM,
		SPACE_MOD_PARAM,

		PLAY_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NOTE_INPUT,
		FM_INPUT,
		GATE_INPUT,
		STRENGTH_INPUT,
		BLOW_INPUT,
		STRIKE_INPUT,

		BOW_TIMBRE_MOD_INPUT,
		FLOW_MOD_INPUT,
		BLOW_TIMBRE_MOD_INPUT,
		MALLET_MOD_INPUT,
		STRIKE_TIMBRE_MOD_INPUT,
		DAMPING_MOD_INPUT,
		GEOMETRY_MOD_INPUT,
		POSITION_MOD_INPUT,
		BRIGHTNESS_MOD_INPUT,
		SPACE_MOD_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		AUX_OUTPUT,
		MAIN_OUTPUT,
		NUM_OUTPUTS
	};

	SampleRateConverter<2> inputSrc;
	SampleRateConverter<2> outputSrc;
	DoubleRingBuffer<Frame<2>, 256> inputBuffer;
	DoubleRingBuffer<Frame<2>, 256> outputBuffer;

	uint16_t reverb_buffer[32768] = {};
	elements::Part *part;
	float lights[2] = {};

	Elements();
	~Elements();
	void step();

	json_t *toJson() {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "model", json_integer(getModel()));
		return rootJ;
	}

	void fromJson(json_t *rootJ) {
		json_t *modelJ = json_object_get(rootJ, "model");
		if (modelJ) {
			setModel(json_integer_value(modelJ));
		}
	}

	int getModel() {
		return (int)part->resonator_model();
	}

	void setModel(int model) {
		part->set_resonator_model((elements::ResonatorModel)model);
	}
};


Elements::Elements() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);

	part = new elements::Part();
	// In the Mutable Instruments code, Part doesn't initialize itself, so zero it here.
	memset(part, 0, sizeof(*part));
	part->Init(reverb_buffer);
	// Just some random numbers
	uint32_t seed[3] = {1, 2, 3};
	part->Seed(seed, 3);
}

Elements::~Elements() {
	delete part;
}

void Elements::step() {
	// Get input
	if (!inputBuffer.full()) {
		Frame<2> inputFrame;
		inputFrame.samples[0] = getf(inputs[BLOW_INPUT]) / 5.0;
		inputFrame.samples[1] = getf(inputs[STRIKE_INPUT]) / 5.0;
		inputBuffer.push(inputFrame);
	}

	// Render frames
	if (outputBuffer.empty()) {
		float blow[16] = {};
		float strike[16] = {};
		float main[16];
		float aux[16];

		// Convert input buffer
		{
			inputSrc.setRatio(32000.0 / gSampleRate);
			Frame<2> inputFrames[16];
			int inLen = inputBuffer.size();
			int outLen = 16;
			inputSrc.process(inputBuffer.startData(), &inLen, inputFrames, &outLen);
			inputBuffer.startIncr(inLen);

			for (int i = 0; i < outLen; i++) {
				blow[i] = inputFrames[i].samples[0];
				strike[i] = inputFrames[i].samples[1];
			}
		}

		// Set patch from parameters
		elements::Patch* p = part->mutable_patch();
		p->exciter_envelope_shape = params[CONTOUR_PARAM];
		p->exciter_bow_level = params[BOW_PARAM];
		p->exciter_blow_level = params[BLOW_PARAM];
		p->exciter_strike_level = params[STRIKE_PARAM];

#define BIND(_p, _m, _i) clampf(params[_p] + 3.3*quadraticBipolar(params[_m])*getf(inputs[_i])/5.0, 0.0, 0.9995)

		p->exciter_bow_timbre = BIND(BOW_TIMBRE_PARAM, BOW_TIMBRE_MOD_PARAM, BOW_TIMBRE_MOD_INPUT);
		p->exciter_blow_meta = BIND(FLOW_PARAM, FLOW_MOD_PARAM, FLOW_MOD_INPUT);
		p->exciter_blow_timbre = BIND(BLOW_TIMBRE_PARAM, BLOW_TIMBRE_MOD_PARAM, BLOW_TIMBRE_MOD_INPUT);
		p->exciter_strike_meta = BIND(MALLET_PARAM, MALLET_MOD_PARAM, MALLET_MOD_INPUT);
		p->exciter_strike_timbre = BIND(STRIKE_TIMBRE_PARAM, STRIKE_TIMBRE_MOD_PARAM, STRIKE_TIMBRE_MOD_INPUT);
		p->resonator_geometry = BIND(GEOMETRY_PARAM, GEOMETRY_MOD_PARAM, GEOMETRY_MOD_INPUT);
		p->resonator_brightness = BIND(BRIGHTNESS_PARAM, BRIGHTNESS_MOD_PARAM, BRIGHTNESS_MOD_INPUT);
		p->resonator_damping = BIND(DAMPING_PARAM, DAMPING_MOD_PARAM, DAMPING_MOD_INPUT);
		p->resonator_position = BIND(POSITION_PARAM, POSITION_MOD_PARAM, POSITION_MOD_INPUT);
		p->space = clampf(params[SPACE_PARAM] + params[SPACE_MOD_PARAM]*getf(inputs[SPACE_MOD_INPUT])/5.0, 0.0, 2.0);

		// Get performance inputs
		elements::PerformanceState performance;
		performance.note = 12.0*getf(inputs[NOTE_INPUT]) + roundf(params[COARSE_PARAM]) + params[FINE_PARAM] + 69.0;
		performance.modulation = 3.3*quarticBipolar(params[FM_PARAM]) * 49.5 * getf(inputs[FM_INPUT])/5.0;
		performance.gate = params[PLAY_PARAM] >= 1.0 || getf(inputs[GATE_INPUT]) >= 1.0;
		performance.strength = clampf(1.0 - getf(inputs[STRENGTH_INPUT])/5.0, 0.0, 1.0);

		// Generate audio
		part->Process(performance, blow, strike, main, aux, 16);

		// Convert output buffer
		{
			Frame<2> outputFrames[16];
			for (int i = 0; i < 16; i++) {
				outputFrames[i].samples[0] = main[i];
				outputFrames[i].samples[1] = aux[i];
			}

			outputSrc.setRatio(gSampleRate / 32000.0);
			int inLen = 16;
			int outLen = outputBuffer.capacity();
			outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
			outputBuffer.endIncr(outLen);
		}

		// Set lights
		lights[0] = part->exciter_level();
		lights[1] = part->resonator_level();
	}

	// Set output
	if (!outputBuffer.empty()) {
		Frame<2> outputFrame = outputBuffer.shift();
		setf(outputs[AUX_OUTPUT], 5.0 * outputFrame.samples[0]);
		setf(outputs[MAIN_OUTPUT], 5.0 * outputFrame.samples[1]);
	}
}


ElementsWidget::ElementsWidget() {
	Elements *module = new Elements();
	setModule(module);
	box.size = Vec(15*34, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load("plugins/AudibleInstruments/res/Elements.png");
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(480, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(480, 365)));

	addParam(createParam<Rogan1PSWhite>(Vec(29, 43), module, Elements::CONTOUR_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<Rogan1PSWhite>(Vec(100, 43), module, Elements::BOW_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSRed>(Vec(170, 43), module, Elements::BLOW_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Rogan1PSGreen>(Vec(240, 43), module, Elements::STRIKE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSWhite>(Vec(311, 43), module, Elements::COARSE_PARAM, -30.0, 30.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(382, 43), module, Elements::FINE_PARAM, -2.0, 2.0, 0.0));
	addParam(createParam<Rogan1PSWhite>(Vec(452, 43), module, Elements::FM_PARAM, -1.0, 1.0, 0.0));

	addParam(createParam<Rogan3PSRed>(Vec(116, 117), module, Elements::FLOW_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan3PSGreen>(Vec(213, 117), module, Elements::MALLET_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan3PSWhite>(Vec(327, 117), module, Elements::GEOMETRY_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan3PSWhite>(Vec(424, 117), module, Elements::BRIGHTNESS_PARAM, 0.0, 1.0, 0.5));

	addParam(createParam<Rogan1PSWhite>(Vec(100, 203), module, Elements::BOW_TIMBRE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSRed>(Vec(171, 203), module, Elements::BLOW_TIMBRE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSGreen>(Vec(240, 203), module, Elements::STRIKE_TIMBRE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSWhite>(Vec(311, 203), module, Elements::DAMPING_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSWhite>(Vec(381, 203), module, Elements::POSITION_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSWhite>(Vec(452, 203), module, Elements::SPACE_PARAM, 0.0, 2.0, 0.0));

	addParam(createParam<Trimpot>(Vec(104, 274), module, Elements::BOW_TIMBRE_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(143, 274), module, Elements::FLOW_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(181, 274), module, Elements::BLOW_TIMBRE_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(220, 274), module, Elements::MALLET_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(258, 274), module, Elements::STRIKE_TIMBRE_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(316, 274), module, Elements::DAMPING_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(354, 274), module, Elements::GEOMETRY_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(393, 274), module, Elements::POSITION_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(431, 274), module, Elements::BRIGHTNESS_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Trimpot>(Vec(470, 274), module, Elements::SPACE_MOD_PARAM, -2.0, 2.0, 0.0));

	addInput(createInput<PJ3410Port>(Vec(16, 175), module, Elements::NOTE_INPUT));
	addInput(createInput<PJ3410Port>(Vec(52, 175), module, Elements::FM_INPUT));

	addInput(createInput<PJ3410Port>(Vec(16, 221), module, Elements::GATE_INPUT));
	addInput(createInput<PJ3410Port>(Vec(52, 221), module, Elements::STRENGTH_INPUT));

	addInput(createInput<PJ3410Port>(Vec(16, 267), module, Elements::BLOW_INPUT));
	addInput(createInput<PJ3410Port>(Vec(52, 267), module, Elements::STRIKE_INPUT));

	addOutput(createOutput<PJ3410Port>(Vec(16, 313), module, Elements::AUX_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(52, 313), module, Elements::MAIN_OUTPUT));

	addInput(createInput<PJ3410Port>(Vec(97, 313), module, Elements::BOW_TIMBRE_MOD_INPUT));
	addInput(createInput<PJ3410Port>(Vec(136, 313), module, Elements::FLOW_MOD_INPUT));
	addInput(createInput<PJ3410Port>(Vec(174, 313), module, Elements::BLOW_TIMBRE_MOD_INPUT));
	addInput(createInput<PJ3410Port>(Vec(213, 313), module, Elements::MALLET_MOD_INPUT));
	addInput(createInput<PJ3410Port>(Vec(251, 313), module, Elements::STRIKE_TIMBRE_MOD_INPUT));
	addInput(createInput<PJ3410Port>(Vec(309, 313), module, Elements::DAMPING_MOD_INPUT));
	addInput(createInput<PJ3410Port>(Vec(347, 313), module, Elements::GEOMETRY_MOD_INPUT));
	addInput(createInput<PJ3410Port>(Vec(386, 313), module, Elements::POSITION_MOD_INPUT));
	addInput(createInput<PJ3410Port>(Vec(424, 313), module, Elements::BRIGHTNESS_MOD_INPUT));
	addInput(createInput<PJ3410Port>(Vec(463, 313), module, Elements::SPACE_MOD_INPUT));

	addParam(createParam<CKD6>(Vec(36, 116), module, Elements::PLAY_PARAM, 0.0, 1.0, 0.0));

	addChild(createValueLight<MediumLight<GreenRedPolarityLight>>(Vec(184, 165), &module->lights[0]));
	addChild(createValueLight<MediumLight<GreenRedPolarityLight>>(Vec(395, 165), &module->lights[1]));
}

struct ModelItem : MenuItem {
	ModuleWidget *moduleWidget;
	int model;
	void onAction() {
		Elements *module = dynamic_cast<Elements*>(moduleWidget->module);
		assert(module);
		module->setModel(model);
	}
	void step() {
		Elements *module = dynamic_cast<Elements*>(moduleWidget->module);
		assert(module);
		rightText = (module->getModel() == model) ? "Enabled" : "";
	}
};

Menu *ElementsWidget::createContextMenu() {
	Menu *menu = ModuleWidget::createContextMenu();

	MenuLabel *spacerLabel = new MenuLabel();
	menu->pushChild(spacerLabel);

	MenuLabel *modeLabel = new MenuLabel();
	modeLabel->text = "Alternative Models";
	menu->pushChild(modeLabel);

	ModelItem *originalItem = new ModelItem();
	originalItem->text = "Original";
	originalItem->moduleWidget = this;
	originalItem->model = 0;
	menu->pushChild(originalItem);

	ModelItem *stringItem = new ModelItem();
	stringItem->text = "Non-linear string";
	stringItem->moduleWidget = this;
	stringItem->model = 1;
	menu->pushChild(stringItem);

	ModelItem *chordsItem = new ModelItem();
	chordsItem->text = "Chords";
	chordsItem->moduleWidget = this;
	chordsItem->model = 2;
	menu->pushChild(chordsItem);

	return menu;
}
