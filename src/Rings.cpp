#include "AudibleInstruments.hpp"
#include <string.h>
#include "rings/dsp/part.h"
#include "rings/dsp/strummer.h"
#include "rings/dsp/string_synth_part.h"


struct Rings : Module {
	enum ParamIds {
		POLYPHONY_PARAM,
		RESONATOR_PARAM,

		FREQUENCY_PARAM,
		STRUCTURE_PARAM,
		BRIGHTNESS_PARAM,
		DAMPING_PARAM,
		POSITION_PARAM,

		BRIGHTNESS_MOD_PARAM,
		FREQUENCY_MOD_PARAM,
		DAMPING_MOD_PARAM,
		STRUCTURE_MOD_PARAM,
		POSITION_MOD_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		BRIGHTNESS_MOD_INPUT,
		FREQUENCY_MOD_INPUT,
		DAMPING_MOD_INPUT,
		STRUCTURE_MOD_INPUT,
		POSITION_MOD_INPUT,

		STRUM_INPUT,
		PITCH_INPUT,
		IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ODD_OUTPUT,
		EVEN_OUTPUT,
		NUM_OUTPUTS
	};

	uint16_t reverb_buffer[32768] = {};
	int bufferFrame = 0;
	float in[24] = {};
	float out[24] = {};
	float aux[24] = {};
	rings::Part part;
	rings::StringSynthPart string_synth;
	rings::Strummer strummer;
	bool strum = false;
	bool lastStrum = false;

	Rings();
	~Rings();
	void step();
};


Rings::Rings() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);

	memset(&strummer, 0, sizeof(strummer));
	memset(&part, 0, sizeof(part));
	memset(&string_synth, 0, sizeof(string_synth));

	strummer.Init(0.01, 44100.0 / 24);
	part.Init(reverb_buffer);
	string_synth.Init(reverb_buffer);
}

Rings::~Rings() {
}

void Rings::step() {
	// TODO Sample rate conversion from 48000 Hz
	// TODO
	// "Normalized to a pulse/burst generator that reacts to note changes on the V/OCT input."
	in[bufferFrame] = clampf(getf(inputs[IN_INPUT])/5.0, -1.0, 1.0);
	// "Note that you need to insert a jack into each output to split the signals: when only one jack is inserted, both signals are mixed together."
	if (outputs[ODD_OUTPUT] && outputs[EVEN_OUTPUT]) {
		setf(outputs[ODD_OUTPUT], clampf(out[bufferFrame], -1.0, 1.0)*5.0);
		setf(outputs[EVEN_OUTPUT], clampf(aux[bufferFrame], -1.0, 1.0)*5.0);
	}
	else {
		float v = clampf(out[bufferFrame] + aux[bufferFrame], -1.0, 1.0)*5.0;
		setf(outputs[ODD_OUTPUT], v);
		setf(outputs[EVEN_OUTPUT], v);
	}

	if (!strum) {
		strum = getf(inputs[STRUM_INPUT]) >= 1.0;
	}

	if (++bufferFrame >= 24) {
		bufferFrame = 0;

		// modes
		int polyphony = 1;
		switch ((int) roundf(params[POLYPHONY_PARAM])) {
			case 1: polyphony = 2; break;
			case 2: polyphony = 4; break;
		}
		if (polyphony != part.polyphony()) {
			part.set_polyphony(polyphony);
		}

		int model = (int) roundf(params[RESONATOR_PARAM]);
		if (0 <= model && model < rings::RESONATOR_MODEL_LAST) {
			part.set_model((rings::ResonatorModel) model);
		}

		// Patch
		rings::Patch patch;
		float structure = params[STRUCTURE_PARAM] + 3.3*quadraticBipolar(params[STRUCTURE_MOD_PARAM])*getf(inputs[STRUCTURE_MOD_INPUT])/5.0;
		patch.structure = clampf(structure, 0.0, 0.9995);
		patch.brightness = clampf(params[BRIGHTNESS_PARAM] + 3.3*quadraticBipolar(params[BRIGHTNESS_MOD_PARAM])*getf(inputs[BRIGHTNESS_MOD_INPUT])/5.0, 0.0, 1.0);
		patch.damping = clampf(params[DAMPING_PARAM] + 3.3*quadraticBipolar(params[DAMPING_MOD_PARAM])*getf(inputs[DAMPING_MOD_INPUT])/5.0, 0.0, 0.9995);
		patch.position = clampf(params[POSITION_PARAM] + 3.3*quadraticBipolar(params[POSITION_MOD_PARAM])*getf(inputs[POSITION_MOD_INPUT])/5.0, 0.0, 0.9995);

		// Performance
		rings::PerformanceState performance_state;
		performance_state.note = 12.0*getf(inputs[PITCH_INPUT], 1/12.0);
		float transpose = params[FREQUENCY_PARAM];
		// Quantize transpose if pitch input is connected
		if (inputs[PITCH_INPUT]) {
			transpose = roundf(transpose);
		}
		performance_state.tonic = 12.0 + clampf(transpose, 0, 60.0);
		performance_state.fm = clampf(48.0 * 3.3*quarticBipolar(params[FREQUENCY_MOD_PARAM]) * getf(inputs[FREQUENCY_MOD_INPUT], 1.0)/5.0, -48.0, 48.0);

		performance_state.internal_exciter = !inputs[IN_INPUT];
		performance_state.internal_strum = !inputs[STRUM_INPUT];
		performance_state.internal_note = !inputs[PITCH_INPUT];

		// TODO
		// "Normalized to a step detector on the V/OCT input and a transient detector on the IN input."
		performance_state.strum = strum && !lastStrum;
		lastStrum = strum;
		strum = false;

		performance_state.chord = clampf(roundf(structure * (rings::kNumChords - 1)), 0, rings::kNumChords - 1);

		// Process audio
		if (0) {
			// strummer.Process(NULL, 24, &performance_state);
			// string_synth.Process(performance_state, patch, in, out, aux, 24);
		}
		else {
			strummer.Process(in, 24, &performance_state);
			part.Process(performance_state, patch, in, out, aux, 24);
		}
	}
}


RingsWidget::RingsWidget() : ModuleWidget(new Rings()) {
	box.size = Vec(15*14, 380);

	{
		AudiblePanel *panel = new AudiblePanel();
		panel->imageFilename = "plugins/AudibleInstruments/res/Rings.png";
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew(Vec(15, 0)));
	addChild(createScrew(Vec(180, 0)));
	addChild(createScrew(Vec(15, 365)));
	addChild(createScrew(Vec(180, 365)));

	addParam(createParam<MediumToggleSwitch>(Vec(14, 40), module, Rings::POLYPHONY_PARAM, 0.0, 2.0, 0.0));
	addParam(createParam<MediumToggleSwitch>(Vec(179, 40), module, Rings::RESONATOR_PARAM, 0.0, 2.0, 0.0));

	addParam(createParam<LargeWhiteKnob>(Vec(30, 73), module, Rings::FREQUENCY_PARAM, 0.0, 60.0, 30.0));
	addParam(createParam<LargeWhiteKnob>(Vec(127, 73), module, Rings::STRUCTURE_PARAM, 0.0, 1.0, 0.5));

	addParam(createParam<SmallWhiteKnob>(Vec(14, 159), module, Rings::BRIGHTNESS_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallWhiteKnob>(Vec(84, 159), module, Rings::DAMPING_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallWhiteKnob>(Vec(155, 159), module, Rings::POSITION_PARAM, 0.0, 1.0, 0.5));

	addParam(createParam<TinyBlackKnob>(Vec(19, 229), module, Rings::BRIGHTNESS_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(57, 229), module, Rings::FREQUENCY_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(95, 229), module, Rings::DAMPING_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(134, 229), module, Rings::STRUCTURE_MOD_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(172, 229), module, Rings::POSITION_MOD_PARAM, -1.0, 1.0, 0.0));

	addInput(createInput(Vec(18, 275), module, Rings::BRIGHTNESS_MOD_INPUT));
	addInput(createInput(Vec(56, 275), module, Rings::FREQUENCY_MOD_INPUT));
	addInput(createInput(Vec(95, 275), module, Rings::DAMPING_MOD_INPUT));
	addInput(createInput(Vec(133, 275), module, Rings::STRUCTURE_MOD_INPUT));
	addInput(createInput(Vec(171, 275), module, Rings::POSITION_MOD_INPUT));

	addInput(createInput(Vec(18, 318), module, Rings::STRUM_INPUT));
	addInput(createInput(Vec(56, 318), module, Rings::PITCH_INPUT));
	addInput(createInput(Vec(95, 318), module, Rings::IN_INPUT));
	addOutput(createOutput(Vec(133, 318), module, Rings::ODD_OUTPUT));
	addOutput(createOutput(Vec(171, 318), module, Rings::EVEN_OUTPUT));

	Rings *rings = dynamic_cast<Rings*>(module);
	addChild(createValueLight<SmallModeLight>(Vec(39, 45), &rings->params[Rings::POLYPHONY_PARAM]));
	addChild(createValueLight<SmallModeLight>(Vec(164, 45), &rings->params[Rings::RESONATOR_PARAM]));
}
