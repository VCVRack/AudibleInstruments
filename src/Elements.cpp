#include "plugin.hpp"
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
	enum LightIds {
		GATE_LIGHT,
		EXCITER_LIGHT,
		RESONATOR_LIGHT,
		NUM_LIGHTS
	};

	dsp::SampleRateConverter<16 * 2> inputSrc;
	dsp::SampleRateConverter<16 * 2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<16 * 2>, 256> inputBuffer;
	dsp::DoubleRingBuffer<dsp::Frame<16 * 2>, 256> outputBuffer;

	uint16_t reverb_buffers[16][32768] = {};
	elements::Part* parts[16];

	Elements() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(CONTOUR_PARAM, 0.0, 1.0, 1.0, "Envelope contour");
		configParam(BOW_PARAM, 0.0, 1.0, 0.0, "Bow exciter");
		configParam(BLOW_PARAM, 0.0, 1.0, 0.0, "Blow exciter");
		configParam(STRIKE_PARAM, 0.0, 1.0, 0.5, "Percussive noise amount");
		configParam(COARSE_PARAM, -30.0, 30.0, 0.0, "Coarse frequency adjustment");
		configParam(FINE_PARAM, -2.0, 2.0, 0.0, "Fine frequency adjustment");
		configParam(FM_PARAM, -1.0, 1.0, 0.0, "FM input attenuverter");
		configParam(FLOW_PARAM, 0.0, 1.0, 0.5, "Air flow noise color");
		configParam(MALLET_PARAM, 0.0, 1.0, 0.5, "Percussive noise type");
		configParam(GEOMETRY_PARAM, 0.0, 1.0, 0.5, "Resonator geometry");
		configParam(BRIGHTNESS_PARAM, 0.0, 1.0, 0.5, "Brightness");
		configParam(BOW_TIMBRE_PARAM, 0.0, 1.0, 0.5, "Bow timbre");
		configParam(BLOW_TIMBRE_PARAM, 0.0, 1.0, 0.5, "Blow timbre");
		configParam(STRIKE_TIMBRE_PARAM, 0.0, 1.0, 0.5, "Strike timbre");
		configParam(DAMPING_PARAM, 0.0, 1.0, 0.5, "Energy dissipation speed");
		configParam(POSITION_PARAM, 0.0, 1.0, 0.5, "Excitation position");
		configParam(SPACE_PARAM, 0.0, 2.0, 0.0, "Reverb space");
		configParam(BOW_TIMBRE_MOD_PARAM, -1.0, 1.0, 0.0, "Bow timbre CV");
		configParam(FLOW_MOD_PARAM, -1.0, 1.0, 0.0, "Air flow noise CV");
		configParam(BLOW_TIMBRE_MOD_PARAM, -1.0, 1.0, 0.0, "Blow timbre CV");
		configParam(MALLET_MOD_PARAM, -1.0, 1.0, 0.0, "Percussive noise CV");
		configParam(STRIKE_TIMBRE_MOD_PARAM, -1.0, 1.0, 0.0, "Strike timbre CV");
		configParam(DAMPING_MOD_PARAM, -1.0, 1.0, 0.0, "Energy dissipation speed CV");
		configParam(GEOMETRY_MOD_PARAM, -1.0, 1.0, 0.0, "Resonator geometry CV");
		configParam(POSITION_MOD_PARAM, -1.0, 1.0, 0.0, "Excitation position CV");
		configParam(BRIGHTNESS_MOD_PARAM, -1.0, 1.0, 0.0, "Brightness CV");
		configParam(SPACE_MOD_PARAM, -2.0, 2.0, 0.0, "Reverb space CV");
		configButton(PLAY_PARAM, "Play");

		configInput(NOTE_INPUT, "Pitch (1V/oct)");
		configInput(FM_INPUT, "FM");
		configInput(GATE_INPUT, "Gate");
		configInput(STRENGTH_INPUT, "Strength");
		configInput(BLOW_INPUT, "External blow");
		configInput(STRIKE_INPUT, "External strike");
		configInput(BOW_TIMBRE_MOD_INPUT, "Bow timbre");
		configInput(FLOW_MOD_INPUT, "Air flow noise");
		configInput(BLOW_TIMBRE_MOD_INPUT, "Blow timbre");
		configInput(MALLET_MOD_INPUT, "Percussive noise");
		configInput(STRIKE_TIMBRE_MOD_INPUT, "Strike timbre");
		configInput(DAMPING_MOD_INPUT, "Energy dissipation speed");
		configInput(GEOMETRY_MOD_INPUT, "Resonator geometry");
		configInput(POSITION_MOD_INPUT, "Excitation position");
		configInput(BRIGHTNESS_MOD_INPUT, "Brightness");
		configInput(SPACE_MOD_INPUT, "Reverb space");

		configOutput(AUX_OUTPUT, "Left");
		configOutput(MAIN_OUTPUT, "Right");

		configBypass(BLOW_INPUT, AUX_OUTPUT);
		configBypass(STRIKE_INPUT, MAIN_OUTPUT);

		for (int c = 0; c < 16; c++) {
			parts[c] = new elements::Part();
			// In the Mutable Instruments code, Part doesn't initialize itself, so zero it here.
			std::memset(parts[c], 0, sizeof(*parts[c]));
			parts[c]->Init(reverb_buffers[c]);
			// Just some random numbers
			uint32_t seed[3] = {1, 2, 3};
			parts[c]->Seed(seed, 3);
		}
	}

	~Elements() {
		for (int c = 0; c < 16; c++) {
			delete parts[c];
		}
	}

	void onReset() override {
		setModel(0);
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[NOTE_INPUT].getChannels(), 1);

		// Get input
		if (!inputBuffer.full()) {
			dsp::Frame<16 * 2> inputFrame = {};
			for (int c = 0; c < channels; c++) {
				inputFrame.samples[c * 2 + 0] = inputs[BLOW_INPUT].getPolyVoltage(c) / 5.0;
				inputFrame.samples[c * 2 + 1] = inputs[STRIKE_INPUT].getPolyVoltage(c) / 5.0;
			}
			inputBuffer.push(inputFrame);
		}

		// Generate output if output buffer is empty
		if (outputBuffer.empty()) {
			// blow[channel][bufferIndex]
			float blow[16][16] = {};
			float strike[16][16] = {};

			// Convert input buffer
			{
				inputSrc.setRates(args.sampleRate, 32000);
				inputSrc.setChannels(channels * 2);
				int inLen = inputBuffer.size();
				int outLen = 16;
				dsp::Frame<16 * 2> inputFrames[outLen];
				inputSrc.process(inputBuffer.startData(), &inLen, inputFrames, &outLen);
				inputBuffer.startIncr(inLen);

				for (int c = 0; c < channels; c++) {
					for (int i = 0; i < outLen; i++) {
						blow[c][i] = inputFrames[i].samples[c * 2 + 0];
						strike[c][i] = inputFrames[i].samples[c * 2 + 1];
					}
				}
			}

			// Process channels
			// main[channel][bufferIndex]
			float main[16][16];
			float aux[16][16];
			float gateLight = 0.f;
			float exciterLight = 0.f;
			float resonatorLight = 0.f;

			for (int c = 0; c < channels; c++) {
				// Set patch from parameters
				elements::Patch* p = parts[c]->mutable_patch();
				p->exciter_envelope_shape = params[CONTOUR_PARAM].getValue();
				p->exciter_bow_level = params[BOW_PARAM].getValue();
				p->exciter_blow_level = params[BLOW_PARAM].getValue();
				p->exciter_strike_level = params[STRIKE_PARAM].getValue();

#define BIND(_p, _m, _i) clamp(params[_p].getValue() + 3.3f * dsp::quadraticBipolar(params[_m].getValue()) * inputs[_i].getPolyVoltage(c) / 5.f, 0.f, 0.9995f)

				p->exciter_bow_timbre = BIND(BOW_TIMBRE_PARAM, BOW_TIMBRE_MOD_PARAM, BOW_TIMBRE_MOD_INPUT);
				p->exciter_blow_meta = BIND(FLOW_PARAM, FLOW_MOD_PARAM, FLOW_MOD_INPUT);
				p->exciter_blow_timbre = BIND(BLOW_TIMBRE_PARAM, BLOW_TIMBRE_MOD_PARAM, BLOW_TIMBRE_MOD_INPUT);
				p->exciter_strike_meta = BIND(MALLET_PARAM, MALLET_MOD_PARAM, MALLET_MOD_INPUT);
				p->exciter_strike_timbre = BIND(STRIKE_TIMBRE_PARAM, STRIKE_TIMBRE_MOD_PARAM, STRIKE_TIMBRE_MOD_INPUT);
				p->resonator_geometry = BIND(GEOMETRY_PARAM, GEOMETRY_MOD_PARAM, GEOMETRY_MOD_INPUT);
				p->resonator_brightness = BIND(BRIGHTNESS_PARAM, BRIGHTNESS_MOD_PARAM, BRIGHTNESS_MOD_INPUT);
				p->resonator_damping = BIND(DAMPING_PARAM, DAMPING_MOD_PARAM, DAMPING_MOD_INPUT);
				p->resonator_position = BIND(POSITION_PARAM, POSITION_MOD_PARAM, POSITION_MOD_INPUT);
				p->space = clamp(params[SPACE_PARAM].getValue() + params[SPACE_MOD_PARAM].getValue() * inputs[SPACE_MOD_INPUT].getPolyVoltage(c) / 5.f, 0.f, 2.f);

				// Get performance inputs
				elements::PerformanceState performance;
				performance.note = 12.f * inputs[NOTE_INPUT].getVoltage(c) + std::round(params[COARSE_PARAM].getValue()) + params[FINE_PARAM].getValue() + 69.f;
				performance.modulation = 3.3f * dsp::quarticBipolar(params[FM_PARAM].getValue()) * 49.5f * inputs[FM_INPUT].getPolyVoltage(c) / 5.f;
				performance.gate = params[PLAY_PARAM].getValue() >= 1.f || inputs[GATE_INPUT].getPolyVoltage(c) >= 1.f;
				performance.strength = clamp(1.f - inputs[STRENGTH_INPUT].getPolyVoltage(c) / 5.f, 0.f, 1.f);

				// Generate audio
				parts[c]->Process(performance, blow[c], strike[c], main[c], aux[c], 16);

				// Set lights based on first poly channel
				gateLight = std::max(gateLight, performance.gate ? 0.75f : 0.f);
				exciterLight = std::max(exciterLight, parts[c]->exciter_level());
				resonatorLight = std::max(resonatorLight, parts[c]->resonator_level());
			}

			// Set lights
			lights[GATE_LIGHT].setBrightness(gateLight);
			lights[EXCITER_LIGHT].setBrightness(exciterLight);
			lights[RESONATOR_LIGHT].setBrightness(resonatorLight);

			// Convert output buffer
			{
				dsp::Frame<16 * 2> outputFrames[16];
				for (int c = 0; c < channels; c++) {
					for (int i = 0; i < 16; i++) {
						outputFrames[i].samples[c * 2 + 0] = main[c][i];
						outputFrames[i].samples[c * 2 + 1] = aux[c][i];
					}
				}

				outputSrc.setRates(32000, args.sampleRate);
				outputSrc.setChannels(channels * 2);
				int inLen = 16;
				int outLen = outputBuffer.capacity();
				outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
				outputBuffer.endIncr(outLen);
			}
		}

		// Set output
		if (!outputBuffer.empty()) {
			dsp::Frame<16 * 2> outputFrame = outputBuffer.shift();
			for (int c = 0; c < channels; c++) {
				outputs[AUX_OUTPUT].setVoltage(5.f * outputFrame.samples[c * 2 + 0], c);
				outputs[MAIN_OUTPUT].setVoltage(5.f * outputFrame.samples[c * 2 + 1], c);
			}
		}

		outputs[AUX_OUTPUT].setChannels(channels);
		outputs[MAIN_OUTPUT].setChannels(channels);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "model", json_integer(getModel()));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* modelJ = json_object_get(rootJ, "model");
		if (modelJ) {
			setModel(json_integer_value(modelJ));
		}
	}

	int getModel() {
		// Use the first channel's Part as the reference model
		if (parts[0]->easter_egg())
			return -1;
		return (int) parts[0]->resonator_model();
	}

	/** Sets the resonator model.
	-1 means easter egg (Ominous voice)
	*/
	void setModel(int model) {
		if (model < 0) {
			for (int c = 0; c < 16; c++) {
				parts[c]->set_easter_egg(true);
			}
		}
		else {
			for (int c = 0; c < 16; c++) {
				parts[c]->set_easter_egg(false);
				parts[c]->set_resonator_model((elements::ResonatorModel) model);
			}
		}
	}
};


struct ElementsWidget : ModuleWidget {
	ElementsWidget(Elements* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Elements.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(480, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(480, 365)));

		addParam(createParam<Rogan1PSWhite>(Vec(28, 42), module, Elements::CONTOUR_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(99, 42), module, Elements::BOW_PARAM));
		addParam(createParam<Rogan1PSRed>(Vec(169, 42), module, Elements::BLOW_PARAM));
		addParam(createParam<Rogan1PSGreen>(Vec(239, 42), module, Elements::STRIKE_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(310, 42), module, Elements::COARSE_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(381, 42), module, Elements::FINE_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(451, 42), module, Elements::FM_PARAM));

		addParam(createParam<Rogan3PSRed>(Vec(115, 116), module, Elements::FLOW_PARAM));
		addParam(createParam<Rogan3PSGreen>(Vec(212, 116), module, Elements::MALLET_PARAM));
		addParam(createParam<Rogan3PSWhite>(Vec(326, 116), module, Elements::GEOMETRY_PARAM));
		addParam(createParam<Rogan3PSWhite>(Vec(423, 116), module, Elements::BRIGHTNESS_PARAM));

		addParam(createParam<Rogan1PSWhite>(Vec(99, 202), module, Elements::BOW_TIMBRE_PARAM));
		addParam(createParam<Rogan1PSRed>(Vec(170, 202), module, Elements::BLOW_TIMBRE_PARAM));
		addParam(createParam<Rogan1PSGreen>(Vec(239, 202), module, Elements::STRIKE_TIMBRE_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(310, 202), module, Elements::DAMPING_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(380, 202), module, Elements::POSITION_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(451, 202), module, Elements::SPACE_PARAM));

		addParam(createParam<Trimpot>(Vec(104.5, 273), module, Elements::BOW_TIMBRE_MOD_PARAM));
		addParam(createParam<Trimpot>(Vec(142.5, 273), module, Elements::FLOW_MOD_PARAM));
		addParam(createParam<Trimpot>(Vec(181.5, 273), module, Elements::BLOW_TIMBRE_MOD_PARAM));
		addParam(createParam<Trimpot>(Vec(219.5, 273), module, Elements::MALLET_MOD_PARAM));
		addParam(createParam<Trimpot>(Vec(257.5, 273), module, Elements::STRIKE_TIMBRE_MOD_PARAM));
		addParam(createParam<Trimpot>(Vec(315.5, 273), module, Elements::DAMPING_MOD_PARAM));
		addParam(createParam<Trimpot>(Vec(354.5, 273), module, Elements::GEOMETRY_MOD_PARAM));
		addParam(createParam<Trimpot>(Vec(392.5, 273), module, Elements::POSITION_MOD_PARAM));
		addParam(createParam<Trimpot>(Vec(430.5, 273), module, Elements::BRIGHTNESS_MOD_PARAM));
		addParam(createParam<Trimpot>(Vec(469.5, 273), module, Elements::SPACE_MOD_PARAM));

		addInput(createInput<PJ301MPort>(Vec(20, 178), module, Elements::NOTE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(55, 178), module, Elements::FM_INPUT));

		addInput(createInput<PJ301MPort>(Vec(20, 224), module, Elements::GATE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(55, 224), module, Elements::STRENGTH_INPUT));

		addInput(createInput<PJ301MPort>(Vec(20, 270), module, Elements::BLOW_INPUT));
		addInput(createInput<PJ301MPort>(Vec(55, 270), module, Elements::STRIKE_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(20, 316), module, Elements::AUX_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(55, 316), module, Elements::MAIN_OUTPUT));

		addInput(createInput<PJ301MPort>(Vec(101, 316), module, Elements::BOW_TIMBRE_MOD_INPUT));
		addInput(createInput<PJ301MPort>(Vec(139, 316), module, Elements::FLOW_MOD_INPUT));
		addInput(createInput<PJ301MPort>(Vec(178, 316), module, Elements::BLOW_TIMBRE_MOD_INPUT));
		addInput(createInput<PJ301MPort>(Vec(216, 316), module, Elements::MALLET_MOD_INPUT));
		addInput(createInput<PJ301MPort>(Vec(254, 316), module, Elements::STRIKE_TIMBRE_MOD_INPUT));
		addInput(createInput<PJ301MPort>(Vec(312, 316), module, Elements::DAMPING_MOD_INPUT));
		addInput(createInput<PJ301MPort>(Vec(350, 316), module, Elements::GEOMETRY_MOD_INPUT));
		addInput(createInput<PJ301MPort>(Vec(389, 316), module, Elements::POSITION_MOD_INPUT));
		addInput(createInput<PJ301MPort>(Vec(427, 316), module, Elements::BRIGHTNESS_MOD_INPUT));
		addInput(createInput<PJ301MPort>(Vec(466, 316), module, Elements::SPACE_MOD_INPUT));

		addParam(createParam<CKD6>(Vec(36, 116), module, Elements::PLAY_PARAM));

		struct GateLight : YellowLight {
			GateLight() {
				box.size = Vec(28 - 6, 28 - 6);
				bgColor = color::BLACK_TRANSPARENT;
			}
		};

		addChild(createLight<GateLight>(Vec(36 + 3, 116 + 3), module, Elements::GATE_LIGHT));
		addChild(createLight<MediumLight<GreenLight>>(Vec(184, 165), module, Elements::EXCITER_LIGHT));
		addChild(createLight<MediumLight<RedLight>>(Vec(395, 165), module, Elements::RESONATOR_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		Elements* module = dynamic_cast<Elements*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createMenuLabel("Models"));

		static const std::vector<std::pair<std::string, int>> modelLabels = {
			std::make_pair("Original", 0),
			std::make_pair("Non-linear string", 1),
			std::make_pair("Chords", 2),
			std::make_pair("Ominous voice", -1),
		};
		
		for (auto modelLabel : modelLabels) {
			menu->addChild(createCheckMenuItem(modelLabel.first, "",
				[=]() {return module->getModel() == modelLabel.second;},
				[=]() {module->setModel(modelLabel.second);}
			));
		}
	}
};


Model* modelElements = createModel<Elements, ElementsWidget>("Elements");
