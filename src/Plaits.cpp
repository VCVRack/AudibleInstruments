#include "plugin.hpp"
#include <osdialog.h>
#include <atomic>
#include <mutex>

#pragma GCC diagnostic push
#ifndef __clang__
	#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include "plaits/dsp/voice.h"
#include "plaits/user_data_receiver.h"
#pragma GCC diagnostic pop


// Size of a custom data block, as stored in the hardware's flash.
static const size_t kUserDataSize = 4096;
// The wavetable engine can read a whole custom wave past the end of the block,
// so the voices get a zero-padded copy.
static const size_t kUserDataBufferSize = 4352;
// Hardware button long press time
static const float kLongPressTime = 2.f;


/** Reads the first channel of a PCM or float WAV file. Returns false if the file isn't one. */
static bool readWav(const std::vector<uint8_t>& file, std::vector<float>& samples, float& sampleRate) {
	auto u16 = [&](size_t i) {return (uint32_t) file[i] | (uint32_t) file[i + 1] << 8;};
	auto u32 = [&](size_t i) {return u16(i) | u16(i + 2) << 16;};
	if (file.size() < 12 || std::memcmp(&file[0], "RIFF", 4) || std::memcmp(&file[8], "WAVE", 4))
		return false;

	int format = 0;
	int channels = 0;
	int bits = 0;
	size_t pos = 12;
	while (pos + 8 <= file.size()) {
		size_t size = u32(pos + 4);
		size_t data = pos + 8;
		size_t end = std::min(data + size, file.size());
		if (!std::memcmp(&file[pos], "fmt ", 4) && size >= 16 && data + 16 <= file.size()) {
			format = u16(data);
			channels = u16(data + 2);
			sampleRate = u32(data + 4);
			bits = u16(data + 14);
			// WAVE_FORMAT_EXTENSIBLE keeps the format in its subformat GUID.
			if (format == 0xfffe && size >= 26 && data + 26 <= file.size())
				format = u16(data + 24);
		}
		else if (!std::memcmp(&file[pos], "data", 4) && channels > 0) {
			size_t bytes = bits / 8;
			bool isInt = (format == 1 && bytes >= 1 && bytes <= 4);
			bool isFloat = (format == 3 && bits == 32);
			if (!(isInt || isFloat) || !(sampleRate > 0.f))
				return false;
			size_t frameSize = bytes * channels;
			for (size_t i = data; i + frameSize <= end; i += frameSize) {
				float x;
				if (isFloat) {
					uint32_t u = u32(i);
					std::memcpy(&x, &u, 4);
				}
				else if (bytes == 1) {
					x = (file[i] - 128) / 128.f;
				}
				else {
					uint32_t u = 0;
					for (size_t b = 0; b < bytes; b++)
						u |= (uint32_t) file[i + b] << (8 * (b + 4 - bytes));
					x = (int32_t) u / 2147483648.f;
				}
				samples.push_back(x);
			}
			return true;
		}
		pos = data + size + (size & 1);
	}
	return false;
}


/** Decodes custom data from the Plaits editor's audio, as the firmware does from the TIMBRE input. */
static bool decodeUserData(const std::vector<float>& samples, float sampleRate, std::vector<uint8_t>& data) {
	data.assign(kUserDataSize, 0);
	plaits::UserDataReceiver receiver;
	receiver.Init(data.data(), kUserDataSize);
	// The firmware reads the CV once per 12-sample block at 48 kHz.
	double step = sampleRate / 4000.0;
	for (double t = 0.0; t + 1 < samples.size(); t += step) {
		size_t i = t;
		float x = crossfade(samples[i], samples[i + 1], (float) (t - i));
		if (receiver.Process(x) == stm_audio_bootloader::PACKET_DECODER_STATE_END_OF_TRANSMISSION) {
			if (receiver.progress() == 1.f)
				return true;
			receiver.Reset();
		}
	}
	return receiver.progress() == 1.f;
}


struct PlaitsFrequencyQuantity : ParamQuantity {
	float getDisplayValue() override;
	void setDisplayValue(float displayValue) override;
	std::string getUnit() override;
};


struct Plaits : Module {
	enum ParamIds {
		MODEL1_PARAM,
		MODEL2_PARAM,
		FREQ_PARAM,
		HARMONICS_PARAM,
		TIMBRE_PARAM,
		MORPH_PARAM,
		TIMBRE_CV_PARAM,
		FREQ_CV_PARAM,
		MORPH_CV_PARAM,
		LPG_COLOR_PARAM,
		LPG_DECAY_PARAM,
		FREQ_RANGE_PARAM,
		OCTAVES_ROOT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENGINE_INPUT,
		TIMBRE_INPUT,
		FREQ_INPUT,
		MORPH_INPUT,
		HARMONICS_INPUT,
		TRIGGER_INPUT,
		LEVEL_INPUT,
		NOTE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		AUX_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(MODEL_LIGHT, 8 * 3),
		NUM_LIGHTS
	};
	enum UiMode {
		UI_MODE_NORMAL,
		UI_MODE_DISPLAY_ALTERNATE_PARAMETERS,
		UI_MODE_DISPLAY_OCTAVE,
	};

	plaits::Voice voice[16];
	// Keep the original sixteen-engine CV quantization and trigger latching.
	stmlib::HysteresisQuantizer engineQuantizer[16];
	plaits::DelayLine<float, plaits::kMaxTriggerDelay> engineTriggerDelay[16];
	float engineTriggerBuffer[16][plaits::kMaxTriggerDelay] = {};
	float engineCv[16] = {};
	bool engineTriggerHigh[16] = {};
	plaits::Patch patch = {};
	char shared_buffer[16][16384] = {};
	float triPhase = 0.f;

	// With firmware 1.2, patch.engine uses its numbering, where the original sixteen engines start at 8.
	bool firmware12 = false;
	bool altNavigation = false;
	bool colorBlind = false;
	UiMode uiMode = UI_MODE_NORMAL;
	// Display for the knobs the widget swaps in, like holding a button. Not saved.
	UiMode editMode = UI_MODE_NORMAL;
	stmlib::HysteresisQuantizer2 octaveQuantizer;
	bool buttonPressed[2] = {};
	float pressTime[2] = {};
	bool ignoreRelease[2] = {};

	// Custom data for one firmware 1.2 engine, like the hardware's single flash page. Guarded by userDataMutex.
	std::vector<uint8_t> userData;
	int userDataEngine = -1;
	std::mutex userDataMutex;
	std::atomic<bool> userDataChanged{true};
	// Padded copy the voices read, owned by the audio thread.
	std::vector<uint8_t> voiceUserData;
	bool voiceUserDataEnabled = false;

	dsp::SampleRateConverter<16 * 2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<16 * 2>, 256> outputBuffer;
	bool lowCpu = false;

	dsp::BooleanTrigger model1Trigger;
	dsp::BooleanTrigger model2Trigger;

	Plaits() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configButton(MODEL1_PARAM, "Pitched models");
		configButton(MODEL2_PARAM, "Noise/percussive models");
		configParam<PlaitsFrequencyQuantity>(FREQ_PARAM, -4.0, 4.0, 0.0, "Frequency", " semitones", 0.f, 12.f);
		configParam(HARMONICS_PARAM, 0.0, 1.0, 0.5, "Harmonics", "%", 0.f, 100.f);
		configParam(TIMBRE_PARAM, 0.0, 1.0, 0.5, "Timbre", "%", 0.f, 100.f);
		configParam(LPG_COLOR_PARAM, 0.0, 1.0, 0.5, "Lowpass gate response", "%", 0.f, 100.f);
		configParam(MORPH_PARAM, 0.0, 1.0, 0.5, "Morph", "%", 0.f, 100.f);
		configParam(LPG_DECAY_PARAM, 0.0, 1.0, 0.5, "Lowpass gate decay", "%", 0.f, 100.f);
		configParam(TIMBRE_CV_PARAM, -1.0, 1.0, 0.0, "Timbre CV");
		configParam(FREQ_CV_PARAM, -1.0, 1.0, 0.0, "Frequency CV");
		configParam(MORPH_CV_PARAM, -1.0, 1.0, 0.0, "Morph CV");
		configSwitch(FREQ_RANGE_PARAM, 0.0, 10.0, 10.0, "Frequency range", {
			"LFO", "C0 ±7", "C1 ±7", "C2 ±7", "C3 ±7", "C4 ±7", "C5 ±7", "C6 ±7", "C7 ±7", "Octaves", "Full",
		});
		configParam(OCTAVES_ROOT_PARAM, -7.0, 7.0, 0.0, "Octaves root", " semitones");

		configInput(ENGINE_INPUT, "Model");
		configInput(TIMBRE_INPUT, "Timbre");
		configInput(FREQ_INPUT, "FM");
		configInput(MORPH_INPUT, "Morph");
		configInput(HARMONICS_INPUT, "Harmonics");
		configInput(TRIGGER_INPUT, "Trigger");
		configInput(LEVEL_INPUT, "Level");
		configInput(NOTE_INPUT, "Pitch (1V/oct)");

		configOutput(OUT_OUTPUT, "Main");
		configOutput(AUX_OUTPUT, "Auxiliary");

		for (int i = 0; i < 16; i++) {
			stmlib::BufferAllocator allocator(shared_buffer[i], sizeof(shared_buffer[i]));
			voice[i].Init(&allocator);
			engineQuantizer[i].Init();
			engineTriggerDelay[i].Init(engineTriggerBuffer[i]);
		}
		octaveQuantizer.Init(9, 0.01f, false);

		onReset();
	}

	void onReset() override {
		patch.engine = firmware12 ? 8 : 0;
		altNavigation = false;
		uiMode = UI_MODE_NORMAL;
		patch.lpg_colour = 0.5f;
		patch.decay = 0.5f;
	}

	void onRandomize() override {
		if (firmware12)
			setEngine(random::u32() % 24);
		else
			patch.engine = random::u32() % 16;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "lowCpu", json_boolean(lowCpu));
		// Original firmware numbering, for older versions
		json_object_set_new(rootJ, "model", json_integer(firmware12 ? std::max(patch.engine - 8, 0) : patch.engine));
		json_object_set_new(rootJ, "firmware12", json_boolean(firmware12));
		json_object_set_new(rootJ, "engine", json_integer(patch.engine));
		json_object_set_new(rootJ, "altNavigation", json_boolean(altNavigation));
		json_object_set_new(rootJ, "colorBlind", json_boolean(colorBlind));

		{
			std::lock_guard<std::mutex> lock(userDataMutex);
			if (userDataEngine >= 0) {
				json_object_set_new(rootJ, "userDataEngine", json_integer(userDataEngine));
				json_object_set_new(rootJ, "userData", json_string(string::toBase64(userData).c_str()));
			}
		}

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* lowCpuJ = json_object_get(rootJ, "lowCpu");
		if (lowCpuJ)
			lowCpu = json_boolean_value(lowCpuJ);

		json_t* modelJ = json_object_get(rootJ, "model");
		if (modelJ)
			patch.engine = json_integer_value(modelJ);

		json_t* altNavigationJ = json_object_get(rootJ, "altNavigation");
		altNavigation = altNavigationJ && json_boolean_value(altNavigationJ);

		json_t* colorBlindJ = json_object_get(rootJ, "colorBlind");
		if (colorBlindJ)
			colorBlind = json_boolean_value(colorBlindJ);

		json_t* firmware12J = json_object_get(rootJ, "firmware12");
		firmware12 = firmware12J && json_boolean_value(firmware12J);
		uiMode = UI_MODE_NORMAL;
		if (firmware12) {
			json_t* engineJ = json_object_get(rootJ, "engine");
			setEngine(engineJ ? clamp((int) json_integer_value(engineJ), 0, 23) : patch.engine + 8);
		}

		json_t* userDataEngineJ = json_object_get(rootJ, "userDataEngine");
		json_t* userDataJ = json_object_get(rootJ, "userData");
		std::vector<uint8_t> data;
		if (userDataEngineJ && json_is_string(userDataJ)) {
			data = string::fromBase64(json_string_value(userDataJ));
			if (data.size() != kUserDataSize)
				data.clear();
		}
		if (data.empty())
			clearUserData();
		else
			setUserData(clamp((int) json_integer_value(userDataEngineJ), 0, plaits::kMaxEngines - 1), std::move(data));

		// Legacy <=1.0.2
		json_t* lpgColorJ = json_object_get(rootJ, "lpgColor");
		if (lpgColorJ)
			params[LPG_COLOR_PARAM].setValue(json_number_value(lpgColorJ));

		// Legacy <=1.0.2
		json_t* decayJ = json_object_get(rootJ, "decay");
		if (decayJ)
			params[LPG_DECAY_PARAM].setValue(json_number_value(decayJ));
	}

	void setFirmware12(bool firmware12) {
		if (firmware12 == this->firmware12)
			return;
		// Firmware 1.2 inserts eight engines before the original sixteen, which the original firmware can't select.
		patch.engine = firmware12 ? patch.engine + 8 : std::max(patch.engine - 8, 0);
		uiMode = UI_MODE_NORMAL;
		this->firmware12 = firmware12;
	}

	void setEngine(int engine) {
		// Like loading the hardware's settings, the new engines are only reachable with alternate navigation.
		if (firmware12 && engine < 8)
			altNavigation = true;
		patch.engine = engine;
	}

	bool hasUserData(int engine) {
		std::lock_guard<std::mutex> lock(userDataMutex);
		return engine == userDataEngine;
	}

	/** Replaces the data of any other engine, which reverts to its built-in data. */
	void setUserData(int engine, std::vector<uint8_t> data) {
		std::lock_guard<std::mutex> lock(userDataMutex);
		userData = std::move(data);
		userDataEngine = userData.empty() ? -1 : engine;
		userDataChanged = true;
	}

	void clearUserData() {
		setUserData(-1, {});
	}

	/** Returns an error message, or an empty string on success. */
	std::string loadUserData(int engine, const std::string& path) {
		std::vector<uint8_t> data;
		try {
			data = system::readFile(path);
		}
		catch (Exception& e) {
			return e.what();
		}

		std::vector<float> samples;
		float sampleRate = 0.f;
		if (readWav(data, samples, sampleRate)) {
			if (!decodeUserData(samples, sampleRate, data))
				return "No Plaits data found in this audio file.";
			// Like UserData::Save(), only accept data tagged for this engine.
			if (engine < data[kUserDataSize - 2] || engine > data[kUserDataSize - 1])
				return "This data is for a different model.";
		}
		else {
			// Strip the header and checksum of a DX7 32-voice bulk dump, with or without the end byte like the editor.
			if ((data.size() == kUserDataSize + 8 || data.size() == kUserDataSize + 7) && data[0] == 0xf0 && data[1] == 0x43 && data[3] == 0x09)
				data = std::vector<uint8_t>(data.begin() + 6, data.begin() + 6 + kUserDataSize);

			// The 6-op FM engines and wave terrain read the whole block, the wavetable engine starts with a 64-wave map.
			size_t minSize = (engine == 13) ? 64 : kUserDataSize - 2;
			if (data.size() < minSize || data.size() > kUserDataSize)
				return string::f("Expected %d bytes of data, got %d.", (int) kUserDataSize, (int) data.size());
			data.resize(kUserDataSize);
		}

		// Tag the data like UserData::Save() does, which overwrites its last two bytes.
		data[kUserDataSize - 2] = 'U';
		data[kUserDataSize - 1] = ' ' + engine;
		setUserData(engine, std::move(data));
		return "";
	}

	void applyUserData() {
		if (!userDataChanged && voiceUserDataEnabled == firmware12)
			return;
		std::unique_lock<std::mutex> lock(userDataMutex, std::try_to_lock);
		if (!lock.owns_lock())
			return;
		userDataChanged = false;
		voiceUserDataEnabled = firmware12;
		voiceUserData = userData;
		if (!voiceUserData.empty())
			voiceUserData.resize(kUserDataBufferSize);
		for (int e = 0; e < plaits::kMaxEngines; e++) {
			const uint8_t* data = (firmware12 && e == userDataEngine) ? voiceUserData.data() : NULL;
			for (int c = 0; c < 16; c++)
				voice[c].set_user_data(e, data);
		}
	}

	void navigate(int button) {
		ignoreRelease[0] = ignoreRelease[1] = true;
		if (altNavigation) {
			patch.engine = (patch.engine + (button == 0 ? 23 : 1)) % 24;
		}
		else {
			int newBank = button + 1;
			int cycle = (newBank == patch.engine / 8) ? 1 : 0;
			patch.engine = newBank * 8 + (patch.engine % 8 + cycle) % 8;
		}
	}

	// Port of Ui::ReadSwitches() from firmware 1.2
	void processButtons(float dt) {
		bool released[2];
		for (int i = 0; i < 2; i++) {
			bool pressed = params[MODEL1_PARAM + i].getValue() > 0.f;
			if (pressed && !buttonPressed[i]) {
				pressTime[i] = 0.f;
				ignoreRelease[i] = false;
			}
			released[i] = !pressed && buttonPressed[i];
			buttonPressed[i] = pressed;
			pressTime[i] = pressed ? pressTime[i] + dt : 0.f;
		}

		if (uiMode != UI_MODE_NORMAL) {
			if (released[0] || released[1])
				uiMode = UI_MODE_NORMAL;
			return;
		}

		// Long double press calibrates the hardware, which isn't needed here.
		if (pressTime[0] >= kLongPressTime && pressTime[1] >= kLongPressTime) {
			pressTime[0] = pressTime[1] = 0.f;
			ignoreRelease[0] = ignoreRelease[1] = true;
		}
		// Long press displays the hidden parameters.
		if (pressTime[0] >= kLongPressTime && pressTime[1] == 0.f) {
			pressTime[0] = 0.f;
			uiMode = UI_MODE_DISPLAY_ALTERNATE_PARAMETERS;
		}
		if (pressTime[1] >= kLongPressTime && pressTime[0] == 0.f) {
			pressTime[1] = 0.f;
			uiMode = UI_MODE_DISPLAY_OCTAVE;
		}

		if ((released[0] && !ignoreRelease[0] && pressTime[1] > 0.f) || (released[1] && !ignoreRelease[1] && pressTime[0] > 0.f)) {
			ignoreRelease[0] = ignoreRelease[1] = true;
			altNavigation = !altNavigation;
		}
		else if (released[0] && !ignoreRelease[0]) {
			navigate(0);
		}
		else if (released[1] && !ignoreRelease[1]) {
			navigate(1);
		}
	}

	// Port of Ui::UpdateLEDs() from firmware 1.2, whose displays are the same as the original firmware's
	void setFirmware12Lights(int channels, float tri, UiMode uiMode) {
		float green[8] = {};
		float red[8] = {};
		int triangle = std::min((int) (tri * 16.f), 15);

		if (uiMode == UI_MODE_NORMAL) {
			auto addEngine = [&](int e, float brightness) {
				int row = e % 8;
				int bank = e / 8;
				if (colorBlind) {
					// Yellow with a PWM duty cycle of 16, 4 or 1 out of 16 instead of a color per bank
					brightness = std::min(brightness, 1.f / (1 << (2 * bank)));
					green[row] = std::max(green[row], brightness);
					red[row] = std::max(red[row], brightness);
				}
				else {
					// Yellow, green and red banks
					if (bank != 2)
						green[row] = std::max(green[row], brightness);
					if (bank != 1)
						red[row] = std::max(red[row], brightness);
				}
			};
			for (int c = 0; c < channels; c++) {
				int activeEngine = voice[c].active_engine();
				if (activeEngine >= 0 && activeEngine < 24)
					addEngine(activeEngine, 1.f);
			}
			addEngine(patch.engine, tri);
		}
		else if (uiMode == UI_MODE_DISPLAY_ALTERNATE_PARAMETERS) {
			for (int parameter = 0; parameter < 2; parameter++) {
				float value = (parameter == 0) ? params[LPG_COLOR_PARAM].getValue() : params[LPG_DECAY_PARAM].getValue();
				value -= 0.001f;
				for (int i = 0; i < 4; i++) {
					int row = parameter * 4 + 3 - i;
					green[row] = red[row] = clamp(value * 4.f, 0.f, 1.f);
					value -= 0.25f;
				}
			}
		}
		else if (uiMode == UI_MODE_DISPLAY_OCTAVE) {
			int octave = (int) params[FREQ_RANGE_PARAM].getValue();
			for (int i = 0; i < 8; i++) {
				bool on;
				if (octave == 0)
					on = i != (triangle >> 1);
				else if (octave == 10)
					on = true;
				else if (octave == 9)
					on = (i & 1) != ((triangle >> 3) & 1);
				else
					on = (octave - 1) == i;
				green[7 - i] = red[7 - i] = on;
			}
		}

		for (int i = 0; i < 8; i++) {
			// Both dies lit look orange on the hardware, so show their overlap with the orange light.
			float orange = std::min(green[i], red[i]);
			lights[MODEL_LIGHT + i * 3 + 0].setBrightness(green[i] - orange);
			lights[MODEL_LIGHT + i * 3 + 1].setBrightness(red[i] - orange);
			lights[MODEL_LIGHT + i * 3 + 2].setBrightness(orange);
		}
	}

	// Port of the frequency range handling in Ui::Poll() from firmware 1.2
	float getFirmware12Note() {
		float transposition = params[FREQ_PARAM].getValue() / 4.f;
		int octave = (int) params[FREQ_RANGE_PARAM].getValue();
		if (octave == 0)
			return -48.37f + transposition * 60.f;
		if (octave == 9) {
			float fineTune = (params[OCTAVES_ROOT_PARAM].getValue() + 7.f) / 14.f;
			return 53.f + fineTune * 14.f + 12.f * (octaveQuantizer.Process(0.5f * transposition + 0.5f) - 4);
		}
		if (octave == 10)
			return 60.f + transposition * 48.f;
		return transposition * 7.f + octave * 12.f;
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[NOTE_INPUT].getChannels(), 1);

		if (outputBuffer.empty()) {
			const int blockSize = 12;

			applyUserData();

			// Model buttons
			if (firmware12)
				processButtons(lowCpu ? blockSize * args.sampleTime : blockSize / 48000.f);
			else if (model1Trigger.process(params[MODEL1_PARAM].getValue())) {
				if (patch.engine >= 8) {
					patch.engine -= 8;
				}
				else {
					patch.engine = (patch.engine + 1) % 8;
				}
			}
			if (!firmware12 && model2Trigger.process(params[MODEL2_PARAM].getValue())) {
				if (patch.engine < 8) {
					patch.engine += 8;
				}
				else {
					patch.engine = (patch.engine + 1) % 8 + 8;
				}
			}

			// Model lights
			// Pulse light at 2 Hz
			triPhase += 2.f * args.sampleTime * blockSize;
			if (triPhase >= 1.f)
				triPhase -= 1.f;
			float tri = (triPhase < 0.5f) ? triPhase * 2.f : (1.f - triPhase) * 2.f;

			// Get active engines of all voice channels
			bool activeEngines[16] = {};
			bool pulse = false;
			for (int c = 0; c < channels; c++) {
				int activeEngine = voice[c].active_engine() - 8;
				if (activeEngine >= 0 && activeEngine < 16)
					activeEngines[activeEngine] = true;
				// Pulse the light if at least one voice is using a different engine.
				if (activeEngine != patch.engine)
					pulse = true;
			}

			// Set model lights
			for (int i = 0; i < 16; i++) {
				// Transpose the [light][color] table
				int lightId = (i % 8) * 3 + (i / 8);
				float brightness = activeEngines[i];
				if (patch.engine == i && pulse)
					brightness = tri;
				lights[MODEL_LIGHT + lightId].setBrightness(brightness);
			}
			// Firmware 1.2 replaces the original lights. Holding a button wins over the widget's edit mode.
			UiMode displayMode = (uiMode != UI_MODE_NORMAL) ? uiMode : editMode;
			if (firmware12 || displayMode != UI_MODE_NORMAL)
				setFirmware12Lights(channels, tri, displayMode);

			// Calculate pitch for lowCpu mode if needed
			float pitch = params[FREQ_PARAM].getValue();
			if (lowCpu)
				pitch += std::log2(48000.f * args.sampleTime);
			// Update patch
			patch.note = 60.f + pitch * 12.f;
			// Firmware 1.2 replaces the frequency response, keeping the lowCpu correction.
			if (firmware12)
				patch.note += getFirmware12Note() - (60.f + params[FREQ_PARAM].getValue() * 12.f);
			patch.harmonics = params[HARMONICS_PARAM].getValue();
			patch.timbre = params[TIMBRE_PARAM].getValue();
			patch.morph = params[MORPH_PARAM].getValue();
			patch.lpg_colour = params[LPG_COLOR_PARAM].getValue();
			patch.decay = params[LPG_DECAY_PARAM].getValue();
			patch.frequency_modulation_amount = params[FREQ_CV_PARAM].getValue();
			patch.timbre_modulation_amount = params[TIMBRE_CV_PARAM].getValue();
			patch.morph_modulation_amount = params[MORPH_CV_PARAM].getValue();

			// Render output buffer for each voice
			dsp::Frame<16 * 2> outputFrames[blockSize];
			for (int c = 0; c < channels; c++) {
				// Construct modulations
				plaits::Modulations modulations;
				modulations.engine = 0.f;
				modulations.note = inputs[NOTE_INPUT].getVoltage(c) * 12.f;
				modulations.frequency = inputs[FREQ_INPUT].getPolyVoltage(c) * 6.f;
				modulations.harmonics = inputs[HARMONICS_INPUT].getPolyVoltage(c) / 5.f;
				modulations.timbre = inputs[TIMBRE_INPUT].getPolyVoltage(c) / 8.f;
				modulations.morph = inputs[MORPH_INPUT].getPolyVoltage(c) / 8.f;
				// Triggers at around 0.7 V
				modulations.trigger = inputs[TRIGGER_INPUT].getPolyVoltage(c) / 3.f;
				modulations.level = inputs[LEVEL_INPUT].getPolyVoltage(c) / 8.f;

				modulations.frequency_patched = inputs[FREQ_INPUT].isConnected();
				modulations.timbre_patched = inputs[TIMBRE_INPUT].isConnected();
				modulations.morph_patched = inputs[MORPH_INPUT].isConnected();
				modulations.trigger_patched = inputs[TRIGGER_INPUT].isConnected();
				modulations.level_patched = inputs[LEVEL_INPUT].isConnected();

				// Render frames
				plaits::Voice::Frame output[blockSize];
				// Match Voice's delayed trigger before latching the legacy engine CV.
				engineTriggerDelay[c].Write(modulations.trigger);
				float trigger = engineTriggerDelay[c].Read(plaits::kTriggerDelay);
				if (!engineTriggerHigh[c] && trigger > 0.3f) {
					engineTriggerHigh[c] = true;
					engineCv[c] = inputs[ENGINE_INPUT].getPolyVoltage(c) / 5.f;
				}
				else if (engineTriggerHigh[c] && trigger < 0.1f) {
					engineTriggerHigh[c] = false;
				}
				if (!modulations.trigger_patched)
					engineCv[c] = inputs[ENGINE_INPUT].getPolyVoltage(c) / 5.f;
				// Firmware 1.2 inserts eight engines before the original sixteen.
				plaits::Patch firmwarePatch = patch;
				firmwarePatch.engine = 8 + engineQuantizer[c].Process(patch.engine, engineCv[c], 16, 0.25f);
				if (firmware12) {
					// Voice quantizes the engine CV over all 24 engines and latches it on triggers.
					firmwarePatch.engine = patch.engine;
					modulations.engine = inputs[ENGINE_INPUT].getPolyVoltage(c) / 5.f;
				}
				voice[c].Render(firmwarePatch, modulations, output, blockSize);

				// Convert output to frames
				for (int i = 0; i < blockSize; i++) {
					outputFrames[i].samples[c * 2 + 0] = output[i].out / 32768.f;
					outputFrames[i].samples[c * 2 + 1] = output[i].aux / 32768.f;
				}
			}

			// Convert output
			if (lowCpu) {
				int len = std::min((int) outputBuffer.capacity(), blockSize);
				std::memcpy(outputBuffer.endData(), outputFrames, len * sizeof(outputFrames[0]));
				outputBuffer.endIncr(len);
			}
			else {
				outputSrc.setRates(48000, (int) args.sampleRate);
				int inLen = blockSize;
				int outLen = outputBuffer.capacity();
				outputSrc.setChannels(channels * 2);
				outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
				outputBuffer.endIncr(outLen);
			}
		}

		// Set output
		if (!outputBuffer.empty()) {
			dsp::Frame<16 * 2> outputFrame = outputBuffer.shift();
			for (int c = 0; c < channels; c++) {
				// Inverting op-amp on outputs
				outputs[OUT_OUTPUT].setVoltage(-outputFrame.samples[c * 2 + 0] * 5.f, c);
				outputs[AUX_OUTPUT].setVoltage(-outputFrame.samples[c * 2 + 1] * 5.f, c);
			}
		}
		outputs[OUT_OUTPUT].setChannels(channels);
		outputs[AUX_OUTPUT].setChannels(channels);
	}
};


// The frequency knob's response depends on the firmware 1.2 frequency range.
static int getFrequencyRange(Module* module) {
	Plaits* plaits = dynamic_cast<Plaits*>(module);
	if (!plaits || !plaits->firmware12)
		return 10;
	return (int) plaits->params[Plaits::FREQ_RANGE_PARAM].getValue();
}

float PlaitsFrequencyQuantity::getDisplayValue() {
	float transposition = getValue() / 4.f;
	int range = getFrequencyRange(module);
	if (range == 0)
		return dsp::FREQ_A4 * std::pow(2.f, (-48.37f + transposition * 60.f - 69.f) / 12.f);
	if (range == 9)
		return clamp((int) (9.f * (0.5f * transposition + 0.5f)), 0, 8) - 4;
	if (range == 10)
		return ParamQuantity::getDisplayValue();
	return transposition * 7.f;
}

void PlaitsFrequencyQuantity::setDisplayValue(float displayValue) {
	int range = getFrequencyRange(module);
	float transposition;
	if (range == 0) {
		if (!(displayValue > 0.f))
			return;
		transposition = (69.f + 12.f * std::log2(displayValue / dsp::FREQ_A4) + 48.37f) / 60.f;
	}
	else if (range == 9) {
		transposition = (std::round(displayValue) + 4.f + 0.5f) / 9.f * 2.f - 1.f;
	}
	else if (range == 10) {
		ParamQuantity::setDisplayValue(displayValue);
		return;
	}
	else {
		transposition = displayValue / 7.f;
	}
	setValue(clamp(transposition * 4.f, getMinValue(), getMaxValue()));
}

std::string PlaitsFrequencyQuantity::getUnit() {
	int range = getFrequencyRange(module);
	if (range == 0)
		return " Hz";
	if (range == 9)
		return " octaves";
	return " semitones";
}


static const std::string newModelLabels[8] = {
	"Classic waveshapes with filter",
	"Phase distortion",
	"6-op FM 1",
	"6-op FM 2",
	"6-op FM 3",
	"Wave terrain",
	"String machine",
	"Chiptune",
};


static const std::string modelLabels[16] = {
	"Pair of classic waveforms",
	"Waveshaping oscillator",
	"Two operator FM",
	"Granular formant oscillator",
	"Harmonic oscillator",
	"Wavetable oscillator",
	"Chords",
	"Vowel and speech synthesis",
	"Granular cloud",
	"Filtered noise",
	"Particle noise",
	"Inharmonic string modeling",
	"Modal resonator",
	"Analog bass drum",
	"Analog snare drum",
	"Analog hi-hat",
};


// Rack blends green and red to yellow, so firmware 1.2's yellow gets its own color.
struct GreenRedOrangeLight : GrayModuleLightWidget {
	GreenRedOrangeLight() {
		addBaseColor(SCHEME_GREEN);
		addBaseColor(SCHEME_RED);
		addBaseColor(nvgRGB(0xff, 0x80, 0x00));
	}
};


struct PlaitsWidget : ModuleWidget {
	bool lpgMode = false;
	bool rangeMode = false;

	PlaitsWidget(Plaits* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Plaits.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<TL1105>(mm2px(Vec(23.32685, 14.6539)), module, Plaits::MODEL1_PARAM));
		addParam(createParam<TL1105>(mm2px(Vec(32.22764, 14.6539)), module, Plaits::MODEL2_PARAM));
		addParam(createParam<Rogan3PSWhite>(mm2px(Vec(3.1577, 20.21088)), module, Plaits::FREQ_PARAM));
		addParam(createParam<Rogan3PSWhite>(mm2px(Vec(39.3327, 20.21088)), module, Plaits::HARMONICS_PARAM));
		addParam(createParam<Rogan1PSWhite>(mm2px(Vec(4.04171, 49.6562)), module, Plaits::TIMBRE_PARAM));
		addParam(createParam<Rogan1PSWhite>(mm2px(Vec(42.71716, 49.6562)), module, Plaits::MORPH_PARAM));
		addParam(createParam<Trimpot>(mm2px(Vec(7.88712, 77.60705)), module, Plaits::TIMBRE_CV_PARAM));
		addParam(createParam<Trimpot>(mm2px(Vec(27.2245, 77.60705)), module, Plaits::FREQ_CV_PARAM));
		addParam(createParam<Trimpot>(mm2px(Vec(46.56189, 77.60705)), module, Plaits::MORPH_CV_PARAM));

		ParamWidget* lpgColorParam = createParam<Rogan1PSBlue>(mm2px(Vec(4.04171, 49.6562)), module, Plaits::LPG_COLOR_PARAM);
		lpgColorParam->hide();
		addParam(lpgColorParam);
		ParamWidget* decayParam = createParam<Rogan1PSBlue>(mm2px(Vec(42.71716, 49.6562)), module, Plaits::LPG_DECAY_PARAM);
		decayParam->hide();
		addParam(decayParam);
		ParamWidget* octavesRootParam = createParam<Rogan3PSBlue>(mm2px(Vec(3.1577, 20.21088)), module, Plaits::OCTAVES_ROOT_PARAM);
		octavesRootParam->hide();
		addParam(octavesRootParam);
		ParamWidget* freqRangeParam = createParam<Rogan3PSBlue>(mm2px(Vec(39.3327, 20.21088)), module, Plaits::FREQ_RANGE_PARAM);
		freqRangeParam->hide();
		addParam(freqRangeParam);

		addInput(createInput<PJ301MPort>(mm2px(Vec(3.31381, 92.48067)), module, Plaits::ENGINE_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(14.75983, 92.48067)), module, Plaits::TIMBRE_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(26.20655, 92.48067)), module, Plaits::FREQ_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(37.65257, 92.48067)), module, Plaits::MORPH_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(49.0986, 92.48067)), module, Plaits::HARMONICS_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.31381, 107.08103)), module, Plaits::TRIGGER_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(14.75983, 107.08103)), module, Plaits::LEVEL_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(26.20655, 107.08103)), module, Plaits::NOTE_INPUT));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(37.65257, 107.08103)), module, Plaits::OUT_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(49.0986, 107.08103)), module, Plaits::AUX_OUTPUT));

		addChild(createLight<MediumLight<GreenRedOrangeLight>>(mm2px(Vec(28.79498, 23.31649)), module, Plaits::MODEL_LIGHT + 0 * 3));
		addChild(createLight<MediumLight<GreenRedOrangeLight>>(mm2px(Vec(28.79498, 28.71704)), module, Plaits::MODEL_LIGHT + 1 * 3));
		addChild(createLight<MediumLight<GreenRedOrangeLight>>(mm2px(Vec(28.79498, 34.1162)), module, Plaits::MODEL_LIGHT + 2 * 3));
		addChild(createLight<MediumLight<GreenRedOrangeLight>>(mm2px(Vec(28.79498, 39.51675)), module, Plaits::MODEL_LIGHT + 3 * 3));
		addChild(createLight<MediumLight<GreenRedOrangeLight>>(mm2px(Vec(28.79498, 44.91731)), module, Plaits::MODEL_LIGHT + 4 * 3));
		addChild(createLight<MediumLight<GreenRedOrangeLight>>(mm2px(Vec(28.79498, 50.31785)), module, Plaits::MODEL_LIGHT + 5 * 3));
		addChild(createLight<MediumLight<GreenRedOrangeLight>>(mm2px(Vec(28.79498, 55.71771)), module, Plaits::MODEL_LIGHT + 6 * 3));
		addChild(createLight<MediumLight<GreenRedOrangeLight>>(mm2px(Vec(28.79498, 61.11827)), module, Plaits::MODEL_LIGHT + 7 * 3));
	}

	void step() override {
		Plaits* module = dynamic_cast<Plaits*>(this->module);
		if (module) {
			// The frequency range only exists in firmware 1.2.
			if (rangeMode && !module->firmware12)
				setRangeMode(false);

			bool altNavigation = module->firmware12 && module->altNavigation;
			module->paramQuantities[Plaits::MODEL1_PARAM]->name = altNavigation ? "Previous model" : "Pitched models";
			module->paramQuantities[Plaits::MODEL2_PARAM]->name = altNavigation ? "Next model" : "Noise/percussive models";
		}
		ModuleWidget::step();
	}

	void appendUserDataItem(Menu* menu, Plaits* module, std::string label, int engine) {
		menu->addChild(createSubmenuItem(label, module->hasUserData(engine) ? "Custom" : "Built-in",
			[=](Menu* menu) {
				menu->addChild(createMenuItem("Load...", "", [=]() {
					char* pathC = osdialog_file(OSDIALOG_OPEN, NULL, NULL, NULL);
					if (!pathC)
						return;
					std::string path = pathC;
					std::free(pathC);
					std::string error = module->loadUserData(engine, path);
					if (!error.empty())
						osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, error.c_str());
				}));
				menu->addChild(createMenuItem("Clear", "", [=]() {
					module->clearUserData();
				}, !module->hasUserData(engine)));
			}
		));
	}

	void appendContextMenu(Menu* menu) override {
		Plaits* module = dynamic_cast<Plaits*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("Low CPU (disable resampling)", "", &module->lowCpu));

		menu->addChild(createBoolMenuItem("Edit LPG response/decay", "",
			[=]() {return this->getLpgMode();},
			[=](bool val) {this->setLpgMode(val);}
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolMenuItem("Firmware 1.2", "",
			[=]() {return module->firmware12;},
			[=](bool val) {module->setFirmware12(val);}
		));

		// The original sixteen engines start at 8 in firmware 1.2.
		int offset = module->firmware12 ? 8 : 0;

		if (module->firmware12) {
			menu->addChild(createBoolPtrMenuItem("Alternate navigation", "", &module->altNavigation));
			menu->addChild(createBoolPtrMenuItem("Color-blind LEDs", "", &module->colorBlind));

			menu->addChild(createBoolMenuItem("Edit frequency range/octaves root", "",
				[=]() {return this->getRangeMode();},
				[=](bool val) {this->setRangeMode(val);}
			));

			SwitchQuantity* rangeQuantity = dynamic_cast<SwitchQuantity*>(module->paramQuantities[Plaits::FREQ_RANGE_PARAM]);
			menu->addChild(createIndexSubmenuItem("Frequency range", rangeQuantity->labels,
				[=]() {return (size_t) rangeQuantity->getValue();},
				[=](size_t val) {rangeQuantity->setValue(val);}
			));

			menu->addChild(createSubmenuItem("Custom data", "",
				[=](Menu* menu) {
					appendUserDataItem(menu, module, "6-op FM 1", 2);
					appendUserDataItem(menu, module, "6-op FM 2", 3);
					appendUserDataItem(menu, module, "6-op FM 3", 4);
					appendUserDataItem(menu, module, "Wave terrain", 5);
					appendUserDataItem(menu, module, "Wavetable", 13);
				}
			));

			menu->addChild(new MenuSeparator);
			menu->addChild(createMenuLabel("New models"));

			for (int i = 0; i < 8; i++) {
				menu->addChild(createCheckMenuItem(newModelLabels[i], "",
					[=]() {return module->patch.engine == i;},
					[=]() {module->setEngine(i);}
				));
			}
		}

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Pitched models"));

		for (int i = 0; i < 8; i++) {
			menu->addChild(createCheckMenuItem(modelLabels[i], "",
				[=]() {return module->patch.engine == i + offset;},
				[=]() {module->patch.engine = i + offset;}
			));
		}

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Noise/percussive models"));

		for (int i = 8; i < 16; i++) {
			menu->addChild(createCheckMenuItem(modelLabels[i], "",
				[=]() {return module->patch.engine == i + offset;},
				[=]() {module->patch.engine = i + offset;}
			));
		}
	}

	void setLpgMode(bool lpgMode) {
		// ModuleWidget::getParam() doesn't work if the ModuleWidget doesn't have a module.
		if (!module)
			return;
		if (lpgMode) {
			getParam(Plaits::MORPH_PARAM)->hide();
			getParam(Plaits::TIMBRE_PARAM)->hide();
			getParam(Plaits::LPG_DECAY_PARAM)->show();
			getParam(Plaits::LPG_COLOR_PARAM)->show();
		}
		else {
			getParam(Plaits::MORPH_PARAM)->show();
			getParam(Plaits::TIMBRE_PARAM)->show();
			getParam(Plaits::LPG_DECAY_PARAM)->hide();
			getParam(Plaits::LPG_COLOR_PARAM)->hide();
		}
		this->lpgMode = lpgMode;
		// Like holding the left button, the LEDs show the LPG settings.
		getModule<Plaits>()->editMode = lpgMode ? Plaits::UI_MODE_DISPLAY_ALTERNATE_PARAMETERS : rangeMode ? Plaits::UI_MODE_DISPLAY_OCTAVE : Plaits::UI_MODE_NORMAL;
	}

	bool getLpgMode() {
		return this->lpgMode;
	}

	// Like holding the right button on the hardware, FREQUENCY sets the octaves root and HARMONICS the range.
	void setRangeMode(bool rangeMode) {
		if (!module)
			return;
		if (rangeMode) {
			getParam(Plaits::FREQ_PARAM)->hide();
			getParam(Plaits::HARMONICS_PARAM)->hide();
			getParam(Plaits::OCTAVES_ROOT_PARAM)->show();
			getParam(Plaits::FREQ_RANGE_PARAM)->show();
		}
		else {
			getParam(Plaits::FREQ_PARAM)->show();
			getParam(Plaits::HARMONICS_PARAM)->show();
			getParam(Plaits::OCTAVES_ROOT_PARAM)->hide();
			getParam(Plaits::FREQ_RANGE_PARAM)->hide();
		}
		this->rangeMode = rangeMode;
		getModule<Plaits>()->editMode = rangeMode ? Plaits::UI_MODE_DISPLAY_OCTAVE : lpgMode ? Plaits::UI_MODE_DISPLAY_ALTERNATE_PARAMETERS : Plaits::UI_MODE_NORMAL;
	}

	bool getRangeMode() {
		return this->rangeMode;
	}
};


Model* modelPlaits = createModel<Plaits, PlaitsWidget>("Plaits");
