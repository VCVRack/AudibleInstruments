#include "plugin.hpp"
#include "frames/keyframer.h"
#include "frames/poly_lfo.h"
#include "frames/resources.h"


// VCA gain for a 16-bit level and response setting, following Keyframer::ConvertToDacCode(). The firmware
// blends the DAC codes of its linear and exponential responses, weighted by lut_response_balance. Per
// frames/resources/lookup_tables.py, the 2164 sees a 2.5 V full-scale DAC at -30 dB/V, so the exponential
// response spans 75 dB. Response 0 keeps the exact linear gain (hardware's linear table matches it down to a
// -72 dB floor), floating point avoids the DAC's 12-bit steps, and a closed VCA is silent rather than at the
// 2164's -72 to -75 dB floor.
static float vcaGain(uint16_t level, uint8_t response) {
	if (response == 0 || level == 0)
		return level / 65535.f;
	float index = level / 64.f;
	int i = std::min((int) index, 1023);
	float linear = crossfade((float) frames::lut_vca_linear[i], (float) frames::lut_vca_linear[i + 1], index - i);
	float exponential = 65535.f - level;
	float balance = frames::lut_response_balance[response] / 32768.f;
	float code = linear + (exponential - linear) * balance;
	return std::pow(10.f, -3.75f * code / 65536.f);
}

// The response curve used before patch version 2: a 200^x approximation of the 2164, crossfaded linearly.
static float legacyVcaGain(float gain, uint8_t response) {
	if (response > 0) {
		const float expBase = 200.0;
		float expGain = rescale(powf(expBase, gain), 1.0f, expBase, 0.0f, 1.0f);
		gain = crossfade(gain, expGain, response / 255.0f);
	}
	return gain;
}


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
		// Appended to preserve existing patch and automation IDs. Set from the context menu, not the panel.
		ENUMS(RESPONSE_PARAMS, 4),
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
	bool sequencer_mode = false;
	bool legacyResponse = false;
	bool stepArmed = false;
	int sequencerStep = 0;
	int previousPosition = -2;
	int previousNearest = -2;
	bool canFireStep = false;
	dsp::PulseGenerator frameStepPulse;
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
		for (int i = 0; i < 4; i++) {
			// The hardware sets each channel's response, 0-255, by holding DELETE and turning that channel's knob.
			ParamQuantity* pq = configParam(RESPONSE_PARAMS + i, 0.f, 1.f, 0.f, string::f("Channel %d response", i + 1), "%", 0.f, 100.f);
			pq->description = "0% linear, 100% exponential (VCA response)";
			pq->randomizeEnabled = false;
		}

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

		memset((void*) &keyframer, 0, sizeof(keyframer));
		keyframer.Init();
		memset((void*) &poly_lfo, 0, sizeof(poly_lfo));
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
		float frameModulation = params[MODULATION_PARAM].getValue() * inputs[FRAME_INPUT].getVoltage() / 10.f * 65535.f;
		int32_t timestampMod = timestamp + frameModulation;
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
			if (sequencer_mode) {
				// The hardware's 33k/100k ADC stage and 3.3 V reference,
				// followed by firmware's doubling, give full scale at 5 V.
				// Keep the existing animation CV mapping above unchanged.
				float stepModulation = 2.f * frameModulation;
				if (stepModulation < 21845.f)
					stepArmed = true;
				if (stepModulation > 43690.f && stepArmed) {
					stepArmed = false;
					++sequencerStep;
				}
				if (sequencerStep >= keyframer.num_keyframes())
					sequencerStep = 0;
				if (keyframer.num_keyframes() > 0)
					timestampMod = keyframer.keyframe(sequencerStep).timestamp;
			}
			keyframer.Evaluate(timestampMod);

			// Match frames.cc: crossing a boundary fires once, and a change
			// of nearest keyframe rearms it (avoids chatter at a boundary).
			int position = keyframer.position();
			if (position != previousPosition) {
				previousPosition = position;
				if (canFireStep && keyframer.num_keyframes() > 0) {
					frameStepPulse.trigger(1e-3f);
					canFireStep = false;
				}
			}
			int nearest = keyframer.nearest_keyframe();
			if (nearest != previousNearest) {
				previousNearest = nearest;
				canFireStep = true;
			}
		}
		outputs[FRAME_STEP_OUTPUT].setVoltage(frameStepPulse.process(args.sampleTime) ? 5.f : 0.f);

		// Get gains
		float gains[4];
		for (int i = 0; i < 4; i++) {
			uint8_t response = (uint8_t) std::round(params[RESPONSE_PARAMS + i].getValue() * 255.f);
			keyframer.mutable_settings(i)->response = response;
			if (legacyResponse) {
				float level = poly_lfo_mode ? poly_lfo.level16(i) / 65535.0 : keyframer.level(i) / 65535.0;
				gains[i] = legacyVcaGain(level, response);
			}
			else if (poly_lfo_mode) {
				// As in firmware (PolyLfo::Render), poly LFO levels always use the linear response.
				gains[i] = poly_lfo.level16(i) / 65535.0;
			}
			else {
				gains[i] = vcaGain(keyframer.level(i), response);
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
		json_object_set_new(rootJ, "sequencer", json_boolean(sequencer_mode));

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
		json_object_set_new(rootJ, "legacyResponse", json_boolean(legacyResponse));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// Patches saved before the hardware response curve keep the legacy curve, so their levels do not change.
		json_t* legacyResponseJ = json_object_get(rootJ, "legacyResponse");
		legacyResponse = legacyResponseJ ? json_is_true(legacyResponseJ) : true;
		sequencer_mode = json_is_true(json_object_get(rootJ, "sequencer"));
		resetStepState();
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
					// Patches saved before the response parameters existed store it only here.
					json_t* responseJ = json_object_get(channelJ, "response");
					if (responseJ) {
						int response = clamp((int) json_integer_value(responseJ), 0, 255);
						keyframer.mutable_settings(i)->response = response;
						params[RESPONSE_PARAMS + i].setValue(response / 255.f);
					}
				}
			}
		}
	}

	void resetStepState() {
		stepArmed = false;
		sequencerStep = 0;
		previousPosition = previousNearest = -2;
		canFireStep = false;
		frameStepPulse.reset();
	}

	void onReset() override {
		poly_lfo_mode = false;
		sequencer_mode = false;
		legacyResponse = false;
		resetStepState();
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
		shadow->opacity = 0.0;
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
			menu->addChild(createSubmenuItem(string::f("Channel %d", c + 1), "",
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
						menu->addChild(createCheckMenuItem(curveLabels[i], "",
							[=]() {return module->keyframer.mutable_settings(c)->easing_curve == i;},
							[=]() {module->keyframer.mutable_settings(c)->easing_curve = (frames::EasingCurve) i;}
						));
					}

					menu->addChild(new MenuSeparator);

					menu->addChild(createMenuLabel("Response curve"));

					// Continuous, as on hardware: a blend between the linear and exponential VCA responses.
					ui::Slider* slider = new ui::Slider;
					slider->quantity = module->getParamQuantity(Frames::RESPONSE_PARAMS + c);
					slider->box.size.x = 200.f;
					menu->addChild(slider);

					menu->addChild(createCheckMenuItem("Linear", "",
						[=]() {return module->params[Frames::RESPONSE_PARAMS + c].getValue() == 0.f;},
						[=]() {module->getParamQuantity(Frames::RESPONSE_PARAMS + c)->setValue(0.f);}
					));
					menu->addChild(createCheckMenuItem("Exponential", "",
						[=]() {return module->params[Frames::RESPONSE_PARAMS + c].getValue() == 1.f;},
						[=]() {module->getParamQuantity(Frames::RESPONSE_PARAMS + c)->setValue(1.f);}
					));
				}
			));
		}

		menu->addChild(createMenuItem("Clear keyframes", "",
			[=]() {module->keyframer.Clear();}
		));

		menu->addChild(createBoolPtrMenuItem("Legacy response curve", "", &module->legacyResponse));

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Alternate modes"));

		static const std::vector<std::string> modeLabels = {
			"Keyframer",
			"Poly LFO",
			"Sequencer",
		};
		for (int i = 0; i < (int) modeLabels.size(); i++) {
			menu->addChild(createCheckMenuItem(modeLabels[i], "",
				[=]() {return (module->poly_lfo_mode ? 1 : module->sequencer_mode ? 2 : 0) == i;},
				[=]() {
					module->poly_lfo_mode = i == 1;
					module->sequencer_mode = i == 2;
				}
			));
		}
	}
};


Model* modelFrames = createModel<Frames, FramesWidget>("Frames");
