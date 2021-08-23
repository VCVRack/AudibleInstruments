// Mutable Instruments Streams emulation for VCV Rack
// Copyright (C) 2020 Tyler Coy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <string>
#include <algorithm>
#include "plugin.hpp"
#include "Streams/streams.hpp"

namespace streams {

struct StreamsChannelMode {
	ProcessorFunction function;
	bool alternate;
	std::string label;
};

static constexpr int kNumChannelModes = 10;

static const StreamsChannelMode kChannelModeTable[kNumChannelModes] = {
	{PROCESSOR_FUNCTION_ENVELOPE,          false, "Envelope"},
	{PROCESSOR_FUNCTION_VACTROL,           false, "Vactrol"},
	{PROCESSOR_FUNCTION_FOLLOWER,          false, "Follower"},
	{PROCESSOR_FUNCTION_COMPRESSOR,        false, "Compressor"},
	{PROCESSOR_FUNCTION_ENVELOPE,          true,  "AR envelope"},
	{PROCESSOR_FUNCTION_VACTROL,           true,  "Plucked vactrol"},
	{PROCESSOR_FUNCTION_FOLLOWER,          true,  "Cutoff controller"},
	{PROCESSOR_FUNCTION_COMPRESSOR,        true,  "Slow compressor"},
	{PROCESSOR_FUNCTION_FILTER_CONTROLLER, true,  "Direct VCF controller"},
	{PROCESSOR_FUNCTION_LORENZ_GENERATOR,  false, "Lorenz generator"},
};

struct StreamsMonitorMode {
	MonitorMode mode;
	std::string label;
};

static constexpr int kNumMonitorModes = 4;

static const StreamsMonitorMode kMonitorModeTable[kNumMonitorModes] = {
	{MONITOR_MODE_EXCITE_IN, "Excite"},
	{MONITOR_MODE_VCA_CV,    "Level"},
	{MONITOR_MODE_AUDIO_IN,  "In"},
	{MONITOR_MODE_OUTPUT,    "Out"},
};

}

struct Streams : Module {
	enum ParamIds {
		CH1_SHAPE_PARAM,
		CH1_MOD_PARAM,
		CH1_LEVEL_MOD_PARAM,
		CH1_RESPONSE_PARAM,
		CH2_SHAPE_PARAM,
		CH2_MOD_PARAM,
		CH2_LEVEL_MOD_PARAM,
		CH2_RESPONSE_PARAM,
		CH1_FUNCTION_BUTTON_PARAM,
		CH2_FUNCTION_BUTTON_PARAM,
		METERING_BUTTON_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CH1_EXCITE_INPUT,
		CH1_SIGNAL_INPUT,
		CH1_LEVEL_INPUT,
		CH2_EXCITE_INPUT,
		CH2_SIGNAL_INPUT,
		CH2_LEVEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CH1_SIGNAL_OUTPUT,
		CH2_SIGNAL_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CH1_LIGHT_1_G,
		CH1_LIGHT_1_R,
		CH1_LIGHT_2_G,
		CH1_LIGHT_2_R,
		CH1_LIGHT_3_G,
		CH1_LIGHT_3_R,
		CH1_LIGHT_4_G,
		CH1_LIGHT_4_R,
		CH2_LIGHT_1_G,
		CH2_LIGHT_1_R,
		CH2_LIGHT_2_G,
		CH2_LIGHT_2_R,
		CH2_LIGHT_3_G,
		CH2_LIGHT_3_R,
		CH2_LIGHT_4_G,
		CH2_LIGHT_4_R,
		NUM_LIGHTS
	};

	streams::StreamsEngine engines[PORT_MAX_CHANNELS];
	int prevNumChannels;
	float brightnesses[NUM_LIGHTS][PORT_MAX_CHANNELS];

	Streams() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(CH1_SHAPE_PARAM,     0.f, 1.f, 0.5f, "Channel 1 shape", "%", 0, 100);
		configParam(CH1_MOD_PARAM,       0.f, 1.f, 0.5f, "Channel 1 mod", "%", 0, 100);
		configParam(CH1_LEVEL_MOD_PARAM, 0.f, 1.f, 0.5f, "Channel 1 level mod", "%", 0, 100);
		configParam(CH2_SHAPE_PARAM,     0.f, 1.f, 0.5f, "Channel 2 shape", "%", 0, 100);
		configParam(CH2_MOD_PARAM,       0.f, 1.f, 0.5f, "Channel 2 mod", "%", 0, 100);
		configParam(CH2_LEVEL_MOD_PARAM, 0.f, 1.f, 0.5f, "Channel 2 level mod", "%", 0, 100);
		configParam(CH1_RESPONSE_PARAM, 0.f, 1.f, 0.5f, "Channel 1 response", "%", 0, 100);
		configParam(CH2_RESPONSE_PARAM, 0.f, 1.f, 0.5f, "Channel 2 response", "%", 0, 100);

		configButton(CH1_FUNCTION_BUTTON_PARAM, "Channel 1 function");
		configButton(CH2_FUNCTION_BUTTON_PARAM, "Channel 2 function");
		configButton(METERING_BUTTON_PARAM, "Meter");

		configInput(CH1_EXCITE_INPUT, "Channel 1 excite");
		configInput(CH1_SIGNAL_INPUT, "Channel 1");
		configInput(CH1_LEVEL_INPUT, "Channel 1 level");
		configInput(CH2_EXCITE_INPUT, "Channel 2 excite");
		configInput(CH2_SIGNAL_INPUT, "Channel 2");
		configInput(CH2_LEVEL_INPUT, "Channel 2 level");

		configOutput(CH1_SIGNAL_OUTPUT, "Channel 1");
		configOutput(CH2_SIGNAL_OUTPUT, "Channel 2");

		configBypass(CH1_SIGNAL_INPUT, CH1_SIGNAL_OUTPUT);
		configBypass(CH2_SIGNAL_INPUT, CH2_SIGNAL_OUTPUT);

		onReset();
	}

	void onReset() override {
		for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
			engines[c].Reset();
		}

		prevNumChannels = 1;
		onSampleRateChange();
	}

	void onSampleRateChange() override {
		float sampleRate = APP->engine->getSampleRate();

		for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
			engines[c].SetSampleRate(sampleRate);
		}
	}

	json_t* dataToJson() override {
		streams::UiSettings settings = engines[0].ui_settings();
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "function1",    json_integer(settings.function[0]));
		json_object_set_new(rootJ, "function2",    json_integer(settings.function[1]));
		json_object_set_new(rootJ, "alternate1",   json_integer(settings.alternate[0]));
		json_object_set_new(rootJ, "alternate2",   json_integer(settings.alternate[1]));
		json_object_set_new(rootJ, "monitorMode", json_integer(settings.monitor_mode));
		json_object_set_new(rootJ, "linked",       json_integer(settings.linked));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* function1J    = json_object_get(rootJ, "function1");
		json_t* function2J    = json_object_get(rootJ, "function2");
		json_t* alternate1J   = json_object_get(rootJ, "alternate1");
		json_t* alternate2J   = json_object_get(rootJ, "alternate2");
		json_t* monitorModeJ = json_object_get(rootJ, "monitorMode");
		json_t* linkedJ       = json_object_get(rootJ, "linked");

		streams::UiSettings settings = {};

		if (function1J)
			settings.function[0]  = json_integer_value(function1J);
		if (function2J)
			settings.function[1]  = json_integer_value(function2J);
		if (alternate1J)
			settings.alternate[0] = json_integer_value(alternate1J);
		if (alternate2J)
			settings.alternate[1] = json_integer_value(alternate2J);
		if (monitorModeJ)
			settings.monitor_mode = json_integer_value(monitorModeJ);
		if (linkedJ)
			settings.linked       = json_integer_value(linkedJ);

		for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
			engines[c].ApplySettings(settings);
		}
	}

	void onRandomize() override {
		for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
			engines[c].Randomize();
		}
	}

	void setLinked(bool linked) {
		streams::UiSettings settings = engines[0].ui_settings();
		settings.linked = linked;

		for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
			engines[c].ApplySettings(settings);
		}
	}

	int getChannelMode(int channel) {
		streams::UiSettings settings = engines[0].ui_settings();
		// Search channel mode index in table
		for (int i = 0; i < streams::kNumChannelModes; i++) {
			if (settings.function[channel] == streams::kChannelModeTable[i].function
				&& settings.alternate[channel] == streams::kChannelModeTable[i].alternate)
				return i;
		}
		return -1;
	}

	void setChannelMode(int channel, int mode_id) {
		streams::UiSettings settings = engines[0].ui_settings();
		settings.function[channel] = streams::kChannelModeTable[mode_id].function;
		settings.alternate[channel] = streams::kChannelModeTable[mode_id].alternate;

		for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
			engines[c].ApplySettings(settings);
		}
	}

	void setMonitorMode(int mode_id) {
		streams::UiSettings settings = engines[0].ui_settings();
		settings.monitor_mode = streams::kMonitorModeTable[mode_id].mode;

		for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
			engines[c].ApplySettings(settings);
		}
	}

	int function(int channel) {
		return engines[0].ui_settings().function[channel];
	}

	int alternate(int channel) {
		return engines[0].ui_settings().alternate[channel];
	}

	bool linked() {
		return engines[0].ui_settings().linked;
	}

	int monitorMode() {
		return engines[0].ui_settings().monitor_mode;
	}

	void process(const ProcessArgs& args) override {
		int numChannels = std::max(inputs[CH1_SIGNAL_INPUT].getChannels(), inputs[CH2_SIGNAL_INPUT].getChannels());
		numChannels = std::max(numChannels, 1);

		if (numChannels > prevNumChannels) {
			for (int c = prevNumChannels; c < numChannels; c++) {
				engines[c].SyncUI(engines[0]);
			}
		}

		prevNumChannels = numChannels;

		// Reuse the same frame object for multiple engines because the params
		// aren't touched.
		streams::StreamsEngine::Frame frame;

		frame.ch1.shape_knob          = params[CH1_SHAPE_PARAM]    .getValue();
		frame.ch1.mod_knob            = params[CH1_MOD_PARAM]      .getValue();
		frame.ch1.level_mod_knob      = params[CH1_LEVEL_MOD_PARAM].getValue();
		frame.ch1.response_knob       = params[CH1_RESPONSE_PARAM] .getValue();
		frame.ch2.shape_knob          = params[CH2_SHAPE_PARAM]    .getValue();
		frame.ch2.mod_knob            = params[CH2_MOD_PARAM]      .getValue();
		frame.ch2.level_mod_knob      = params[CH2_LEVEL_MOD_PARAM].getValue();
		frame.ch2.response_knob       = params[CH2_RESPONSE_PARAM] .getValue();

		frame.ch1.signal_in_connected = inputs[CH1_SIGNAL_INPUT].isConnected();
		frame.ch1.level_cv_connected  = inputs[CH1_LEVEL_INPUT] .isConnected();
		frame.ch2.signal_in_connected = inputs[CH2_SIGNAL_INPUT].isConnected();
		frame.ch2.level_cv_connected  = inputs[CH2_LEVEL_INPUT] .isConnected();

		frame.ch1.function_button     = params[CH1_FUNCTION_BUTTON_PARAM].getValue();
		frame.ch2.function_button     = params[CH2_FUNCTION_BUTTON_PARAM].getValue();
		frame.metering_button         = params[METERING_BUTTON_PARAM].getValue();

		bool lights_updated = false;

		for (int c = 0; c < numChannels; c++) {
			frame.ch1.excite_in = inputs[CH1_EXCITE_INPUT].getPolyVoltage(c);
			frame.ch1.signal_in = inputs[CH1_SIGNAL_INPUT].getPolyVoltage(c);
			frame.ch1.level_cv  = inputs[CH1_LEVEL_INPUT] .getPolyVoltage(c);
			frame.ch2.excite_in = inputs[CH2_EXCITE_INPUT].getPolyVoltage(c);
			frame.ch2.signal_in = inputs[CH2_SIGNAL_INPUT].getPolyVoltage(c);
			frame.ch2.level_cv  = inputs[CH2_LEVEL_INPUT] .getPolyVoltage(c);

			engines[c].Process(frame);

			outputs[CH1_SIGNAL_OUTPUT].setVoltage(frame.ch1.signal_out, c);
			outputs[CH2_SIGNAL_OUTPUT].setVoltage(frame.ch2.signal_out, c);

			if (frame.lights_updated) {
				brightnesses[CH1_LIGHT_1_G][c] = frame.ch1.led_green[0];
				brightnesses[CH1_LIGHT_2_G][c] = frame.ch1.led_green[1];
				brightnesses[CH1_LIGHT_3_G][c] = frame.ch1.led_green[2];
				brightnesses[CH1_LIGHT_4_G][c] = frame.ch1.led_green[3];
				brightnesses[CH1_LIGHT_1_R][c] = frame.ch1.led_red[0];
				brightnesses[CH1_LIGHT_2_R][c] = frame.ch1.led_red[1];
				brightnesses[CH1_LIGHT_3_R][c] = frame.ch1.led_red[2];
				brightnesses[CH1_LIGHT_4_R][c] = frame.ch1.led_red[3];
				brightnesses[CH2_LIGHT_1_G][c] = frame.ch2.led_green[0];
				brightnesses[CH2_LIGHT_2_G][c] = frame.ch2.led_green[1];
				brightnesses[CH2_LIGHT_3_G][c] = frame.ch2.led_green[2];
				brightnesses[CH2_LIGHT_4_G][c] = frame.ch2.led_green[3];
				brightnesses[CH2_LIGHT_1_R][c] = frame.ch2.led_red[0];
				brightnesses[CH2_LIGHT_2_R][c] = frame.ch2.led_red[1];
				brightnesses[CH2_LIGHT_3_R][c] = frame.ch2.led_red[2];
				brightnesses[CH2_LIGHT_4_R][c] = frame.ch2.led_red[3];
			}

			lights_updated |= frame.lights_updated;
		}

		outputs[CH1_SIGNAL_OUTPUT].setChannels(numChannels);
		outputs[CH2_SIGNAL_OUTPUT].setChannels(numChannels);

		if (lights_updated) {
			// Drive lights according to maximum brightness across engines
			for (int i = 0; i < NUM_LIGHTS; i++) {
				float brightness = 0.f;

				for (int c = 0; c < numChannels; c++) {
					brightness = std::max(brightnesses[i][c], brightness);
				}

				lights[i].setBrightness(brightness);
			}
		}
	}
};


struct StreamsWidget : ModuleWidget {
	StreamsWidget(Streams* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Streams.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(11.065, 128.75 - 107.695)), module, Streams::CH1_SHAPE_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(11.065, 128.75 -  84.196)), module, Streams::CH1_MOD_PARAM));
		addParam(createParamCentered<Rogan1PSRed> (mm2px(Vec(11.065, 128.75 -  60.706)), module, Streams::CH1_LEVEL_MOD_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(49.785, 128.75 - 107.695)), module, Streams::CH2_SHAPE_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(49.785, 128.75 -  84.196)), module, Streams::CH2_MOD_PARAM));
		addParam(createParamCentered<Rogan1PSGreen>(mm2px(Vec(49.785, 128.75 -  60.706)), module, Streams::CH2_LEVEL_MOD_PARAM));

		addParam(createParamCentered<Trimpot>(mm2px(Vec(30.425, 128.75 -  68.006)), module, Streams::CH1_RESPONSE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(30.425, 128.75 -  53.406)), module, Streams::CH2_RESPONSE_PARAM));

		addParam(createParamCentered<TL1105>(mm2px(Vec(24.715, 128.75 - 113.726)), module, Streams::CH1_FUNCTION_BUTTON_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(36.135, 128.75 - 113.726)), module, Streams::CH2_FUNCTION_BUTTON_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(30.425, 128.75 -  81.976)), module, Streams::METERING_BUTTON_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.506, 128.75 - 32.136)), module, Streams::CH1_EXCITE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(23.116, 128.75 - 32.136)), module, Streams::CH1_SIGNAL_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.506, 128.75 - 17.526)), module, Streams::CH1_LEVEL_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(52.335, 128.75 - 32.136)), module, Streams::CH2_EXCITE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(37.726, 128.75 - 32.136)), module, Streams::CH2_SIGNAL_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(52.335, 128.75 - 17.526)), module, Streams::CH2_LEVEL_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(23.116, 128.75 - 17.526)), module, Streams::CH1_SIGNAL_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(37.726, 128.75 - 17.526)), module, Streams::CH2_SIGNAL_OUTPUT));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(24.715, 128.75 - 106.746)), module, Streams::CH1_LIGHT_1_G));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(24.715, 128.75 - 101.026)), module, Streams::CH1_LIGHT_2_G));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(24.715, 128.75 -  95.305)), module, Streams::CH1_LIGHT_3_G));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(24.715, 128.75 -  89.585)), module, Streams::CH1_LIGHT_4_G));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(36.135, 128.75 - 106.746)), module, Streams::CH2_LIGHT_1_G));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(36.135, 128.75 - 101.026)), module, Streams::CH2_LIGHT_2_G));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(36.135, 128.75 -  95.305)), module, Streams::CH2_LIGHT_3_G));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(36.135, 128.75 -  89.585)), module, Streams::CH2_LIGHT_4_G));
	}

	void appendContextMenu(Menu* menu) override {
		Streams* module = dynamic_cast<Streams*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolMenuItem("Link channels",
			[=]() {return module->linked();},
			[=](bool val) {module->setLinked(val);}
		));

		std::vector<std::string> modeLabels;
		for (int i = 0; i < streams::kNumChannelModes; i++) {
			modeLabels.push_back(streams::kChannelModeTable[i].label);
		}
		for (int c = 0; c < 2; c++) {
			menu->addChild(createIndexSubmenuItem(string::f("Channel %d mode", c + 1), modeLabels,
				[=]() {return module->getChannelMode(c);},
				[=](int index) {module->setChannelMode(c, index);}
			));
		}

		std::vector<std::string> meterLabels;
		for (int i = 0; i < streams::kNumMonitorModes; i++) {
			meterLabels.push_back(streams::kMonitorModeTable[i].label);
		}
		menu->addChild(createIndexSubmenuItem("Meter", meterLabels,
			[=]() {return module->monitorMode();},
			[=](int index) {module->setMonitorMode(index);}
		));
	}
};

Model* modelStreams = createModel<Streams, StreamsWidget>("Streams");
