#include "AudibleInstruments.hpp"

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

	plaits::Voice voice;
	plaits::Patch patch = {};
	plaits::Modulations modulations = {};
	char shared_buffer[16384] = {};
	float triPhase = 0.f;

	dsp::SampleRateConverter<2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> outputBuffer;
	bool lowCpu = false;
	bool lpg = false;

	dsp::SchmittTrigger model1Trigger;
	dsp::SchmittTrigger model2Trigger;

	Plaits() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(MODEL1_PARAM, 0.0, 1.0, 0.0);
		configParam(MODEL2_PARAM, 0.0, 1.0, 0.0);
		configParam(FREQ_PARAM, -4.0, 4.0, 0.0);
		configParam(HARMONICS_PARAM, 0.0, 1.0, 0.5);
		configParam(TIMBRE_PARAM, 0.0, 1.0, 0.5);
		configParam(MORPH_PARAM, 0.0, 1.0, 0.5);
		configParam(TIMBRE_CV_PARAM, -1.0, 1.0, 0.0);
		configParam(FREQ_CV_PARAM, -1.0, 1.0, 0.0);
		configParam(MORPH_CV_PARAM, -1.0, 1.0, 0.0);

		stmlib::BufferAllocator allocator(shared_buffer, sizeof(shared_buffer));
		voice.Init(&allocator);

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

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "lowCpu", json_boolean(lowCpu));
		json_object_set_new(rootJ, "model", json_integer(patch.engine));
		json_object_set_new(rootJ, "lpgColor", json_real(patch.lpg_colour));
		json_object_set_new(rootJ, "decay", json_real(patch.decay));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *lowCpuJ = json_object_get(rootJ, "lowCpu");
		if (lowCpuJ)
			lowCpu = json_boolean_value(lowCpuJ);

		json_t *modelJ = json_object_get(rootJ, "model");
		if (modelJ)
			patch.engine = json_integer_value(modelJ);

		json_t *lpgColorJ = json_object_get(rootJ, "lpgColor");
		if (lpgColorJ)
			patch.lpg_colour = json_number_value(lpgColorJ);

		json_t *decayJ = json_object_get(rootJ, "decay");
		if (decayJ)
			patch.decay = json_number_value(decayJ);
	}

	void process(const ProcessArgs &args) override {
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
			int activeEngine = voice.active_engine();
			triPhase += 2.f * args.sampleTime * blockSize;
			if (triPhase >= 1.f)
				triPhase -= 1.f;
			float tri = (triPhase < 0.5f) ? triPhase * 2.f : (1.f - triPhase) * 2.f;

			for (int i = 0; i < 8; i++) {
				lights[MODEL_LIGHT + 2*i + 0].setBrightness((activeEngine == i) ? 1.f : (patch.engine == i) ? tri : 0.f);
				lights[MODEL_LIGHT + 2*i + 1].setBrightness((activeEngine == i + 8) ? 1.f : (patch.engine == i + 8) ? tri : 0.f);
			}

			// Calculate pitch for lowCpu mode if needed
			float pitch = params[FREQ_PARAM].getValue();
			if (lowCpu)
				pitch += log2f(48000.f * args.sampleTime);
			// Update patch
			patch.note = 60.f + pitch * 12.f;
			patch.harmonics = params[HARMONICS_PARAM].getValue();
			if (!lpg) {
				patch.timbre = params[TIMBRE_PARAM].getValue();
				patch.morph = params[MORPH_PARAM].getValue();
			}
			else {
				patch.lpg_colour = params[TIMBRE_PARAM].getValue();
				patch.decay = params[MORPH_PARAM].getValue();
			}
			patch.frequency_modulation_amount = params[FREQ_CV_PARAM].getValue();
			patch.timbre_modulation_amount = params[TIMBRE_CV_PARAM].getValue();
			patch.morph_modulation_amount = params[MORPH_CV_PARAM].getValue();

			// Update modulations
			modulations.engine = inputs[ENGINE_INPUT].getVoltage() / 5.f;
			modulations.note = inputs[NOTE_INPUT].getVoltage() * 12.f;
			modulations.frequency = inputs[FREQ_INPUT].getVoltage() * 6.f;
			modulations.harmonics = inputs[HARMONICS_INPUT].getVoltage() / 5.f;
			modulations.timbre = inputs[TIMBRE_INPUT].getVoltage() / 8.f;
			modulations.morph = inputs[MORPH_INPUT].getVoltage() / 8.f;
			// Triggers at around 0.7 V
			modulations.trigger = inputs[TRIGGER_INPUT].getVoltage() / 3.f;
			modulations.level = inputs[LEVEL_INPUT].getVoltage() / 8.f;

			modulations.frequency_patched = inputs[FREQ_INPUT].isConnected();
			modulations.timbre_patched = inputs[TIMBRE_INPUT].isConnected();
			modulations.morph_patched = inputs[MORPH_INPUT].isConnected();
			modulations.trigger_patched = inputs[TRIGGER_INPUT].isConnected();
			modulations.level_patched = inputs[LEVEL_INPUT].isConnected();

			// Render frames
			plaits::Voice::Frame output[blockSize];
			voice.Render(patch, modulations, output, blockSize);

			// Convert output to frames
			dsp::Frame<2> outputFrames[blockSize];
			for (int i = 0; i < blockSize; i++) {
				outputFrames[i].samples[0] = output[i].out / 32768.f;
				outputFrames[i].samples[1] = output[i].aux / 32768.f;
			}

			// Convert output
			if (lowCpu) {
				int len = std::min((int) outputBuffer.capacity(), blockSize);
				memcpy(outputBuffer.endData(), outputFrames, len * sizeof(dsp::Frame<2>));
				outputBuffer.endIncr(len);
			}
			else {
				outputSrc.setRates(48000, args.sampleRate);
				int inLen = blockSize;
				int outLen = outputBuffer.capacity();
				outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
				outputBuffer.endIncr(outLen);
			}
		}

		// Set output
		if (!outputBuffer.empty()) {
			dsp::Frame<2> outputFrame = outputBuffer.shift();
			// Inverting op-amp on outputs
			outputs[OUT_OUTPUT].setVoltage(-outputFrame.samples[0] * 5.f);
			outputs[AUX_OUTPUT].setVoltage(-outputFrame.samples[1] * 5.f);
		}
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
	PlaitsWidget(Plaits *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Plaits.svg")));

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

	void appendContextMenu(Menu *menu) override {
		Plaits *module = dynamic_cast<Plaits*>(this->module);

		struct PlaitsLowCpuItem : MenuItem {
			Plaits *module;
			void onAction(const event::Action &e) override {
				module->lowCpu ^= true;
			}
		};

		struct PlaitsLPGItem : MenuItem {
			Plaits *module;
			void onAction(const event::Action &e) override {
				module->lpg ^= true;
			}
		};

		struct PlaitsModelItem : MenuItem {
			Plaits *module;
			int model;
			void onAction(const event::Action &e) override {
				module->patch.engine = model;
			}
		};

		menu->addChild(new MenuEntry);
		PlaitsLowCpuItem *lowCpuItem = createMenuItem<PlaitsLowCpuItem>("Low CPU", CHECKMARK(module->lowCpu));
		lowCpuItem->module = module;
		menu->addChild(lowCpuItem);
		PlaitsLPGItem *lpgItem = createMenuItem<PlaitsLPGItem>("Edit LPG response/decay", CHECKMARK(module->lpg));
		lpgItem->module = module;
		menu->addChild(lpgItem);

		menu->addChild(new MenuEntry());
		menu->addChild(createMenuLabel("Models"));
		for (int i = 0; i < 16; i++) {
			PlaitsModelItem *modelItem = createMenuItem<PlaitsModelItem>(modelLabels[i], CHECKMARK(module->patch.engine == i));
			modelItem->module = module;
			modelItem->model = i;
			menu->addChild(modelItem);
		}
	}
};


Model *modelPlaits = createModel<Plaits, PlaitsWidget>("Plaits");

