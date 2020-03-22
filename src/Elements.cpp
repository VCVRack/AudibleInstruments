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

	dsp::SampleRateConverter<2> inputSrc;
	dsp::SampleRateConverter<2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> inputBuffer;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> outputBuffer;

	uint16_t reverb_buffer[32768] = {};
	elements::Part *part;

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
		configParam(BOW_TIMBRE_MOD_PARAM, -1.0, 1.0, 0.0, "Bow timbre attenuverter");
		configParam(FLOW_MOD_PARAM, -1.0, 1.0, 0.0, "Air flow noise attenuverter");
		configParam(BLOW_TIMBRE_MOD_PARAM, -1.0, 1.0, 0.0, "Blow timbre attenuverter");
		configParam(MALLET_MOD_PARAM, -1.0, 1.0, 0.0, "Percussive noise attenuverter");
		configParam(STRIKE_TIMBRE_MOD_PARAM, -1.0, 1.0, 0.0, "Strike timbre attenuverter");
		configParam(DAMPING_MOD_PARAM, -1.0, 1.0, 0.0, "Energy dissipation speed attenuverter");
		configParam(GEOMETRY_MOD_PARAM, -1.0, 1.0, 0.0, "Resonator geometry attenuverter");
		configParam(POSITION_MOD_PARAM, -1.0, 1.0, 0.0, "Excitation position attenuverter");
		configParam(BRIGHTNESS_MOD_PARAM, -1.0, 1.0, 0.0, "Brightness attenuverter");
		configParam(SPACE_MOD_PARAM, -2.0, 2.0, 0.0, "Reverb space attenuverter");
		configParam(PLAY_PARAM, 0.0, 1.0, 0.0, "Play");

		part = new elements::Part();
		// In the Mutable Instruments code, Part doesn't initialize itself, so zero it here.
		memset(part, 0, sizeof(*part));
		part->Init(reverb_buffer);
		// Just some random numbers
		uint32_t seed[3] = {1, 2, 3};
		part->Seed(seed, 3);
	}

	~Elements() {
		delete part;
	}

	void process(const ProcessArgs &args) override {
		// Get input
		if (!inputBuffer.full()) {
			dsp::Frame<2> inputFrame;
			inputFrame.samples[0] = inputs[BLOW_INPUT].getVoltage() / 5.0;
			inputFrame.samples[1] = inputs[STRIKE_INPUT].getVoltage() / 5.0;
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
				inputSrc.setRates(args.sampleRate, 32000);
				dsp::Frame<2> inputFrames[16];
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
			p->exciter_envelope_shape = params[CONTOUR_PARAM].getValue();
			p->exciter_bow_level = params[BOW_PARAM].getValue();
			p->exciter_blow_level = params[BLOW_PARAM].getValue();
			p->exciter_strike_level = params[STRIKE_PARAM].getValue();

#define BIND(_p, _m, _i) clamp(params[_p].getValue() + 3.3f*dsp::quadraticBipolar(params[_m].getValue())*inputs[_i].getVoltage()/5.0f, 0.0f, 0.9995f)

			p->exciter_bow_timbre = BIND(BOW_TIMBRE_PARAM, BOW_TIMBRE_MOD_PARAM, BOW_TIMBRE_MOD_INPUT);
			p->exciter_blow_meta = BIND(FLOW_PARAM, FLOW_MOD_PARAM, FLOW_MOD_INPUT);
			p->exciter_blow_timbre = BIND(BLOW_TIMBRE_PARAM, BLOW_TIMBRE_MOD_PARAM, BLOW_TIMBRE_MOD_INPUT);
			p->exciter_strike_meta = BIND(MALLET_PARAM, MALLET_MOD_PARAM, MALLET_MOD_INPUT);
			p->exciter_strike_timbre = BIND(STRIKE_TIMBRE_PARAM, STRIKE_TIMBRE_MOD_PARAM, STRIKE_TIMBRE_MOD_INPUT);
			p->resonator_geometry = BIND(GEOMETRY_PARAM, GEOMETRY_MOD_PARAM, GEOMETRY_MOD_INPUT);
			p->resonator_brightness = BIND(BRIGHTNESS_PARAM, BRIGHTNESS_MOD_PARAM, BRIGHTNESS_MOD_INPUT);
			p->resonator_damping = BIND(DAMPING_PARAM, DAMPING_MOD_PARAM, DAMPING_MOD_INPUT);
			p->resonator_position = BIND(POSITION_PARAM, POSITION_MOD_PARAM, POSITION_MOD_INPUT);
			p->space = clamp(params[SPACE_PARAM].getValue() + params[SPACE_MOD_PARAM].getValue()*inputs[SPACE_MOD_INPUT].getVoltage()/5.0f, 0.0f, 2.0f);

			// Get performance inputs
			elements::PerformanceState performance;
			performance.note = 12.0*inputs[NOTE_INPUT].getVoltage() + roundf(params[COARSE_PARAM].getValue()) + params[FINE_PARAM].getValue() + 69.0;
			performance.modulation = 3.3*dsp::quarticBipolar(params[FM_PARAM].getValue()) * 49.5 * inputs[FM_INPUT].getVoltage()/5.0;
			performance.gate = params[PLAY_PARAM].getValue() >= 1.0 || inputs[GATE_INPUT].getVoltage() >= 1.0;
			performance.strength = clamp(1.0 - inputs[STRENGTH_INPUT].getVoltage()/5.0f, 0.0f, 1.0f);

			// Generate audio
			part->Process(performance, blow, strike, main, aux, 16);

			// Convert output buffer
			{
				dsp::Frame<2> outputFrames[16];
				for (int i = 0; i < 16; i++) {
					outputFrames[i].samples[0] = main[i];
					outputFrames[i].samples[1] = aux[i];
				}

				outputSrc.setRates(32000, args.sampleRate);
				int inLen = 16;
				int outLen = outputBuffer.capacity();
				outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
				outputBuffer.endIncr(outLen);
			}

			// Set lights
			lights[GATE_LIGHT].setBrightness(performance.gate ? 0.75 : 0.0);
			lights[EXCITER_LIGHT].setBrightness(part->exciter_level());
			lights[RESONATOR_LIGHT].setBrightness(part->resonator_level());
		}

		// Set output
		if (!outputBuffer.empty()) {
			dsp::Frame<2> outputFrame = outputBuffer.shift();
			outputs[AUX_OUTPUT].setVoltage(5.0 * outputFrame.samples[0]);
			outputs[MAIN_OUTPUT].setVoltage(5.0 * outputFrame.samples[1]);
		}
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "model", json_integer(getModel()));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
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


struct ElementsModalItem : MenuItem {
	Elements *elements;
	int model;
	void onAction(const event::Action &e) override {
		elements->setModel(model);
	}
	void step() override {
		rightText = CHECKMARK(elements->getModel() == model);
		MenuItem::step();
	}
};


struct ElementsWidget : ModuleWidget {
	ElementsWidget(Elements *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Elements.svg")));

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
				box.size = Vec(28-6, 28-6);
				bgColor = color::BLACK_TRANSPARENT;
			}
		};

		addChild(createLight<GateLight>(Vec(36+3, 116+3), module, Elements::GATE_LIGHT));
		addChild(createLight<MediumLight<GreenLight>>(Vec(184, 165), module, Elements::EXCITER_LIGHT));
		addChild(createLight<MediumLight<RedLight>>(Vec(395, 165), module, Elements::RESONATOR_LIGHT));
	}

	void appendContextMenu(Menu *menu) override {
		Elements *elements = dynamic_cast<Elements*>(module);
		assert(elements);

		menu->addChild(construct<MenuLabel>());
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Alternative models"));
		menu->addChild(construct<ElementsModalItem>(&MenuItem::text, "Original", &ElementsModalItem::elements, elements, &ElementsModalItem::model, 0));
		menu->addChild(construct<ElementsModalItem>(&MenuItem::text, "Non-linear string", &ElementsModalItem::elements, elements, &ElementsModalItem::model, 1));
		menu->addChild(construct<ElementsModalItem>(&MenuItem::text, "Chords", &ElementsModalItem::elements, elements, &ElementsModalItem::model, 2));
	}
};


Model *modelElements = createModel<Elements, ElementsWidget>("Elements");
