#include "plugin.hpp"
#include "clouds/dsp/granular_processor.h"
#include <array>
#include <cstring>
#include <atomic>
#include <mutex>


// Rack-side storage for the firmware's tagged sample-memory format.
// Fixed capacity allows capture/restore without allocating in process().
namespace clouds_memory {
constexpr size_t largeBytes = 118784;
constexpr size_t smallBytes = 65536 - 128;
constexpr size_t maxWords = (2 * smallBytes + sizeof(clouds::PersistentState) + 24) / 4;

struct Slot {
	std::array<uint32_t, maxWords> words = {};
	size_t count = 0;
	int playback = 0;

	bool valid() const {
		if (playback < 0 || playback >= clouds::PLAYBACK_MODE_LAST || count > words.size()
				|| count < 2 + sizeof(clouds::PersistentState) / 4)
			return false;
		if (words[0] != clouds::FourCC<'s', 't', 'a', 't'>::value || words[1] != sizeof(clouds::PersistentState))
			return false;
		clouds::PersistentState state;
		std::memcpy(&state, &words[2], sizeof(state));
		if (state.quality > 3 || state.spectral > 1
				|| bool(state.spectral) != (playback == clouds::PLAYBACK_MODE_SPECTRAL))
			return false;
		int channels = state.quality & 1 ? 1 : 2;
		size_t bytes = channels == 1 ? largeBytes : smallBytes;
		size_t pos = 2 + sizeof(state) / 4;
		for (int c = 0; c < channels; c++) {
			if (pos + 2 + bytes / 4 > count || words[pos] != clouds::FourCC<'b', 'u', 'f', 'f'>::value
					|| words[pos + 1] != bytes)
				return false;
			// Spectral state does not use the time-domain write heads.
			if (!state.spectral) {
				int samples = bytes / (state.quality & 2 ? 1 : 2) - kInterpolationTail;
				if (state.write_head[c] < 0 || state.write_head[c] >= samples)
					return false;
			}
			pos += 2 + bytes / 4;
		}
		return pos == count;
	}

	bool capture(clouds::GranularProcessor& processor) {
		processor.PreparePersistentData();
		clouds::PersistentBlock blocks[4];
		size_t n = 0;
		processor.GetPersistentData(blocks, &n);
		count = 0;
		playback = processor.playback_mode();
		for (size_t i = 0; i < n; i++) {
			const auto& b = blocks[i];
			if (b.size % 4 || count + 2 + b.size / 4 > words.size()) {
				count = 0;
				return false;
			}
			words[count++] = b.tag;
			words[count++] = b.size;
			std::memcpy(&words[count], b.data, b.size);
			count += b.size / 4;
		}
		return valid();
	}

	bool restore(clouds::GranularProcessor& processor) const {
		if (!valid())
			return false;
		processor.set_playback_mode(clouds::PlaybackMode(playback));
		return processor.LoadPersistentData(words.data());
	}
};
} // namespace clouds_memory


struct Clouds : Module {
	enum ParamIds {
		FREEZE_PARAM,
		MODE_PARAM,
		LOAD_PARAM,
		POSITION_PARAM,
		SIZE_PARAM,
		PITCH_PARAM,
		IN_GAIN_PARAM,
		DENSITY_PARAM,
		TEXTURE_PARAM,
		BLEND_PARAM,
		SPREAD_PARAM,
		FEEDBACK_PARAM,
		REVERB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FREEZE_INPUT,
		TRIG_INPUT,
		POSITION_INPUT,
		SIZE_INPUT,
		PITCH_INPUT,
		BLEND_INPUT,
		IN_L_INPUT,
		IN_R_INPUT,
		DENSITY_INPUT,
		TEXTURE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_L_OUTPUT,
		OUT_R_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		FREEZE_LIGHT,
		MIX_GREEN_LIGHT, MIX_RED_LIGHT,
		PAN_GREEN_LIGHT, PAN_RED_LIGHT,
		FEEDBACK_GREEN_LIGHT, FEEDBACK_RED_LIGHT,
		REVERB_GREEN_LIGHT, REVERB_RED_LIGHT,
		NUM_LIGHTS
	};

	dsp::SampleRateConverter<2> inputSrc;
	dsp::SampleRateConverter<2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> inputBuffer;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> outputBuffer;

	uint8_t* block_mem;
	uint8_t* block_ccm;
	clouds::GranularProcessor* processor;

	bool triggered = false;
	bool legacyTrigger = false;
	dsp::SchmittTrigger grainTrigger;
	std::array<clouds_memory::Slot, 4> memorySlots;
	std::mutex memoryMutex;
	std::atomic<int> memoryRequest{0}; // 1..4 save, 5..8 load
	std::atomic<unsigned> populatedSlots{0};

	void requestMemory(int slot, bool save) {
		if (slot < 0 || slot >= 4)
			return;
		int idle = 0;
		memoryRequest.compare_exchange_strong(idle, 1 + slot + (save ? 0 : 4));
	}

	void processMemory() {
		int request = memoryRequest.load();
		if (!request)
			return;
		// Serialization can hold the bank lock; the audio thread never waits.
		std::unique_lock<std::mutex> lock(memoryMutex, std::try_to_lock);
		if (!lock.owns_lock())
			return;
		int slot = (request - 1) % 4;
		if (request <= 4) {
			if (memorySlots[slot].capture(*processor))
				populatedSlots.fetch_or(1u << slot);
			else
				populatedSlots.fetch_and(~(1u << slot));
		}
		else if (memorySlots[slot].restore(*processor)) {
			playback = processor->playback_mode();
			quality = processor->quality();
			freeze = true;
		}
		memoryRequest.store(0);
	}

	dsp::SchmittTrigger freezeTrigger;
	bool freeze = false;
	dsp::SchmittTrigger blendTrigger;
	int blendMode = 0;

	clouds::PlaybackMode playback;
	int quality = 0;

	Clouds() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(POSITION_PARAM, 0.0, 1.0, 0.5, "Grain position");
		configParam(SIZE_PARAM, 0.0, 1.0, 0.5, "Grain size");
		configParam(PITCH_PARAM, -2.0, 2.0, 0.0, "Grain pitch");
		configParam(IN_GAIN_PARAM, 0.0, 1.0, 0.5, "Audio input gain");
		configParam(DENSITY_PARAM, 0.0, 1.0, 0.5, "Grain density");
		configParam(TEXTURE_PARAM, 0.0, 1.0, 0.5, "Grain texture");
		configParam(BLEND_PARAM, 0.0, 1.0, 0.5, "Dry/wet");
		configParam(SPREAD_PARAM, 0.0, 1.0, 0.0, "Stereo spread");
		configParam(FEEDBACK_PARAM, 0.0, 1.0, 0.0, "Feedback amount");
		configParam(REVERB_PARAM, 0.0, 1.0, 0.0, "Reverb amount");
		configButton(FREEZE_PARAM, "Freeze");
		configButton(MODE_PARAM, "Mode");
		configButton(LOAD_PARAM, "Sample memory (right-click module to save/load)");
		configInput(FREEZE_INPUT, "Freeze");
		configInput(TRIG_INPUT, "Trigger");
		configInput(POSITION_INPUT, "Position");
		configInput(SIZE_INPUT, "Size");
		configInput(PITCH_INPUT, "Pitch (1V/oct)");
		configInput(BLEND_INPUT, "Blend");
		configInput(IN_L_INPUT, "Left");
		configInput(IN_R_INPUT, "Right");
		configInput(DENSITY_INPUT, "Density");
		configInput(TEXTURE_INPUT, "Texture");
		configOutput(OUT_L_OUTPUT, "Left");
		configOutput(OUT_R_OUTPUT, "Right");

		configBypass(IN_L_INPUT, OUT_L_OUTPUT);
		configBypass(IN_R_INPUT, OUT_R_OUTPUT);

		const int memLen = 118784;
		const int ccmLen = 65536 - 128;
		block_mem = new uint8_t[memLen]();
		block_ccm = new uint8_t[ccmLen]();
		processor = new clouds::GranularProcessor();
		memset(processor, 0, sizeof(*processor));

		processor->Init(block_mem, memLen, block_ccm, ccmLen);
		onReset();
	}

	~Clouds() {
		delete processor;
		delete[] block_mem;
		delete[] block_ccm;
	}

	void process(const ProcessArgs& args) override {
		// Get input
		dsp::Frame<2> inputFrame = {};
		if (!inputBuffer.full()) {
			inputFrame.samples[0] = inputs[IN_L_INPUT].getVoltage() * params[IN_GAIN_PARAM].getValue() / 5.0;
			inputFrame.samples[1] = inputs[IN_R_INPUT].isConnected() ? inputs[IN_R_INPUT].getVoltage() * params[IN_GAIN_PARAM].getValue() / 5.0 : inputFrame.samples[0];
			inputBuffer.push(inputFrame);
		}

		if (freezeTrigger.process(params[FREEZE_PARAM].getValue())) {
			freeze ^= true;
		}
		if (blendTrigger.process(params[MODE_PARAM].getValue())) {
			blendMode = (blendMode + 1) % 4;
		}

		// Trigger
		bool rising = grainTrigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f);
		if (legacyTrigger ? inputs[TRIG_INPUT].getVoltage() >= 1.f : rising) {
			triggered = true;
		}

		// Render frames
		if (outputBuffer.empty()) {
			clouds::ShortFrame input[32] = {};
			// Convert input buffer
			{
				inputSrc.setRates(args.sampleRate, 32000);
				dsp::Frame<2> inputFrames[32];
				int inLen = inputBuffer.size();
				int outLen = 32;
				inputSrc.process(inputBuffer.startData(), &inLen, inputFrames, &outLen);
				inputBuffer.startIncr(inLen);

				// We might not fill all of the input buffer if there is a deficiency, but this cannot be avoided due to imprecisions between the input and output SRC.
				for (int i = 0; i < outLen; i++) {
					input[i].l = clamp(inputFrames[i].samples[0] * 32767.0f, -32768.0f, 32767.0f);
					input[i].r = clamp(inputFrames[i].samples[1] * 32767.0f, -32768.0f, 32767.0f);
				}
			}

			// Set up processor
			processor->set_playback_mode(playback);
			processor->set_quality(quality);
			processor->Prepare();
			processMemory();

			clouds::Parameters* p = processor->mutable_parameters();
			p->trigger = triggered;
			p->gate = legacyTrigger ? triggered : grainTrigger.isHigh();
			p->freeze = freeze || (inputs[FREEZE_INPUT].getVoltage() >= 1.0);
			p->position = clamp(params[POSITION_PARAM].getValue() + inputs[POSITION_INPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			p->size = clamp(params[SIZE_PARAM].getValue() + inputs[SIZE_INPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			p->pitch = clamp((params[PITCH_PARAM].getValue() + inputs[PITCH_INPUT].getVoltage()) * 12.0f, -48.0f, 48.0f);
			p->density = clamp(params[DENSITY_PARAM].getValue() + inputs[DENSITY_INPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			p->texture = clamp(params[TEXTURE_PARAM].getValue() + inputs[TEXTURE_INPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			p->dry_wet = params[BLEND_PARAM].getValue();
			p->stereo_spread = params[SPREAD_PARAM].getValue();
			p->feedback = params[FEEDBACK_PARAM].getValue();
			// As in the firmware, reverb is applied to the wet signal before the dry/wet mix, so dry audio is not reverberated.
			p->reverb = params[REVERB_PARAM].getValue();
			float blend = inputs[BLEND_INPUT].getVoltage() / 5.0f;
			switch (blendMode) {
				case 0:
					p->dry_wet += blend;
					p->dry_wet = clamp(p->dry_wet, 0.0f, 1.0f);
					break;
				case 1:
					p->stereo_spread += blend;
					p->stereo_spread = clamp(p->stereo_spread, 0.0f, 1.0f);
					break;
				case 2:
					p->feedback += blend;
					p->feedback = clamp(p->feedback, 0.0f, 1.0f);
					break;
				case 3:
					p->reverb += blend;
					p->reverb = clamp(p->reverb, 0.0f, 1.0f);
					break;
			}

			clouds::ShortFrame output[32];
			processor->Process(input, output, 32);

			// Convert output buffer
			{
				dsp::Frame<2> outputFrames[32];
				for (int i = 0; i < 32; i++) {
					outputFrames[i].samples[0] = output[i].l / 32768.0;
					outputFrames[i].samples[1] = output[i].r / 32768.0;
				}

				outputSrc.setRates(32000, args.sampleRate);
				int inLen = 32;
				int outLen = outputBuffer.capacity();
				outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
				outputBuffer.endIncr(outLen);
			}

			triggered = false;
		}

		// Set output
		dsp::Frame<2> outputFrame = {};
		if (!outputBuffer.empty()) {
			outputFrame = outputBuffer.shift();
			outputs[OUT_L_OUTPUT].setVoltage(5.0 * outputFrame.samples[0]);
			outputs[OUT_R_OUTPUT].setVoltage(5.0 * outputFrame.samples[1]);
		}

		// Lights
		clouds::Parameters* p = processor->mutable_parameters();
		dsp::VuMeter vuMeter;
		vuMeter.dBInterval = 6.0;
		dsp::Frame<2> lightFrame = p->freeze ? outputFrame : inputFrame;
		vuMeter.setValue(fmaxf(fabsf(lightFrame.samples[0]), fabsf(lightFrame.samples[1])));
		lights[FREEZE_LIGHT].setBrightness(p->freeze ? 0.75 : 0.0);
		lights[MIX_GREEN_LIGHT].setSmoothBrightness(vuMeter.getBrightness(3), args.sampleTime);
		lights[PAN_GREEN_LIGHT].setSmoothBrightness(vuMeter.getBrightness(2), args.sampleTime);
		lights[FEEDBACK_GREEN_LIGHT].setSmoothBrightness(vuMeter.getBrightness(1), args.sampleTime);
		lights[REVERB_GREEN_LIGHT].setBrightness(0.0);
		lights[MIX_RED_LIGHT].setBrightness(0.0);
		lights[PAN_RED_LIGHT].setBrightness(0.0);
		lights[FEEDBACK_RED_LIGHT].setSmoothBrightness(vuMeter.getBrightness(1), args.sampleTime);
		lights[REVERB_RED_LIGHT].setSmoothBrightness(vuMeter.getBrightness(0), args.sampleTime);
	}

	void onReset() override {
		memoryRequest.store(0);
		legacyTrigger = false;
		triggered = false;
		grainTrigger.reset();
		grainTrigger.process(0.f);
		freeze = false;
		blendMode = 0;
		playback = clouds::PLAYBACK_MODE_GRANULAR;
		quality = 0;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "playback", json_integer((int) playback));
		json_object_set_new(rootJ, "quality", json_integer(quality));
		json_object_set_new(rootJ, "blendMode", json_integer(blendMode));
		json_object_set_new(rootJ, "legacyTrigger", json_boolean(legacyTrigger));
		// Embed saved slots so presets, duplication, undo and patch transfer
		// all carry their audio. Only explicit saves capture the live buffer.
		json_t* slotsJ = json_array();
		{
			std::lock_guard<std::mutex> lock(memoryMutex);
			for (const auto& slot : memorySlots) {
				if (!slot.valid()) {
					json_array_append_new(slotsJ, json_null());
					continue;
				}
				json_t* slotJ = json_object();
				json_object_set_new(slotJ, "version", json_integer(1));
				json_object_set_new(slotJ, "playback", json_integer(slot.playback));
				std::string audio = string::toBase64(reinterpret_cast<const uint8_t*>(slot.words.data()), slot.count * 4);
				json_object_set_new(slotJ, "audio", json_string(audio.c_str()));
				json_array_append_new(slotsJ, slotJ);
			}
		}
		json_object_set_new(rootJ, "memorySlots", slotsJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* legacyJ = json_object_get(rootJ, "legacyTrigger");
		legacyTrigger = legacyJ ? json_is_true(legacyJ) : true;
		{
			std::lock_guard<std::mutex> lock(memoryMutex);
			memoryRequest.store(0);
			unsigned populated = 0;
			json_t* slotsJ = json_object_get(rootJ, "memorySlots");
			for (int i = 0; i < 4; i++) {
				auto& slot = memorySlots[i];
				slot.count = 0;
				json_t* slotJ = json_array_get(slotsJ, i);
				json_t* audioJ = json_object_get(slotJ, "audio");
				json_t* modeJ = json_object_get(slotJ, "playback");
				if (json_integer_value(json_object_get(slotJ, "version")) != 1 || !json_is_string(audioJ)
						|| !json_is_integer(modeJ) || json_integer_value(modeJ) < 0 || json_integer_value(modeJ) >= clouds::PLAYBACK_MODE_LAST
						|| json_string_length(audioJ) > ((clouds_memory::maxWords * 4 + 2) / 3) * 4)
					continue;
				try {
					auto bytes = string::fromBase64(json_string_value(audioJ));
					if (bytes.size() % 4 || bytes.size() > slot.words.size() * 4)
						continue;
					slot.playback = json_integer_value(modeJ);
					slot.count = bytes.size() / 4;
					std::memcpy(slot.words.data(), bytes.data(), bytes.size());
					if (slot.valid())
						populated |= 1u << i;
					else
						slot.count = 0;
				}
				catch (const std::exception&) {
					slot.count = 0;
				}
			}
			populatedSlots.store(populated);
		}
		json_t* playbackJ = json_object_get(rootJ, "playback");
		if (playbackJ) {
			playback = (clouds::PlaybackMode) json_integer_value(playbackJ);
		}

		json_t* qualityJ = json_object_get(rootJ, "quality");
		if (qualityJ) {
			quality = json_integer_value(qualityJ);
		}

		json_t* blendModeJ = json_object_get(rootJ, "blendMode");
		if (blendModeJ) {
			blendMode = json_integer_value(blendModeJ);
		}
	}
};


struct FreezeLight : YellowLight {
	FreezeLight() {
		box.size = Vec(28 - 6, 28 - 6);
		bgColor = color::BLACK_TRANSPARENT;
	}
};


struct CloudsWidget : ModuleWidget {
	ParamWidget* blendParam;
	ParamWidget* spreadParam;
	ParamWidget* feedbackParam;
	ParamWidget* reverbParam;

	CloudsWidget(Clouds* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Clouds.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(240, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(240, 365)));

		addParam(createParam<Rogan3PSRed>(Vec(27, 93), module, Clouds::POSITION_PARAM));
		addParam(createParam<Rogan3PSGreen>(Vec(108, 93), module, Clouds::SIZE_PARAM));
		addParam(createParam<Rogan3PSWhite>(Vec(190, 93), module, Clouds::PITCH_PARAM));

		addParam(createParam<Rogan1PSRed>(Vec(14, 180), module, Clouds::IN_GAIN_PARAM));
		addParam(createParam<Rogan1PSRed>(Vec(81, 180), module, Clouds::DENSITY_PARAM));
		addParam(createParam<Rogan1PSGreen>(Vec(146, 180), module, Clouds::TEXTURE_PARAM));
		blendParam = createParam<Rogan1PSWhite>(Vec(213, 180), module, Clouds::BLEND_PARAM);
		addParam(blendParam);
		spreadParam = createParam<Rogan1PSRed>(Vec(213, 180), module, Clouds::SPREAD_PARAM);
		spreadParam->hide();
		addParam(spreadParam);
		feedbackParam = createParam<Rogan1PSGreen>(Vec(213, 180), module, Clouds::FEEDBACK_PARAM);
		feedbackParam->hide();
		addParam(feedbackParam);
		reverbParam = createParam<Rogan1PSBlue>(Vec(213, 180), module, Clouds::REVERB_PARAM);
		reverbParam->hide();
		addParam(reverbParam);

		addParam(createParam<CKD6>(Vec(12, 43), module, Clouds::FREEZE_PARAM));
		addParam(createParam<TL1105>(Vec(211, 50), module, Clouds::MODE_PARAM));
		addParam(createParam<TL1105>(Vec(239, 50), module, Clouds::LOAD_PARAM));

		addInput(createInput<PJ301MPort>(Vec(15, 274), module, Clouds::FREEZE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(58, 274), module, Clouds::TRIG_INPUT));
		addInput(createInput<PJ301MPort>(Vec(101, 274), module, Clouds::POSITION_INPUT));
		addInput(createInput<PJ301MPort>(Vec(144, 274), module, Clouds::SIZE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(188, 274), module, Clouds::PITCH_INPUT));
		addInput(createInput<PJ301MPort>(Vec(230, 274), module, Clouds::BLEND_INPUT));

		addInput(createInput<PJ301MPort>(Vec(15, 317), module, Clouds::IN_L_INPUT));
		addInput(createInput<PJ301MPort>(Vec(58, 317), module, Clouds::IN_R_INPUT));
		addInput(createInput<PJ301MPort>(Vec(101, 317), module, Clouds::DENSITY_INPUT));
		addInput(createInput<PJ301MPort>(Vec(144, 317), module, Clouds::TEXTURE_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(188, 317), module, Clouds::OUT_L_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(230, 317), module, Clouds::OUT_R_OUTPUT));

		addChild(createLight<FreezeLight>(Vec(12 + 3, 43 + 3), module, Clouds::FREEZE_LIGHT));
		addChild(createLight<MediumLight<GreenRedBicolorLight>>(Vec(82.5, 53), module, Clouds::MIX_GREEN_LIGHT));
		addChild(createLight<MediumLight<GreenRedBicolorLight>>(Vec(114.5, 53), module, Clouds::PAN_GREEN_LIGHT));
		addChild(createLight<MediumLight<GreenRedBicolorLight>>(Vec(145.5, 53), module, Clouds::FEEDBACK_GREEN_LIGHT));
		addChild(createLight<MediumLight<GreenRedBicolorLight>>(Vec(177.5, 53), module, Clouds::REVERB_GREEN_LIGHT));
	}

	void step() override {
		Clouds* module = dynamic_cast<Clouds*>(this->module);

		if (module) {
			blendParam->visible = (module->blendMode == 0);
			spreadParam->visible = (module->blendMode == 1);
			feedbackParam->visible = (module->blendMode == 2);
			reverbParam->visible = (module->blendMode == 3);
		}

		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		Clouds* module = dynamic_cast<Clouds*>(this->module);
		assert(module);
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Legacy trigger-as-gate behavior", "", &module->legacyTrigger));
		menu->addChild(createSubmenuItem("Sample memory", "", [=](Menu* child) {
			if (module->memoryRequest.load())
				child->addChild(createMenuLabel("Waiting for audio processing"));
			for (int i = 0; i < 4; i++) {
				bool occupied = module->populatedSlots.load() & (1u << i);
				bool busy = module->memoryRequest.load() != 0;
				child->addChild(createMenuItem(string::f("Save buffer to slot %d%s", i + 1, occupied ? " (overwrite)" : ""), "",
					[=]() { module->requestMemory(i, true); }, busy));
				child->addChild(createMenuItem(string::f("Load slot %d%s", i + 1, occupied ? "" : " (empty)"), "",
					[=]() { module->requestMemory(i, false); }, busy || !occupied));
			}
		}));

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Blend knob"));

		static const std::vector<std::string> blendLabels = {
			"Wet/dry",
			"Spread",
			"Feedback",
			"Reverb",
		};
		for (int i = 0; i < (int) blendLabels.size(); i++) {
			menu->addChild(createCheckMenuItem(blendLabels[i], "",
				[=]() {return module->blendMode == i;},
				[=]() {module->blendMode = i;}
			));
		}

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Alternate mode"));

		static const std::vector<std::string> playbackLabels = {
			"Granular",
			"Pitch-shifter/time-stretcher",
			"Looping delay",
			"Spectral madness",
		};
		for (int i = 0; i < (int) playbackLabels.size(); i++) {
			menu->addChild(createCheckMenuItem(playbackLabels[i], "",
				[=]() {return module->playback == i;},
				[=]() {module->playback = (clouds::PlaybackMode) i;}
			));
		}

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Quality"));

		static const std::vector<std::string> qualityLabels = {
			"1s 32kHz 16-bit stereo",
			"2s 32kHz 16-bit mono",
			"4s 16kHz 8-bit µ-law stereo",
			"8s 16kHz 8-bit µ-law mono",
		};
		for (int i = 0; i < (int) qualityLabels.size(); i++) {
			menu->addChild(createCheckMenuItem(qualityLabels[i], "",
				[=]() {return module->quality == i;},
				[=]() {module->quality = i;}
			));
		}
	}
};


Model* modelClouds = createModel<Clouds, CloudsWidget>("Clouds");
