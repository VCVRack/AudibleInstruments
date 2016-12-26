#include "AudibleInstruments.hpp"
#include <string.h>
#include "clouds/dsp/granular_processor.h"


struct Clouds : Module {
	enum ParamIds {
		POSITION_PARAM,
		SIZE_PARAM,
		PITCH_PARAM,
		IN_GAIN_PARAM,
		DENSITY_PARAM,
		TEXTURE_PARAM,
		BLEND_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FREEZE_INPUT,
		TRIG_INPUT,
		POSITION_INPUT,
		SIZE_INPUT,
		PITCH_INPUT,
		BLEND_INPUT,
		IN_L_INPUT,
		IN_R_INPUT,
		DENSITY_INPUT,
		TEXTURE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_L_OUTPUT,
		OUT_R_OUTPUT,
		NUM_OUTPUTS
	};

	uint8_t *block_mem;
	uint8_t *block_ccm;
	clouds::GranularProcessor *processor;
	int bufferFrame = 0;
	float inL[32] = {};
	float inR[32] = {};
	float outL[32] = {};
	float outR[32] = {};
	bool triggered = false;

	Clouds();
	~Clouds();
	void step();
};


Clouds::Clouds() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);

	const int memLen = 118784;
	const int ccmLen = 65536 - 128;
	block_mem = new uint8_t[memLen];
	memset(block_mem, 0, memLen);
	block_ccm = new uint8_t[ccmLen];
	memset(block_ccm, 0, ccmLen);
	processor = new clouds::GranularProcessor();
	memset(processor, 0, sizeof(*processor));

	processor->Init(block_mem, memLen, block_ccm, ccmLen);
}

Clouds::~Clouds() {
	delete processor;
	delete[] block_mem;
	delete[] block_ccm;
}

void Clouds::step() {
	// TODO Sample rate conversion from 32000 Hz
	inL[bufferFrame] = getf(inputs[IN_L_INPUT]);
	inR[bufferFrame] = getf(inputs[IN_R_INPUT]);
	setf(outputs[OUT_L_OUTPUT], outL[bufferFrame]);
	setf(outputs[OUT_R_OUTPUT], outR[bufferFrame]);

	// Trigger
	if (getf(inputs[TRIG_INPUT]) >= 1.0) {
		triggered = true;
	}

	if (++bufferFrame >= 32) {
		bufferFrame = 0;
		processor->set_num_channels(2);
		processor->set_low_fidelity(false);
		processor->set_playback_mode(clouds::PLAYBACK_MODE_GRANULAR);
		processor->Prepare();

		clouds::Parameters* p = processor->mutable_parameters();
		p->trigger = triggered;
		p->freeze = (getf(inputs[FREEZE_INPUT]) >= 1.0);
		p->position = clampf(params[POSITION_PARAM] + getf(inputs[POSITION_INPUT]) / 5.0, 0.0, 1.0);
		p->size = clampf(params[SIZE_PARAM] + getf(inputs[SIZE_INPUT]) / 5.0, 0.0, 1.0);
		p->pitch = clampf((params[PITCH_PARAM] + getf(inputs[PITCH_INPUT])) * 12.0, -48.0, 48.0);
		p->density = clampf(params[DENSITY_PARAM] + getf(inputs[DENSITY_INPUT]) / 5.0, 0.0, 1.0);
		p->texture = clampf(params[TEXTURE_PARAM] + getf(inputs[TEXTURE_INPUT]) / 5.0, 0.0, 1.0);
		float blend = clampf(params[BLEND_PARAM] + getf(inputs[BLEND_INPUT]) / 5.0, 0.0, 1.0);
		p->dry_wet = blend;
		p->stereo_spread = 0.0f;
		p->feedback = 0.0f;
		p->reverb = 0.0f;

		clouds::ShortFrame input[32];
		clouds::ShortFrame output[32];
		for (int j = 0; j < 32; j++) {
			input[j].l = clampf(inL[j] * params[IN_GAIN_PARAM] / 5.0, -1.0, 1.0) * 32767;
			input[j].r = clampf(inR[j] * params[IN_GAIN_PARAM] / 5.0, -1.0, 1.0) * 32767;
		}

		processor->Process(input, output, 32);

		for (int j = 0; j < 32; j++) {
			outL[j] = (float)output[j].l / 32767 * 5.0;
			outR[j] = (float)output[j].r / 32767 * 5.0;
		}

		triggered = false;
	}
}


CloudsWidget::CloudsWidget() : ModuleWidget(new Clouds()) {
	box.size = Vec(15*18, 380);

	{
		AudiblePanel *panel = new AudiblePanel();
		panel->imageFilename = "plugins/AudibleInstruments/res/Clouds.png";
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew(Vec(15, 0)));
	addChild(createScrew(Vec(240, 0)));
	addChild(createScrew(Vec(15, 365)));
	addChild(createScrew(Vec(240, 365)));

	// TODO
	// addParam(createParam<MediumMomentarySwitch>(Vec(211, 51), module, Clouds::POSITION_PARAM, 0.0, 1.0, 0.5));
	// addParam(createParam<MediumMomentarySwitch>(Vec(239, 51), module, Clouds::POSITION_PARAM, 0.0, 1.0, 0.5));

	addParam(createParam<LargeRedKnob>(Vec(42-14, 108-14), module, Clouds::POSITION_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<LargeGreenKnob>(Vec(123-14, 108-14), module, Clouds::SIZE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<LargeWhiteKnob>(Vec(205-14, 108-14), module, Clouds::PITCH_PARAM, -2.0, 2.0, 0.0));

	addParam(createParam<SmallRedKnob>(Vec(25-10, 191-10), module, Clouds::IN_GAIN_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallRedKnob>(Vec(92-10, 191-10), module, Clouds::DENSITY_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallGreenKnob>(Vec(157-10, 191-10), module, Clouds::TEXTURE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<SmallWhiteKnob>(Vec(224-10, 191-10), module, Clouds::BLEND_PARAM, 0.0, 1.0, 0.5));

	addInput(createInput(Vec(17, 275), module, Clouds::FREEZE_INPUT));
	addInput(createInput(Vec(60, 275), module, Clouds::TRIG_INPUT));
	addInput(createInput(Vec(103, 275), module, Clouds::POSITION_INPUT));
	addInput(createInput(Vec(146, 275), module, Clouds::SIZE_INPUT));
	addInput(createInput(Vec(190, 275), module, Clouds::PITCH_INPUT));
	addInput(createInput(Vec(233, 275), module, Clouds::BLEND_INPUT));

	addInput(createInput(Vec(17, 318), module, Clouds::IN_L_INPUT));
	addInput(createInput(Vec(60, 318), module, Clouds::IN_R_INPUT));
	addInput(createInput(Vec(103, 318), module, Clouds::DENSITY_INPUT));
	addInput(createInput(Vec(146, 318), module, Clouds::TEXTURE_INPUT));
	addOutput(createOutput(Vec(190, 318), module, Clouds::OUT_L_OUTPUT));
	addOutput(createOutput(Vec(233, 318), module, Clouds::OUT_R_OUTPUT));
}
