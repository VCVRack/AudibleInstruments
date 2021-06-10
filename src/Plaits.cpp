#include "plugin.hpp"

#pragma GCC diagnostic push
#ifndef __clang__
	#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include "plaits/dsp/voice.h"
#pragma GCC diagnostic pop


struct Plaits : Module {
	enum ParamIds {
		MODEL1_PARAM,
		MODEL2_PARAM,
		FREQ_PARAM,
		HARMONICS_PARAM,
		TIMBRE_PARAM,
		MORPH_PARAM,
		TIMBRE_CV_PARAM,
		FREQ_CV_PARAM,
		MORPH_CV_PARAM,
		LPG_COLOR_PARAM,
		LPG_DECAY_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENGINE_INPUT,
		TIMBRE_INPUT,
		FREQ_INPUT,
		MORPH_INPUT,
		HARMONICS_INPUT,
		TRIGGER_INPUT,
		LEVEL_INPUT,
		NOTE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		AUX_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(MODEL_LIGHT, 8 * 2),
		NUM_LIGHTS
	};

	plaits::Voice voice[16];
	plaits::Patch patch = {};
	char shared_buffer[16][16384] = {};
	float triPhase = 0.f;

	dsp::SampleRateConverter<16 * 2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<16 * 2>, 256> outputBuffer;
	bool lowCpu = false;

	dsp::BooleanTrigger model1Trigger;
	dsp::BooleanTrigger model2Trigger;

	Plaits() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configButton(MODEL1_PARAM, "Pitched models");
		configButton(MODEL2_PARAM, "Noise/percussive models");
		configParam(FREQ_PARAM, -4.0, 4.0, 0.0, "Frequency", " semitones", 0.f, 12.f);
		configParam(HARMONICS_PARAM, 0.0, 1.0, 0.5, "Harmonics", "%", 0.f, 100.f);
		configParam(TIMBRE_PARAM, 0.0, 1.0, 0.5, "Timbre", "%", 0.f, 100.f);
		configParam(LPG_COLOR_PARAM, 0.0, 1.0, 0.5, "Lowpass gate response", "%", 0.f, 100.f);
		configParam(MORPH_PARAM, 0.0, 1.0, 0.5, "Morph", "%", 0.f, 100.f);
		configParam(LPG_DECAY_PARAM, 0.0, 1.0, 0.5, "Lowpass gate decay", "%", 0.f, 100.f);
		configParam(TIMBRE_CV_PARAM, -1.0, 1.0, 0.0, "Timbre CV");
		configParam(FREQ_CV_PARAM, -1.0, 1.0, 0.0, "Frequency CV");
		configParam(MORPH_CV_PARAM, -1.0, 1.0, 0.0, "Morph CV");

		configInput(ENGINE_INPUT, "Model");
		configInput(TIMBRE_INPUT, "Timbre");
		configInput(FREQ_INPUT, "FM");
		configInput(MORPH_INPUT, "Morph");
		configInput(HARMONICS_INPUT, "Harmonics");
		configInput(TRIGGER_INPUT, "Trigger");
		configInput(LEVEL_INPUT, "Level");
		configInput(NOTE_INPUT, "Pitch (1V/oct)");

		configOutput(OUT_OUTPUT, "Main");
		configOutput(AUX_OUTPUT, "Auxiliary");

		for (int i = 0; i < 16; i++) {
			stmlib::BufferAllocator allocator(shared_buffer[i], sizeof(shared_buffer[i]));
			voice[i].Init(&allocator);
		}

		onReset();
	}

	void onReset() override {
		patch.engine = 0;
		patch.lpg_colour = 0.5f;
		patch.decay = 0.5f;
	}

	void onRandomize() override {
		patch.engine = random::u32() % 16;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "lowCpu", json_boolean(lowCpu));
		json_object_set_new(rootJ, "model", json_integer(patch.engine));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* lowCpuJ = json_object_get(rootJ, "lowCpu");
		if (lowCpuJ)
			lowCpu = json_boolean_value(lowCpuJ);

		json_t* modelJ = json_object_get(rootJ, "model");
		if (modelJ)
			patch.engine = json_integer_value(modelJ);

		// Legacy <=1.0.2
		json_t* lpgColorJ = json_object_get(rootJ, "lpgColor");
		if (lpgColorJ)
			params[LPG_COLOR_PARAM].setValue(json_number_value(lpgColorJ));

		// Legacy <=1.0.2
		json_t* decayJ = json_object_get(rootJ, "decay");
		if (decayJ)
			params[LPG_DECAY_PARAM].setValue(json_number_value(decayJ));
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[NOTE_INPUT].getChannels(), 1);

		if (outputBuffer.empty()) {
			const int blockSize = 12;

			// Model buttons
			if (model1Trigger.process(params[MODEL1_PARAM].getValue())) {
				if (patch.engine >= 8) {
					patch.engine -= 8;
				}
				else {
					patch.engine = (patch.engine + 1) % 8;
				}
			}
			if (model2Trigger.process(params[MODEL2_PARAM].getValue())) {
				if (patch.engine < 8) {
					patch.engine += 8;
				}
				else {
					patch.engine = (patch.engine + 1) % 8 + 8;
				}
			}

			// Model lights
			// Pulse light at 2 Hz
			triPhase += 2.f * args.sampleTime * blockSize;
			if (triPhase >= 1.f)
				triPhase -= 1.f;
			float tri = (triPhase < 0.5f) ? triPhase * 2.f : (1.f - triPhase) * 2.f;

			// Get active engines of all voice channels
			bool activeEngines[16] = {};
			bool pulse = false;
			for (int c = 0; c < channels; c++) {
				int activeEngine = voice[c].active_engine();
				activeEngines[activeEngine] = true;
				// Pulse the light if at least one voice is using a different engine.
				if (activeEngine != patch.engine)
					pulse = true;
			}

			// Set model lights
			for (int i = 0; i < 16; i++) {
				// Transpose the [light][color] table
				int lightId = (i % 8) * 2 + (i / 8);
				float brightness = activeEngines[i];
				if (patch.engine == i && pulse)
					brightness = tri;
				lights[MODEL_LIGHT + lightId].setBrightness(brightness);
			}

			// Calculate pitch for lowCpu mode if needed
			float pitch = params[FREQ_PARAM].getValue();
			if (lowCpu)
				pitch += std::log2(48000.f * args.sampleTime);
			// Update patch
			patch.note = 60.f + pitch * 12.f;
			patch.harmonics = params[HARMONICS_PARAM].getValue();
			patch.timbre = params[TIMBRE_PARAM].getValue();
			patch.morph = params[MORPH_PARAM].getValue();
			patch.lpg_colour = params[LPG_COLOR_PARAM].getValue();
			patch.decay = params[LPG_DECAY_PARAM].getValue();
			patch.frequency_modulation_amount = params[FREQ_CV_PARAM].getValue();
			patch.timbre_modulation_amount = params[TIMBRE_CV_PARAM].getValue();
			patch.morph_modulation_amount = params[MORPH_CV_PARAM].getValue();

			// Render output buffer for each voice
			dsp::Frame<16 * 2> outputFrames[blockSize];
			for (int c = 0; c < channels; c++) {
				// Construct modulations
				plaits::Modulations modulations;
				modulations.engine = inputs[ENGINE_INPUT].getPolyVoltage(c) / 5.f;
				modulations.note = inputs[NOTE_INPUT].getVoltage(c) * 12.f;
				modulations.frequency = inputs[FREQ_INPUT].getPolyVoltage(c) * 6.f;
				modulations.harmonics = inputs[HARMONICS_INPUT].getPolyVoltage(c) / 5.f;
				modulations.timbre = inputs[TIMBRE_INPUT].getPolyVoltage(c) / 8.f;
				modulations.morph = inputs[MORPH_INPUT].getPolyVoltage(c) / 8.f;
				// Triggers at around 0.7 V
				modulations.trigger = inputs[TRIGGER_INPUT].getPolyVoltage(c) / 3.f;
				modulations.level = inputs[LEVEL_INPUT].getPolyVoltage(c) / 8.f;

				modulations.frequency_patched = inputs[FREQ_INPUT].isConnected();
				modulations.timbre_patched = inputs[TIMBRE_INPUT].isConnected();
				modulations.morph_patched = inputs[MORPH_INPUT].isConnected();
				modulations.trigger_patched = inputs[TRIGGER_INPUT].isConnected();
				modulations.level_patched = inputs[LEVEL_INPUT].isConnected();

				// Render frames
				plaits::Voice::Frame output[blockSize];
				voice[c].Render(patch, modulations, output, blockSize);

				// Convert output to frames
				for (int i = 0; i < blockSize; i++) {
					outputFrames[i].samples[c * 2 + 0] = output[i].out / 32768.f;
					outputFrames[i].samples[c * 2 + 1] = output[i].aux / 32768.f;
				}
			}

			// Convert output
			if (lowCpu) {
				int len = std::min((int) outputBuffer.capacity(), blockSize);
				std::memcpy(outputBuffer.endData(), outputFrames, len * sizeof(outputFrames[0]));
				outputBuffer.endIncr(len);
			}
			else {
				outputSrc.setRates(48000, (int) args.sampleRate);
				int inLen = blockSize;
				int outLen = outputBuffer.capacity();
				outputSrc.setChannels(channels * 2);
				outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
				outputBuffer.endIncr(outLen);
			}
		}

		// Set output
		if (!outputBuffer.empty()) {
			dsp::Frame<16 * 2> outputFrame = outputBuffer.shift();
			for (int c = 0; c < channels; c++) {
				// Inverting op-amp on outputs
				outputs[OUT_OUTPUT].setVoltage(-outputFrame.samples[c * 2 + 0] * 5.f, c);
				outputs[AUX_OUTPUT].setVoltage(-outputFrame.samples[c * 2 + 1] * 5.f, c);
			}
		}
		outputs[OUT_OUTPUT].setChannels(channels);
		outputs[AUX_OUTPUT].setChannels(channels);
	}
};


static const std::string modelLabels[16] = {
	"Pair of classic waveforms",
	"Waveshaping oscillator",
	"Two operator FM",
	"Granular formant oscillator",
	"Harmonic oscillator",
	"Wavetable oscillator",
	"Chords",
	"Vowel and speech synthesis",
	"Granular cloud",
	"Filtered noise",
	"Particle noise",
	"Inharmonic string modeling",
	"Modal resonator",
	"Analog bass drum",
	"Analog snare drum",
	"Analog hi-hat",
};


struct PlaitsWidget : ModuleWidget {
	bool lpgMode = false;

	PlaitsWidget(Plaits* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Plaits.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<TL1105>(mm2px(Vec(23.32685, 14.6539)), module, Plaits::MODEL1_PARAM));
		addParam(createParam<TL1105>(mm2px(Vec(32.22764, 14.6539)), module, Plaits::MODEL2_PARAM));
		addParam(createParam<Rogan3PSWhite>(mm2px(Vec(3.1577, 20.21088)), module, Plaits::FREQ_PARAM));
		addParam(createParam<Rogan3PSWhite>(mm2px(Vec(39.3327, 20.21088)), module, Plaits::HARMONICS_PARAM));
		addParam(createParam<Rogan1PSWhite>(mm2px(Vec(4.04171, 49.6562)), module, Plaits::TIMBRE_PARAM));
		addParam(createParam<Rogan1PSWhite>(mm2px(Vec(42.71716, 49.6562)), module, Plaits::MORPH_PARAM));
		addParam(createParam<Trimpot>(mm2px(Vec(7.88712, 77.60705)), module, Plaits::TIMBRE_CV_PARAM));
		addParam(createParam<Trimpot>(mm2px(Vec(27.2245, 77.60705)), module, Plaits::FREQ_CV_PARAM));
		addParam(createParam<Trimpot>(mm2px(Vec(46.56189, 77.60705)), module, Plaits::MORPH_CV_PARAM));

		ParamWidget* lpgColorParam = createParam<Rogan1PSBlue>(mm2px(Vec(4.04171, 49.6562)), module, Plaits::LPG_COLOR_PARAM);
		lpgColorParam->hide();
		addParam(lpgColorParam);
		ParamWidget* decayParam = createParam<Rogan1PSBlue>(mm2px(Vec(42.71716, 49.6562)), module, Plaits::LPG_DECAY_PARAM);
		decayParam->hide();
		addParam(decayParam);

		addInput(createInput<PJ301MPort>(mm2px(Vec(3.31381, 92.48067)), module, Plaits::ENGINE_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(14.75983, 92.48067)), module, Plaits::TIMBRE_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(26.20655, 92.48067)), module, Plaits::FREQ_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(37.65257, 92.48067)), module, Plaits::MORPH_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(49.0986, 92.48067)), module, Plaits::HARMONICS_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.31381, 107.08103)), module, Plaits::TRIGGER_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(14.75983, 107.08103)), module, Plaits::LEVEL_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(26.20655, 107.08103)), module, Plaits::NOTE_INPUT));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(37.65257, 107.08103)), module, Plaits::OUT_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(49.0986, 107.08103)), module, Plaits::AUX_OUTPUT));

		addChild(createLight<MediumLight<GreenRedLight>>(mm2px(Vec(28.79498, 23.31649)), module, Plaits::MODEL_LIGHT + 0 * 2));
		addChild(createLight<MediumLight<GreenRedLight>>(mm2px(Vec(28.79498, 28.71704)), module, Plaits::MODEL_LIGHT + 1 * 2));
		addChild(createLight<MediumLight<GreenRedLight>>(mm2px(Vec(28.79498, 34.1162)), module, Plaits::MODEL_LIGHT + 2 * 2));
		addChild(createLight<MediumLight<GreenRedLight>>(mm2px(Vec(28.79498, 39.51675)), module, Plaits::MODEL_LIGHT + 3 * 2));
		addChild(createLight<MediumLight<GreenRedLight>>(mm2px(Vec(28.79498, 44.91731)), module, Plaits::MODEL_LIGHT + 4 * 2));
		addChild(createLight<MediumLight<GreenRedLight>>(mm2px(Vec(28.79498, 50.31785)), module, Plaits::MODEL_LIGHT + 5 * 2));
		addChild(createLight<MediumLight<GreenRedLight>>(mm2px(Vec(28.79498, 55.71771)), module, Plaits::MODEL_LIGHT + 6 * 2));
		addChild(createLight<MediumLight<GreenRedLight>>(mm2px(Vec(28.79498, 61.11827)), module, Plaits::MODEL_LIGHT + 7 * 2));
	}

	void appendContextMenu(Menu* menu) override {
		Plaits* module = dynamic_cast<Plaits*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("Low CPU (disable resampling)", &module->lowCpu));

		menu->addChild(createBoolMenuItem("Edit LPG response/decay",
			[=]() {return this->getLpgMode();},
			[=](bool val) {this->setLpgMode(val);}
		));

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Models"));

		for (int i = 0; i < 16; i++) {
			menu->addChild(createCheckMenuItem(modelLabels[i],
				[=]() {return module->patch.engine == i;},
				[=]() {module->patch.engine = i;}
			));
		}
	}

	void setLpgMode(bool lpgMode) {
		// ModuleWidget::getParam() doesn't work if the ModuleWidget doesn't have a module.
		if (!module)
			return;
		if (lpgMode) {
			getParam(Plaits::MORPH_PARAM)->hide();
			getParam(Plaits::TIMBRE_PARAM)->hide();
			getParam(Plaits::LPG_DECAY_PARAM)->show();
			getParam(Plaits::LPG_COLOR_PARAM)->show();
		}
		else {
			getParam(Plaits::MORPH_PARAM)->show();
			getParam(Plaits::TIMBRE_PARAM)->show();
			getParam(Plaits::LPG_DECAY_PARAM)->hide();
			getParam(Plaits::LPG_COLOR_PARAM)->hide();
		}
		this->lpgMode = lpgMode;
	}

	bool getLpgMode() {
		return this->lpgMode;
	}
};


Model* modelPlaits = createModel<Plaits, PlaitsWidget>("Plaits");
