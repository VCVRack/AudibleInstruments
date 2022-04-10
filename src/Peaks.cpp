#include "plugin.hpp"
#include "peaks/processors.h"

enum SwitchIndex {
	SWITCH_TWIN_MODE,
	SWITCH_FUNCTION,
	SWITCH_GATE_TRIG_1,
	SWITCH_GATE_TRIG_2
};

enum EditMode {
	EDIT_MODE_TWIN,
	EDIT_MODE_SPLIT,
	EDIT_MODE_FIRST,
	EDIT_MODE_SECOND,
	EDIT_MODE_LAST
};

enum Function {
	FUNCTION_ENVELOPE,
	FUNCTION_LFO,
	FUNCTION_TAP_LFO,
	FUNCTION_DRUM_GENERATOR,
	FUNCTION_MINI_SEQUENCER,
	FUNCTION_PULSE_SHAPER,
	FUNCTION_PULSE_RANDOMIZER,
	FUNCTION_FM_DRUM_GENERATOR,
	FUNCTION_LAST,
	FUNCTION_FIRST_ALTERNATE_FUNCTION = FUNCTION_MINI_SEQUENCER
};

struct Settings {
	uint8_t edit_mode;
	uint8_t function[2];
	uint8_t pot_value[8];
	bool snap_mode;
};


static const size_t kNumBlocks = 2;
static const size_t kNumChannels = 2;
static const size_t kBlockSize = 4;
static const int32_t kLongPressDuration = 600;
static const uint8_t kNumAdcChannels = 4;
static const uint16_t kAdcThresholdUnlocked = 1 << (16 - 10);  // 10 bits
static const uint16_t kAdcThresholdLocked = 1 << (16 - 8);  // 8 bits


struct Peaks : Module {
	enum ParamIds {
		KNOB_1_PARAM,
		KNOB_2_PARAM,
		KNOB_3_PARAM,
		KNOB_4_PARAM,
		BUTTON_1_PARAM,
		BUTTON_2_PARAM,
		TRIG_1_PARAM,
		TRIG_2_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		GATE_1_INPUT,
		GATE_2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_1_OUTPUT,
		OUT_2_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		TRIG_1_LIGHT,
		TRIG_2_LIGHT,
		TWIN_MODE_LIGHT,
		FUNC_1_LIGHT,
		FUNC_2_LIGHT,
		FUNC_3_LIGHT,
		FUNC_4_LIGHT,
		NUM_LIGHTS
	};

	static const peaks::ProcessorFunction function_table_[FUNCTION_LAST][2];

	EditMode edit_mode_ = EDIT_MODE_TWIN;
	Function function_[2] = {FUNCTION_ENVELOPE, FUNCTION_ENVELOPE};
	Settings settings_;

	uint8_t pot_value_[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	bool snap_mode_ = false;
	bool snapped_[4] = {false, false, false, false};

	int32_t adc_lp_[kNumAdcChannels] = {0, 0, 0, 0};
	int32_t adc_value_[kNumAdcChannels] = {0, 0, 0, 0};
	int32_t adc_threshold_[kNumAdcChannels] = {0, 0, 0, 0};
	long long press_time_[2] = {0, 0};

	peaks::Processors processors[2];

	int16_t output[kBlockSize];
	int16_t brightness[kNumChannels] = {0, 0};

	dsp::SchmittTrigger switches_[2];

	peaks::GateFlags gate_flags[2] = {0, 0};

	dsp::SampleRateConverter<2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> outputBuffer;

	bool initNumberStation = false;

	struct Block {
		peaks::GateFlags input[kNumChannels][kBlockSize];
		uint16_t output[kNumChannels][kBlockSize];
	};

	struct Slice {
		Block* block;
		size_t frame_index;
	};

	Block block_[kNumBlocks];
	size_t io_frame_ = 0;
	size_t io_block_ = 0;
	size_t render_block_ = kNumBlocks / 2;

	struct EditModeParam : ParamQuantity {
		std::string getDisplayValueString() override {
			if (module != nullptr) {
				Peaks* modulePeaks = static_cast<Peaks*>(module);

				if (paramId == BUTTON_1_PARAM) {
					int editMode = modulePeaks->settings_.edit_mode;
					switch (editMode) {
						case EDIT_MODE_SPLIT: return "Split";
						case EDIT_MODE_TWIN: return "Twin";
						case EDIT_MODE_FIRST: return "Expert (Out 1)";
						case EDIT_MODE_SECOND: return "Expert (Out 2)";
						default: return "";
					}
				}
				else {
					assert(false);
				}
			}
			else {
				return "";
			}
		}
	};

	static std::string functionLabelFromEnum(int function) {
		switch (function) {
			case FUNCTION_ENVELOPE: return "Envelope";
			case FUNCTION_LFO: return "LFO";
			case FUNCTION_TAP_LFO: return "Tap LFO";
			case FUNCTION_DRUM_GENERATOR: return "Drum generator";
			case FUNCTION_MINI_SEQUENCER: return "Sequencer (easter egg)";
			case FUNCTION_PULSE_SHAPER: return "Pulse Shaper (easter egg";
			case FUNCTION_PULSE_RANDOMIZER: return "Pulse randomizer (easter egg)";
			case FUNCTION_FM_DRUM_GENERATOR: return "Drum generate FM (easter egg)";
			default: return "";
		}
	}

	struct FunctionParam : ParamQuantity {
		std::string getDisplayValueString() override {
			if (module != nullptr) {
				Peaks* modulePeaks = static_cast<Peaks*>(module);

				if (paramId == BUTTON_2_PARAM) {
					int function1 = modulePeaks->settings_.function[0];
					int function2 = modulePeaks->settings_.function[1];
					if (function1 == function2) {
						return functionLabelFromEnum(function1);
					}
					else {
						return "1: " + functionLabelFromEnum(function1) + ", 2: " + functionLabelFromEnum(function2);
					}
				}
				else {
					assert(false);
				}
			}
			else {
				return "";
			}
		}
	};

	Peaks() {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(KNOB_1_PARAM, 0.0f, 65535.0f, 32678.0f, "Knob 1", "", 0.f, 1.f / 65535.f);
		configParam(KNOB_2_PARAM, 0.0f, 65535.0f, 32678.0f, "Knob 2", "", 0.f, 1.f / 65535.f);
		configParam(KNOB_3_PARAM, 0.0f, 65535.0f, 32678.0f, "Knob 3", "", 0.f, 1.f / 65535.f);
		configParam(KNOB_4_PARAM, 0.0f, 65535.0f, 32678.0f, "Knob 4", "", 0.f, 1.f / 65535.f);
		configButton<EditModeParam>(BUTTON_1_PARAM, "Edit Mode");
		configButton<FunctionParam>(BUTTON_2_PARAM, "Function");
		configButton(TRIG_1_PARAM, "Trigger 1");
		configButton(TRIG_2_PARAM, "Trigger 2");

		settings_.edit_mode = EDIT_MODE_TWIN;
		settings_.function[0] = FUNCTION_ENVELOPE;
		settings_.function[1] = FUNCTION_ENVELOPE;
		settings_.snap_mode = false;
		std::fill(&settings_.pot_value[0], &settings_.pot_value[8], 0);

		memset(&processors[0], 0, sizeof(processors[0]));
		memset(&processors[1], 0, sizeof(processors[1]));
		processors[0].Init(0);
		processors[1].Init(1);
	}

	void onReset() override {
		init();
	}

	void init() {
		std::fill(&pot_value_[0], &pot_value_[8], 0);
		std::fill(&press_time_[0], &press_time_[1], 0);
		std::fill(&brightness[0], &brightness[1], 0);
		std::fill(&adc_lp_[0], &adc_lp_[kNumAdcChannels], 0);
		std::fill(&adc_value_[0], &adc_value_[kNumAdcChannels], 0);
		std::fill(&adc_threshold_[0], &adc_threshold_[kNumAdcChannels], 0);
		std::fill(&snapped_[0], &snapped_[kNumAdcChannels], false);

		edit_mode_ = static_cast<EditMode>(settings_.edit_mode);
		function_[0] = static_cast<Function>(settings_.function[0]);
		function_[1] = static_cast<Function>(settings_.function[1]);
		std::copy(&settings_.pot_value[0], &settings_.pot_value[8], &pot_value_[0]);

		if (edit_mode_ == EDIT_MODE_FIRST || edit_mode_ == EDIT_MODE_SECOND) {
			lockPots();
			for (uint8_t i = 0; i < 4; ++i) {
				processors[0].set_parameter(
				  i,
				  static_cast<uint16_t>(pot_value_[i]) << 8);
				processors[1].set_parameter(
				  i,
				  static_cast<uint16_t>(pot_value_[i + 4]) << 8);
			}
		}

		snap_mode_ = settings_.snap_mode;

		changeControlMode();
		setFunction(0, function_[0]);
		setFunction(1, function_[1]);
	}

	void updateKnobDescriptions() {

		if (processors[0].function() == peaks::PROCESSOR_FUNCTION_NUMBER_STATION) {
			getParamQuantity(KNOB_1_PARAM)->description = "????";
			getParamQuantity(KNOB_2_PARAM)->description = "????";
			getParamQuantity(KNOB_3_PARAM)->description = "????";
			getParamQuantity(KNOB_4_PARAM)->description = "????";
			return;
		}

		if (edit_mode_ == EDIT_MODE_SPLIT) {
			switch (function_[0]) {
				case FUNCTION_ENVELOPE: {
					getParamQuantity(KNOB_1_PARAM)->description = "Ch. 1 Attack";
					getParamQuantity(KNOB_2_PARAM)->description = "Ch. 1 Decay";
					getParamQuantity(KNOB_3_PARAM)->description = "Ch. 2 Attack";
					getParamQuantity(KNOB_4_PARAM)->description = "Ch. 2 Decay";
					break;
				}
				case FUNCTION_LFO: {
					getParamQuantity(KNOB_1_PARAM)->description = "Ch. 1 Frequency";
					getParamQuantity(KNOB_2_PARAM)->description = "Ch. 1 Waveform";
					getParamQuantity(KNOB_3_PARAM)->description = "Ch. 2 Frequency";
					getParamQuantity(KNOB_4_PARAM)->description = "Ch. 2 Waveform";
					break;
				}
				case FUNCTION_TAP_LFO: {
					getParamQuantity(KNOB_1_PARAM)->description = "Ch. 1 Waveform";
					getParamQuantity(KNOB_2_PARAM)->description = "Ch. 1 Waveform variation";
					getParamQuantity(KNOB_3_PARAM)->description = "Ch. 2 Waveform";
					getParamQuantity(KNOB_4_PARAM)->description = "Ch. 2 Waveform variation";
					break;
				}
				case FUNCTION_DRUM_GENERATOR: {
					getParamQuantity(KNOB_1_PARAM)->description = "Ch. 1 BD Tone";
					getParamQuantity(KNOB_2_PARAM)->description = "Ch. 1 BD Decay";
					getParamQuantity(KNOB_3_PARAM)->description = "Ch. 2 SD Tone";
					getParamQuantity(KNOB_4_PARAM)->description = "Ch. 2 SD Snappy";
					break;
				}
				case FUNCTION_MINI_SEQUENCER: {
					getParamQuantity(KNOB_1_PARAM)->description = "Ch. 1 Step 1";
					getParamQuantity(KNOB_2_PARAM)->description = "Ch. 1 Step 2";
					getParamQuantity(KNOB_3_PARAM)->description = "Ch. 2 Step 1";
					getParamQuantity(KNOB_4_PARAM)->description = "Ch. 2 Step 2";
					break;
				}
				case FUNCTION_PULSE_SHAPER: {
					getParamQuantity(KNOB_1_PARAM)->description = "Ch. 1 Delay";
					getParamQuantity(KNOB_2_PARAM)->description = "Ch. 1 Number of repeats";
					getParamQuantity(KNOB_3_PARAM)->description = "Ch. 2 Delay";
					getParamQuantity(KNOB_4_PARAM)->description = "Ch. 2 Number of repeats";
					break;
				}
				case FUNCTION_PULSE_RANDOMIZER: {
					getParamQuantity(KNOB_1_PARAM)->description = "Ch. 1 Acceptance/regeneration probability";
					getParamQuantity(KNOB_2_PARAM)->description = "Ch. 1 Delay";
					getParamQuantity(KNOB_3_PARAM)->description = "Ch. 2 Acceptance/regeneration probability";
					getParamQuantity(KNOB_4_PARAM)->description = "Ch. 2 Delay";
					break;
				}
				case FUNCTION_FM_DRUM_GENERATOR: {
					getParamQuantity(KNOB_1_PARAM)->description = "Ch. 1 BD presets morphing";
					getParamQuantity(KNOB_2_PARAM)->description = "Ch. 1 BD presets variations";
					getParamQuantity(KNOB_3_PARAM)->description = "Ch. 2 SD presets morphing";
					getParamQuantity(KNOB_4_PARAM)->description = "Ch. 2 SD presets variations";
					break;
				}
				default: break;
			}
		}
		else {

			int currentFunction = -1;
			// same for both
			if (edit_mode_ == EDIT_MODE_TWIN) {
				currentFunction = function_[0]; 	// == function_[1]
			}
			// if expert, pick the active set of labels
			else if (edit_mode_ == EDIT_MODE_FIRST || edit_mode_ == EDIT_MODE_SECOND) {
				currentFunction = function_[edit_mode_ - EDIT_MODE_FIRST];
			}
			else {
				return;
			}

			std::string channelText = (edit_mode_ == EDIT_MODE_TWIN) ? "Ch. 1&2 " : string::f("Ch. %d ", edit_mode_ - EDIT_MODE_FIRST + 1);

			switch (currentFunction) {
				case FUNCTION_ENVELOPE: {
					getParamQuantity(KNOB_1_PARAM)->description = channelText + "Attack";
					getParamQuantity(KNOB_2_PARAM)->description = channelText + "Decay";
					getParamQuantity(KNOB_3_PARAM)->description = channelText + "Sustain";
					getParamQuantity(KNOB_4_PARAM)->description = channelText + "Release";
					break;
				}
				case FUNCTION_LFO: {
					getParamQuantity(KNOB_1_PARAM)->description = channelText + "Frequency";
					getParamQuantity(KNOB_2_PARAM)->description = channelText + "Waveform (sine, linear slope, square, steps, random)";
					getParamQuantity(KNOB_3_PARAM)->description = channelText + "Waveform variation (wavefolder for sine; ascending/triangle/descending balance for slope, pulse-width for square, number of steps, and interpolation method)";
					getParamQuantity(KNOB_4_PARAM)->description = channelText + "Phase on restart";
					break;
				}
				case FUNCTION_TAP_LFO: {
					getParamQuantity(KNOB_1_PARAM)->description = channelText + "Amplitude";
					getParamQuantity(KNOB_2_PARAM)->description = channelText + "Waveform";
					getParamQuantity(KNOB_3_PARAM)->description = channelText + "Waveform variation";
					getParamQuantity(KNOB_4_PARAM)->description = channelText + "Phase on restart";
					break;
				}
				case FUNCTION_DRUM_GENERATOR: {
					getParamQuantity(KNOB_1_PARAM)->description = channelText + "Base frequency";
					getParamQuantity(KNOB_2_PARAM)->description = channelText + "Frequency modulation (“Punch” for BD, “Tone” for SD)";
					getParamQuantity(KNOB_3_PARAM)->description = channelText + "High-frequency content (“Tone” for BD, “Snappy” for SD)";
					getParamQuantity(KNOB_4_PARAM)->description = channelText + "Decay";
					break;
				}
				case FUNCTION_MINI_SEQUENCER: {
					getParamQuantity(KNOB_1_PARAM)->description = channelText + "Step 1";
					getParamQuantity(KNOB_2_PARAM)->description = channelText + "Step 2";
					getParamQuantity(KNOB_3_PARAM)->description = channelText + "Step 3";
					getParamQuantity(KNOB_4_PARAM)->description = channelText + "Step 4";
					break;
				}
				case FUNCTION_PULSE_SHAPER: {
					getParamQuantity(KNOB_1_PARAM)->description = channelText + "Pre-delay";
					getParamQuantity(KNOB_2_PARAM)->description = channelText + "Gate duration";
					getParamQuantity(KNOB_3_PARAM)->description = channelText + "Delay";
					getParamQuantity(KNOB_4_PARAM)->description = channelText + "Number of repeats";
					break;
				}
				case FUNCTION_PULSE_RANDOMIZER: {
					getParamQuantity(KNOB_1_PARAM)->description = channelText + "Probability that an incoming trigger is processed";
					getParamQuantity(KNOB_2_PARAM)->description = channelText + "Probability that the trigger is regenerated after the delay";
					getParamQuantity(KNOB_3_PARAM)->description = channelText + "Delay time";
					getParamQuantity(KNOB_4_PARAM)->description = channelText + "Jitter";
					break;
				}
				case FUNCTION_FM_DRUM_GENERATOR: {
					getParamQuantity(KNOB_1_PARAM)->description = channelText + "Frequency";
					getParamQuantity(KNOB_2_PARAM)->description = channelText + "FM intensity";
					getParamQuantity(KNOB_3_PARAM)->description = channelText + "FM and AM envelope decay time (the FM envelope has a shorter decay than the AM envelope, but the two values are tied to this parameter)";
					getParamQuantity(KNOB_4_PARAM)->description = channelText + "Color. At 12 o'clock, no modification is brought to the oscillator signal. Turn right to increase the amount of noise (for snares). Turn left to increase the amount of distortion (for 909 style kicks).";
					break;
				}
				default: break;
			}
		}

	}

	json_t* dataToJson() override {

		saveState();

		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "edit_mode", json_integer((int)settings_.edit_mode));
		json_object_set_new(rootJ, "fcn_channel_1", json_integer((int)settings_.function[0]));
		json_object_set_new(rootJ, "fcn_channel_2", json_integer((int)settings_.function[1]));

		json_t* potValuesJ = json_array();
		for (int p : pot_value_) {
			json_t* pJ = json_integer(p);
			json_array_append_new(potValuesJ, pJ);
		}
		json_object_set_new(rootJ, "pot_values", potValuesJ);

		json_object_set_new(rootJ, "snap_mode", json_boolean(settings_.snap_mode));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* editModeJ = json_object_get(rootJ, "edit_mode");
		if (editModeJ) {
			settings_.edit_mode = static_cast<EditMode>(json_integer_value(editModeJ));
		}

		json_t* fcnChannel1J = json_object_get(rootJ, "fcn_channel_1");
		if (fcnChannel1J) {
			settings_.function[0] = static_cast<Function>(json_integer_value(fcnChannel1J));
		}

		json_t* fcnChannel2J = json_object_get(rootJ, "fcn_channel_2");
		if (fcnChannel2J) {
			settings_.function[1] = static_cast<Function>(json_integer_value(fcnChannel2J));
		}

		json_t* snapModeJ = json_object_get(rootJ, "snap_mode");
		if (snapModeJ) {
			settings_.snap_mode = json_boolean_value(snapModeJ);
		}

		json_t* potValuesJ = json_object_get(rootJ, "pot_values");
		size_t potValueId;
		json_t* pJ;
		json_array_foreach(potValuesJ, potValueId, pJ) {
			if (potValueId < sizeof(pot_value_) / sizeof(pot_value_)[0]) {
				settings_.pot_value[potValueId] = json_integer_value(pJ);
			}
		}

		// Update module internal state from settings.
		init();
	}

	void process(const ProcessArgs& args) override {
		poll();
		pollPots();
		updateKnobDescriptions();

		// Initialize "secret" number station mode.
		if (initNumberStation) {
			processors[0].set_function(peaks::PROCESSOR_FUNCTION_NUMBER_STATION);
			processors[1].set_function(peaks::PROCESSOR_FUNCTION_NUMBER_STATION);
			initNumberStation = false;
		}

		if (outputBuffer.empty()) {

			while (render_block_ != io_block_) {
				process(&block_[render_block_], kBlockSize);
				render_block_ = (render_block_ + 1) % kNumBlocks;
			}

			uint32_t external_gate_inputs = 0;
			external_gate_inputs |= (inputs[GATE_1_INPUT].getVoltage() ? 1 : 0);
			external_gate_inputs |= (inputs[GATE_2_INPUT].getVoltage() ? 2 : 0);

			uint32_t buttons = 0;
			buttons |= (params[TRIG_1_PARAM].getValue() ? 1 : 0);
			buttons |= (params[TRIG_2_PARAM].getValue() ? 2 : 0);

			uint32_t gate_inputs = external_gate_inputs | buttons;

			// Prepare sample rate conversion.
			// Peaks is sampling at 48kHZ.
			outputSrc.setRates(48000, args.sampleRate);
			int inLen = kBlockSize;
			int outLen = outputBuffer.capacity();
			dsp::Frame<2> f[kBlockSize];

			// Process an entire block of data from the IOBuffer.
			for (size_t k = 0; k < kBlockSize; ++k) {

				Slice slice = NextSlice(1);

				for (size_t i = 0; i < kNumChannels; ++i) {
					gate_flags[i] = peaks::ExtractGateFlags(
					                  gate_flags[i],
					                  gate_inputs & (1 << i));

					f[k].samples[i] = slice.block->output[i][slice.frame_index];
				}

				// A hack to make channel 1 aware of what's going on in channel 2. Used to
				// reset the sequencer.
				slice.block->input[0][slice.frame_index] = gate_flags[0] | (gate_flags[1] << 4) | (buttons & 8 ? peaks::GATE_FLAG_FROM_BUTTON : 0);

				slice.block->input[1][slice.frame_index] = gate_flags[1] | (buttons & 2 ? peaks::GATE_FLAG_FROM_BUTTON : 0);
			}

			outputSrc.process(f, &inLen, outputBuffer.endData(), &outLen);
			outputBuffer.endIncr(outLen);
		}

		// Update outputs.
		if (!outputBuffer.empty()) {
			dsp::Frame<2> f = outputBuffer.shift();

			// Peaks manual says output spec is 0..8V for envelopes and 10Vpp for audio/CV.
			// TODO Check the output values against an actual device.
			outputs[OUT_1_OUTPUT].setVoltage(rescale(static_cast<float>(f.samples[0]), 0.0f, 65535.f, -8.0f, 8.0f));
			outputs[OUT_2_OUTPUT].setVoltage(rescale(static_cast<float>(f.samples[1]), 0.0f, 65535.f, -8.0f, 8.0f));
		}
	}

	inline Slice NextSlice(size_t size) {
		Slice s;
		s.block = &block_[io_block_];
		s.frame_index = io_frame_;
		io_frame_ += size;
		if (io_frame_ >= kBlockSize) {
			io_frame_ -= kBlockSize;
			io_block_ = (io_block_ + 1) % kNumBlocks;
		}
		return s;
	}

	inline Function function() const {
		return edit_mode_ == EDIT_MODE_SECOND ? function_[1] : function_[0];
	}

	inline void set_led_brightness(int channel, int16_t value) {
		brightness[channel] = value;
	}

	inline void process(Block* block, size_t size) {
		for (size_t i = 0; i < kNumChannels; ++i) {
			processors[i].Process(block->input[i], output, size);
			set_led_brightness(i, output[0]);
			for (size_t j = 0; j < size; ++j) {
				// From calibration_data.h, shifting signed to unsigned values.
				int32_t shifted_value = 32767 + static_cast<int32_t>(output[j]);
				CONSTRAIN(shifted_value, 0, 65535);
				block->output[i][j] = static_cast<uint16_t>(shifted_value);
			}
		}
	}

	void changeControlMode();
	void setFunction(uint8_t index, Function f);
	void onPotChanged(uint16_t id, uint16_t value);
	void onSwitchReleased(uint16_t id, uint16_t data);
	void saveState();
	void lockPots();
	void poll();
	void pollPots();
	void refreshLeds();

	long long getSystemTimeMs();
};

const peaks::ProcessorFunction Peaks::function_table_[FUNCTION_LAST][2] = {
	{ peaks::PROCESSOR_FUNCTION_ENVELOPE, peaks::PROCESSOR_FUNCTION_ENVELOPE },
	{ peaks::PROCESSOR_FUNCTION_LFO, peaks::PROCESSOR_FUNCTION_LFO },
	{ peaks::PROCESSOR_FUNCTION_TAP_LFO, peaks::PROCESSOR_FUNCTION_TAP_LFO },
	{ peaks::PROCESSOR_FUNCTION_BASS_DRUM, peaks::PROCESSOR_FUNCTION_SNARE_DRUM },

	{ peaks::PROCESSOR_FUNCTION_MINI_SEQUENCER, peaks::PROCESSOR_FUNCTION_MINI_SEQUENCER },
	{ peaks::PROCESSOR_FUNCTION_PULSE_SHAPER, peaks::PROCESSOR_FUNCTION_PULSE_SHAPER },
	{ peaks::PROCESSOR_FUNCTION_PULSE_RANDOMIZER, peaks::PROCESSOR_FUNCTION_PULSE_RANDOMIZER },
	{ peaks::PROCESSOR_FUNCTION_FM_DRUM, peaks::PROCESSOR_FUNCTION_FM_DRUM },
};


void Peaks::changeControlMode() {
	uint16_t parameters[4];
	for (int i = 0; i < 4; ++i) {
		parameters[i] = adc_value_[i];
	}

	if (edit_mode_ == EDIT_MODE_SPLIT) {
		processors[0].CopyParameters(&parameters[0], 2);
		processors[1].CopyParameters(&parameters[2], 2);
		processors[0].set_control_mode(peaks::CONTROL_MODE_HALF);
		processors[1].set_control_mode(peaks::CONTROL_MODE_HALF);
	}
	else if (edit_mode_ == EDIT_MODE_TWIN) {
		processors[0].CopyParameters(&parameters[0], 4);
		processors[1].CopyParameters(&parameters[0], 4);
		processors[0].set_control_mode(peaks::CONTROL_MODE_FULL);
		processors[1].set_control_mode(peaks::CONTROL_MODE_FULL);
	}
	else {
		processors[0].set_control_mode(peaks::CONTROL_MODE_FULL);
		processors[1].set_control_mode(peaks::CONTROL_MODE_FULL);
	}
}

void Peaks::setFunction(uint8_t index, Function f) {
	if (edit_mode_ == EDIT_MODE_SPLIT || edit_mode_ == EDIT_MODE_TWIN) {
		function_[0] = function_[1] = f;
		processors[0].set_function(function_table_[f][0]);
		processors[1].set_function(function_table_[f][1]);
	}
	else {
		function_[index] = f;
		processors[index].set_function(function_table_[f][index]);
	}
}

void Peaks::onSwitchReleased(uint16_t id, uint16_t data) {
	switch (id) {
		case SWITCH_TWIN_MODE:
			if (data > kLongPressDuration) {
				edit_mode_ = static_cast<EditMode>(
				               (edit_mode_ + EDIT_MODE_FIRST) % EDIT_MODE_LAST);
				function_[0] = function_[1];
				processors[0].set_function(function_table_[function_[0]][0]);
				processors[1].set_function(function_table_[function_[0]][1]);
				lockPots();
			}
			else {
				if (edit_mode_ <= EDIT_MODE_SPLIT) {
					edit_mode_ = static_cast<EditMode>(EDIT_MODE_SPLIT - edit_mode_);
				}
				else {
					edit_mode_ = static_cast<EditMode>(EDIT_MODE_SECOND - (edit_mode_ & 1));
					lockPots();
				}
			}
			changeControlMode();
			saveState();
			break;

		case SWITCH_FUNCTION: {
			Function f = function();
			if (data > kLongPressDuration) {
				f = static_cast<Function>((f + FUNCTION_FIRST_ALTERNATE_FUNCTION) % FUNCTION_LAST);
			}
			else {
				if (f <= FUNCTION_DRUM_GENERATOR) {
					f = static_cast<Function>((f + 1) & 3);
				}
				else {
					f = static_cast<Function>(((f + 1) & 3) + FUNCTION_FIRST_ALTERNATE_FUNCTION);
				}
			}
			setFunction(edit_mode_ - EDIT_MODE_FIRST, f);
			saveState();
		}
		break;

		case SWITCH_GATE_TRIG_1:
			// no-op
			break;

		case SWITCH_GATE_TRIG_2:
			// no-op
			break;
	}
}

void Peaks::lockPots() {
	std::fill(
	  &adc_threshold_[0],
	  &adc_threshold_[kNumAdcChannels],
	  kAdcThresholdLocked);
	std::fill(&snapped_[0], &snapped_[kNumAdcChannels], false);
}

void Peaks::pollPots() {
	for (uint8_t i = 0; i < kNumAdcChannels; ++i) {
		adc_lp_[i] = (int32_t(params[KNOB_1_PARAM + i].getValue()) + adc_lp_[i] * 7) >> 3;
		int32_t value = adc_lp_[i];
		int32_t current_value = adc_value_[i];
		if (value >= current_value + adc_threshold_[i] ||
		    value <= current_value - adc_threshold_[i] ||
		    !adc_threshold_[i]) {
			onPotChanged(i, value);
			adc_value_[i] = value;
			adc_threshold_[i] = kAdcThresholdUnlocked;
		}
	}
}

void Peaks::onPotChanged(uint16_t id, uint16_t value) {
	switch (edit_mode_) {
		case EDIT_MODE_TWIN:
			processors[0].set_parameter(id, value);
			processors[1].set_parameter(id, value);
			pot_value_[id] = value >> 8;
			break;

		case EDIT_MODE_SPLIT:
			if (id < 2) {
				processors[0].set_parameter(id, value);
			}
			else {
				processors[1].set_parameter(id - 2, value);
			}
			pot_value_[id] = value >> 8;
			break;

		case EDIT_MODE_FIRST:
		case EDIT_MODE_SECOND: {
			uint8_t index = id + (edit_mode_ - EDIT_MODE_FIRST) * 4;
			peaks::Processors* p = &processors[edit_mode_ - EDIT_MODE_FIRST];

			int16_t delta = static_cast<int16_t>(pot_value_[index]) - \
			                static_cast<int16_t>(value >> 8);
			if (delta < 0) {
				delta = -delta;
			}

			if (!snap_mode_ || snapped_[id] || delta <= 2) {
				p->set_parameter(id, value);
				pot_value_[index] = value >> 8;
				snapped_[id] = true;
			}
		}
		break;

		case EDIT_MODE_LAST:
			break;
	}
}

long long Peaks::getSystemTimeMs() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
	         std::chrono::steady_clock::now().time_since_epoch()
	       ).count();
}

void Peaks::poll() {
	for (uint8_t i = 0; i < 2; ++i) {
		if (switches_[i].process(params[BUTTON_1_PARAM + i].getValue())) {
			press_time_[i] = getSystemTimeMs();
		}

		if (switches_[i].isHigh() && press_time_[i] != 0) {
			int32_t pressed_time = getSystemTimeMs() - press_time_[i];
			if (pressed_time > kLongPressDuration) {
				onSwitchReleased(SWITCH_TWIN_MODE + i, pressed_time);
				press_time_[i] = 0;  // Inhibit next release event
			}
		}
		if (!switches_[i].isHigh() && press_time_[i] != 0) {
			int32_t delta = getSystemTimeMs() - press_time_[i] + 1;
			onSwitchReleased(SWITCH_TWIN_MODE + i, delta);
			press_time_[i] = 0; // Not in original code!
		}
	}

	refreshLeds();
}

void Peaks::saveState() {
	settings_.edit_mode = edit_mode_;
	settings_.function[0] = function_[0];
	settings_.function[1] = function_[1];
	std::copy(&pot_value_[0], &pot_value_[8], &settings_.pot_value[0]);
	settings_.snap_mode = snap_mode_;
}

void Peaks::refreshLeds() {
	uint8_t flash = (getSystemTimeMs() >> 7) & 7;
	switch (edit_mode_) {
		case EDIT_MODE_FIRST:
			lights[TWIN_MODE_LIGHT].value = (flash == 1) ? 1.0f : 0.0f;
			break;
		case EDIT_MODE_SECOND:
			lights[TWIN_MODE_LIGHT].value = (flash == 1 || flash == 3) ? 1.0f : 0.0f;
			break;
		default:
			lights[TWIN_MODE_LIGHT].value = (edit_mode_ & 1) ? 1.0f : 0.0f;
			break;
	}
	if ((getSystemTimeMs() & 256) && function() >= FUNCTION_FIRST_ALTERNATE_FUNCTION) {
		for (size_t i = 0; i < 4; ++i) {
			lights[FUNC_1_LIGHT + i].value = 0.0f;
		}
	}
	else {
		for (size_t i = 0; i < 4; ++i) {
			lights[FUNC_1_LIGHT + i].value = ((function() & 3) == i) ? 1.0f : 0.0f;
		}
	}

	uint8_t b[2];
	for (uint8_t i = 0; i < 2; ++i) {
		switch (function_[i]) {
			case FUNCTION_DRUM_GENERATOR:
			case FUNCTION_FM_DRUM_GENERATOR:
				b[i] = (int16_t) abs(brightness[i]) >> 8;
				b[i] = b[i] >= 255 ? 255 : b[i];
				break;
			case FUNCTION_LFO:
			case FUNCTION_TAP_LFO:
			case FUNCTION_MINI_SEQUENCER: {
				int32_t brightnessVal = int32_t(brightness[i]) * 409 >> 8;
				brightnessVal += 32768;
				brightnessVal >>= 8;
				CONSTRAIN(brightnessVal, 0, 255);
				b[i] = brightnessVal;
			}
			break;
			default:
				b[i] = brightness[i] >> 7;
				break;
		}
	}

	if (processors[0].function() == peaks::PROCESSOR_FUNCTION_NUMBER_STATION) {
		uint8_t pattern = processors[0].number_station().digit()
		                  ^ processors[1].number_station().digit();
		for (size_t i = 0; i < 4; ++i) {
			lights[FUNC_1_LIGHT + i].value = (pattern & 1) ? 1.0f : 0.0f;
			pattern = pattern >> 1;
		}
		b[0] = processors[0].number_station().gate() ? 255 : 0;
		b[1] = processors[1].number_station().gate() ? 255 : 0;
	}

	const float deltaTime = APP->engine->getSampleTime();
	lights[TRIG_1_LIGHT].setSmoothBrightness(rescale(static_cast<float>(b[0]), 0.0f, 255.0f, 0.0f, 1.0f), deltaTime);
	lights[TRIG_2_LIGHT].setSmoothBrightness(rescale(static_cast<float>(b[1]), 0.0f, 255.0f, 0.0f, 1.0f), deltaTime);
}

struct PeaksWidget : ModuleWidget {
	PeaksWidget(Peaks* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Peaks.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<TL1105>((Vec(8.5, 52)), module, Peaks::BUTTON_1_PARAM));
		addParam(createParam<TL1105>((Vec(8.5, 89)), module, Peaks::BUTTON_2_PARAM));

		addParam(createParamCentered<LEDBezel>(Vec(22, 200), module, Peaks::TRIG_1_PARAM));
		addParam(createParamCentered<LEDBezel>(Vec(22, 285), module, Peaks::TRIG_2_PARAM));

		addChild(createLightCentered<LEDBezelLight<GreenLight>>(Vec(22, 200), module, Peaks::TRIG_1_LIGHT));
		addChild(createLightCentered<LEDBezelLight<GreenLight>>(Vec(22, 285), module, Peaks::TRIG_2_LIGHT));
		addChild(createLight<MediumLight<GreenLight>>(Vec(11.88, 74), module, Peaks::TWIN_MODE_LIGHT));
		addChild(createLight<MediumLight<GreenLight>>(Vec(11.88, 111), module, Peaks::FUNC_1_LIGHT));
		addChild(createLight<MediumLight<GreenLight>>(Vec(11.88, 126.75), module, Peaks::FUNC_2_LIGHT));
		addChild(createLight<MediumLight<GreenLight>>(Vec(11.88, 142.5), module, Peaks::FUNC_3_LIGHT));
		addChild(createLight<MediumLight<GreenLight>>(Vec(11.88, 158), module, Peaks::FUNC_4_LIGHT));

		addParam(createParam<Rogan1PSWhite>(Vec(61, 51), module, Peaks::KNOB_1_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(61, 115), module, Peaks::KNOB_2_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(61, 179), module, Peaks::KNOB_3_PARAM));
		addParam(createParam<Rogan1PSWhite>(Vec(61, 244), module, Peaks::KNOB_4_PARAM));


		addInput(createInput<PJ301MPort>((Vec(10, 230)), module, Peaks::GATE_1_INPUT));
		addInput(createInput<PJ301MPort>((Vec(10, 315)), module, Peaks::GATE_2_INPUT));

		addOutput(createOutput<PJ301MPort>((Vec(53., 315.)), module, Peaks::OUT_1_OUTPUT));
		addOutput(createOutput<PJ301MPort>((Vec(86., 315.)), module, Peaks::OUT_2_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {

		menu->addChild(new MenuSeparator);
		Peaks* peaks = dynamic_cast<Peaks*>(this->module);


		menu->addChild(construct<MenuLabel>());

		menu->addChild(createBoolPtrMenuItem("Snap mode", "", &peaks->snap_mode_));

		menu->addChild(construct<MenuLabel>());
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Secret Modes"));

		menu->addChild(createBoolMenuItem("Number station", "",
		[ = ]() {
			return peaks->processors[0].function() == peaks::PROCESSOR_FUNCTION_NUMBER_STATION;
		},
		[ = ](bool val) {
			peaks->initNumberStation = true;
		}
		                                 ));
	}

};



Model* modelPeaks = createModel<Peaks, PeaksWidget>("Peaks");
