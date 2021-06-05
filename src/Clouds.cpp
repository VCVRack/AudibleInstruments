#include "plugin.hpp"
#include "clouds/dsp/granular_processor.h"


struct Clouds : Module {
	enum ParamIds {
		FREEZE_PARAM,
		MODE_PARAM,
		LOAD_PARAM,
		POSITION_PARAM,
		SIZE_PARAM,
		PITCH_PARAM,
		IN_GAIN_PARAM,
		DENSITY_PARAM,
		TEXTURE_PARAM,
		BLEND_PARAM,
		SPREAD_PARAM,
		FEEDBACK_PARAM,
		REVERB_PARAM,
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
	enum LightIds {
		FREEZE_LIGHT,
		MIX_GREEN_LIGHT, MIX_RED_LIGHT,
		PAN_GREEN_LIGHT, PAN_RED_LIGHT,
		FEEDBACK_GREEN_LIGHT, FEEDBACK_RED_LIGHT,
		REVERB_GREEN_LIGHT, REVERB_RED_LIGHT,
		NUM_LIGHTS
	};

	dsp::SampleRateConverter<2> inputSrc;
	dsp::SampleRateConverter<2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> inputBuffer;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> outputBuffer;

	uint8_t* block_mem;
	uint8_t* block_ccm;
	clouds::GranularProcessor* processor;

	bool triggered = false;

	dsp::SchmittTrigger freezeTrigger;
	bool freeze = false;
	dsp::SchmittTrigger blendTrigger;
	int blendMode = 0;

	clouds::PlaybackMode playback;
	int quality = 0;

	Clouds() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(POSITION_PARAM, 0.0, 1.0, 0.5, "Grain position");
		configParam(SIZE_PARAM, 0.0, 1.0, 0.5, "Grain size");
		configParam(PITCH_PARAM, -2.0, 2.0, 0.0, "Grain pitch");
		configParam(IN_GAIN_PARAM, 0.0, 1.0, 0.5, "Audio input gain");
		configParam(DENSITY_PARAM, 0.0, 1.0, 0.5, "Grain density");
		configParam(TEXTURE_PARAM, 0.0, 1.0, 0.5, "Grain texture");
		configParam(BLEND_PARAM, 0.0, 1.0, 0.5, "Dry/wet");
		configParam(SPREAD_PARAM, 0.0, 1.0, 0.0, "Stereo spread");
		configParam(FEEDBACK_PARAM, 0.0, 1.0, 0.0, "Feedback amount");
		configParam(REVERB_PARAM, 0.0, 1.0, 0.0, "Reverb amount");
		configParam(FREEZE_PARAM, 0.0, 1.0, 0.0, "Freeze");
		configParam(MODE_PARAM, 0.0, 1.0, 0.0, "Mode");
		configParam(LOAD_PARAM, 0.0, 1.0, 0.0, "Load/save");

		const int memLen = 118784;
		const int ccmLen = 65536 - 128;
		block_mem = new uint8_t[memLen]();
		block_ccm = new uint8_t[ccmLen]();
		processor = new clouds::GranularProcessor();
		memset(processor, 0, sizeof(*processor));

		processor->Init(block_mem, memLen, block_ccm, ccmLen);
		onReset();
	}

	~Clouds() {
		delete processor;
		delete[] block_mem;
		delete[] block_ccm;
	}

	void process(const ProcessArgs& args) override {
		// Get input
		dsp::Frame<2> inputFrame = {};
		if (!inputBuffer.full()) {
			inputFrame.samples[0] = inputs[IN_L_INPUT].getVoltage() * params[IN_GAIN_PARAM].getValue() / 5.0;
			inputFrame.samples[1] = inputs[IN_R_INPUT].isConnected() ? inputs[IN_R_INPUT].getVoltage() * params[IN_GAIN_PARAM].getValue() / 5.0 : inputFrame.samples[0];
			inputBuffer.push(inputFrame);
		}

		if (freezeTrigger.process(params[FREEZE_PARAM].getValue())) {
			freeze ^= true;
		}
		if (blendTrigger.process(params[MODE_PARAM].getValue())) {
			blendMode = (blendMode + 1) % 4;
		}

		// Trigger
		if (inputs[TRIG_INPUT].getVoltage() >= 1.0) {
			triggered = true;
		}

		// Render frames
		if (outputBuffer.empty()) {
			clouds::ShortFrame input[32] = {};
			// Convert input buffer
			{
				inputSrc.setRates(args.sampleRate, 32000);
				dsp::Frame<2> inputFrames[32];
				int inLen = inputBuffer.size();
				int outLen = 32;
				inputSrc.process(inputBuffer.startData(), &inLen, inputFrames, &outLen);
				inputBuffer.startIncr(inLen);

				// We might not fill all of the input buffer if there is a deficiency, but this cannot be avoided due to imprecisions between the input and output SRC.
				for (int i = 0; i < outLen; i++) {
					input[i].l = clamp(inputFrames[i].samples[0] * 32767.0f, -32768.0f, 32767.0f);
					input[i].r = clamp(inputFrames[i].samples[1] * 32767.0f, -32768.0f, 32767.0f);
				}
			}

			// Set up processor
			processor->set_playback_mode(playback);
			processor->set_quality(quality);
			processor->Prepare();

			clouds::Parameters* p = processor->mutable_parameters();
			p->trigger = triggered;
			p->gate = triggered;
			p->freeze = freeze || (inputs[FREEZE_INPUT].getVoltage() >= 1.0);
			p->position = clamp(params[POSITION_PARAM].getValue() + inputs[POSITION_INPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			p->size = clamp(params[SIZE_PARAM].getValue() + inputs[SIZE_INPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			p->pitch = clamp((params[PITCH_PARAM].getValue() + inputs[PITCH_INPUT].getVoltage()) * 12.0f, -48.0f, 48.0f);
			p->density = clamp(params[DENSITY_PARAM].getValue() + inputs[DENSITY_INPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			p->texture = clamp(params[TEXTURE_PARAM].getValue() + inputs[TEXTURE_INPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			p->dry_wet = params[BLEND_PARAM].getValue();
			p->stereo_spread = params[SPREAD_PARAM].getValue();
			p->feedback = params[FEEDBACK_PARAM].getValue();
			// TODO
			// Why doesn't dry audio get reverbed?
			p->reverb = params[REVERB_PARAM].getValue();
			float blend = inputs[BLEND_INPUT].getVoltage() / 5.0f;
			switch (blendMode) {
				case 0:
					p->dry_wet += blend;
					p->dry_wet = clamp(p->dry_wet, 0.0f, 1.0f);
					break;
				case 1:
					p->stereo_spread += blend;
					p->stereo_spread = clamp(p->stereo_spread, 0.0f, 1.0f);
					break;
				case 2:
					p->feedback += blend;
					p->feedback = clamp(p->feedback, 0.0f, 1.0f);
					break;
				case 3:
					p->reverb += blend;
					p->reverb = clamp(p->reverb, 0.0f, 1.0f);
					break;
			}

			clouds::ShortFrame output[32];
			processor->Process(input, output, 32);

			// Convert output buffer
			{
				dsp::Frame<2> outputFrames[32];
				for (int i = 0; i < 32; i++) {
					outputFrames[i].samples[0] = output[i].l / 32768.0;
					outputFrames[i].samples[1] = output[i].r / 32768.0;
				}

				outputSrc.setRates(32000, args.sampleRate);
				int inLen = 32;
				int outLen = outputBuffer.capacity();
				outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
				outputBuffer.endIncr(outLen);
			}

			triggered = false;
		}

		// Set output
		dsp::Frame<2> outputFrame = {};
		if (!outputBuffer.empty()) {
			outputFrame = outputBuffer.shift();
			outputs[OUT_L_OUTPUT].setVoltage(5.0 * outputFrame.samples[0]);
			outputs[OUT_R_OUTPUT].setVoltage(5.0 * outputFrame.samples[1]);
		}

		// Lights
		clouds::Parameters* p = processor->mutable_parameters();
		dsp::VuMeter vuMeter;
		vuMeter.dBInterval = 6.0;
		dsp::Frame<2> lightFrame = p->freeze ? outputFrame : inputFrame;
		vuMeter.setValue(fmaxf(fabsf(lightFrame.samples[0]), fabsf(lightFrame.samples[1])));
		lights[FREEZE_LIGHT].setBrightness(p->freeze ? 0.75 : 0.0);
		lights[MIX_GREEN_LIGHT].setSmoothBrightness(vuMeter.getBrightness(3), args.sampleTime);
		lights[PAN_GREEN_LIGHT].setSmoothBrightness(vuMeter.getBrightness(2), args.sampleTime);
		lights[FEEDBACK_GREEN_LIGHT].setSmoothBrightness(vuMeter.getBrightness(1), args.sampleTime);
		lights[REVERB_GREEN_LIGHT].setBrightness(0.0);
		lights[MIX_RED_LIGHT].setBrightness(0.0);
		lights[PAN_RED_LIGHT].setBrightness(0.0);
		lights[FEEDBACK_RED_LIGHT].setSmoothBrightness(vuMeter.getBrightness(1), args.sampleTime);
		lights[REVERB_RED_LIGHT].setSmoothBrightness(vuMeter.getBrightness(0), args.sampleTime);
	}

	void onReset() override {
		freeze = false;
		blendMode = 0;
		playback = clouds::PLAYBACK_MODE_GRANULAR;
		quality = 0;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "playback", json_integer((int) playback));
		json_object_set_new(rootJ, "quality", json_integer(quality));
		json_object_set_new(rootJ, "blendMode", json_integer(blendMode));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* playbackJ = json_object_get(rootJ, "playback");
		if (playbackJ) {
			playback = (clouds::PlaybackMode) json_integer_value(playbackJ);
		}

		json_t* qualityJ = json_object_get(rootJ, "quality");
		if (qualityJ) {
			quality = json_integer_value(qualityJ);
		}

		json_t* blendModeJ = json_object_get(rootJ, "blendMode");
		if (blendModeJ) {
			blendMode = json_integer_value(blendModeJ);
		}
	}
};


struct FreezeLight : YellowLight {
	FreezeLight() {
		box.size = Vec(28 - 6, 28 - 6);
		bgColor = color::BLACK_TRANSPARENT;
	}
};


struct CloudsWidget : ModuleWidget {
	ParamWidget* blendParam;
	ParamWidget* spreadParam;
	ParamWidget* feedbackParam;
	ParamWidget* reverbParam;

	CloudsWidget(Clouds* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Clouds.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(240, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(240, 365)));

		addParam(createParam<Rogan3PSRed>(Vec(27, 93), module, Clouds::POSITION_PARAM));
		addParam(createParam<Rogan3PSGreen>(Vec(108, 93), module, Clouds::SIZE_PARAM));
		addParam(createParam<Rogan3PSWhite>(Vec(190, 93), module, Clouds::PITCH_PARAM));

		addParam(createParam<Rogan1PSRed>(Vec(14, 180), module, Clouds::IN_GAIN_PARAM));
		addParam(createParam<Rogan1PSRed>(Vec(81, 180), module, Clouds::DENSITY_PARAM));
		addParam(createParam<Rogan1PSGreen>(Vec(146, 180), module, Clouds::TEXTURE_PARAM));
		blendParam = createParam<Rogan1PSWhite>(Vec(213, 180), module, Clouds::BLEND_PARAM);
		addParam(blendParam);
		spreadParam = createParam<Rogan1PSRed>(Vec(213, 180), module, Clouds::SPREAD_PARAM);
		spreadParam->hide();
		addParam(spreadParam);
		feedbackParam = createParam<Rogan1PSGreen>(Vec(213, 180), module, Clouds::FEEDBACK_PARAM);
		feedbackParam->hide();
		addParam(feedbackParam);
		reverbParam = createParam<Rogan1PSBlue>(Vec(213, 180), module, Clouds::REVERB_PARAM);
		reverbParam->hide();
		addParam(reverbParam);

		addParam(createParam<CKD6>(Vec(12, 43), module, Clouds::FREEZE_PARAM));
		addParam(createParam<TL1105>(Vec(211, 50), module, Clouds::MODE_PARAM));
		addParam(createParam<TL1105>(Vec(239, 50), module, Clouds::LOAD_PARAM));

		addInput(createInput<PJ301MPort>(Vec(15, 274), module, Clouds::FREEZE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(58, 274), module, Clouds::TRIG_INPUT));
		addInput(createInput<PJ301MPort>(Vec(101, 274), module, Clouds::POSITION_INPUT));
		addInput(createInput<PJ301MPort>(Vec(144, 274), module, Clouds::SIZE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(188, 274), module, Clouds::PITCH_INPUT));
		addInput(createInput<PJ301MPort>(Vec(230, 274), module, Clouds::BLEND_INPUT));

		addInput(createInput<PJ301MPort>(Vec(15, 317), module, Clouds::IN_L_INPUT));
		addInput(createInput<PJ301MPort>(Vec(58, 317), module, Clouds::IN_R_INPUT));
		addInput(createInput<PJ301MPort>(Vec(101, 317), module, Clouds::DENSITY_INPUT));
		addInput(createInput<PJ301MPort>(Vec(144, 317), module, Clouds::TEXTURE_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(188, 317), module, Clouds::OUT_L_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(230, 317), module, Clouds::OUT_R_OUTPUT));

		addChild(createLight<FreezeLight>(Vec(12 + 3, 43 + 3), module, Clouds::FREEZE_LIGHT));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(82.5, 53), module, Clouds::MIX_GREEN_LIGHT));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(114.5, 53), module, Clouds::PAN_GREEN_LIGHT));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(145.5, 53), module, Clouds::FEEDBACK_GREEN_LIGHT));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(177.5, 53), module, Clouds::REVERB_GREEN_LIGHT));
	}

	void step() override {
		Clouds* module = dynamic_cast<Clouds*>(this->module);

		if (module) {
			blendParam->visible = (module->blendMode == 0);
			spreadParam->visible = (module->blendMode == 1);
			feedbackParam->visible = (module->blendMode == 2);
			reverbParam->visible = (module->blendMode == 3);
		}

		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		Clouds* module = dynamic_cast<Clouds*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Blend knob"));

		static const std::vector<std::string> blendLabels = {
			"Wet/dry",
			"Spread",
			"Feedback",
			"Reverb",
		};
		for (int i = 0; i < (int) blendLabels.size(); i++) {
			menu->addChild(createCheckMenuItem(blendLabels[i],
				[=]() {return module->blendMode == i;},
				[=]() {module->blendMode = i;}
			));
		}

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Alternate mode"));

		static const std::vector<std::string> playbackLabels = {
			"Granular",
			"Pitch-shifter/time-stretcher",
			"Looping delay",
			"Spectral madness",
		};
		for (int i = 0; i < (int) playbackLabels.size(); i++) {
			menu->addChild(createCheckMenuItem(playbackLabels[i],
				[=]() {return module->playback == i;},
				[=]() {module->playback = (clouds::PlaybackMode) i;}
			));
		}

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Quality"));

		static const std::vector<std::string> qualityLabels = {
			"1s 32kHz 16-bit stereo",
			"2s 32kHz 16-bit mono",
			"4s 16kHz 8-bit µ-law stereo",
			"8s 16kHz 8-bit µ-law mono",
		};
		for (int i = 0; i < (int) qualityLabels.size(); i++) {
			menu->addChild(createCheckMenuItem(qualityLabels[i],
				[=]() {return module->quality == i;},
				[=]() {module->quality = i;}
			));
		}
	}
};


Model* modelClouds = createModel<Clouds, CloudsWidget>("Clouds");
