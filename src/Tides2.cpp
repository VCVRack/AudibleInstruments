#include "plugin.hpp"
#include "stmlib/dsp/hysteresis_quantizer.h"
#include "stmlib/dsp/units.h"
#include "tides2/poly_slope_generator.h"
#include "tides2/ramp_extractor.h"
#include "tides2/io_buffer.h"


static const float kRootScaled[3] = {
	0.125f,
	2.0f,
	130.81f
};

static const tides2::Ratio kRatios[20] = {
	{ 0.0625f, 16 },
	{ 0.125f, 8 },
	{ 0.1666666f, 6 },
	{ 0.25f, 4 },
	{ 0.3333333f, 3 },
	{ 0.5f, 2 },
	{ 0.6666666f, 3 },
	{ 0.75f, 4 },
	{ 0.8f, 5 },
	{ 1, 1 },
	{ 1, 1 },
	{ 1.25f, 4 },
	{ 1.3333333f, 3 },
	{ 1.5f, 2 },
	{ 2.0f, 1 },
	{ 3.0f, 1 },
	{ 4.0f, 1 },
	{ 6.0f, 1 },
	{ 8.0f, 1 },
	{ 16.0f, 1 },
};


struct Tides2 : Module {
	enum ParamIds {
		RANGE_PARAM,
		MODE_PARAM,
		RAMP_PARAM,
		FREQUENCY_PARAM,
		SHAPE_PARAM,
		SMOOTHNESS_PARAM,
		SLOPE_PARAM,
		SHIFT_PARAM,
		SLOPE_CV_PARAM,
		FREQUENCY_CV_PARAM,
		SMOOTHNESS_CV_PARAM,
		SHAPE_CV_PARAM,
		SHIFT_CV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		SLOPE_INPUT,
		FREQUENCY_INPUT,
		V_OCT_INPUT,
		SMOOTHNESS_INPUT,
		SHAPE_INPUT,
		SHIFT_INPUT,
		TRIG_INPUT,
		CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUT_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(RANGE_LIGHT, 2),
		ENUMS(OUTPUT_MODE_LIGHT, 2),
		ENUMS(RAMP_MODE_LIGHT, 2),
		ENUMS(OUTPUT_LIGHTS, 4),
		NUM_LIGHTS
	};

	tides2::PolySlopeGenerator poly_slope_generator;
	tides2::RampExtractor ramp_extractor;
	stmlib::HysteresisQuantizer ratio_index_quantizer;

	// State
	int range;
	tides2::OutputMode output_mode;
	tides2::RampMode ramp_mode;
	dsp::BooleanTrigger rangeTrigger;
	dsp::BooleanTrigger modeTrigger;
	dsp::BooleanTrigger rampTrigger;

	// Buffers
	tides2::PolySlopeGenerator::OutputSample out[tides2::kBlockSize] = {};
	stmlib::GateFlags trig_flags[tides2::kBlockSize] = {};
	stmlib::GateFlags clock_flags[tides2::kBlockSize] = {};
	stmlib::GateFlags previous_trig_flag = stmlib::GATE_FLAG_LOW;
	stmlib::GateFlags previous_clock_flag = stmlib::GATE_FLAG_LOW;

	bool must_reset_ramp_extractor = true;
	tides2::OutputMode previous_output_mode = tides2::OUTPUT_MODE_GATES;
	uint8_t frame = 0;

	Tides2() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RANGE_PARAM, 0.0, 1.0, 0.0, "Frequency range");
		configParam(MODE_PARAM, 0.0, 1.0, 0.0, "Output mode");
		configParam(FREQUENCY_PARAM, -48, 48, 0.0, "Frequency");
		configParam(SHAPE_PARAM, 0.0, 1.0, 0.5, "Shape");
		configParam(RAMP_PARAM, 0.0, 1.0, 0.0, "Ramp mode");
		configParam(SMOOTHNESS_PARAM, 0.0, 1.0, 0.5, "Waveshape transformation");
		configParam(SLOPE_PARAM, 0.0, 1.0, 0.5, "Ascending/descending ratio");
		configParam(SHIFT_PARAM, 0.0, 1.0, 0.5, "Output polarization and shifting");
		configParam(SLOPE_CV_PARAM, -1.0, 1.0, 0.0, "Slope CV");
		configParam(FREQUENCY_CV_PARAM, -1.0, 1.0, 0.0, "Frequency CV");
		configParam(SMOOTHNESS_CV_PARAM, -1.0, 1.0, 0.0, "Smoothness CV");
		configParam(SHAPE_CV_PARAM, -1.0, 1.0, 0.0, "Shape CV");
		configParam(SHIFT_CV_PARAM, -1.0, 1.0, 0.0, "Shift CV");

		poly_slope_generator.Init();
		ratio_index_quantizer.Init();
		onReset();
		onSampleRateChange();
	}

	void onReset() override {
		range = 1;
		output_mode = tides2::OUTPUT_MODE_GATES;
		ramp_mode = tides2::RAMP_MODE_LOOPING;
	}

	void onRandomize() override {
		range = random::u32() % 3;
		output_mode = (tides2::OutputMode)(random::u32() % 4);
		ramp_mode = (tides2::RampMode)(random::u32() % 3);
	}

	void onSampleRateChange() override {
		ramp_extractor.Init(APP->engine->getSampleRate(), 40.f / APP->engine->getSampleRate());
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "range", json_integer(range));
		json_object_set_new(rootJ, "output", json_integer(output_mode));
		json_object_set_new(rootJ, "ramp", json_integer(ramp_mode));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* rangeJ = json_object_get(rootJ, "range");
		if (rangeJ)
			range = json_integer_value(rangeJ);

		json_t* outputJ = json_object_get(rootJ, "output");
		if (outputJ)
			output_mode = (tides2::OutputMode) json_integer_value(outputJ);

		json_t* rampJ = json_object_get(rootJ, "ramp");
		if (rampJ)
			ramp_mode = (tides2::RampMode) json_integer_value(rampJ);
	}

	void process(const ProcessArgs& args) override {
		// Switches
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.f)) {
			range = (range + 1) % 3;
		}
		if (modeTrigger.process(params[MODE_PARAM].getValue() > 0.f)) {
			output_mode = (tides2::OutputMode)((output_mode + 1) % 4);
		}
		if (rampTrigger.process(params[RAMP_PARAM].getValue() > 0.f)) {
			ramp_mode = (tides2::RampMode)((ramp_mode + 1) % 3);
		}

		// Input gates
		trig_flags[frame] = stmlib::ExtractGateFlags(previous_trig_flag, inputs[TRIG_INPUT].getVoltage() >= 1.7f);
		previous_trig_flag = trig_flags[frame];

		clock_flags[frame] = stmlib::ExtractGateFlags(previous_clock_flag, inputs[CLOCK_INPUT].getVoltage() >= 1.7f);
		previous_clock_flag = clock_flags[frame];

		// Process block
		if (++frame >= tides2::kBlockSize) {
			frame = 0;

			tides2::Range range_mode = (range < 2) ? tides2::RANGE_CONTROL : tides2::RANGE_AUDIO;
			float note = clamp(params[FREQUENCY_PARAM].getValue() + 12.f * inputs[V_OCT_INPUT].getVoltage(), -96.f, 96.f);
			float fm = clamp(params[FREQUENCY_CV_PARAM].getValue() * inputs[FREQUENCY_INPUT].getVoltage() * 12.f, -96.f, 96.f);
			float transposition = note + fm;

			float ramp[tides2::kBlockSize];
			float frequency;

			if (inputs[CLOCK_INPUT].isConnected()) {
				if (must_reset_ramp_extractor) {
					ramp_extractor.Reset();
				}

				tides2::Ratio r = ratio_index_quantizer.Lookup(kRatios, 0.5f + transposition * 0.0105f, 20);
				frequency = ramp_extractor.Process(
				              range_mode == tides2::RANGE_AUDIO,
				              range_mode == tides2::RANGE_AUDIO && ramp_mode == tides2::RAMP_MODE_AR,
				              r,
				              clock_flags,
				              ramp,
				              tides2::kBlockSize);
				must_reset_ramp_extractor = false;
			}
			else {
				frequency = kRootScaled[range] / args.sampleRate * stmlib::SemitonesToRatio(transposition);
				must_reset_ramp_extractor = true;
			}

			// Get parameters
			float slope = clamp(params[SLOPE_PARAM].getValue() + dsp::cubic(params[SLOPE_CV_PARAM].getValue()) * inputs[SLOPE_INPUT].getVoltage() / 10.f, 0.f, 1.f);
			float shape = clamp(params[SHAPE_PARAM].getValue() + dsp::cubic(params[SHAPE_CV_PARAM].getValue()) * inputs[SHAPE_INPUT].getVoltage() / 10.f, 0.f, 1.f);
			float smoothness = clamp(params[SMOOTHNESS_PARAM].getValue() + dsp::cubic(params[SMOOTHNESS_CV_PARAM].getValue()) * inputs[SMOOTHNESS_INPUT].getVoltage() / 10.f, 0.f, 1.f);
			float shift = clamp(params[SHIFT_PARAM].getValue() + dsp::cubic(params[SHIFT_CV_PARAM].getValue()) * inputs[SHIFT_INPUT].getVoltage() / 10.f, 0.f, 1.f);

			if (output_mode != previous_output_mode) {
				poly_slope_generator.Reset();
				previous_output_mode = output_mode;
			}

			// Render generator
			poly_slope_generator.Render(
			  ramp_mode,
			  output_mode,
			  range_mode,
			  frequency,
			  slope,
			  shape,
			  smoothness,
			  shift,
			  trig_flags,
			  !inputs[TRIG_INPUT].isConnected() && inputs[CLOCK_INPUT].isConnected() ? ramp : NULL,
			  out,
			  tides2::kBlockSize);

			// Set lights
			lights[RANGE_LIGHT + 0].value = (range == 0 || range == 1);
			lights[RANGE_LIGHT + 1].value = (range == 1 || range == 2);
			lights[OUTPUT_MODE_LIGHT + 0].value = (output_mode == tides2::OUTPUT_MODE_AMPLITUDE || output_mode == tides2::OUTPUT_MODE_SLOPE_PHASE);
			lights[OUTPUT_MODE_LIGHT + 1].value = (output_mode == tides2::OUTPUT_MODE_FREQUENCY || output_mode == tides2::OUTPUT_MODE_SLOPE_PHASE);
			lights[RAMP_MODE_LIGHT + 0].value = (ramp_mode == tides2::RAMP_MODE_AD || ramp_mode == tides2::RAMP_MODE_LOOPING);
			lights[RAMP_MODE_LIGHT + 1].value = (ramp_mode == tides2::RAMP_MODE_AR || ramp_mode == tides2::RAMP_MODE_LOOPING);
		}

		// Outputs
		for (int i = 0; i < 4; i++) {
			float value = out[frame].channel[i];
			outputs[OUT_OUTPUTS + i].setVoltage(value);
			lights[OUTPUT_LIGHTS + i].setSmoothBrightness(value, args.sampleTime);
		}
	}
};


struct Tides2Widget : ModuleWidget {
	Tides2Widget(Tides2* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Tides2.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<TL1105>(mm2px(Vec(7.425, 16.15)), module, Tides2::RANGE_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(63.325, 16.15)), module, Tides2::MODE_PARAM));
		addParam(createParamCentered<Rogan3PSWhite>(mm2px(Vec(16.325, 33.449)), module, Tides2::FREQUENCY_PARAM));
		addParam(createParamCentered<Rogan3PSWhite>(mm2px(Vec(54.425, 33.449)), module, Tides2::SHAPE_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(35.375, 38.699)), module, Tides2::RAMP_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(35.375, 55.549)), module, Tides2::SMOOTHNESS_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(11.575, 60.599)), module, Tides2::SLOPE_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(59.175, 60.599)), module, Tides2::SHIFT_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(9.276, 80.599)), module, Tides2::SLOPE_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.324, 80.599)), module, Tides2::FREQUENCY_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(35.375, 80.599)), module, Tides2::SMOOTHNESS_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(48.425, 80.599)), module, Tides2::SHAPE_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(61.475, 80.599)), module, Tides2::SHIFT_CV_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.775, 96.499)), module, Tides2::SLOPE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.225, 96.499)), module, Tides2::FREQUENCY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(29.675, 96.499)), module, Tides2::V_OCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(41.125, 96.499)), module, Tides2::SMOOTHNESS_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(52.575, 96.499)), module, Tides2::SHAPE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(64.025, 96.499)), module, Tides2::SHIFT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.775, 111.099)), module, Tides2::TRIG_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.225, 111.099)), module, Tides2::CLOCK_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29.675, 111.099)), module, Tides2::OUT_OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(41.125, 111.099)), module, Tides2::OUT_OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(52.575, 111.099)), module, Tides2::OUT_OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(64.025, 111.099)), module, Tides2::OUT_OUTPUTS + 3));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(13.776, 16.149)), module, Tides2::RANGE_LIGHT));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(56.975, 16.149)), module, Tides2::OUTPUT_MODE_LIGHT));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(35.375, 33.449)), module, Tides2::RAMP_MODE_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(26.174, 104.749)), module, Tides2::OUTPUT_LIGHTS + 0));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(37.625, 104.749)), module, Tides2::OUTPUT_LIGHTS + 1));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(49.075, 104.749)), module, Tides2::OUTPUT_LIGHTS + 2));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(60.525, 104.749)), module, Tides2::OUTPUT_LIGHTS + 3));
	}
};


Model* modelTides2 = createModel<Tides2, Tides2Widget>("Tides2");
