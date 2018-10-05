#include "AudibleInstruments.hpp"
#include "dsp/digital.hpp"
// #include "stages/chain_state.h"
#include "stages/segment_generator.h"
#include "stages/oscillator.h"

// Must match io_buffer.h
static const int NUM_CHANNELS = 6;
static const int BLOCK_SIZE = 8;
static const int MAX_MODULES = 6;

struct SineOscillator {
	float phase = 0.f;

	float step(float offset) {

		// Implement a simple sine oscillator
		float deltaTime = engineGetSampleTime();

		float freq = 0.5f;

		// Accumulate the phase
		phase += freq * deltaTime;
		if (phase >= 1.0f)
			phase -= 1.0f;

		// Compute the sine output
		float sine = sinf(2.0f * M_PI * (phase + offset));
		return sine;
	}
};

struct LongPressButton {

	enum Events {
		NO_PRESS,
		SHORT_PRESS,
		LONG_PRESS
	};

	float pressedTime = 0.f;
	BooleanTrigger trigger;

	Events step(Param& param) {
		auto result = NO_PRESS;

		bool pressed = param.value > 0.f;
		if (pressed && pressedTime >= 0.f) {
			pressedTime += engineGetSampleTime();
			if (pressedTime >= 1.f) {
				pressedTime = -1.f;
				result = LONG_PRESS;
			}
		}

		// Check if released
		if (trigger.process(!pressed)) {
			if (pressedTime >= 0.f) {
				result = SHORT_PRESS;
			}
			pressedTime = 0.f;
		}

		return result;
	}
};

struct GroupBuilder {

	GroupBuilder() {
		for (size_t i = 0; i < NUM_CHANNELS*MAX_MODULES; i += 1) {
			groupSize[i] = 0;
		}
	}

	size_t groupSize[NUM_CHANNELS*MAX_MODULES];
	bool isPatched = false;

	bool buildGroups(std::vector<Input> *inputs, size_t first, size_t count) {
		bool changed = false;
		isPatched = false;
		size_t activeGroup = 0;

		for (int i = count-1; i >= 0; i -= 1) {
			auto patched = (*inputs)[first + i].active;

			activeGroup += 1;
			if (!patched) {
				changed = changed || groupSize[i] != 0;
				groupSize[i] = 0;
			} else if (patched) {
				isPatched = true;
				changed = changed || groupSize[i] != activeGroup;
				groupSize[i] = activeGroup;
				activeGroup = 0;
			}
		}

		return changed;
	}

	int groupForSegment(int segment) {
		int group = 0;
		int currentGroupSize = 0;

		for (int i = 0; i < NUM_CHANNELS * MAX_MODULES; i += 1) {
			if (currentGroupSize <= 0) {
				currentGroupSize = max(1, groupSize[i]);
				group = i;
			}

			if (segment == i) {
				return group;
			}

			currentGroupSize -= 1;
		}

		return segment;
	}
};

struct Stages : Module {
	enum ParamIds {
		ENUMS(SHAPE_PARAMS, NUM_CHANNELS*MAX_MODULES),
		ENUMS(TYPE_PARAMS, NUM_CHANNELS*MAX_MODULES),
		ENUMS(LEVEL_PARAMS, NUM_CHANNELS*MAX_MODULES),
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(LEVEL_INPUTS, NUM_CHANNELS*MAX_MODULES),
		ENUMS(GATE_INPUTS, NUM_CHANNELS*MAX_MODULES),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(ENVELOPE_OUTPUTS, NUM_CHANNELS*MAX_MODULES),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(TYPE_LIGHTS, NUM_CHANNELS*2*MAX_MODULES),
		ENUMS(ENVELOPE_LIGHTS, NUM_CHANNELS*MAX_MODULES),
		NUM_LIGHTS
	};

	int numModules = 1;

	stages::segment::Configuration configurations[NUM_CHANNELS*MAX_MODULES];
	bool configuration_changed[NUM_CHANNELS*MAX_MODULES];
	stages::SegmentGenerator segment_generator[NUM_CHANNELS*MAX_MODULES];
	SineOscillator oscillator[NUM_CHANNELS*MAX_MODULES];
	bool abloop;
	// stages::ChainState chain_state;
	// stages::Settings settings;

	// Buttons
	LongPressButton typeButtons[NUM_CHANNELS*MAX_MODULES];

	// Buffers
	float envelopeBuffer[NUM_CHANNELS*MAX_MODULES][BLOCK_SIZE] = {};
	stmlib::GateFlags last_gate_flags[NUM_CHANNELS*MAX_MODULES] = {};
	stmlib::GateFlags gate_flags[NUM_CHANNELS*MAX_MODULES][BLOCK_SIZE] = {};
	int blockIndex = 0;
	GroupBuilder groupBuilder;

	Stages() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		// chain_state.Init(NULL, NULL);
		onReset();
	}

	void onReset() override {
		stages::kSampleRate = engineGetSampleRate();

		abloop = false;

		for (size_t i = 0; i < NUM_CHANNELS*MAX_MODULES; ++i) {
			segment_generator[i].Init();

			configurations[i].type = stages::segment::TYPE_RAMP;
			configurations[i].loop = false;
			configuration_changed[i] = true;
		}
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "numModules", json_integer(numModules));

		json_t *configurationsJ = json_array();
		for (int i = 0; i < NUM_CHANNELS*MAX_MODULES; i++) {
			json_t *configurationJ = json_object();
			json_object_set_new(configurationJ, "type", json_integer(configurations[i].type));
			json_object_set_new(configurationJ, "loop", json_boolean(configurations[i].loop));
			json_array_insert_new(configurationsJ, i, configurationJ);
		}
		json_object_set_new(rootJ, "configurations", configurationsJ);

		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		json_t *numModulesJ = json_object_get(rootJ, "numModules");
		if (numModulesJ)
			numModules = json_integer_value(numModulesJ);

		json_t *configurationsJ = json_object_get(rootJ, "configurations");
		for (int i = 0; i < NUM_CHANNELS*MAX_MODULES; i++) {
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

	void onSampleRateChange() override {
		stages::kSampleRate = engineGetSampleRate();
		for (int i = 0; i < NUM_CHANNELS*MAX_MODULES; i++) {
			segment_generator[i].InitRamps();
		}
	}

	void stepBlock() {
		// Get parameters
		float primaries[NUM_CHANNELS*numModules];
		float secondaries[NUM_CHANNELS*numModules];
		for (int i = 0; i < NUM_CHANNELS*numModules; i++) {
			primaries[i] = clamp(params[LEVEL_PARAMS + i].value + inputs[LEVEL_INPUTS + i].value / 8.f, 0.f, 1.f);
			secondaries[i] = params[SHAPE_PARAMS + i].value;
		}

		// See if the group associations have changed since the last group
		auto groups_changed = groupBuilder.buildGroups(&inputs, GATE_INPUTS, NUM_CHANNELS * numModules);

		// Process block
		stages::SegmentGenerator::Output out[BLOCK_SIZE] = {};
		for (int i = 0; i < NUM_CHANNELS*numModules;) {
			
			// Check if the config needs applying to the segment generator for this group
			bool segment_changed = groups_changed;
			int numberOfLoops = 0;
			for (int j = 0; j < max(1, groupBuilder.groupSize[i]); j += 1) {
				numberOfLoops += configurations[i + j].loop ? 1 : 0;
				segment_changed |= configuration_changed[i + j];
				configuration_changed[i + j] = false;
			}

			if (segment_changed) {
				if (numberOfLoops > 2) {
					for (int j = 0; j < max(1, groupBuilder.groupSize[i]); j += 1) {
						configurations[i + j].loop = false;
					}
				}
				segment_generator[i].Configure(groupBuilder.groupSize[i] > 0, &configurations[i], max(1, groupBuilder.groupSize[i]));
			}

			// Set the segment parameters on the generator we're about to process
			for (int j = 0; j < segment_generator[i].num_segments(); j += 1) {
				segment_generator[i].set_segment_parameters(j, primaries[i + j], secondaries[i + j]);
			}

			segment_generator[i].Process(gate_flags[i], out, BLOCK_SIZE);

			// Set the outputs for the active segment in each output sample
			// All outputs also go to the first segment
			for (int j = 0; j < BLOCK_SIZE; j++) {
				for (int k = 1; k < segment_generator[i].num_segments(); k += 1) {
					envelopeBuffer[i + k][j] = k == out[j].segment ? 1- out[j].phase : 0.f;
				}
				envelopeBuffer[i][j] = out[j].value;
			}

			i += segment_generator[i].num_segments();
		}
	}

	void toggleMode(int i) {
		configurations[i].type = (stages::segment::Type) ((configurations[i].type + 1) % 3);
		configuration_changed[i] = true;
	}

	void toggleLoop(int i) {
		configuration_changed[i] = true;
		configurations[i].loop = !configurations[i].loop;

		// ensure that we're the only loop item in the group
		if (configurations[i].loop) {
			int group = groupBuilder.groupForSegment(i);

			// See how many loop items we have
			int loopitems = 0;

			for (size_t j = 0; j < groupBuilder.groupSize[group]; j += 1) {
				loopitems += configurations[group + j].loop ? 1 : 0;
			}

			// If we've got too many loop items, clear down to the one loop
			if ((abloop && loopitems > 2) || (!abloop && loopitems > 1)) {
				for (size_t j = 0; j < groupBuilder.groupSize[group]; j += 1) {
					configurations[group + j].loop = (group + (int)j) == i;
				}
				loopitems = 1;
			}

			// Turn abloop off if we've got 2 or more loops
			if (loopitems >= 2) {
				abloop = false;
			}
		}
	}

	void step() override {
		// Buttons
		for (int i = 0; i < NUM_CHANNELS * numModules; i++) {
			switch (typeButtons[i].step(params[TYPE_PARAMS + i])) {
				default:
				case LongPressButton::NO_PRESS: break;
				case LongPressButton::SHORT_PRESS: toggleMode(i); break;
				case LongPressButton::LONG_PRESS: toggleLoop(i); break;
			}
		}

		// Input
		for (int i = 0; i < NUM_CHANNELS * numModules; i++) {
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
		int currentGroupSize = 0;
		int loopcount = 0;
		for (int i = 0; i < NUM_CHANNELS * numModules; i++) {
			float envelope = envelopeBuffer[i][blockIndex];
			outputs[ENVELOPE_OUTPUTS + i].value = envelope * 8.f;
			lights[ENVELOPE_LIGHTS + i].setBrightnessSmooth(envelope);

			if (currentGroupSize <= 0) {
				currentGroupSize = max(1, groupBuilder.groupSize[i]);
				loopcount = 0;
			}
			currentGroupSize -= 1;

			loopcount += configurations[i].loop ? 1 : 0;
			auto flashlevel = 1.f;

			if (!configurations[i].loop) {
				oscillator[i].step(0.f); // move the oscillator on to keep the lights in sync
			} else if (configurations[i].loop && loopcount == 1) {
				flashlevel = abs(oscillator[i].step(0.f));
			} else {
				flashlevel = abs(oscillator[i].step(0.25f));
			}

			lights[TYPE_LIGHTS + i*2 + 0].setBrightness((configurations[i].type == 0 || configurations[i].type == 1) * flashlevel);
			lights[TYPE_LIGHTS + i*2 + 1].setBrightness((configurations[i].type == 1 || configurations[i].type == 2) * flashlevel);

		}
	}
};


struct StagesWidget : ModuleWidget {

	Stages* module;
	int lastNumModules = 1;

	Widget* topLeftScrew;
	Widget* topRightScrew;
	Widget* bottomLeftScrew;
	Widget* bottomRightScrew;

	StagesWidget(Stages *module) : ModuleWidget(module) {
		this->module = module;
		setPanel(SVG::load(assetPlugin(plugin, "res/Stages.svg")));

		this->box.size.x = 210 * module->numModules;
		this->panel->box.size = this->box.size;

		topLeftScrew = Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0));
		topRightScrew = Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0));
		bottomLeftScrew = Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH));
		bottomRightScrew = Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH));
		addChild(topLeftScrew);
		addChild(topRightScrew);
		addChild(bottomLeftScrew);
		addChild(bottomRightScrew);

		for(auto i = 0; i < MAX_MODULES; i += 1) {
			float offset = 71.12 * i;
			addParam(ParamWidget::create<Trimpot>(mm2px(Vec(offset + 3.72965, 13.98158)), module, Stages::SHAPE_PARAMS + 0 + (NUM_CHANNELS*i), 0.0, 1.0, 0.5));
			addParam(ParamWidget::create<Trimpot>(mm2px(Vec(offset + 15.17012, 13.98158)), module, Stages::SHAPE_PARAMS + 1 + (NUM_CHANNELS*i), 0.0, 1.0, 0.5));
			addParam(ParamWidget::create<Trimpot>(mm2px(Vec(offset + 26.6099, 13.98158)), module, Stages::SHAPE_PARAMS + 2 + (NUM_CHANNELS*i), 0.0, 1.0, 0.5));
			addParam(ParamWidget::create<Trimpot>(mm2px(Vec(offset + 38.07174, 13.98158)), module, Stages::SHAPE_PARAMS + 3 + (NUM_CHANNELS*i), 0.0, 1.0, 0.5));
			addParam(ParamWidget::create<Trimpot>(mm2px(Vec(offset + 49.51152, 13.98158)), module, Stages::SHAPE_PARAMS + 4 + (NUM_CHANNELS*i), 0.0, 1.0, 0.5));
			addParam(ParamWidget::create<Trimpot>(mm2px(Vec(offset + 60.95199, 13.98158)), module, Stages::SHAPE_PARAMS + 5 + (NUM_CHANNELS*i), 0.0, 1.0, 0.5));
			addParam(ParamWidget::create<TL1105>(mm2px(Vec(offset + 4.17259, 32.37248)), module, Stages::TYPE_PARAMS + 0 + (NUM_CHANNELS*i), 0.0, 1.0, 0.0));
			addParam(ParamWidget::create<TL1105>(mm2px(Vec(offset + 15.61237, 32.37248)), module, Stages::TYPE_PARAMS + 1 + (NUM_CHANNELS*i), 0.0, 1.0, 0.0));
			addParam(ParamWidget::create<TL1105>(mm2px(Vec(offset + 27.05284, 32.37248)), module, Stages::TYPE_PARAMS + 2 + (NUM_CHANNELS*i), 0.0, 1.0, 0.0));
			addParam(ParamWidget::create<TL1105>(mm2px(Vec(offset + 38.51399, 32.37248)), module, Stages::TYPE_PARAMS + 3 + (NUM_CHANNELS*i), 0.0, 1.0, 0.0));
			addParam(ParamWidget::create<TL1105>(mm2px(Vec(offset + 49.95446, 32.37248)), module, Stages::TYPE_PARAMS + 4 + (NUM_CHANNELS*i), 0.0, 1.0, 0.0));
			addParam(ParamWidget::create<TL1105>(mm2px(Vec(offset + 61.39424, 32.37248)), module, Stages::TYPE_PARAMS + 5 + (NUM_CHANNELS*i), 0.0, 1.0, 0.0));
			addParam(ParamWidget::create<LEDSliderGreen>(mm2px(Vec(offset + 3.36193, 43.06508)), module, Stages::LEVEL_PARAMS + 0 + (NUM_CHANNELS*i), 0.0, 1.0, 1.0));
			addParam(ParamWidget::create<LEDSliderGreen>(mm2px(Vec(offset + 14.81619, 43.06508)), module, Stages::LEVEL_PARAMS + 1 + (NUM_CHANNELS*i), 0.0, 1.0, 1.0));
			addParam(ParamWidget::create<LEDSliderGreen>(mm2px(Vec(offset + 26.26975, 43.06508)), module, Stages::LEVEL_PARAMS + 2 + (NUM_CHANNELS*i), 0.0, 1.0, 1.0));
			addParam(ParamWidget::create<LEDSliderGreen>(mm2px(Vec(offset + 37.70265, 43.06508)), module, Stages::LEVEL_PARAMS + 3 + (NUM_CHANNELS*i), 0.0, 1.0, 1.0));
			addParam(ParamWidget::create<LEDSliderGreen>(mm2px(Vec(offset + 49.15759, 43.06508)), module, Stages::LEVEL_PARAMS + 4 + (NUM_CHANNELS*i), 0.0, 1.0, 1.0));
			addParam(ParamWidget::create<LEDSliderGreen>(mm2px(Vec(offset + 60.61184, 43.06508)), module, Stages::LEVEL_PARAMS + 5 + (NUM_CHANNELS*i), 0.0, 1.0, 1.0));

			addInput(Port::create<PJ301MPort>(mm2px(Vec(offset + 2.70756, 77.75277)), Port::INPUT, module, Stages::LEVEL_INPUTS + 0 + (NUM_CHANNELS*i)));
			addInput(Port::create<PJ301MPort>(mm2px(Vec(offset + 14.14734, 77.75277)), Port::INPUT, module, Stages::LEVEL_INPUTS + 1 + (NUM_CHANNELS*i)));
			addInput(Port::create<PJ301MPort>(mm2px(Vec(offset + 25.58781, 77.75277)), Port::INPUT, module, Stages::LEVEL_INPUTS + 2 + (NUM_CHANNELS*i)));
			addInput(Port::create<PJ301MPort>(mm2px(Vec(offset + 37.04896, 77.75277)), Port::INPUT, module, Stages::LEVEL_INPUTS + 3 + (NUM_CHANNELS*i)));
			addInput(Port::create<PJ301MPort>(mm2px(Vec(offset + 48.48943, 77.75277)), Port::INPUT, module, Stages::LEVEL_INPUTS + 4 + (NUM_CHANNELS*i)));
			addInput(Port::create<PJ301MPort>(mm2px(Vec(offset + 59.92921, 77.75277)), Port::INPUT, module, Stages::LEVEL_INPUTS + 5 + (NUM_CHANNELS*i)));
			
			addInput(Port::create<PJ301MPort>(mm2px(Vec(offset + 2.70756, 92.35239)), Port::INPUT, module, Stages::GATE_INPUTS + 0 + (NUM_CHANNELS*i)));
			addInput(Port::create<PJ301MPort>(mm2px(Vec(offset + 14.14734, 92.35239)), Port::INPUT, module, Stages::GATE_INPUTS + 1 + (NUM_CHANNELS*i)));
			addInput(Port::create<PJ301MPort>(mm2px(Vec(offset + 25.58781, 92.35239)), Port::INPUT, module, Stages::GATE_INPUTS + 2 + (NUM_CHANNELS*i)));
			addInput(Port::create<PJ301MPort>(mm2px(Vec(offset + 37.04896, 92.35239)), Port::INPUT, module, Stages::GATE_INPUTS + 3 + (NUM_CHANNELS*i)));
			addInput(Port::create<PJ301MPort>(mm2px(Vec(offset + 48.48943, 92.35239)), Port::INPUT, module, Stages::GATE_INPUTS + 4 + (NUM_CHANNELS*i)));
			addInput(Port::create<PJ301MPort>(mm2px(Vec(offset + 59.92921, 92.35239)), Port::INPUT, module, Stages::GATE_INPUTS + 5 + (NUM_CHANNELS*i)));

			addOutput(Port::create<PJ301MPort>(mm2px(Vec(offset + 2.70756, 106.95203)), Port::OUTPUT, module, Stages::ENVELOPE_OUTPUTS + 0 + (NUM_CHANNELS*i)));
			addOutput(Port::create<PJ301MPort>(mm2px(Vec(offset + 14.14734, 106.95203)), Port::OUTPUT, module, Stages::ENVELOPE_OUTPUTS + 1 + (NUM_CHANNELS*i)));
			addOutput(Port::create<PJ301MPort>(mm2px(Vec(offset + 25.58781, 106.95203)), Port::OUTPUT, module, Stages::ENVELOPE_OUTPUTS + 2 + (NUM_CHANNELS*i)));
			addOutput(Port::create<PJ301MPort>(mm2px(Vec(offset + 37.04896, 106.95203)), Port::OUTPUT, module, Stages::ENVELOPE_OUTPUTS + 3 + (NUM_CHANNELS*i)));
			addOutput(Port::create<PJ301MPort>(mm2px(Vec(offset + 48.48943, 106.95203)), Port::OUTPUT, module, Stages::ENVELOPE_OUTPUTS + 4 + (NUM_CHANNELS*i)));
			addOutput(Port::create<PJ301MPort>(mm2px(Vec(offset + 59.92921, 106.95203)), Port::OUTPUT, module, Stages::ENVELOPE_OUTPUTS + 5 + (NUM_CHANNELS*i)));

			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(mm2px(Vec(offset + 5.27737, 26.74447)), module, Stages::TYPE_LIGHTS + (0*2) + (NUM_CHANNELS*2*i)));
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(mm2px(Vec(offset + 16.73784, 26.74447)), module, Stages::TYPE_LIGHTS + (1*2) + (NUM_CHANNELS*2*i)));
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(mm2px(Vec(offset + 28.1783, 26.74447)), module, Stages::TYPE_LIGHTS + (2*2) + (NUM_CHANNELS*2*i)));
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(mm2px(Vec(offset + 39.61877, 26.74447)), module, Stages::TYPE_LIGHTS + (3*2) + (NUM_CHANNELS*2*i)));
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(mm2px(Vec(offset + 51.07923, 26.74447)), module, Stages::TYPE_LIGHTS + (4*2) + (NUM_CHANNELS*2*i)));
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(mm2px(Vec(offset + 62.51971, 26.74447)), module, Stages::TYPE_LIGHTS + (5*2) + (NUM_CHANNELS*2*i)));
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(mm2px(Vec(offset + 2.29462, 103.19253)), module, Stages::ENVELOPE_LIGHTS + 0 + (NUM_CHANNELS*i)));
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(mm2px(Vec(offset + 13.73509, 103.19253)), module, Stages::ENVELOPE_LIGHTS + 1 + (NUM_CHANNELS*i)));
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(mm2px(Vec(offset + 25.17556, 103.19253)), module, Stages::ENVELOPE_LIGHTS + 2 + (NUM_CHANNELS*i)));
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(mm2px(Vec(offset + 36.63671, 103.19253)), module, Stages::ENVELOPE_LIGHTS + 3 + (NUM_CHANNELS*i)));
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(mm2px(Vec(offset + 48.07649, 103.19253)), module, Stages::ENVELOPE_LIGHTS + 4 + (NUM_CHANNELS*i)));
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(mm2px(Vec(offset + 59.51696, 103.19253)), module, Stages::ENVELOPE_LIGHTS + 5 + (NUM_CHANNELS*i)));
		}
	}

	void step() override {
		if (module->numModules != lastNumModules) {
			resize(module->numModules);

			// Remove wires which no longer have a port
			for (int i = 0; i < inputs.size(); i++) {
				if (inputs[i]->portId >= Stages::LEVEL_INPUTS + (module->numModules * NUM_CHANNELS) && inputs[i]->portId < Stages::LEVEL_INPUTS + (NUM_CHANNELS * MAX_MODULES)) {
					gRackWidget->wireContainer->removeAllWires(inputs[i]);
				}
				if (inputs[i]->portId >= Stages::GATE_INPUTS + (module->numModules * NUM_CHANNELS) && inputs[i]->portId < Stages::GATE_INPUTS + (NUM_CHANNELS * MAX_MODULES)) {
					gRackWidget->wireContainer->removeAllWires(inputs[i]);
				}
			}
			for (int i = 0; i < outputs.size(); i++) {
				if (outputs[i]->portId >= Stages::ENVELOPE_OUTPUTS + (module->numModules * NUM_CHANNELS) && outputs[i]->portId < Stages::ENVELOPE_OUTPUTS + (NUM_CHANNELS * MAX_MODULES)) {
					gRackWidget->wireContainer->removeAllWires(outputs[i]);
				}
			}
		}

		ModuleWidget::step();
	}

	void resize(int width) {
		lastNumModules = width;
		this->box.size.x = width * 210;
		this->panel->box.size = this->box.size;

		topRightScrew->box.pos.x = box.size.x - 2 * RACK_GRID_WIDTH;
		bottomRightScrew->box.pos.x = box.size.x - 2 * RACK_GRID_WIDTH;
	}

	void appendContextMenu(Menu *menu) override {
		Stages *module = dynamic_cast<Stages*>(this->module);

		struct ABLoopItem : MenuItem {
			Stages *module;
			void onAction(EventAction &e) override {
				module->abloop = true;
			}
		};

		menu->addChild(MenuEntry::create());
		ABLoopItem *abloopItem = MenuItem::create<ABLoopItem>("Set A/B Loop", CHECKMARK(module->abloop));
		abloopItem->module = module;
		menu->addChild(abloopItem);

		struct ExpandoItem : MenuItem {
			Stages *module;
			int width = 1;
			void onAction(EventAction &e) override {
				module->numModules = width;
			}
		};

		struct ModuleLinkingItem : MenuItem {
			Stages* module;

			Menu *createChildMenu() override {
				Menu *menu = new Menu();
				for (auto i = 1; i <= MAX_MODULES; i += 1) {
					ExpandoItem *expandoItem = MenuItem::create<ExpandoItem>(stringf("%dx", i), CHECKMARK(module->numModules == i));
					expandoItem->module = module;
					expandoItem->width = i;
					menu->addChild(expandoItem);
				}
				return menu;
			}
		};

		menu->addChild(MenuEntry::create());
		ModuleLinkingItem *moduleLinkingItem = MenuItem::create<ModuleLinkingItem>("Linked Modules");
		moduleLinkingItem->module = module;
		menu->addChild(moduleLinkingItem);

	}



};


Model *modelStages = Model::create<Stages, StagesWidget>("Audible Instruments", "Stages", "Segment Generator", FUNCTION_GENERATOR_TAG, ENVELOPE_GENERATOR_TAG);
