#include "AudibleInstruments.hpp"
#include <string.h>
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

	uint16_t reverb_buffer[32768] = {};
	elements::Part *part;
	int bufferFrame = 0;
	float blow[16] = {};
	float strike[16] = {};
	float main[16] = {};
	float aux[16] = {};
	float lights[2] = {};

	Elements();
	~Elements();
	void step();
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
	// TODO Sample rate convert from 32000Hz
	blow[bufferFrame] = getf(inputs[BLOW_INPUT]);
	strike[bufferFrame] = getf(inputs[STRIKE_INPUT]);
	setf(outputs[AUX_OUTPUT], 5.0*aux[bufferFrame]);
	setf(outputs[MAIN_OUTPUT], 5.0*main[bufferFrame]);

	if (++bufferFrame >= 16) {
		bufferFrame = 0;
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
		performance.note = 12.0*getf(inputs[NOTE_INPUT]) + roundf(params[COARSE_PARAM]) + 3.3*quadraticBipolar(params[FINE_PARAM]) + 69.0;
		performance.modulation = 3.3*quarticBipolar(params[FM_PARAM]) * 49.5 * getf(inputs[FM_INPUT])/5.0;
		performance.gate = params[PLAY_PARAM] >= 1.0 || getf(inputs[GATE_INPUT]) >= 1.0;
		performance.strength = clampf(1.0 - getf(inputs[STRENGTH_INPUT])/5.0, 0.0, 1.0);

		// Generate audio
		part->Process(performance, blow, strike, main, aux, 16);

		// Set lights
		lights[0] = part->exciter_level();
		lights[1] = part->resonator_level();
	}
}


ElementsWidget::ElementsWidget() : ModuleWidget(new Elements()) {
	box.size = Vec(15*34, 380);

	{
		AudiblePanel *panel = new AudiblePanel();
		panel->imageFilename = "plugins/AudibleInstruments/res/Elements.png";
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew(Vec(15, 0)));
	addChild(createScrew(Vec(480, 0)));
	addChild(createScrew(Vec(15, 365)));
	addChild(createScrew(Vec(480, 365)));

	addParam(createParam<SmallWhiteKnob>(Vec(49-20, 63-20), module, Elements::CONTOUR_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallWhiteKnob>(Vec(120-20, 63-20), module, Elements::BOW_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallRedKnob>(Vec(190-20, 63-20), module, Elements::BLOW_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallGreenKnob>(Vec(260-20, 63-20), module, Elements::STRIKE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallWhiteKnob>(Vec(331-20, 63-20), module, Elements::COARSE_PARAM, -30.0, 30.0, 0.0));
	addParam(createParam<SmallWhiteKnob>(Vec(402-20, 63-20), module, Elements::FINE_PARAM, -2.0, 2.0, 0.0));
	addParam(createParam<SmallWhiteKnob>(Vec(472-20, 63-20), module, Elements::FM_PARAM, -1.0, 1.0, 0.0));

	addParam(createParam<LargeRedKnob>(Vec(142-26, 143-26), module, Elements::FLOW_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<LargeGreenKnob>(Vec(239-26, 143-26), module, Elements::MALLET_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<LargeWhiteKnob>(Vec(353-26, 143-26), module, Elements::GEOMETRY_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<LargeWhiteKnob>(Vec(450-26, 143-26), module, Elements::BRIGHTNESS_PARAM, 0.0, 1.0, 0.5));

	addParam(createParam<SmallWhiteKnob>(Vec(120-20, 223-20), module, Elements::BOW_TIMBRE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallRedKnob>(Vec(191-20, 223-20), module, Elements::BLOW_TIMBRE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallGreenKnob>(Vec(260-20, 223-20), module, Elements::STRIKE_TIMBRE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallWhiteKnob>(Vec(331-20, 223-20), module, Elements::DAMPING_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallWhiteKnob>(Vec(401-20, 223-20), module, Elements::POSITION_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallWhiteKnob>(Vec(472-20, 223-20), module, Elements::SPACE_PARAM, 0.0, 2.0, 0.0));

	addParam(createParam<TinyBlackKnob>(Vec(113-9, 283-9), module, Elements::BOW_TIMBRE_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(152-9, 283-9), module, Elements::FLOW_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(190-9, 283-9), module, Elements::BLOW_TIMBRE_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(229-9, 283-9), module, Elements::MALLET_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(267-9, 283-9), module, Elements::STRIKE_TIMBRE_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(325-9, 283-9), module, Elements::DAMPING_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(363-9, 283-9), module, Elements::GEOMETRY_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(402-9, 283-9), module, Elements::POSITION_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(440-9, 283-9), module, Elements::BRIGHTNESS_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(479-9, 283-9), module, Elements::SPACE_MOD_PARAM, -2.0, 2.0, 0.0));

	addInput(createInput(Vec(32-10, 190-10), module, Elements::NOTE_INPUT));
	addInput(createInput(Vec(68-10, 190-10), module, Elements::FM_INPUT));

	addInput(createInput(Vec(32-10, 236-10), module, Elements::GATE_INPUT));
	addInput(createInput(Vec(68-10, 236-10), module, Elements::STRENGTH_INPUT));

	addInput(createInput(Vec(32-10, 282-10), module, Elements::BLOW_INPUT));
	addInput(createInput(Vec(68-10, 282-10), module, Elements::STRIKE_INPUT));

	addOutput(createOutput(Vec(32-10, 328-10), module, Elements::AUX_OUTPUT));
	addOutput(createOutput(Vec(68-10, 328-10), module, Elements::MAIN_OUTPUT));

	addInput(createInput(Vec(113-10, 328-10), module, Elements::BOW_TIMBRE_MOD_INPUT));
	addInput(createInput(Vec(152-10, 328-10), module, Elements::FLOW_MOD_INPUT));
	addInput(createInput(Vec(190-10, 328-10), module, Elements::BLOW_TIMBRE_MOD_INPUT));
	addInput(createInput(Vec(229-10, 328-10), module, Elements::MALLET_MOD_INPUT));
	addInput(createInput(Vec(267-10, 328-10), module, Elements::STRIKE_TIMBRE_MOD_INPUT));
	addInput(createInput(Vec(325-10, 328-10), module, Elements::DAMPING_MOD_INPUT));
	addInput(createInput(Vec(363-10, 328-10), module, Elements::GEOMETRY_MOD_INPUT));
	addInput(createInput(Vec(402-10, 328-10), module, Elements::POSITION_MOD_INPUT));
	addInput(createInput(Vec(440-10, 328-10), module, Elements::BRIGHTNESS_MOD_INPUT));
	addInput(createInput(Vec(479-10, 328-10), module, Elements::SPACE_MOD_INPUT));
	// addInputPort(createInputPort(Vec(479-10, 328-10), module, Elements::SPACE_MOD_INPUT));

	addParam(createParam<LargeMomentarySwitch>(Vec(36, 116), module, Elements::PLAY_PARAM, 0.0, 2.0, 0.0));

	Elements *elements = dynamic_cast<Elements*>(module);
	addChild(createValueLight<MediumValueLight>(Vec(184, 168), &elements->lights[0]));
	addChild(createValueLight<MediumValueLight>(Vec(395, 168), &elements->lights[1]));
}
