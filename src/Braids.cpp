#include "plugin.hpp"
#include "braids/macro_oscillator.h"
#include "braids/vco_jitter_source.h"
#include "braids/signature_waveshaper.h"


struct Braids : Module {
	enum ParamIds {
		FINE_PARAM,
		COARSE_PARAM,
		FM_PARAM,
		TIMBRE_PARAM,
		MODULATION_PARAM,
		COLOR_PARAM,
		SHAPE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TRIG_INPUT,
		PITCH_INPUT,
		FM_INPUT,
		TIMBRE_INPUT,
		COLOR_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};

	braids::MacroOscillator osc;
	braids::SettingsData settings;
	braids::VcoJitterSource jitter_source;
	braids::SignatureWaveshaper ws;

	dsp::SampleRateConverter<1> src;
	dsp::DoubleRingBuffer<dsp::Frame<1>, 256> outputBuffer;
	bool lastTrig = false;
	bool lowCpu = false;

	Braids() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
		configParam(SHAPE_PARAM, 0.0, 1.0, 0.0, "Model");
		configParam(FINE_PARAM, -1.0, 1.0, 0.0, "Fine frequency", " semitones");
		configParam(COARSE_PARAM, -5.0, 3.0, -1.0, "Coarse frequency", " semitones", 0.f, 12.f, 12.f);
		configParam(FM_PARAM, -1.0, 1.0, 0.0, "FM");
		configParam(TIMBRE_PARAM, 0.0, 1.0, 0.5, "Timbre", "%", 0.f, 100.f);
		configParam(MODULATION_PARAM, -1.0, 1.0, 0.0, "Modulation");
		configParam(COLOR_PARAM, 0.0, 1.0, 0.5, "Color", "%", 0.f, 100.f);
		configInput(TRIG_INPUT, "Trigger");
		configInput(PITCH_INPUT, "Pitch (1V/oct)");
		configInput(FM_INPUT, "FM");
		configInput(TIMBRE_INPUT, "Timbre");
		configInput(COLOR_INPUT, "Color");
		configOutput(OUT_OUTPUT, "Audio");

		std::memset(&osc, 0, sizeof(osc));
		osc.Init();
		std::memset(&jitter_source, 0, sizeof(jitter_source));
		jitter_source.Init();
		std::memset(&ws, 0, sizeof(ws));
		ws.Init(0x0000);
		std::memset(&settings, 0, sizeof(settings));

		// List of supported settings
		settings.meta_modulation = 0;
		settings.vco_drift = 0;
		settings.signature = 0;
	}

	void process(const ProcessArgs& args) override {
		// Trigger
		bool trig = inputs[TRIG_INPUT].getVoltage() >= 1.0;
		if (!lastTrig && trig) {
			osc.Strike();
		}
		lastTrig = trig;

		// Render frames
		if (outputBuffer.empty()) {
			float fm = params[FM_PARAM].getValue() * inputs[FM_INPUT].getVoltage();

			// Set shape
			int shape = std::round(params[SHAPE_PARAM].getValue() * braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
			if (settings.meta_modulation) {
				shape += std::round(fm / 10.0 * braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
			}
			settings.shape = clamp(shape, 0, braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);

			// Setup oscillator from settings
			osc.set_shape((braids::MacroOscillatorShape) settings.shape);

			// Set timbre/modulation
			float timbre = params[TIMBRE_PARAM].getValue() + params[MODULATION_PARAM].getValue() * inputs[TIMBRE_INPUT].getVoltage() / 5.0;
			float modulation = params[COLOR_PARAM].getValue() + inputs[COLOR_INPUT].getVoltage() / 5.0;
			int16_t param1 = rescale(clamp(timbre, 0.0f, 1.0f), 0.0f, 1.0f, 0, INT16_MAX);
			int16_t param2 = rescale(clamp(modulation, 0.0f, 1.0f), 0.0f, 1.0f, 0, INT16_MAX);
			osc.set_parameters(param1, param2);

			// Set pitch
			float pitchV = inputs[PITCH_INPUT].getVoltage() + params[COARSE_PARAM].getValue() + params[FINE_PARAM].getValue() / 12.0;
			if (!settings.meta_modulation)
				pitchV += fm;
			if (lowCpu)
				pitchV += std::log2(96000.f * args.sampleTime);
			int32_t pitch = (pitchV * 12.0 + 60) * 128;
			pitch += jitter_source.Render(settings.vco_drift);
			pitch = clamp(pitch, 0, 16383);
			osc.set_pitch(pitch);

			// TODO: add a sync input buffer (must be sample rate converted)
			uint8_t sync_buffer[24] = {};

			int16_t render_buffer[24];
			osc.Render(sync_buffer, render_buffer, 24);

			// Signature waveshaping, decimation (not yet supported), and bit reduction (not yet supported)
			uint16_t signature = settings.signature * settings.signature * 4095;
			for (size_t i = 0; i < 24; i++) {
				const int16_t bit_mask = 0xffff;
				int16_t sample = render_buffer[i] & bit_mask;
				int16_t warped = ws.Transform(sample);
				render_buffer[i] = stmlib::Mix(sample, warped, signature);
			}

			if (lowCpu) {
				for (int i = 0; i < 24; i++) {
					dsp::Frame<1> f;
					f.samples[0] = render_buffer[i] / 32768.0;
					outputBuffer.push(f);
				}
			}
			else {
				// Sample rate convert
				dsp::Frame<1> in[24];
				for (int i = 0; i < 24; i++) {
					in[i].samples[0] = render_buffer[i] / 32768.0;
				}
				src.setRates(96000, args.sampleRate);

				int inLen = 24;
				int outLen = outputBuffer.capacity();
				src.process(in, &inLen, outputBuffer.endData(), &outLen);
				outputBuffer.endIncr(outLen);
			}
		}

		// Output
		if (!outputBuffer.empty()) {
			dsp::Frame<1> f = outputBuffer.shift();
			outputs[OUT_OUTPUT].setVoltage(5.0 * f.samples[0]);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_t* settingsJ = json_array();
		uint8_t* settingsArray = &settings.shape;
		for (int i = 0; i < 20; i++) {
			json_t* settingJ = json_integer(settingsArray[i]);
			json_array_insert_new(settingsJ, i, settingJ);
		}
		json_object_set_new(rootJ, "settings", settingsJ);

		json_t* lowCpuJ = json_boolean(lowCpu);
		json_object_set_new(rootJ, "lowCpu", lowCpuJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* settingsJ = json_object_get(rootJ, "settings");
		if (settingsJ) {
			uint8_t* settingsArray = &settings.shape;
			for (int i = 0; i < 20; i++) {
				json_t* settingJ = json_array_get(settingsJ, i);
				if (settingJ)
					settingsArray[i] = json_integer_value(settingJ);
			}
		}

		json_t* lowCpuJ = json_object_get(rootJ, "lowCpu");
		if (lowCpuJ) {
			lowCpu = json_boolean_value(lowCpuJ);
		}
	}

	int getShapeParam() {
		return std::round(params[SHAPE_PARAM].getValue() * braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
	}
	void setShapeParam(int shape) {
		params[SHAPE_PARAM].setValue(shape / (float) braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
	}
};


struct ShapeInfo {
	std::string code;
	std::string label;
};


static const std::vector<ShapeInfo> SHAPE_INFOS = {
	{"CSAW", "Quirky sawtooth"},
	{"/\\-_", "Triangle to saw"},
	{"//-_", "Sawtooth wave with dephasing"},
	{"FOLD", "Wavefolded sine/triangle"},
	{"uuuu", "Buzz"},
	{"SUB-", "Square sub"},
	{"SUB/", "Saw sub"},
	{"SYN-", "Square sync"},
	{"SYN/", "Saw sync"},
	{"//x3", "Triple saw"},
	{"-_x3", "Triple square"},
	{"/\\x3", "Triple triangle"},
	{"SIx3", "Triple sine"},
	{"RING", "Triple ring mod"},
	{"////", "Saw swarm"},
	{"//uu", "Saw comb"},
	{"TOY*", "Circuit-bent toy"},
	{"ZLPF", "Low-pass filtered waveform"},
	{"ZPKF", "Peak filtered waveform"},
	{"ZBPF", "Band-pass filtered waveform"},
	{"ZHPF", "High-pass filtered waveform"},
	{"VOSM", "VOSIM formant"},
	{"VOWL", "Speech synthesis"},
	{"VFOF", "FOF speech synthesis"},
	{"HARM", "12 sine harmonics"},
	{"FM  ", "2-operator phase-modulation"},
	{"FBFM", "2-operator phase-modulation with feedback"},
	{"WTFM", "2-operator phase-modulation with chaotic feedback"},
	{"PLUK", "Plucked string"},
	{"BOWD", "Bowed string"},
	{"BLOW", "Blown reed"},
	{"FLUT", "Flute"},
	{"BELL", "Bell"},
	{"DRUM", "Drum"},
	{"KICK", "Kick drum circuit simulation"},
	{"CYMB", "Cymbal"},
	{"SNAR", "Snare"},
	{"WTBL", "Wavetable"},
	{"WMAP", "2D wavetable"},
	{"WLIN", "1D wavetable"},
	{"WTx4", "4-voice paraphonic 1D wavetable"},
	{"NOIS", "Filtered noise"},
	{"TWNQ", "Twin peaks noise"},
	{"CLKN", "Clocked noise"},
	{"CLOU", "Granular cloud"},
	{"PRTC", "Particle noise"},
	{"QPSK", "Digital modulation"},
};


struct BraidsDisplay : TransparentWidget {
	Braids* module;
	std::shared_ptr<Font> font;

	BraidsDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/hdad-segment14-1.002/Segment14.ttf"));
	}

	void draw(const DrawArgs& args) override {
		int shape = module ? module->settings.shape : 0;

		// Background
		NVGcolor backgroundColor = nvgRGB(0x38, 0x38, 0x38);
		NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 5.0);
		nvgFillColor(args.vg, backgroundColor);
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, 1.0);
		nvgStrokeColor(args.vg, borderColor);
		nvgStroke(args.vg);

		// Text
		nvgGlobalAlpha(args.vg, 1.0);
		nvgFontSize(args.vg, 38);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 2.5);

		Vec textPos = Vec(9, 48);
		NVGcolor textColor = nvgRGB(0xaf, 0xd2, 0x2c);
		nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
		// Background of all segments
		nvgText(args.vg, textPos.x, textPos.y, "~~~~", NULL);
		nvgFillColor(args.vg, textColor);
		nvgText(args.vg, textPos.x, textPos.y, SHAPE_INFOS[shape].code.c_str(), NULL);
	}
};


struct BraidsWidget : ModuleWidget {
	BraidsWidget(Braids* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Braids.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(210, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(210, 365)));

		addParam(createParam<Rogan2SGray>(Vec(176, 59), module, Braids::SHAPE_PARAM));

		addParam(createParam<Rogan2PSWhite>(Vec(19, 138), module, Braids::FINE_PARAM));
		addParam(createParam<Rogan2PSWhite>(Vec(97, 138), module, Braids::COARSE_PARAM));
		addParam(createParam<Rogan2PSWhite>(Vec(176, 138), module, Braids::FM_PARAM));

		addParam(createParam<Rogan2PSGreen>(Vec(19, 217), module, Braids::TIMBRE_PARAM));
		addParam(createParam<Rogan2PSGreen>(Vec(97, 217), module, Braids::MODULATION_PARAM));
		addParam(createParam<Rogan2PSRed>(Vec(176, 217), module, Braids::COLOR_PARAM));

		addInput(createInput<PJ301MPort>(Vec(10, 316), module, Braids::TRIG_INPUT));
		addInput(createInput<PJ301MPort>(Vec(47, 316), module, Braids::PITCH_INPUT));
		addInput(createInput<PJ301MPort>(Vec(84, 316), module, Braids::FM_INPUT));
		addInput(createInput<PJ301MPort>(Vec(122, 316), module, Braids::TIMBRE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(160, 316), module, Braids::COLOR_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(205, 316), module, Braids::OUT_OUTPUT));

		BraidsDisplay* display = new BraidsDisplay();
		display->box.pos = Vec(14, 53);
		display->box.size = Vec(148, 56);
		display->module = module;
		addChild(display);
	}

	void appendContextMenu(Menu* menu) override {
		Braids* module = dynamic_cast<Braids*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> shapeLabels;
		for (const ShapeInfo& s : SHAPE_INFOS) {
			shapeLabels.push_back(s.code + ": " + s.label);
		}
		menu->addChild(createIndexSubmenuItem("Model", shapeLabels,
			[=]() {return module->getShapeParam();},
			[=](int i) {module->setShapeParam(i);}
		));

		menu->addChild(createBoolPtrMenuItem("META: FM CV selects model", &module->settings.meta_modulation));

		menu->addChild(createBoolMenuItem("DRFT: Pitch drift",
			[=]() {return module->settings.vco_drift;},
			[=](bool val) {module->settings.vco_drift = val ? 4 : 0;}
		));

		menu->addChild(createBoolMenuItem("SIGN: Waveform imperfections",
			[=]() {return module->settings.signature;},
			[=](bool val) {module->settings.signature = val ? 4 : 0;}
		));

		menu->addChild(createBoolPtrMenuItem("Low CPU (disable resampling)", &module->lowCpu));
	}
};


Model* modelBraids = createModel<Braids, BraidsWidget>("Braids");
