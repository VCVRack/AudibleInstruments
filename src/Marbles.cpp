#include "plugin.hpp"
#include "marbles/random/random_generator.h"
#include "marbles/random/random_stream.h"
#include "marbles/random/t_generator.h"
#include "marbles/random/x_y_generator.h"
#include "marbles/note_filter.h"
#include "marbles/clock_self_patching_detector.h"


static const int BLOCK_SIZE = 5;


static const marbles::Scale preset_scales[6] = {
	// C major
	{
		1.0f,
		12,
		{
			{ 0.0000f, 255 },  // C
			{ 0.0833f, 16 },   // C#
			{ 0.1667f, 96 },   // D
			{ 0.2500f, 24 },   // D#
			{ 0.3333f, 128 },  // E
			{ 0.4167f, 64 },   // F
			{ 0.5000f, 8 },    // F#
			{ 0.5833f, 192 },  // G
			{ 0.6667f, 16 },   // G#
			{ 0.7500f, 96 },   // A
			{ 0.8333f, 24 },   // A#
			{ 0.9167f, 128 },  // B
		}
	},

	// C minor
	{
		1.0f,
		12,
		{
			{ 0.0000f, 255 },  // C
			{ 0.0833f, 16 },   // C#
			{ 0.1667f, 96 },   // D
			{ 0.2500f, 128 },  // Eb
			{ 0.3333f, 8 },    // E
			{ 0.4167f, 64 },   // F
			{ 0.5000f, 4 },    // F#
			{ 0.5833f, 192 },  // G
			{ 0.6667f, 96 },   // G#
			{ 0.7500f, 16 },   // A
			{ 0.8333f, 128 },  // Bb
			{ 0.9167f, 16 },   // B
		}
	},

	// Pentatonic
	{
		1.0f,
		12,
		{
			{ 0.0000f, 255 },  // C
			{ 0.0833f, 4 },    // C#
			{ 0.1667f, 96 },   // D
			{ 0.2500f, 4 },    // Eb
			{ 0.3333f, 4 },    // E
			{ 0.4167f, 140 },  // F
			{ 0.5000f, 4 },    // F#
			{ 0.5833f, 192 },  // G
			{ 0.6667f, 4 },    // G#
			{ 0.7500f, 96 },   // A
			{ 0.8333f, 4 },    // Bb
			{ 0.9167f, 4 },    // B
		}
	},

	// Pelog
	{
		1.0f,
		7,
		{
			{ 0.0000f, 255 },  // C
			{ 0.1275f, 128 },  // Db+
			{ 0.2625f, 32 },  // Eb-
			{ 0.4600f, 8 },    // F#-
			{ 0.5883f, 192 },  // G
			{ 0.7067f, 64 },  // Ab
			{ 0.8817f, 16 },    // Bb+
		}
	},

	// Raag Bhairav That
	{
		1.0f,
		12,
		{
			{ 0.0000f, 255 }, // ** Sa
			{ 0.0752f, 128 }, // ** Komal Re
			{ 0.1699f, 4 },   //    Re
			{ 0.2630f, 4 },   //    Komal Ga
			{ 0.3219f, 128 }, // ** Ga
			{ 0.4150f, 64 },  // ** Ma
			{ 0.4918f, 4 },   //    Tivre Ma
			{ 0.5850f, 192 }, // ** Pa
			{ 0.6601f, 64 },  // ** Komal Dha
			{ 0.7549f, 4 },   //    Dha
			{ 0.8479f, 4 },   //    Komal Ni
			{ 0.9069f, 64 },  // ** Ni
		}
	},

	// Raag Shri
	{
		1.0f,
		12,
		{
			{ 0.0000f, 255 }, // ** Sa
			{ 0.0752f, 4 },   //    Komal Re
			{ 0.1699f, 128 }, // ** Re
			{ 0.2630f, 64 },  // ** Komal Ga
			{ 0.3219f, 4 },   //    Ga
			{ 0.4150f, 128 }, // ** Ma
			{ 0.4918f, 4 },   //    Tivre Ma
			{ 0.5850f, 192 }, // ** Pa
			{ 0.6601f, 4 },   //    Komal Dha
			{ 0.7549f, 64 },  // ** Dha
			{ 0.8479f, 128 }, // ** Komal Ni
			{ 0.9069f, 4 },   //    Ni
		}
	},
};


// Panel buttons act on release, or as a long press once held for 2 s, as in the firmware UI.
struct MarblesButton {
	enum Press {
		NONE,
		SHORT,
		LONG,
	};
	bool held = false;
	bool longFired = false;
	float heldTime = 0.f;

	Press process(bool pressed, float sampleTime) {
		if (pressed) {
			if (!held) {
				held = true;
				longFired = false;
				heldTime = 0.f;
			}
			else if (!longFired) {
				heldTime += sampleTime;
				if (heldTime >= 2.f) {
					longFired = true;
					return LONG;
				}
			}
			return NONE;
		}
		Press press = (held && !longFired) ? SHORT : NONE;
		held = false;
		return press;
	}
};


struct Marbles : Module {
	enum ParamIds {
		T_DEJA_VU_PARAM,
		X_DEJA_VU_PARAM,
		DEJA_VU_PARAM,
		T_RATE_PARAM,
		X_SPREAD_PARAM,
		T_MODE_PARAM,
		X_MODE_PARAM,
		DEJA_VU_LENGTH_PARAM,
		T_BIAS_PARAM,
		X_BIAS_PARAM,
		T_RANGE_PARAM,
		X_RANGE_PARAM,
		EXTERNAL_PARAM,
		T_JITTER_PARAM,
		X_STEPS_PARAM,
		// Append parameters to preserve existing patch and automation IDs.
		Y_RATE_PARAM,
		Y_SPREAD_PARAM,
		Y_BIAS_PARAM,
		Y_STEPS_PARAM,
		GATE_BIAS_PARAM,
		GATE_JITTER_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		T_BIAS_INPUT,
		X_BIAS_INPUT,
		T_CLOCK_INPUT,
		T_RATE_INPUT,
		T_JITTER_INPUT,
		DEJA_VU_INPUT,
		X_STEPS_INPUT,
		X_SPREAD_INPUT,
		X_CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		T1_OUTPUT,
		T2_OUTPUT,
		T3_OUTPUT,
		Y_OUTPUT,
		X1_OUTPUT,
		X2_OUTPUT,
		X3_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		T_DEJA_VU_LIGHT,
		X_DEJA_VU_LIGHT,
		ENUMS(T_MODE_LIGHTS, 2),
		ENUMS(X_MODE_LIGHTS, 2),
		ENUMS(T_RANGE_LIGHTS, 2),
		ENUMS(X_RANGE_LIGHTS, 2),
		EXTERNAL_LIGHT,
		T1_LIGHT,
		T2_LIGHT,
		T3_LIGHT,
		ENUMS(Y_LIGHT, 2),
		ENUMS(X1_LIGHT, 2),
		ENUMS(X2_LIGHT, 2),
		ENUMS(X3_LIGHT, 2),
		NUM_LIGHTS
	};

	marbles::RandomGenerator random_generator;
	marbles::RandomStream random_stream;
	marbles::TGenerator t_generator;
	marbles::XYGenerator xy_generator;
	marbles::NoteFilter note_filter;

	// State
	MarblesButton tDejaVuButton;
	MarblesButton xDejaVuButton;
	MarblesButton tModeButton;
	MarblesButton xModeButton;
	MarblesButton tRangeButton;
	MarblesButton xRangeButton;
	MarblesButton externalButton;
	enum DejaVuState {
		DEJA_VU_OFF,
		DEJA_VU_ON,
		// Long press: the loop stays locked regardless of the DEJA VU knob.
		DEJA_VU_LOCKED,
	};
	int t_deja_vu;
	int x_deja_vu;
	// Long-pressing X RANGE selects the scale until the buttons have been idle for 4 s.
	bool selectingScale = false;
	float buttonIdleTime = 0.f;
	float lightTime = 0.f;
	int t_mode;
	int x_mode;
	int t_range;
	int x_range;
	bool external;
	int x_scale;
	int x_clock_source_internal;
	int y_range;
	// Patches saved before Y had its own settings drove Y from X's range and knobs.
	bool legacyYFollowsX = false;
	// Hardware Y quantizes to octaves only; optionally use X's selected scale instead.
	bool yScaleFollowsX = false;
	bool yScaleLoadedFollowsX = false;
	bool explicitReset = false;
	bool tResetPending = false;
	bool xResetPending = false;
	dsp::SchmittTrigger tResetTrigger;
	dsp::SchmittTrigger xResetTrigger;
	marbles::ClockSelfPatchingDetector selfPatchingDetector[3];
	marbles::IOBuffer::Block clockHistory = {};

	// Buffers
	stmlib::GateFlags t_clocks[BLOCK_SIZE] = {};
	stmlib::GateFlags last_t_clock = 0;
	stmlib::GateFlags xy_clocks[BLOCK_SIZE] = {};
	stmlib::GateFlags last_xy_clock = 0;
	float ramp_master[BLOCK_SIZE] = {};
	float ramp_external[BLOCK_SIZE] = {};
	float ramp_slave[2][BLOCK_SIZE] = {};
	bool gates[BLOCK_SIZE * 2] = {};
	float voltages[BLOCK_SIZE * 4] = {};
	int blockIndex = 0;

	Marbles() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configButton(T_DEJA_VU_PARAM, "T deja vu");
		configButton(X_DEJA_VU_PARAM, "X deja vu");
		configParam(DEJA_VU_PARAM, 0.0, 1.0, 0.5, "Deja vu probability");
		configParam(T_RATE_PARAM, -1.0, 1.0, 0.0, "Clock rate");
		configParam(X_SPREAD_PARAM, 0.0, 1.0, 0.5, "Probability distribution");
		configButton(T_MODE_PARAM, "T mode");
		configButton(X_MODE_PARAM, "X mode");
		configParam(DEJA_VU_LENGTH_PARAM, 0.0, 1.0, 0.0, "Loop length");
		configParam(T_BIAS_PARAM, 0.0, 1.0, 0.5, "Gate bias");
		configParam(X_BIAS_PARAM, 0.0, 1.0, 0.5, "Distribution bias");
		configButton(T_RANGE_PARAM, "Clock range mode");
		configButton(X_RANGE_PARAM, "Output voltage range mode");
		configButton(EXTERNAL_PARAM, "External processing mode");
		configParam(T_JITTER_PARAM, 0.0, 1.0, 0.0, "Randomness amount");
		configParam(X_STEPS_PARAM, 0.0, 1.0, 0.5, "Smoothness");
		configSwitch(Y_RATE_PARAM, 0.f, 11.f, 6.f, "Y divider ratio",
			{"1/64", "1/48", "1/32", "1/24", "1/16", "1/12", "1/8", "1/6", "1/4", "1/3", "1/2", "1"});
		configParam(Y_SPREAD_PARAM, 0.f, 1.f, 0.5f, "Y spread", "%", 0.f, 100.f);
		configParam(Y_BIAS_PARAM, 0.f, 1.f, 0.5f, "Y bias", "%", 0.f, 100.f);
		configParam(Y_STEPS_PARAM, 0.f, 1.f, 0.f, "Y steps", "%", 0.f, 100.f);
		configParam(GATE_BIAS_PARAM, 0.f, 1.f, 0.5f, "T gate length", "%", 0.f, 100.f);
		configParam(GATE_JITTER_PARAM, 0.f, 1.f, 0.f, "T gate length randomness", "%", 0.f, 100.f);
		for (int i = Y_RATE_PARAM; i < NUM_PARAMS; i++)
			getParamQuantity(i)->randomizeEnabled = false;

		configInput(T_BIAS_INPUT, "T bias");
		configInput(X_BIAS_INPUT, "X bias");
		configInput(T_CLOCK_INPUT, "T clock");
		configInput(T_RATE_INPUT, "T rate");
		configInput(T_JITTER_INPUT, "T jitter");
		configInput(DEJA_VU_INPUT, "Deja vu");
		configInput(X_STEPS_INPUT, "X steps");
		configInput(X_SPREAD_INPUT, "X spread");
		configInput(X_CLOCK_INPUT, "X clock");

		configOutput(T1_OUTPUT, "T₁");
		configOutput(T2_OUTPUT, "T₂");
		configOutput(T3_OUTPUT, "T₃");
		configOutput(Y_OUTPUT, "Y");
		configOutput(X1_OUTPUT, "X₁");
		configOutput(X2_OUTPUT, "X₂");
		configOutput(X3_OUTPUT, "X₃");

		random_generator.Init(1);
		random_stream.Init(&random_generator);
		note_filter.Init();
		onSampleRateChange();
		onReset();
	}

	void onReset() override {
		t_deja_vu = DEJA_VU_OFF;
		x_deja_vu = DEJA_VU_OFF;
		selectingScale = false;
		t_mode = 0;
		x_mode = 0;
		t_range = 1;
		x_range = 2;
		external = false;
		x_scale = 0;
		params[Y_RATE_PARAM].setValue(6);
		x_clock_source_internal = 0;
		y_range = 2;
		params[Y_SPREAD_PARAM].setValue(0.5f);
		params[Y_BIAS_PARAM].setValue(0.5f);
		params[Y_STEPS_PARAM].setValue(0.f);
		params[GATE_BIAS_PARAM].setValue(0.5f);
		params[GATE_JITTER_PARAM].setValue(0.f);
		legacyYFollowsX = false;
		yScaleFollowsX = false;
		explicitReset = false;
		tResetPending = xResetPending = false;
		tResetTrigger.reset();
		xResetTrigger.reset();
		for (int i = 0; i < 3; i++)
			selfPatchingDetector[i].Init(i);
	}

	void onRandomize() override {
		t_mode = random::u32() % 3;
		x_mode = random::u32() % 3;
		t_range = random::u32() % 3;
		x_range = random::u32() % 3;
	}

	void onSampleRateChange() override {
		float sampleRate = APP->engine->getSampleRate();
		t_generator.Init(&random_stream, sampleRate);
		xy_generator.Init(&random_stream, sampleRate);

		// Set scales. As on hardware, the shared loader covers X only, leaving Y with the default octave scale.
		for (int i = 0; i < 6; i++) {
			xy_generator.LoadScale(i, preset_scales[i]);
		}
		yScaleLoadedFollowsX = false;
	}

	void loadYScales() {
		marbles::Scale octaves;
		octaves.Init();
		for (int i = 0; i < 6; i++)
			xy_generator.LoadScale(marbles::kNumXChannels, i, yScaleFollowsX ? preset_scales[i] : octaves);
		yScaleLoadedFollowsX = yScaleFollowsX;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		// Older builds read these as on/off; locking is saved separately.
		json_object_set_new(rootJ, "t_deja_vu", json_boolean(t_deja_vu != DEJA_VU_OFF));
		json_object_set_new(rootJ, "x_deja_vu", json_boolean(x_deja_vu != DEJA_VU_OFF));
		json_object_set_new(rootJ, "t_deja_vu_locked", json_boolean(t_deja_vu == DEJA_VU_LOCKED));
		json_object_set_new(rootJ, "x_deja_vu_locked", json_boolean(x_deja_vu == DEJA_VU_LOCKED));
		json_object_set_new(rootJ, "t_mode", json_integer(t_mode));
		json_object_set_new(rootJ, "x_mode", json_integer(x_mode));
		json_object_set_new(rootJ, "t_range", json_integer(t_range));
		json_object_set_new(rootJ, "x_range", json_integer(x_range));
		json_object_set_new(rootJ, "external", json_boolean(external));
		json_object_set_new(rootJ, "x_scale", json_integer(x_scale));
		// Retain legacy fields so older builds can read the knob settings.
		json_object_set_new(rootJ, "y_divider_index", json_integer(clamp(int(std::round(params[Y_RATE_PARAM].getValue())), 0, 11)));
		json_object_set_new(rootJ, "x_clock_source_internal", json_integer(x_clock_source_internal));
		json_object_set_new(rootJ, "y_range", json_integer(y_range));
		json_object_set_new(rootJ, "y_spread", json_real(params[Y_SPREAD_PARAM].getValue()));
		json_object_set_new(rootJ, "y_bias", json_real(params[Y_BIAS_PARAM].getValue()));
		json_object_set_new(rootJ, "y_steps", json_real(params[Y_STEPS_PARAM].getValue()));
		json_object_set_new(rootJ, "t_pulse_width_mean", json_real(params[GATE_BIAS_PARAM].getValue()));
		json_object_set_new(rootJ, "t_pulse_width_std", json_real(params[GATE_JITTER_PARAM].getValue()));
		json_object_set_new(rootJ, "explicitReset", json_boolean(explicitReset));
		json_object_set_new(rootJ, "legacyYFollowsX", json_boolean(legacyYFollowsX));
		json_object_set_new(rootJ, "yScaleFollowsX", json_boolean(yScaleFollowsX));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// Migrate settings from patches saved before these became Rack parameters.
		json_t* meanJ = json_object_get(rootJ, "t_pulse_width_mean");
		params[GATE_BIAS_PARAM].setValue(meanJ ? clamp(float(json_number_value(meanJ)), 0.f, 1.f) : 0.f);
		json_t* stdJ = json_object_get(rootJ, "t_pulse_width_std");
		params[GATE_JITTER_PARAM].setValue(stdJ ? clamp(float(json_number_value(stdJ)), 0.f, 1.f) : 0.f);
		explicitReset = json_is_true(json_object_get(rootJ, "explicitReset"));
		// Patches saved before independent Y settings have no y_range.
		json_t* legacyYJ = json_object_get(rootJ, "legacyYFollowsX");
		legacyYFollowsX = legacyYJ ? json_boolean_value(legacyYJ) : !json_object_get(rootJ, "y_range");
		yScaleFollowsX = json_is_true(json_object_get(rootJ, "yScaleFollowsX"));
		json_t* t_deja_vuJ = json_object_get(rootJ, "t_deja_vu");
		if (t_deja_vuJ)
			t_deja_vu = json_boolean_value(t_deja_vuJ) ? DEJA_VU_ON : DEJA_VU_OFF;
		if (json_is_true(json_object_get(rootJ, "t_deja_vu_locked")))
			t_deja_vu = DEJA_VU_LOCKED;

		json_t* x_deja_vuJ = json_object_get(rootJ, "x_deja_vu");
		if (x_deja_vuJ)
			x_deja_vu = json_boolean_value(x_deja_vuJ) ? DEJA_VU_ON : DEJA_VU_OFF;
		if (json_is_true(json_object_get(rootJ, "x_deja_vu_locked")))
			x_deja_vu = DEJA_VU_LOCKED;

		json_t* t_modeJ = json_object_get(rootJ, "t_mode");
		if (t_modeJ)
			t_mode = clamp((int) json_integer_value(t_modeJ), 0, 6);

		json_t* x_modeJ = json_object_get(rootJ, "x_mode");
		if (x_modeJ)
			x_mode = clamp((int) json_integer_value(x_modeJ), 0, 2);

		json_t* t_rangeJ = json_object_get(rootJ, "t_range");
		if (t_rangeJ)
			t_range = clamp((int) json_integer_value(t_rangeJ), 0, 2);

		json_t* x_rangeJ = json_object_get(rootJ, "x_range");
		if (x_rangeJ)
			x_range = clamp((int) json_integer_value(x_rangeJ), 0, 2);

		json_t* externalJ = json_object_get(rootJ, "external");
		if (externalJ)
			external = json_boolean_value(externalJ);

		json_t* x_scaleJ = json_object_get(rootJ, "x_scale");
		if (x_scaleJ)
			x_scale = clamp((int) json_integer_value(x_scaleJ), 0, 5);

		json_t* y_divider_indexJ = json_object_get(rootJ, "y_divider_index");
		if (y_divider_indexJ)
			params[Y_RATE_PARAM].setValue(clamp(float(json_integer_value(y_divider_indexJ)), 0.f, 11.f));

		json_t* x_clock_source_internalJ = json_object_get(rootJ, "x_clock_source_internal");
		if (x_clock_source_internalJ)
			x_clock_source_internal = clamp((int) json_integer_value(x_clock_source_internalJ), 0, 3);

		json_t* y_rangeJ = json_object_get(rootJ, "y_range");
		if (y_rangeJ)
			y_range = clamp((int) json_integer_value(y_rangeJ), 0, 2);

		json_t* y_spreadJ = json_object_get(rootJ, "y_spread");
		if (y_spreadJ)
			params[Y_SPREAD_PARAM].setValue(clamp(float(json_number_value(y_spreadJ)), 0.f, 1.f));

		json_t* y_biasJ = json_object_get(rootJ, "y_bias");
		if (y_biasJ)
			params[Y_BIAS_PARAM].setValue(clamp(float(json_number_value(y_biasJ)), 0.f, 1.f));

		json_t* y_stepsJ = json_object_get(rootJ, "y_steps");
		if (y_stepsJ)
			params[Y_STEPS_PARAM].setValue(clamp(float(json_number_value(y_stepsJ)), 0.f, 1.f));
	}

	void process(const ProcessArgs& args) override {
		// Buttons, following the firmware's Ui::OnSwitchReleased()
		MarblesButton::Press press;
		bool buttonEvent = false;
		if ((press = tDejaVuButton.process(params[T_DEJA_VU_PARAM].getValue() > 0.f, args.sampleTime))) {
			t_deja_vu = nextDejaVuState(t_deja_vu, press == MarblesButton::LONG);
			buttonEvent = true;
		}
		if ((press = xDejaVuButton.process(params[X_DEJA_VU_PARAM].getValue() > 0.f, args.sampleTime))) {
			x_deja_vu = nextDejaVuState(x_deja_vu, press == MarblesButton::LONG);
			buttonEvent = true;
		}
		if ((press = tModeButton.process(params[T_MODE_PARAM].getValue() > 0.f, args.sampleTime))) {
			// Long press switches to the second bank of models; short press returns to the
			// same model in the first bank, or cycles models within it.
			if (press == MarblesButton::LONG) {
				if (t_mode < 3)
					t_mode += 3;
			}
			else {
				t_mode = (t_mode < 3) ? (t_mode + 1) % 3 : t_mode % 3;
			}
			buttonEvent = true;
		}
		if ((press = xModeButton.process(params[X_MODE_PARAM].getValue() > 0.f, args.sampleTime))) {
			x_mode = (x_mode + 1) % 3;
			buttonEvent = true;
		}
		if ((press = tRangeButton.process(params[T_RANGE_PARAM].getValue() > 0.f, args.sampleTime))) {
			t_range = (t_range + 1) % 3;
			buttonEvent = true;
		}
		if ((press = xRangeButton.process(params[X_RANGE_PARAM].getValue() > 0.f, args.sampleTime))) {
			if (press == MarblesButton::LONG) {
				selectingScale = true;
			}
			else if (selectingScale) {
				x_scale = (x_scale + 1) % 6;
			}
			else if (!external) {
				// Range does not apply while X processes the external input.
				x_range = (x_range + 1) % 3;
			}
			buttonEvent = true;
		}
		if ((press = externalButton.process(params[EXTERNAL_PARAM].getValue() > 0.f, args.sampleTime))) {
			// On hardware, a long press records a scale instead.
			if (press == MarblesButton::SHORT)
				external = !external;
			buttonEvent = true;
		}
		buttonIdleTime = buttonEvent ? 0.f : buttonIdleTime + args.sampleTime;
		if (selectingScale && buttonIdleTime > 4.f)
			selectingScale = false;

		// Clocks
		bool t_gate = (inputs[T_CLOCK_INPUT].getVoltage() >= 1.7f);
		last_t_clock = stmlib::ExtractGateFlags(last_t_clock, t_gate);
		t_clocks[blockIndex] = last_t_clock;

		bool x_gate = (inputs[X_CLOCK_INPUT].getVoltage() >= 1.7f);
		last_xy_clock = stmlib::ExtractGateFlags(last_xy_clock, x_gate);
		xy_clocks[blockIndex] = last_xy_clock;
		clockHistory.input[1][blockIndex] = last_xy_clock;
		for (int i = 0; i < 3; i++)
			clockHistory.gate_output[i][blockIndex] = outputs[T1_OUTPUT + i].getVoltage() >= 1.7f;

		bool tReset = tResetTrigger.process(inputs[T_JITTER_INPUT].getVoltage(), 0.5f, 1.f);
		bool xReset = xResetTrigger.process(inputs[X_STEPS_INPUT].getVoltage(), 0.5f, 1.f);
		tResetPending = explicitReset && (tResetPending || tReset);
		xResetPending = explicitReset && (xResetPending || xReset);

		// Process block
		if (++blockIndex >= BLOCK_SIZE) {
			blockIndex = 0;
			stepBlock();
		}

		// Lights and outputs

		// Firmware LED timing: 256 ms blink, 64 ms fast blink, 512 ms triangle pulse.
		lightTime += args.sampleTime;
		if (lightTime >= 1.024f)
			lightTime -= 1.024f;
		bool slowBlink = std::fmod(lightTime, 0.256f) > 0.128f;
		bool fastBlink = std::fmod(lightTime, 0.064f) > 0.032f;
		float pulse = 1.f - std::fabs(1.f - std::fmod(lightTime, 0.512f) / 0.256f);

		// While selecting a scale, hardware shows only the X RANGE LED.
		float panel = selectingScale ? 0.f : 1.f;

		lights[T_DEJA_VU_LIGHT].setBrightness(panel * dejaVuLight(t_deja_vu, pulse));
		lights[X_DEJA_VU_LIGHT].setBrightness(panel * dejaVuLight(x_deja_vu, pulse));

		// The second bank of T models blinks.
		int t_mode3 = t_mode % 3;
		float t_mode_on = panel * (t_mode < 3 || slowBlink);
		lights[T_MODE_LIGHTS + 0].setBrightness(t_mode_on * (t_mode3 == 0 || t_mode3 == 1));
		lights[T_MODE_LIGHTS + 1].setBrightness(t_mode_on * (t_mode3 == 1 || t_mode3 == 2));

		lights[X_MODE_LIGHTS + 0].setBrightness(panel * (x_mode == 0 || x_mode == 1));
		lights[X_MODE_LIGHTS + 1].setBrightness(panel * (x_mode == 1 || x_mode == 2));

		lights[T_RANGE_LIGHTS + 0].setBrightness(panel * (t_range == 0 || t_range == 1));
		lights[T_RANGE_LIGHTS + 1].setBrightness(panel * (t_range == 1 || t_range == 2));

		if (selectingScale) {
			// Scales 1-3 blink slowly in green/yellow/red, scales 4-6 quickly.
			int color = x_scale % 3;
			float on = (x_scale < 3) ? slowBlink : fastBlink;
			lights[X_RANGE_LIGHTS + 0].setBrightness(on * (color == 0 || color == 1));
			lights[X_RANGE_LIGHTS + 1].setBrightness(on * (color == 1 || color == 2));
		}
		else {
			// The range LED is off while X processes the external input.
			float x_range_on = external ? 0.f : 1.f;
			lights[X_RANGE_LIGHTS + 0].setBrightness(x_range_on * (x_range == 0 || x_range == 1));
			lights[X_RANGE_LIGHTS + 1].setBrightness(x_range_on * (x_range == 1 || x_range == 2));
		}

		lights[EXTERNAL_LIGHT].setBrightness(panel * external);

		outputs[T1_OUTPUT].setVoltage(gates[blockIndex * 2 + 0] ? 10.f : 0.f);
		lights[T1_LIGHT].setSmoothBrightness(gates[blockIndex * 2 + 0], args.sampleTime);
		outputs[T2_OUTPUT].setVoltage((ramp_master[blockIndex] < 0.5f) ? 10.f : 0.f);
		lights[T2_LIGHT].setSmoothBrightness(ramp_master[blockIndex] < 0.5f, args.sampleTime);
		outputs[T3_OUTPUT].setVoltage(gates[blockIndex * 2 + 1] ? 10.f : 0.f);
		lights[T3_LIGHT].setSmoothBrightness(gates[blockIndex * 2 + 1], args.sampleTime);

		outputs[X1_OUTPUT].setVoltage(voltages[blockIndex * 4 + 0]);
		setVoltageLight(X1_LIGHT, voltages[blockIndex * 4 + 0], args.sampleTime);
		outputs[X2_OUTPUT].setVoltage(voltages[blockIndex * 4 + 1]);
		setVoltageLight(X2_LIGHT, voltages[blockIndex * 4 + 1], args.sampleTime);
		outputs[X3_OUTPUT].setVoltage(voltages[blockIndex * 4 + 2]);
		setVoltageLight(X3_LIGHT, voltages[blockIndex * 4 + 2], args.sampleTime);
		outputs[Y_OUTPUT].setVoltage(voltages[blockIndex * 4 + 3]);
		setVoltageLight(Y_LIGHT, voltages[blockIndex * 4 + 3], args.sampleTime);
	}

	static int nextDejaVuState(int state, bool longPress) {
		if (state == DEJA_VU_OFF)
			return longPress ? DEJA_VU_LOCKED : DEJA_VU_ON;
		if (state == DEJA_VU_LOCKED)
			return DEJA_VU_ON;
		return longPress ? DEJA_VU_LOCKED : DEJA_VU_OFF;
	}

	static float dejaVuAmount(int state, float deja_vu) {
		// A DEJA VU amount of 0.5 locks the loop.
		return (state == DEJA_VU_LOCKED) ? 0.5f : (state == DEJA_VU_ON) ? deja_vu : 0.f;
	}

	static float dejaVuLight(int state, float pulse) {
		// Locked deja vu pulses.
		return (state == DEJA_VU_LOCKED) ? pulse : (state == DEJA_VU_ON) ? 1.f : 0.f;
	}

	// The hardware drives each bicolor X/Y LED with a current proportional to the output voltage: green when positive, red when negative.
	void setVoltageLight(int lightId, float voltage, float sampleTime) {
		lights[lightId + 0].setSmoothBrightness(clamp(voltage / 5.f, 0.f, 1.f), sampleTime);
		lights[lightId + 1].setSmoothBrightness(clamp(-voltage / 5.f, 0.f, 1.f), sampleTime);
	}

	void stepBlock() {
		// Ramps

		marbles::Ramps ramps;
		ramps.master = ramp_master;
		ramps.external = ramp_external;
		ramps.slave[0] = ramp_slave[0];
		ramps.slave[1] = ramp_slave[1];

		float deja_vu = clamp(params[DEJA_VU_PARAM].getValue() + inputs[DEJA_VU_INPUT].getVoltage() / 5.f, 0.f, 1.f);
		static const int loop_length[] = {
			1, 1, 1, 2, 2,
			2, 2, 2, 3, 3,
			3, 3, 4, 4, 4,
			4, 4, 5, 5, 6,
			6, 6, 7, 7, 8,
			8, 8, 10, 10, 12,
			12, 12, 14, 14, 16,
			16
		};
		float deja_vu_length_index = params[DEJA_VU_LENGTH_PARAM].getValue() * (LENGTHOF(loop_length) - 1);
		int deja_vu_length = loop_length[(int) roundf(deja_vu_length_index)];

		// Set up TGenerator

		bool t_external_clock = inputs[T_CLOCK_INPUT].isConnected();

		t_generator.set_model((marbles::TGeneratorModel) t_mode);
		t_generator.set_range((marbles::TGeneratorRange) t_range);
		float t_rate = 60.f * (params[T_RATE_PARAM].getValue() + inputs[T_RATE_INPUT].getVoltage() / 5.f);
		t_generator.set_rate(t_rate);
		float t_bias = clamp(params[T_BIAS_PARAM].getValue() + inputs[T_BIAS_INPUT].getVoltage() / 5.f, 0.f, 1.f);
		t_generator.set_bias(t_bias);
		float t_jitter = clamp(params[T_JITTER_PARAM].getValue() + (explicitReset ? 0.f : inputs[T_JITTER_INPUT].getVoltage() / 5.f), 0.f, 1.f);
		t_generator.set_jitter(t_jitter);
		t_generator.set_deja_vu(dejaVuAmount(t_deja_vu, deja_vu));
		t_generator.set_length(deja_vu_length);
		t_generator.set_pulse_width_mean(params[GATE_BIAS_PARAM].getValue());
		t_generator.set_pulse_width_std(params[GATE_JITTER_PARAM].getValue());

		bool tReset = tResetPending;
		tResetPending = false;
		t_generator.Process(t_external_clock, &tReset, t_clocks, ramps, gates, BLOCK_SIZE);

		// Set up XYGenerator

		marbles::ClockSource x_clock_source = (marbles::ClockSource) x_clock_source_internal;
		if (inputs[X_CLOCK_INPUT].isConnected()) {
			x_clock_source = marbles::CLOCK_SOURCE_EXTERNAL;
			// As on hardware, a T output patched into X CLOCK is detected from matching edges
			// (after several clocks) and X then follows that T output's exact internal ramp.
			// This also covers indirect routes, and avoids querying Rack's engine lock from the audio thread.
			size_t bestScore = 8;
			for (int i = 0; i < 3; i++) {
				size_t score = selfPatchingDetector[i].Process(&clockHistory, BLOCK_SIZE);
				if (score >= bestScore) {
					x_clock_source = marbles::ClockSource(marbles::CLOCK_SOURCE_INTERNAL_T1 + i);
					bestScore = score;
				}
			}
		}
		else {
			for (int i = 0; i < 3; i++)
				selfPatchingDetector[i].Init(i);
		}

		marbles::GroupSettings x;
		x.control_mode = (marbles::ControlMode) x_mode;
		x.voltage_range = (marbles::VoltageRange) x_range;
		// External mode samples the X SPREAD jack alone. Its input stage and default ADC calibration
		// map ±5 V to ±1 and clip beyond, so X reproduces the input voltage (1 V/oct).
		float note_cv = clamp(inputs[X_SPREAD_INPUT].getVoltage() / 5.f, -1.f, 1.f);
		float u = note_filter.Process(0.5f * (note_cv + 1.f));
		x.register_mode = external;
		x.register_value = u;

		// In external mode, firmware sets the SPREAD attenuverter to 12 o'clock (no CV), so the jack only feeds the register.
		float x_spread = clamp(params[X_SPREAD_PARAM].getValue() + (external ? 0.f : inputs[X_SPREAD_INPUT].getVoltage() / 5.f), 0.f, 1.f);
		x.spread = x_spread;
		float x_bias = clamp(params[X_BIAS_PARAM].getValue() + inputs[X_BIAS_INPUT].getVoltage() / 5.f, 0.f, 1.f);
		x.bias = x_bias;
		float x_steps = clamp(params[X_STEPS_PARAM].getValue() + (explicitReset ? 0.f : inputs[X_STEPS_INPUT].getVoltage() / 5.f), 0.f, 1.f);
		x.steps = x_steps;
		x.deja_vu = dejaVuAmount(x_deja_vu, deja_vu);
		x.length = deja_vu_length;
		x.ratio.p = 1;
		x.ratio.q = 1;
		x.scale_index = x_scale;

		marbles::GroupSettings y;
		y.control_mode = marbles::CONTROL_MODE_IDENTICAL;
		y.register_mode = false;
		y.register_value = 0.0f;
		if (legacyYFollowsX) {
			y.voltage_range = x.voltage_range;
			y.spread = x_spread;
			y.bias = x_bias;
			y.steps = x_steps;
		}
		else {
			y.voltage_range = (marbles::VoltageRange) y_range;
			y.spread = params[Y_SPREAD_PARAM].getValue();
			y.bias = params[Y_BIAS_PARAM].getValue();
			y.steps = params[Y_STEPS_PARAM].getValue();
		}
		y.deja_vu = 0.0f;
		y.length = 1;
		static const marbles::Ratio y_divider_ratios[] = {
			{ 1, 64 },
			{ 1, 48 },
			{ 1, 32 },
			{ 1, 24 },
			{ 1, 16 },
			{ 1, 12 },
			{ 1, 8 },
			{ 1, 6 },
			{ 1, 4 },
			{ 1, 3 },
			{ 1, 2 },
			{ 1, 1 },
		};
		y.ratio = y_divider_ratios[clamp(int(std::round(params[Y_RATE_PARAM].getValue())), 0, 11)];
		y.scale_index = x_scale;
		if (yScaleLoadedFollowsX != yScaleFollowsX)
			loadYScales();

		bool xReset = xResetPending;
		xResetPending = false;
		if (x_clock_source != marbles::CLOCK_SOURCE_EXTERNAL)
			xReset |= tReset;
		xy_generator.Process(x_clock_source, x, y, &xReset, xy_clocks, ramps, voltages, BLOCK_SIZE);
	}
};


template <typename BASE>
struct CKD6Light : BASE {
	CKD6Light() {
		this->box.size = Vec(22, 22);
	}
};


struct MarblesWidget : ModuleWidget {
	int editMode = 0;

	MarblesWidget(Marbles* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Marbles.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<CKD6>(mm2px(Vec(16.545, 17.794)), module, Marbles::T_DEJA_VU_PARAM));
		addParam(createParamCentered<CKD6>(mm2px(Vec(74.845, 17.794)), module, Marbles::X_DEJA_VU_PARAM));
		addParam(createParamCentered<Rogan2PSWhite>(mm2px(Vec(45.695, 22.244)), module, Marbles::DEJA_VU_PARAM));
		addParam(createParamCentered<Rogan3PSWhite>(mm2px(Vec(23.467, 35.264)), module, Marbles::T_RATE_PARAM));
		addParam(createParamCentered<Rogan3PSWhite>(mm2px(Vec(67.945, 35.243)), module, Marbles::X_SPREAD_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(6.945, 38.794)), module, Marbles::T_MODE_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(84.445, 38.793)), module, Marbles::X_MODE_PARAM));
		addParam(createParamCentered<Rogan2PSWhite>(mm2px(Vec(45.695, 51.144)), module, Marbles::DEJA_VU_LENGTH_PARAM));
		addParam(createParamCentered<Rogan2PSWhite>(mm2px(Vec(9.545, 58.394)), module, Marbles::T_BIAS_PARAM));
		addParam(createParamCentered<Rogan2PSWhite>(mm2px(Vec(81.844, 58.394)), module, Marbles::X_BIAS_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(26.644, 59.694)), module, Marbles::T_RANGE_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(64.744, 59.694)), module, Marbles::X_RANGE_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(45.694, 67.294)), module, Marbles::EXTERNAL_PARAM));
		addParam(createParamCentered<Rogan2PSWhite>(mm2px(Vec(31.544, 73.694)), module, Marbles::T_JITTER_PARAM));
		addParam(createParamCentered<Rogan2PSWhite>(mm2px(Vec(59.845, 73.694)), module, Marbles::X_STEPS_PARAM));

		{
			ParamWidget* knob = createParamCentered<Rogan3PSGreen>(mm2px(Vec(23.467, 35.264)), module, Marbles::Y_RATE_PARAM);
			knob->hide();
			addParam(knob);
		}

		{
			ParamWidget* knob = createParamCentered<Rogan3PSGreen>(mm2px(Vec(67.945, 35.243)), module, Marbles::Y_SPREAD_PARAM);
			knob->hide();
			addParam(knob);
		}

		{
			ParamWidget* knob = createParamCentered<Rogan2PSGreen>(mm2px(Vec(81.844, 58.394)), module, Marbles::Y_BIAS_PARAM);
			knob->hide();
			addParam(knob);
		}

		{
			ParamWidget* knob = createParamCentered<Rogan2PSGreen>(mm2px(Vec(59.845, 73.694)), module, Marbles::Y_STEPS_PARAM);
			knob->hide();
			addParam(knob);
		}

		{
			ParamWidget* knob = createParamCentered<Rogan2PSRed>(mm2px(Vec(9.545, 58.394)), module, Marbles::GATE_BIAS_PARAM);
			knob->hide();
			addParam(knob);
		}

		{
			ParamWidget* knob = createParamCentered<Rogan2PSRed>(mm2px(Vec(31.544, 73.694)), module, Marbles::GATE_JITTER_PARAM);
			knob->hide();
			addParam(knob);
		}


		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.545, 81.944)), module, Marbles::T_BIAS_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(81.844, 81.944)), module, Marbles::X_BIAS_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.545, 96.544)), module, Marbles::T_CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(21.595, 96.544)), module, Marbles::T_RATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(33.644, 96.544)), module, Marbles::T_JITTER_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(45.695, 96.544)), module, Marbles::DEJA_VU_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(57.745, 96.544)), module, Marbles::X_STEPS_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(69.795, 96.544)), module, Marbles::X_SPREAD_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(81.844, 96.544)), module, Marbles::X_CLOCK_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(9.545, 111.144)), module, Marbles::T1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(21.595, 111.144)), module, Marbles::T2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.644, 111.144)), module, Marbles::T3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(45.695, 111.144)), module, Marbles::Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(57.745, 111.144)), module, Marbles::X1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(69.795, 111.144)), module, Marbles::X2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(81.844, 111.144)), module, Marbles::X3_OUTPUT));

		addChild(createLightCentered<CKD6Light<GreenLight>>(mm2px(Vec(16.545, 17.794)), module, Marbles::T_DEJA_VU_LIGHT));
		addChild(createLightCentered<CKD6Light<GreenLight>>(mm2px(Vec(74.845, 17.794)), module, Marbles::X_DEJA_VU_LIGHT));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(6.944, 29.894)), module, Marbles::T_MODE_LIGHTS));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(84.444, 29.894)), module, Marbles::X_MODE_LIGHTS));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(26.644, 53.994)), module, Marbles::T_RANGE_LIGHTS));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(64.744, 53.994)), module, Marbles::X_RANGE_LIGHTS));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(45.695, 76.194)), module, Marbles::EXTERNAL_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(6.044, 104.794)), module, Marbles::T1_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(18.094, 104.794)), module, Marbles::T2_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(30.145, 104.794)), module, Marbles::T3_LIGHT));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(42.194, 104.794)), module, Marbles::Y_LIGHT));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(54.244, 104.794)), module, Marbles::X1_LIGHT));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(66.294, 104.794)), module, Marbles::X2_LIGHT));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(78.344, 104.794)), module, Marbles::X3_LIGHT));
	}

	void setEditMode(int mode) {
		if (!module)
			return;
		editMode = mode;
		const int normal[] = {Marbles::T_RATE_PARAM, Marbles::X_SPREAD_PARAM,
			Marbles::X_BIAS_PARAM, Marbles::X_STEPS_PARAM, Marbles::T_BIAS_PARAM, Marbles::T_JITTER_PARAM};
		const int alternate[] = {Marbles::Y_RATE_PARAM, Marbles::Y_SPREAD_PARAM,
			Marbles::Y_BIAS_PARAM, Marbles::Y_STEPS_PARAM, Marbles::GATE_BIAS_PARAM, Marbles::GATE_JITTER_PARAM};
		for (int i = 0; i < 6; i++) {
			bool editing = editMode == (i < 4 ? 1 : 2);
			getParam(normal[i])->visible = !editing;
			getParam(alternate[i])->visible = editing;
		}
	}

	void appendContextMenu(Menu* menu) override {
		Marbles* module = dynamic_cast<Marbles*>(this->module);

		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Knob edit mode", {"Default", "Y generator", "Gate"},
			[=]() {return editMode;},
			[=](int mode) {setEditMode(mode);}
		));

		menu->addChild(createIndexPtrSubmenuItem("T mode", {
			"Complementary Bernoulli",
			"Clusters",
			"Drums",
			"Independent Bernoulli",
			"Divider",
			"Three states",
			"Markov",
		}, &module->t_mode));

		menu->addChild(createIndexPtrSubmenuItem("T range", {
			"1/4x",
			"1x",
			"4x",
		}, &module->t_range));

		menu->addChild(createIndexPtrSubmenuItem("X mode", {
			"Identical",
			"Bump",
			"Tilt",
		}, &module->x_mode));

		menu->addChild(createIndexPtrSubmenuItem("X range", {
			"Narrow",
			"Positive",
			"Full",
		}, &module->x_range));

		menu->addChild(createIndexPtrSubmenuItem("Scales", {
			"Major",
			"Minor",
			"Pentatonic",
			"Pelog",
			"Raag Bhairav That",
			"Raag Shri",
		}, &module->x_scale));

		menu->addChild(createIndexPtrSubmenuItem("Internal X clock source", {
			"T₁ → X₁, T₂ → X₂, T₃ → X₃",
			"T₁ → X₁, X₂, X₃",
			"T₂ → X₁, X₂, X₃",
			"T₃ → X₁, X₂, X₃",
		}, &module->x_clock_source_internal));

		menu->addChild(createIndexSubmenuItem("Y divider ratio", {
			"1/64",
			"1/48",
			"1/32",
			"1/24",
			"1/16",
			"1/12",
			"1/8",
			"1/6",
			"1/4",
			"1/3",
			"1/2",
			"1",
		},
			[=]() {return int(std::round(module->params[Marbles::Y_RATE_PARAM].getValue()));},
			[=](int value) {module->getParamQuantity(Marbles::Y_RATE_PARAM)->setValue(value);}
		));

		menu->addChild(createIndexPtrSubmenuItem("Y range", {
			"Narrow",
			"Positive",
			"Full",
		}, &module->y_range));

		menu->addChild(createIndexSubmenuItem("Y scale", {
			"Octaves (hardware)",
			"Same as X",
		},
			[=]() {return module->yScaleFollowsX ? 1 : 0;},
			[=](int value) {module->yScaleFollowsX = value == 1;}
		));

		menu->addChild(createBoolPtrMenuItem("Legacy Y follows X range and knobs", "", &module->legacyYFollowsX));

		menu->addChild(createBoolPtrMenuItem("Jitter/Steps CV inputs reset T/X", "", &module->explicitReset));

		for (int paramId : {Marbles::GATE_BIAS_PARAM, Marbles::GATE_JITTER_PARAM,
			Marbles::Y_SPREAD_PARAM, Marbles::Y_BIAS_PARAM, Marbles::Y_STEPS_PARAM}) {
			ui::Slider* slider = new ui::Slider;
			slider->quantity = module->getParamQuantity(paramId);
			slider->box.size.x = 200.f;
			menu->addChild(slider);
		}
	}
};


Model* modelMarbles = createModel<Marbles, MarblesWidget>("Marbles");
