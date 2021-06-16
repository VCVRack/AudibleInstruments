#include "plugin.hpp"
#include "frames/keyframer.h"
#include "frames/poly_lfo.h"


struct Frames : Module {
	enum ParamIds {
		GAIN1_PARAM,
		GAIN2_PARAM,
		GAIN3_PARAM,
		GAIN4_PARAM,
		ADD_PARAM,
		DEL_PARAM,
		FRAME_PARAM,
		MODULATION_PARAM,
		OFFSET_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ALL_INPUT,
		IN1_INPUT,
		IN2_INPUT,
		IN3_INPUT,
		IN4_INPUT,
		FRAME_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MIX_OUTPUT,
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		OUT3_OUTPUT,
		OUT4_OUTPUT,
		FRAME_STEP_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		GAIN1_LIGHT,
		EDIT_LIGHT = GAIN1_LIGHT + 4,
		FRAME_LIGHT,
		NUM_LIGHTS = FRAME_LIGHT + 3
	};

	frames::Keyframer keyframer;
	frames::PolyLfo poly_lfo;
	bool poly_lfo_mode = false;
	uint16_t lastControls[4] = {};

	dsp::SchmittTrigger addTrigger;
	dsp::SchmittTrigger delTrigger;

	Frames() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(GAIN1_PARAM, 0.0, 1.0, 0.0, "Channel 1 gain");
		configParam(GAIN2_PARAM, 0.0, 1.0, 0.0, "Channel 2 gain");
		configParam(GAIN3_PARAM, 0.0, 1.0, 0.0, "Channel 3 gain");
		configParam(GAIN4_PARAM, 0.0, 1.0, 0.0, "Channel 4 gain");
		configParam(FRAME_PARAM, 0.0, 1.0, 0.0, "Frame");
		configParam(MODULATION_PARAM, -1.0, 1.0, 0.0, "Animation attenuverter");
		configButton(ADD_PARAM, "Add keyframe");
		configButton(DEL_PARAM, "Delete keyframe");
		configSwitch(OFFSET_PARAM, 0.0, 1.0, 0.0, "Offset", {"+0V", "+10V"});

		configInput(ALL_INPUT, "All");
		configInput(IN1_INPUT, "Channel 1");
		configInput(IN2_INPUT, "Channel 2");
		configInput(IN3_INPUT, "Channel 3");
		configInput(IN4_INPUT, "Channel 4");
		configInput(FRAME_INPUT, "Frame");

		configOutput(MIX_OUTPUT, "Mix");
		configOutput(OUT1_OUTPUT, "Channel 1");
		configOutput(OUT2_OUTPUT, "Channel 2");
		configOutput(OUT3_OUTPUT, "Channel 3");
		configOutput(OUT4_OUTPUT, "Channel 4");
		configOutput(FRAME_STEP_OUTPUT, "Frame step");

		memset(&keyframer, 0, sizeof(keyframer));
		keyframer.Init();
		memset(&poly_lfo, 0, sizeof(poly_lfo));
		poly_lfo.Init();

		onReset();
	}

	void process(const ProcessArgs& args) override {
		// Set gain and timestamp knobs
		uint16_t controls[4];
		for (int i = 0; i < 4; i++) {
			controls[i] = params[GAIN1_PARAM + i].getValue() * 65535.0;
		}

		int32_t timestamp = params[FRAME_PARAM].getValue() * 65535.0;
		int32_t timestampMod = timestamp + params[MODULATION_PARAM].getValue() * inputs[FRAME_INPUT].getVoltage() / 10.0 * 65535.0;
		timestamp = clamp(timestamp, 0, 65535);
		timestampMod = clamp(timestampMod, 0, 65535);
		int16_t nearestIndex = -1;
		if (!poly_lfo_mode) {
			nearestIndex = keyframer.FindNearestKeyframe(timestamp, 2048);
		}

		// Render, handle buttons
		if (poly_lfo_mode) {
			if (controls[0] != lastControls[0])
				poly_lfo.set_shape(controls[0]);
			if (controls[1] != lastControls[1])
				poly_lfo.set_shape_spread(controls[1]);
			if (controls[2] != lastControls[2])
				poly_lfo.set_spread(controls[2]);
			if (controls[3] != lastControls[3])
				poly_lfo.set_coupling(controls[3]);
			poly_lfo.Render(timestampMod);
		}
		else {
			for (int i = 0; i < 4; i++) {
				if (controls[i] != lastControls[i]) {
					// Update recently moved control
					if (keyframer.num_keyframes() == 0) {
						keyframer.set_immediate(i, controls[i]);
					}
					if (nearestIndex >= 0) {
						frames::Keyframe* nearestKeyframe = keyframer.mutable_keyframe(nearestIndex);
						nearestKeyframe->values[i] = controls[i];
					}
				}
			}

			if (addTrigger.process(params[ADD_PARAM].getValue())) {
				if (nearestIndex < 0) {
					keyframer.AddKeyframe(timestamp, controls);
				}
			}
			if (delTrigger.process(params[DEL_PARAM].getValue())) {
				if (nearestIndex >= 0) {
					int32_t nearestTimestamp = keyframer.keyframe(nearestIndex).timestamp;
					keyframer.RemoveKeyframe(nearestTimestamp);
				}
			}
			keyframer.Evaluate(timestampMod);
		}

		// Get gains
		float gains[4];
		for (int i = 0; i < 4; i++) {
			if (poly_lfo_mode) {
				// gains[i] = poly_lfo.level(i) / 255.0;
				gains[i] = poly_lfo.level16(i) / 65535.0;
			}
			else {
				float lin = keyframer.level(i) / 65535.0;
				gains[i] = lin;
			}
			// Simulate SSM2164
			if (keyframer.mutable_settings(i)->response > 0) {
				const float expBase = 200.0;
				float expGain = rescale(powf(expBase, gains[i]), 1.0f, expBase, 0.0f, 1.0f);
				gains[i] = crossfade(gains[i], expGain, keyframer.mutable_settings(i)->response / 255.0f);
			}
		}

		// Update last controls
		for (int i = 0; i < 4; i++) {
			lastControls[i] = controls[i];
		}

		// Get inputs
		float all = ((int)params[OFFSET_PARAM].getValue() == 1) ? 10.0 : 0.0;
		if (inputs[ALL_INPUT].isConnected()) {
			all = inputs[ALL_INPUT].getVoltage();
		}

		float ins[4];
		for (int i = 0; i < 4; i++) {
			ins[i] = inputs[IN1_INPUT + i].getNormalVoltage(all) * gains[i];
		}

		// Set outputs
		float mix = 0.0;

		for (int i = 0; i < 4; i++) {
			if (outputs[OUT1_OUTPUT + i].isConnected()) {
				outputs[OUT1_OUTPUT + i].setVoltage(ins[i]);
			}
			else {
				mix += ins[i];
			}
		}

		outputs[MIX_OUTPUT].setVoltage(clamp(mix / 2.0, -10.0f, 10.0f));

		// Set lights
		for (int i = 0; i < 4; i++) {
			lights[GAIN1_LIGHT + i].setBrightness(gains[i]);
		}

		if (poly_lfo_mode) {
			lights[EDIT_LIGHT].value = (poly_lfo.level(0) > 128 ? 1.0 : 0.0);
		}
		else {
			lights[EDIT_LIGHT].value = (nearestIndex >= 0 ? 1.0 : 0.0);
		}

		// Set frame light colors
		const uint8_t* colors;
		if (poly_lfo_mode) {
			colors = poly_lfo.color();
		}
		else {
			colors = keyframer.color();
		}
		for (int i = 0; i < 3; i++) {
			float c = colors[i] / 255.f;
			// c = 1.f - (1.f - c) * 1.25f;
			lights[FRAME_LIGHT + i].setBrightness(c);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "polyLfo", json_boolean(poly_lfo_mode));

		json_t* keyframesJ = json_array();
		for (int i = 0; i < keyframer.num_keyframes(); i++) {
			json_t* keyframeJ = json_array();
			frames::Keyframe* keyframe = keyframer.mutable_keyframe(i);
			json_array_append_new(keyframeJ, json_integer(keyframe->timestamp));
			for (int k = 0; k < 4; k++) {
				json_array_append_new(keyframeJ, json_integer(keyframe->values[k]));
			}
			json_array_append_new(keyframesJ, keyframeJ);
		}
		json_object_set_new(rootJ, "keyframes", keyframesJ);

		json_t* channelsJ = json_array();
		for (int i = 0; i < 4; i++) {
			json_t* channelJ = json_object();
			json_object_set_new(channelJ, "curve", json_integer((int) keyframer.mutable_settings(i)->easing_curve));
			json_object_set_new(channelJ, "response", json_integer(keyframer.mutable_settings(i)->response));
			json_array_append_new(channelsJ, channelJ);
		}
		json_object_set_new(rootJ, "channels", channelsJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* polyLfoJ = json_object_get(rootJ, "polyLfo");
		if (polyLfoJ)
			poly_lfo_mode = json_boolean_value(polyLfoJ);

		json_t* keyframesJ = json_object_get(rootJ, "keyframes");
		if (keyframesJ) {
			json_t* keyframeJ;
			size_t i;
			json_array_foreach(keyframesJ, i, keyframeJ) {
				uint16_t timestamp = json_integer_value(json_array_get(keyframeJ, 0));
				uint16_t values[4];
				for (int k = 0; k < 4; k++) {
					values[k] = json_integer_value(json_array_get(keyframeJ, k + 1));
				}
				keyframer.AddKeyframe(timestamp, values);
			}
		}

		json_t* channelsJ = json_object_get(rootJ, "channels");
		if (channelsJ) {
			for (int i = 0; i < 4; i++) {
				json_t* channelJ = json_array_get(channelsJ, i);
				if (channelJ) {
					json_t* curveJ = json_object_get(channelJ, "curve");
					if (curveJ)
						keyframer.mutable_settings(i)->easing_curve = (frames::EasingCurve) json_integer_value(curveJ);
					json_t* responseJ = json_object_get(channelJ, "response");
					if (responseJ)
						keyframer.mutable_settings(i)->response = json_integer_value(responseJ);
				}
			}
		}
	}

	void onReset() override {
		poly_lfo_mode = false;
		keyframer.Clear();
		for (int i = 0; i < 4; i++) {
			keyframer.mutable_settings(i)->easing_curve = frames::EASING_CURVE_LINEAR;
			keyframer.mutable_settings(i)->response = 0;
		}
	}
	void onRandomize() override {
		// TODO
		// Maybe something useful should go in here?
	}
};


struct CKSSRot : SVGSwitch {
	CKSSRot() {
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/CKSS_rot_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/CKSS_rot_1.svg")));
	}
};


struct FramesWidget : ModuleWidget {
	FramesWidget(Frames* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Frames.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

		addParam(createParam<Rogan1PSWhite>(Vec(14, 52), module, Frames::GAIN1_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(81, 52), module, Frames::GAIN2_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(149, 52), module, Frames::GAIN3_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(216, 52), module, Frames::GAIN4_PARAM));
		addParam(createParamCentered<Rogan6PSWhite>(Vec(133.556641, 159.560532), module, Frames::FRAME_PARAM));
		addParam(createParam<Rogan1PSGreen>(Vec(208, 141), module, Frames::MODULATION_PARAM));
		addParam(createParam<CKD6>(Vec(19, 123), module, Frames::ADD_PARAM));
		addParam(createParam<CKD6>(Vec(19, 172), module, Frames::DEL_PARAM));
		addParam(createParam<CKSSRot>(Vec(18, 239), module, Frames::OFFSET_PARAM));

		addInput(createInput<PJ301MPort>(Vec(16, 273), module, Frames::ALL_INPUT));
		addInput(createInput<PJ301MPort>(Vec(59, 273), module, Frames::IN1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(102, 273), module, Frames::IN2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(145, 273), module, Frames::IN3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(188, 273), module, Frames::IN4_INPUT));
		addInput(createInput<PJ301MPort>(Vec(231, 273), module, Frames::FRAME_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(16, 315), module, Frames::MIX_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(59, 315), module, Frames::OUT1_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(102, 315), module, Frames::OUT2_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(145, 315), module, Frames::OUT3_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(188, 315), module, Frames::OUT4_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(231, 315), module, Frames::FRAME_STEP_OUTPUT));

		addChild(createLight<SmallLight<GreenLight>>(Vec(30, 101), module, Frames::GAIN1_LIGHT + 0));
		addChild(createLight<SmallLight<GreenLight>>(Vec(97, 101), module, Frames::GAIN1_LIGHT + 1));
		addChild(createLight<SmallLight<GreenLight>>(Vec(165, 101), module, Frames::GAIN1_LIGHT + 2));
		addChild(createLight<SmallLight<GreenLight>>(Vec(232, 101), module, Frames::GAIN1_LIGHT + 3));
		addChild(createLight<MediumLight<GreenLight>>(Vec(61, 155), module, Frames::EDIT_LIGHT));

		addChild(createLightCentered<Rogan6PSLight<RedGreenBlueLight>>(Vec(133.556641, 159.560532), module, Frames::FRAME_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		Frames* module = dynamic_cast<Frames*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Channel settings"));

		for (int c = 0; c < 4; c++) {
			menu->addChild(createSubmenuItem(string::f("Channel %d", c + 1),
				[=](Menu* menu) {
					menu->addChild(createMenuLabel("Interpolation curve"));

					static const std::vector<std::string> curveLabels = {
						"Step",
						"Linear",
						"Accelerating",
						"Decelerating",
						"Departure/arrival",
						"Bouncing",
					};
					for (int i = 0; i < (int) curveLabels.size(); i++) {
						menu->addChild(createCheckMenuItem(curveLabels[i],
							[=]() {return module->keyframer.mutable_settings(c)->easing_curve == i;},
							[=]() {module->keyframer.mutable_settings(c)->easing_curve = (frames::EasingCurve) i;}
						));
					}

					menu->addChild(new MenuSeparator);

					menu->addChild(createMenuLabel("Response curve"));

					menu->addChild(createCheckMenuItem("Linear",
						[=]() {return module->keyframer.mutable_settings(c)->response == 0;},
						[=]() {module->keyframer.mutable_settings(c)->response = 0;}
					));
					menu->addChild(createCheckMenuItem("Exponential",
						[=]() {return module->keyframer.mutable_settings(c)->response == 255;},
						[=]() {module->keyframer.mutable_settings(c)->response = 255;}
					));
				}
			));
		}

		menu->addChild(createMenuItem("Clear keyframes", "",
			[=]() {module->keyframer.Clear();}
		));

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Alternate modes"));

		static const std::vector<std::string> modeLabels = {
			"Keyframer",
			"Poly LFO",
		};
		for (int i = 0; i < (int) modeLabels.size(); i++) {
			menu->addChild(createCheckMenuItem(modeLabels[i],
				[=]() {return module->poly_lfo_mode == i;},
				[=]() {module->poly_lfo_mode = i;}
			));
		}
	}
};


Model* modelFrames = createModel<Frames, FramesWidget>("Frames");
