#include "AudibleInstruments.hpp"
#include "dsp/digital.hpp"
// #include "stages/chain_state.h"
#include "stages/segment_generator.h"
#include "stages/oscillator.h"


// Must match io_buffer.h
static const int NUM_CHANNELS = 6;
static const int BLOCK_SIZE = 8;


struct Stages : Module {
	enum ParamIds {
		ENUMS(SHAPE_PARAMS, NUM_CHANNELS),
		ENUMS(TYPE_PARAMS, NUM_CHANNELS),
		ENUMS(LEVEL_PARAMS, NUM_CHANNELS),
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(LEVEL_INPUTS, NUM_CHANNELS),
		ENUMS(GATE_INPUTS, NUM_CHANNELS),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(ENVELOPE_OUTPUTS, NUM_CHANNELS),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(TYPE_LIGHTS, NUM_CHANNELS*2),
		ENUMS(ENVELOPE_LIGHTS, NUM_CHANNELS),
		NUM_LIGHTS
	};

	stages::segment::Configuration configurations[NUM_CHANNELS];
	stages::SegmentGenerator segment_generator[NUM_CHANNELS];
	stages::Oscillator oscillator[NUM_CHANNELS];
	// stages::ChainState chain_state;
	// stages::Settings settings;

	// Buttons
	BooleanTrigger typeTriggers[NUM_CHANNELS];
	float pressedTime = 0.f;

	// Buffers
	float envelopeBuffer[NUM_CHANNELS][BLOCK_SIZE] = {};
	stmlib::GateFlags last_gate_flags[NUM_CHANNELS] = {};
	stmlib::GateFlags gate_flags[NUM_CHANNELS][BLOCK_SIZE] = {};
	int blockIndex = 0;

	Stages() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		// chain_state.Init(NULL, NULL);
		onReset();
	}

	void onReset() override {
		for (size_t i = 0; i < NUM_CHANNELS; ++i) {
			segment_generator[i].Init();
			oscillator[i].Init();

			configurations[i].type = stages::segment::TYPE_RAMP;
			configurations[i].loop = false;
		}
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();

		json_t *configurationsJ = json_array();
		for (int i = 0; i < NUM_CHANNELS; i++) {
			json_t *configurationJ = json_object();
			json_object_set_new(configurationJ, "type", json_integer(configurations[i].type));
			json_object_set_new(configurationJ, "loop", json_boolean(configurations[i].loop));
			json_array_insert_new(configurationsJ, i, configurationJ);
		}
		json_object_set_new(rootJ, "configurations", configurationsJ);

		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		json_t *configurationsJ = json_object_get(rootJ, "configurations");
		for (int i = 0; i < NUM_CHANNELS; i++) {
			json_t *configurationJ = json_array_get(configurationsJ, i);
			if (configurationJ) {
				json_t *typeJ = json_object_get(configurationJ, "type");
				if (typeJ)
					configurations[i].type = (stages::segment::Type) json_integer_value(typeJ);

				json_t *loopJ = json_object_get(configurationJ, "loop");
				if (loopJ)
					configurations[i].loop = json_boolean_value(loopJ);
			}
		}
	}

	void stepBlock() {
		// Get parameters
		float primaries[NUM_CHANNELS];
		float secondaries[NUM_CHANNELS];
		for (int i = 0; i < NUM_CHANNELS; i++) {
			primaries[i] = clamp(params[LEVEL_PARAMS + i].value + inputs[LEVEL_INPUTS].value / 8.f, 0.f, 1.f);
			secondaries[i] = params[SHAPE_PARAMS + i].value;
		}

		// // Get patched state
		// for (int i = 0; i < NUM_CHANNELS; i++) {
		// 	bool p = inputs[GATE_INPUTS + i].active;
		// 	if (p != patched[i])
		// 		dirty_configurations[i] = true;
		// 	patched[i] = p;
		// }

		// Process block
		stages::SegmentGenerator::Output out[BLOCK_SIZE] = {};
		for (int i = 0; i < NUM_CHANNELS; i++) {
			bool led_state = segment_generator[i].Process(gate_flags[i], out, BLOCK_SIZE);
			for (int j = 0; j < BLOCK_SIZE; j++) {
				envelopeBuffer[i][j] = out[j].value;
			}
		}
	}

	void step() override {
		// Buttons
		for (int i = 0; i < NUM_CHANNELS; i++) {
			bool pressed = params[TYPE_PARAMS + i].value > 0.f;
			if (pressed) {
				pressedTime += engineGetSampleTime();
			}
			// Check if released
			if (typeTriggers[i].process(!pressed)) {
				if (pressedTime >= 1.f) {
					configurations[i].loop = !configurations[i].loop;
				}
				else {
					configurations[i].type = (stages::segment::Type) ((configurations[i].type + 1) % 3);
				}
				pressedTime = 0.f;
			}
		}

		// Input
		for (int i = 0; i < NUM_CHANNELS; i++) {
			bool gate = (inputs[GATE_INPUTS + i].value >= 1.7f);
			last_gate_flags[i] = stmlib::ExtractGateFlags(last_gate_flags[i], gate);
			gate_flags[i][blockIndex] = last_gate_flags[i];
		}

		// Process block
		if (++blockIndex >= BLOCK_SIZE) {
			blockIndex = 0;
			stepBlock();
		}

		// Output
		for (int i = 0; i < NUM_CHANNELS; i++) {
			float envelope = envelopeBuffer[i][blockIndex];
			outputs[ENVELOPE_OUTPUTS + i].value = envelope * 10.f;
			lights[ENVELOPE_LIGHTS + i].setBrightnessSmooth(envelope);

			lights[TYPE_LIGHTS + i*2 + 0].setBrightness(configurations[i].type == 0 || configurations[i].type == 1);
			lights[TYPE_LIGHTS + i*2 + 1].setBrightness(configurations[i].type == 1 || configurations[i].type == 2);
		}
	}
};


struct StagesWidget : ModuleWidget {
	StagesWidget(Stages *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/Stages.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(ParamWidget::create<Trimpot>(mm2px(Vec(3.72965, 13.98158)), module, Stages::SHAPE_PARAMS + 0, 0.0, 1.0, 0.5));
		addParam(ParamWidget::create<Trimpot>(mm2px(Vec(15.17012, 13.98158)), module, Stages::SHAPE_PARAMS + 1, 0.0, 1.0, 0.5));
		addParam(ParamWidget::create<Trimpot>(mm2px(Vec(26.6099, 13.98158)), module, Stages::SHAPE_PARAMS + 2, 0.0, 1.0, 0.5));
		addParam(ParamWidget::create<Trimpot>(mm2px(Vec(38.07174, 13.98158)), module, Stages::SHAPE_PARAMS + 3, 0.0, 1.0, 0.5));
		addParam(ParamWidget::create<Trimpot>(mm2px(Vec(49.51152, 13.98158)), module, Stages::SHAPE_PARAMS + 4, 0.0, 1.0, 0.5));
		addParam(ParamWidget::create<Trimpot>(mm2px(Vec(60.95199, 13.98158)), module, Stages::SHAPE_PARAMS + 5, 0.0, 1.0, 0.5));
		addParam(ParamWidget::create<TL1105>(mm2px(Vec(4.17259, 32.37248)), module, Stages::TYPE_PARAMS + 0, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<TL1105>(mm2px(Vec(15.61237, 32.37248)), module, Stages::TYPE_PARAMS + 1, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<TL1105>(mm2px(Vec(27.05284, 32.37248)), module, Stages::TYPE_PARAMS + 2, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<TL1105>(mm2px(Vec(38.51399, 32.37248)), module, Stages::TYPE_PARAMS + 3, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<TL1105>(mm2px(Vec(49.95446, 32.37248)), module, Stages::TYPE_PARAMS + 4, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<TL1105>(mm2px(Vec(61.39424, 32.37248)), module, Stages::TYPE_PARAMS + 5, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<LEDSliderGreen>(mm2px(Vec(3.36193, 43.06508)), module, Stages::LEVEL_PARAMS + 0, 0.0, 1.0, 1.0));
		addParam(ParamWidget::create<LEDSliderGreen>(mm2px(Vec(14.81619, 43.06508)), module, Stages::LEVEL_PARAMS + 1, 0.0, 1.0, 1.0));
		addParam(ParamWidget::create<LEDSliderGreen>(mm2px(Vec(26.26975, 43.06508)), module, Stages::LEVEL_PARAMS + 2, 0.0, 1.0, 1.0));
		addParam(ParamWidget::create<LEDSliderGreen>(mm2px(Vec(37.70265, 43.06508)), module, Stages::LEVEL_PARAMS + 3, 0.0, 1.0, 1.0));
		addParam(ParamWidget::create<LEDSliderGreen>(mm2px(Vec(49.15759, 43.06508)), module, Stages::LEVEL_PARAMS + 4, 0.0, 1.0, 1.0));
		addParam(ParamWidget::create<LEDSliderGreen>(mm2px(Vec(60.61184, 43.06508)), module, Stages::LEVEL_PARAMS + 5, 0.0, 1.0, 1.0));

		addInput(Port::create<PJ301MPort>(mm2px(Vec(2.70756, 77.75277)), Port::INPUT, module, Stages::LEVEL_INPUTS + 0));
		addInput(Port::create<PJ301MPort>(mm2px(Vec(14.14734, 77.75277)), Port::INPUT, module, Stages::LEVEL_INPUTS + 1));
		addInput(Port::create<PJ301MPort>(mm2px(Vec(25.58781, 77.75277)), Port::INPUT, module, Stages::LEVEL_INPUTS + 2));
		addInput(Port::create<PJ301MPort>(mm2px(Vec(37.04896, 77.75277)), Port::INPUT, module, Stages::LEVEL_INPUTS + 3));
		addInput(Port::create<PJ301MPort>(mm2px(Vec(48.48943, 77.75277)), Port::INPUT, module, Stages::LEVEL_INPUTS + 4));
		addInput(Port::create<PJ301MPort>(mm2px(Vec(59.92921, 77.75277)), Port::INPUT, module, Stages::LEVEL_INPUTS + 5));
		addInput(Port::create<PJ301MPort>(mm2px(Vec(2.70756, 92.35239)), Port::INPUT, module, Stages::GATE_INPUTS + 0));
		addInput(Port::create<PJ301MPort>(mm2px(Vec(14.14734, 92.35239)), Port::INPUT, module, Stages::GATE_INPUTS + 1));
		addInput(Port::create<PJ301MPort>(mm2px(Vec(25.58781, 92.35239)), Port::INPUT, module, Stages::GATE_INPUTS + 2));
		addInput(Port::create<PJ301MPort>(mm2px(Vec(37.04896, 92.35239)), Port::INPUT, module, Stages::GATE_INPUTS + 3));
		addInput(Port::create<PJ301MPort>(mm2px(Vec(48.48943, 92.35239)), Port::INPUT, module, Stages::GATE_INPUTS + 4));
		addInput(Port::create<PJ301MPort>(mm2px(Vec(59.92921, 92.35239)), Port::INPUT, module, Stages::GATE_INPUTS + 5));

		addOutput(Port::create<PJ301MPort>(mm2px(Vec(2.70756, 106.95203)), Port::OUTPUT, module, Stages::ENVELOPE_OUTPUTS + 0));
		addOutput(Port::create<PJ301MPort>(mm2px(Vec(14.14734, 106.95203)), Port::OUTPUT, module, Stages::ENVELOPE_OUTPUTS + 1));
		addOutput(Port::create<PJ301MPort>(mm2px(Vec(25.58781, 106.95203)), Port::OUTPUT, module, Stages::ENVELOPE_OUTPUTS + 2));
		addOutput(Port::create<PJ301MPort>(mm2px(Vec(37.04896, 106.95203)), Port::OUTPUT, module, Stages::ENVELOPE_OUTPUTS + 3));
		addOutput(Port::create<PJ301MPort>(mm2px(Vec(48.48943, 106.95203)), Port::OUTPUT, module, Stages::ENVELOPE_OUTPUTS + 4));
		addOutput(Port::create<PJ301MPort>(mm2px(Vec(59.92921, 106.95203)), Port::OUTPUT, module, Stages::ENVELOPE_OUTPUTS + 5));

		addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(mm2px(Vec(5.27737, 26.74447)), module, Stages::TYPE_LIGHTS + 0*2));
		addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(mm2px(Vec(16.73784, 26.74447)), module, Stages::TYPE_LIGHTS + 1*2));
		addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(mm2px(Vec(28.1783, 26.74447)), module, Stages::TYPE_LIGHTS + 2*2));
		addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(mm2px(Vec(39.61877, 26.74447)), module, Stages::TYPE_LIGHTS + 3*2));
		addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(mm2px(Vec(51.07923, 26.74447)), module, Stages::TYPE_LIGHTS + 4*2));
		addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(mm2px(Vec(62.51971, 26.74447)), module, Stages::TYPE_LIGHTS + 5*2));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(mm2px(Vec(2.29462, 103.19253)), module, Stages::ENVELOPE_LIGHTS + 0));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(mm2px(Vec(13.73509, 103.19253)), module, Stages::ENVELOPE_LIGHTS + 1));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(mm2px(Vec(25.17556, 103.19253)), module, Stages::ENVELOPE_LIGHTS + 2));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(mm2px(Vec(36.63671, 103.19253)), module, Stages::ENVELOPE_LIGHTS + 3));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(mm2px(Vec(48.07649, 103.19253)), module, Stages::ENVELOPE_LIGHTS + 4));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(mm2px(Vec(59.51696, 103.19253)), module, Stages::ENVELOPE_LIGHTS + 5));
	}
};


Model *modelStages = Model::create<Stages, StagesWidget>("Audible Instruments", "Stages", "Segment Generator", FUNCTION_GENERATOR_TAG, ENVELOPE_GENERATOR_TAG);
