#include <string.h>
#include "AudibleInstruments.hpp"
#include "dsp/samplerate.hpp"
#include "dsp/ringbuffer.hpp"
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

	SampleRateConverter<2> inputSrc;
	SampleRateConverter<2> outputSrc;
	DoubleRingBuffer<Frame<2>, 256> inputBuffer;
	DoubleRingBuffer<Frame<2>, 256> outputBuffer;

	uint8_t *block_mem;
	uint8_t *block_ccm;
	clouds::GranularProcessor *processor;

	bool triggered = false;

	Clouds();
	~Clouds();
	void step();
};


Clouds::Clouds() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {
	const int memLen = 118784;
	const int ccmLen = 65536 - 128;
	block_mem = new uint8_t[memLen]();
	block_ccm = new uint8_t[ccmLen]();
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
	// Get input
	if (!inputBuffer.full()) {
		Frame<2> inputFrame;
		inputFrame.samples[0] = inputs[IN_L_INPUT].value * params[IN_GAIN_PARAM].value / 5.0;
		inputFrame.samples[1] = inputs[IN_R_INPUT].active ? inputs[IN_R_INPUT].value * params[IN_GAIN_PARAM].value / 5.0 : inputFrame.samples[0];
		inputBuffer.push(inputFrame);
	}

	// Trigger
	if (inputs[TRIG_INPUT].value >= 1.0) {
		triggered = true;
	}

	// Render frames
	if (outputBuffer.empty()) {
		clouds::ShortFrame input[32] = {};
		// Convert input buffer
		{
			inputSrc.setRatio(32000.0 / gSampleRate);
			Frame<2> inputFrames[32];
			int inLen = inputBuffer.size();
			int outLen = 32;
			inputSrc.process(inputBuffer.startData(), &inLen, inputFrames, &outLen);
			inputBuffer.startIncr(inLen);

			// We might not fill all of the input buffer if there is a deficiency, but this cannot be avoided due to imprecisions between the input and output SRC.
			for (int i = 0; i < outLen; i++) {
				input[i].l = clampf(inputFrames[i].samples[0] * 32767.0, -32768, 32767);
				input[i].r = clampf(inputFrames[i].samples[1] * 32767.0, -32768, 32767);
			}
		}

		// Set up processor
		processor->set_num_channels(2);
		processor->set_low_fidelity(false);
		// TODO Support the other modes
		processor->set_playback_mode(clouds::PLAYBACK_MODE_GRANULAR);
		processor->Prepare();

		clouds::Parameters* p = processor->mutable_parameters();
		p->trigger = triggered;
		p->gate = triggered;
		p->freeze = (inputs[FREEZE_INPUT].value >= 1.0);
		p->position = clampf(params[POSITION_PARAM].value + inputs[POSITION_INPUT].value / 5.0, 0.0, 1.0);
		p->size = clampf(params[SIZE_PARAM].value + inputs[SIZE_INPUT].value / 5.0, 0.0, 1.0);
		p->pitch = clampf((params[PITCH_PARAM].value + inputs[PITCH_INPUT].value) * 12.0, -48.0, 48.0);
		p->density = clampf(params[DENSITY_PARAM].value + inputs[DENSITY_INPUT].value / 5.0, 0.0, 1.0);
		p->texture = clampf(params[TEXTURE_PARAM].value + inputs[TEXTURE_INPUT].value / 5.0, 0.0, 1.0);
		float blend = clampf(params[BLEND_PARAM].value + inputs[BLEND_INPUT].value / 5.0, 0.0, 1.0);
		p->dry_wet = blend;
		p->stereo_spread = 0.0;
		p->feedback = 0.0;
		p->reverb = 0.0;

		clouds::ShortFrame output[32];
		processor->Process(input, output, 32);

		// Convert output buffer
		{
			Frame<2> outputFrames[32];
			for (int i = 0; i < 32; i++) {
				outputFrames[i].samples[0] = output[i].l / 32768.0;
				outputFrames[i].samples[1] = output[i].r / 32768.0;
			}

			outputSrc.setRatio(gSampleRate / 32000.0);
			int inLen = 32;
			int outLen = outputBuffer.capacity();
			outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
			outputBuffer.endIncr(outLen);
		}

		triggered = false;
	}

	// Set output
	if (!outputBuffer.empty()) {
		Frame<2> outputFrame = outputBuffer.shift();
		outputs[OUT_L_OUTPUT].value = 5.0 * outputFrame.samples[0];
		outputs[OUT_R_OUTPUT].value = 5.0 * outputFrame.samples[1];
	}
}


CloudsWidget::CloudsWidget() {
	Clouds *module = new Clouds();
	setModule(module);
	box.size = Vec(15*18, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load(assetPlugin(plugin, "res/Clouds.png"));
		panel->box.size = box.size;
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(240, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(240, 365)));

	// TODO
	// addParam(createParam<MediumMomentarySwitch>(Vec(211, 51), module, Clouds::POSITION_PARAM, 0.0, 1.0, 0.5));
	// addParam(createParam<MediumMomentarySwitch>(Vec(239, 51), module, Clouds::POSITION_PARAM, 0.0, 1.0, 0.5));

	addParam(createParam<Rogan3PSRed>(Vec(28, 94), module, Clouds::POSITION_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan3PSGreen>(Vec(109, 94), module, Clouds::SIZE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan3PSWhite>(Vec(191, 94), module, Clouds::PITCH_PARAM, -2.0, 2.0, 0.0));

	addParam(createParam<Rogan1PSRed>(Vec(15, 181), module, Clouds::IN_GAIN_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSRed>(Vec(82, 181), module, Clouds::DENSITY_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSGreen>(Vec(147, 181), module, Clouds::TEXTURE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan1PSWhite>(Vec(214, 181), module, Clouds::BLEND_PARAM, 0.0, 1.0, 0.5));

	addInput(createInput<PJ3410Port>(Vec(11, 270), module, Clouds::FREEZE_INPUT));
	addInput(createInput<PJ3410Port>(Vec(54, 270), module, Clouds::TRIG_INPUT));
	addInput(createInput<PJ3410Port>(Vec(97, 270), module, Clouds::POSITION_INPUT));
	addInput(createInput<PJ3410Port>(Vec(140, 270), module, Clouds::SIZE_INPUT));
	addInput(createInput<PJ3410Port>(Vec(184, 270), module, Clouds::PITCH_INPUT));
	addInput(createInput<PJ3410Port>(Vec(227, 270), module, Clouds::BLEND_INPUT));

	addInput(createInput<PJ3410Port>(Vec(11, 313), module, Clouds::IN_L_INPUT));
	addInput(createInput<PJ3410Port>(Vec(54, 313), module, Clouds::IN_R_INPUT));
	addInput(createInput<PJ3410Port>(Vec(97, 313), module, Clouds::DENSITY_INPUT));
	addInput(createInput<PJ3410Port>(Vec(140, 313), module, Clouds::TEXTURE_INPUT));
	addOutput(createOutput<PJ3410Port>(Vec(184, 313), module, Clouds::OUT_L_OUTPUT));
	addOutput(createOutput<PJ3410Port>(Vec(227, 313), module, Clouds::OUT_R_OUTPUT));
}
