#include "plugin.hpp"
#include "Beads/dsp/processor.h"
#include "Beads/dsp/synth.h"
#include "Beads/gain_control.h"
#include "Beads/pot_controller.h"


// Processing and UI behavior follow the firmware's beads.cc and ui.cc.
static const int kBlockRate = 4000;
static const int kMaxBlockSize = 12;
static const float kSynthModeDelay = 10.f;
// Codec full scale at the jacks, from the hardware's analog stages
static const float kInputFullScale = 9.f;
static const float kOutputFullScale = 10.f;
static const float kLongPressDuration[] = {4.f, 4.f, 1.5f, INFINITY};

// LED color for each quality setting, from scorched cassette to bright digital.
// The firmware's drive values don't look right on screen, so these are taken from the manual's quality table.
// Bright digital is the firmware's slightly cool white (192, 192, 208) at full scale.
static const uint8_t kQualityColors[4][3] = {
	{214, 71, 121},
	{231, 165, 52},
	{0, 158, 154},
	{236, 236, 255},
};


// Pot laws from the firmware's cv_reader.h
static float attenurandomizerLaw(float x) {
	x -= 0.5f;
	return (2.f / 0.47f) * x * std::max(std::fabs(x) - 0.03f, 0.f);
}

static float detuneLaw(float x) {
	static const float intervals[8] = {0.f, 1.f, 3.f, 4.f, 7.f, 12.f, 24.f, 24.f};
	float detune = 2.f * x - 1.f;
	float sign = detune < 0.f ? -1.f : 1.f;
	detune = clamp(detune * sign * 6.f, 0.f, 5.999f);
	int integral = (int) detune;
	float fractional = std::max((detune - integral - 0.33333333f) * 1.5f, 0.f);
	float t = fractional * fractional * (3.f - 2.f * fractional);
	return crossfade(intervals[integral], intervals[integral + 1], t) * sign;
}

// Knob position that gives y, in the middle of any flat zone of the law
static float invertLaw(float (*law)(float), float y) {
	y = clamp(y, law(0.f), law(1.f));
	float lo = 0.f, hi = 1.f;
	for (int i = 0; i < 24; i++) {
		float x = (lo + hi) / 2.f;
		(law(x) < y - 1e-4f ? lo : hi) = x;
	}
	float start = hi;
	lo = 0.f, hi = 1.f;
	for (int i = 0; i < 24; i++) {
		float x = (lo + hi) / 2.f;
		(law(x) <= y + 1e-4f ? lo : hi) = x;
	}
	return std::max((start + lo) / 2.f, start);
}

// Grain length in seconds, from the firmware's GranularPlayer::ComputeGrainDuration()
static float grainDuration(float size) {
	float shortness = (size < 1.f / 3.f)
		? size * 3.f * (2.f - size * 3.f)
		: 1.f - 1.5f * (size - 1.f / 3.f);
	shortness = std::min(1.f, 1.025f * shortness);
	return 4.f * std::pow(2.f, -9.f * shortness);
}

// The firmware's objects are globals, which start zeroed, and their constructors
// leave most members to Init(). Construct them in zeroed memory to match.
template <typename T>
static T* newZeroed() {
	static_assert(alignof(T) <= alignof(std::max_align_t), "calloc alignment too small");
	void* memory = std::calloc(1, sizeof(T));
	return new (memory) T;
}

template <typename T>
static void deleteZeroed(T* p) {
	p->~T();
	std::free(p);
}

// Fastest grain rates, from the firmware's TriggerGenerator
static const float kMaxGrainFrequency = 130.81278f;
static const int kDivisionRatios[] = {1, 2, 3, 4, 6, 8, 12, 16};


struct BeadsPitchQuantity : ParamQuantity {
	float getDisplayValue() override {
		return detuneLaw(getValue());
	}
	void setDisplayValue(float displayValue) override {
		setValue(invertLaw(detuneLaw, displayValue));
	}
	int getDisplayPrecision() override {
		return 3;
	}
	std::string getUnit() override {
		return " semitones";
	}
};


struct BeadsSizeQuantity : ParamQuantity {
	bool reversed() {
		return getValue() < 1.f / 3.f;
	}
	bool delayMode() {
		return getValue() > 0.99f;
	}
	float getDisplayValue() override {
		return grainDuration(getValue()) * 1000.f;
	}
	// Typed lengths keep the current direction
	void setDisplayValue(float displayValue) override {
		float shortness = -std::log2(clamp(displayValue / 1000.f, 1e-3f, 4.f) / 4.f) / 9.f;
		shortness = std::min(shortness / 1.025f, 1.f);
		if (reversed())
			setValue((1.f - std::sqrt(1.f - shortness)) / 3.f);
		else
			setValue(std::min(1.f / 3.f + (1.f - shortness) / 1.5f, 0.98f));
	}
	int getDisplayPrecision() override {
		return 4;
	}
	std::string getUnit() override {
		return " ms";
	}
	std::string getString() override {
		if (delayMode())
			return getLabel() + ": Delay";
		return ParamQuantity::getString();
	}
	std::string getDescription() override {
		return (reversed() && !delayMode()) ? "Reversed" : "";
	}
};


struct BeadsDensityQuantity : ParamQuantity {
	enum Mode {
		REGULAR,
		OFF,
		RANDOM,
		DIVIDE,
		PROBABILITY,
	};

	bool dividerMode();

	// SEED patched and latched turns density into a divider and a probability of the SEED gates
	Mode mode() {
		float d = getValue();
		bool divider = dividerMode();
		if (d < 0.48f)
			return divider ? DIVIDE : REGULAR;
		if (d > 0.52f)
			return divider ? PROBABILITY : RANDOM;
		return OFF;
	}
	float getDisplayValue() override {
		float d = getValue();
		switch (mode()) {
			case REGULAR: return kMaxGrainFrequency * std::pow(2.f, -16.f * d);
			case RANDOM: return 0.5f * kMaxGrainFrequency * std::pow(2.f, -16.f * (1.f - d));
			case DIVIDE: return kDivisionRatios[clamp((int) (d * 2.15f * 8.f), 0, 7)];
			case PROBABILITY: {
				float probabilitySqrt = (d - 0.52f) * 2.15f;
				return std::min(probabilitySqrt * probabilitySqrt, 1.f) * 100.f;
			}
			default: return 0.f;
		}
	}
	// Typed values keep the current side of the knob
	void setDisplayValue(float displayValue) override {
		switch (mode()) {
			case REGULAR:
				setValue(clamp(-std::log2(std::max(displayValue, 1e-3f) / kMaxGrainFrequency) / 16.f, 0.f, 0.47f));
				break;
			case OFF:
			case RANDOM:
				if (displayValue <= 0.f && mode() == OFF)
					break;
				setValue(clamp(1.f + std::log2(std::max(displayValue, 1e-3f) / (0.5f * kMaxGrainFrequency)) / 16.f, 0.53f, 1.f));
				break;
			case DIVIDE: {
				int index = 0;
				for (int i = 0; i < 8; i++) {
					if (std::fabs(kDivisionRatios[i] - displayValue) < std::fabs(kDivisionRatios[index] - displayValue))
						index = i;
				}
				setValue((index + 0.5f) / 8.f / 2.15f);
			} break;
			case PROBABILITY:
				setValue(clamp(0.52f + std::sqrt(clamp(displayValue / 100.f, 0.f, 1.f)) / 2.15f, 0.53f, 1.f));
				break;
		}
	}
	int getDisplayPrecision() override {
		return 3;
	}
	std::string getUnit() override {
		switch (mode()) {
			case REGULAR:
			case RANDOM: return " Hz";
			case PROBABILITY: return "%";
			default: return "";
		}
	}
	std::string getString() override {
		switch (mode()) {
			case OFF: return getLabel() + ": Off";
			case DIVIDE: return getLabel() + ": ÷" + getDisplayValueString();
			default: return ParamQuantity::getString();
		}
	}
	std::string getDescription() override {
		switch (mode()) {
			case REGULAR: return "Regular";
			case RANDOM: return "Random";
			case PROBABILITY: return "SEED gate probability";
			default: return "";
		}
	}
};


struct BeadsAttenurandomizerQuantity : ParamQuantity {
	int cvInput = 0;

	float getDisplayValue() override {
		return attenurandomizerLaw(getValue()) * 100.f;
	}
	void setDisplayValue(float displayValue) override {
		setValue(invertLaw(attenurandomizerLaw, displayValue / 100.f));
	}
	int getDisplayPrecision() override {
		return 3;
	}
	std::string getUnit() override {
		return "%";
	}
	// Behavior of RandomizedParameter::value()
	std::string getDescription() override {
		float amount = attenurandomizerLaw(getValue());
		if (amount == 0.f)
			return "";
		if (module && module->inputs[cvInput].isConnected())
			return amount > 0.f ? "CV" : "Random, CV sets amount";
		return amount > 0.f ? "Random, Uniform" : "Random, Peaky";
	}
};


struct Beads : Module {
	enum ParamIds {
		DENSITY_PARAM,
		TIME_PARAM,
		PITCH_PARAM,
		SIZE_PARAM,
		SHAPE_PARAM,
		FEEDBACK_PARAM,
		DRY_WET_PARAM,
		REVERB_PARAM,
		TIME_AMOUNT_PARAM,
		SIZE_AMOUNT_PARAM,
		SHAPE_AMOUNT_PARAM,
		PITCH_AMOUNT_PARAM,
		FREEZE_PARAM,
		SEED_PARAM,
		QUALITY_PARAM,
		ASSIGN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ASSIGN_INPUT,
		TIME_INPUT,
		SIZE_INPUT,
		SHAPE_INPUT,
		PITCH_INPUT,
		DENSITY_INPUT,
		IN_L_INPUT,
		IN_R_INPUT,
		FREEZE_INPUT,
		SEED_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_L_OUTPUT,
		OUT_R_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		FREEZE_LIGHT,
		SEED_LIGHT,
		ENUMS(QUALITY_LIGHT, 3),
		ENUMS(ASSIGN_LIGHT, 3),
		LEVEL_LIGHT,
		NUM_LIGHTS
	};

	// Firmware switch order, which indexes kLongPressDuration
	enum Switch {
		SWITCH_FREEZE,
		SWITCH_SEED,
		SWITCH_QUALITY,
		SWITCH_ASSIGN,
		NUM_SWITCHES
	};

	enum MixParameter {
		MIX_FEEDBACK,
		MIX_DRY_WET,
		MIX_REVERB,
		NUM_MIX_PARAMETERS
	};

	dsp::SampleRateConverter<2> inputSrc;
	dsp::SampleRateConverter<2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> inputBuffer;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> outputBuffer;

	uint8_t* delayBuffer;
	uint16_t* reverbBuffer;
	beads::Processor* processor;
	beads::Synth* synth;
	beads::GainControl* gainControl;
	// Track the mix knobs, or edit hidden settings while QUALITY or ASSIGN is held.
	beads::PotController mixPots[NUM_MIX_PARAMETERS];

	// Persistent settings, as stored by the firmware.
	// Media index: 0 = scorched cassette, 1 = sunny tape, 2 = cold digital, 3 = bright digital
	int quality = beads::Processor::MEDIA_DIGITAL;
	// 0-2 sends the assignable CV to one mix parameter, 3 uses assignAmounts
	int assignDestination = MIX_FEEDBACK;
	uint8_t assignAmounts[NUM_MIX_PARAMETERS] = {255, 0, 128};
	// 0 enables automatic gain control
	uint8_t inputGain = 0;
	bool gateOutput = false;
	bool seedLocked = true;

	dsp::ClockDivider uiDivider;
	double uiTime = 0.0;
	bool switchPressed[NUM_SWITCHES] = {};
	float switchPressTime[NUM_SWITCHES] = {};
	bool switchIgnoreRelease[NUM_SWITCHES] = {};
	bool freezeButton = false;
	bool seedButton = false;

	bool configured = false;
	bool synthMode = false;
	int transitionBlocks = 0;
	float noInputTime = 0.f;
	dsp::SchmittTrigger freezeTrigger;
	dsp::SchmittTrigger seedTrigger;
	bool blockFreeze = false;
	bool blockSeed = false;
	stmlib::GateFlags previousSeed = stmlib::GATE_FLAG_LOW;
	bool freezeLight = false;
	bool seedLight = false;

	Beads() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<BeadsDensityQuantity>(DENSITY_PARAM, 0.0, 1.0, 0.5, "Grain density");
		configParam(TIME_PARAM, 0.0, 1.0, 0.5, "Grain time");
		configParam<BeadsPitchQuantity>(PITCH_PARAM, 0.0, 1.0, 0.5, "Grain pitch");
		configParam<BeadsSizeQuantity>(SIZE_PARAM, 0.0, 1.0, 0.5, "Grain size");
		configParam(SHAPE_PARAM, 0.0, 1.0, 0.5, "Grain shape");
		configParam(FEEDBACK_PARAM, 0.0, 1.0, 0.0, "Feedback", "%", 0, 100);
		configParam(DRY_WET_PARAM, 0.0, 1.0, 0.5, "Dry/wet", "%", 0, 100);
		configParam(REVERB_PARAM, 0.0, 1.0, 0.0, "Reverb", "%", 0, 100);
		configParam<BeadsAttenurandomizerQuantity>(TIME_AMOUNT_PARAM, 0.0, 1.0, 0.5, "Time attenurandomizer")->cvInput = TIME_INPUT;
		configParam<BeadsAttenurandomizerQuantity>(SIZE_AMOUNT_PARAM, 0.0, 1.0, 0.5, "Size attenurandomizer")->cvInput = SIZE_INPUT;
		configParam<BeadsAttenurandomizerQuantity>(SHAPE_AMOUNT_PARAM, 0.0, 1.0, 0.5, "Shape attenurandomizer")->cvInput = SHAPE_INPUT;
		configParam<BeadsAttenurandomizerQuantity>(PITCH_AMOUNT_PARAM, 0.0, 1.0, 0.5, "Pitch attenurandomizer")->cvInput = PITCH_INPUT;
		configButton(FREEZE_PARAM, "Freeze");
		configButton(SEED_PARAM, "Seed");
		configButton(QUALITY_PARAM, "Quality");
		configButton(ASSIGN_PARAM, "Assignable CV destination");
		configInput(ASSIGN_INPUT, "Assignable CV");
		configInput(TIME_INPUT, "Time");
		configInput(SIZE_INPUT, "Size");
		configInput(SHAPE_INPUT, "Shape");
		configInput(PITCH_INPUT, "Pitch (1V/oct)");
		configInput(DENSITY_INPUT, "Density");
		configInput(IN_L_INPUT, "Left");
		configInput(IN_R_INPUT, "Right");
		configInput(FREEZE_INPUT, "Freeze");
		configInput(SEED_INPUT, "Seed");
		configOutput(OUT_L_OUTPUT, "Left");
		configOutput(OUT_R_OUTPUT, "Right");

		configBypass(IN_L_INPUT, OUT_L_OUTPUT);
		configBypass(IN_R_INPUT, OUT_R_OUTPUT);

		delayBuffer = new uint8_t[768 * 1024]();
		reverbBuffer = new uint16_t[32768]();
		processor = newZeroed<beads::Processor>();
		synth = newZeroed<beads::Synth>();
		gainControl = newZeroed<beads::GainControl>();

		uint8_t* delayBuffers[3] = {
			delayBuffer,
			delayBuffer + 256 * 1024,
			delayBuffer + 512 * 1024,
		};
		processor->Init(delayBuffers, reverbBuffer);
		synth->Init(reverbBuffer);
		gainControl->Init(48000.f, beads::GainControl::SOFT_CLIPPING_MODE_OFF, true);

		for (int i = 0; i < NUM_MIX_PARAMETERS; i++) {
			mixPots[i].Init(1.f, 0.f);
			mixPots[i].ProcessControlRate(params[FEEDBACK_PARAM + i].getValue());
		}

		uiDivider.setDivision(32);
	}

	~Beads() {
		deleteZeroed(gainControl);
		deleteZeroed(synth);
		deleteZeroed(processor);
		delete[] reverbBuffer;
		delete[] delayBuffer;
	}

	void process(const ProcessArgs& args) override {
		if (uiDivider.process()) {
			float dt = args.sampleTime * uiDivider.getDivision();
			pollUi(dt);
			updateLights(dt);
		}

		// Get input
		if (!inputBuffer.full()) {
			// The codec clips at full scale.
			dsp::Frame<2> inputFrame;
			inputFrame.samples[0] = clamp(inputs[IN_L_INPUT].getVoltage() / kInputFullScale, -1.f, 1.f);
			inputFrame.samples[1] = clamp(inputs[IN_R_INPUT].getVoltage() / kInputFullScale, -1.f, 1.f);
			inputBuffer.push(inputFrame);
		}

		// Hold gates until the next block so short triggers are not lost.
		freezeTrigger.process(rescale(inputs[FREEZE_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f));
		blockFreeze |= freezeTrigger.isHigh();
		seedTrigger.process(rescale(inputs[SEED_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f));
		blockSeed |= seedTrigger.isHigh();

		// Render frames
		if (outputBuffer.empty()) {
			renderBlock(args.sampleRate);
		}

		// Set output
		dsp::Frame<2> outputFrame = {};
		if (!outputBuffer.empty()) {
			outputFrame = outputBuffer.shift();
		}
		if (outputs[OUT_R_OUTPUT].isConnected()) {
			outputs[OUT_L_OUTPUT].setVoltage(kOutputFullScale * outputFrame.samples[0]);
			// Audio is not inverted like the hardware's output stage, but the
			// firmware's -0.8 gate still needs inverting to become a positive gate.
			float r = gateOutput ? -outputFrame.samples[1] : outputFrame.samples[1];
			outputs[OUT_R_OUTPUT].setVoltage(kOutputFullScale * r);
		}
		else {
			// L carries a mono mix when R is unpatched.
			float mix = clamp(outputFrame.samples[0] + outputFrame.samples[1], -1.f, 1.f);
			outputs[OUT_L_OUTPUT].setVoltage(kOutputFullScale * mix);
		}
	}

	float mediaSampleRate() {
		return synthMode ? 48000.f : processor->sample_rate();
	}

	// Port of ConfigureAndStartCodec()
	void configure(bool newSynthMode, bool resetGainControl) {
		synthMode = newSynthMode;
		beads::GainControl::SoftClippingMode softClipping = beads::GainControl::SOFT_CLIPPING_MODE_OFF;
		if (processor->media() == beads::Processor::MEDIA_DIRTY_TAPE)
			softClipping = beads::GainControl::SOFT_CLIPPING_MODE_ON;
		else if (processor->media() == beads::Processor::MEDIA_CLEAN_TAPE)
			softClipping = beads::GainControl::SOFT_CLIPPING_MODE_DYNAMIC;
		gainControl->Init(mediaSampleRate(), softClipping, resetGainControl);
		// Blank the output and hold the gain control while the new configuration settles.
		transitionBlocks = kBlockRate / 4;
		noInputTime = 0.f;
		configured = true;
	}

	// Port of MonitorStateChange(): switches to the wavetable synth after 10
	// seconds without audio input, and reconfigures the processor when quality or
	// input patching changes.
	void monitorStateChange(bool inL, bool inR, bool frozen, float blockTime) {
		if (configured && (frozen || transitionBlocks)) {
			noInputTime = 0.f;
			return;
		}

		if (configured && !inL && !inR) {
			if (!synthMode) {
				noInputTime += blockTime;
				if (noInputTime >= kSynthModeDelay) {
					configure(true, true);
				}
			}
		}
		else {
			beads::Processor::Media media = (beads::Processor::Media) quality;
			bool mono = !inL || !inR;
			bool monoChanged = mono != processor->mono();
			if (!configured || synthMode || monoChanged || media != processor->media()) {
				bool resetGainControl = !configured || monoChanged || synthMode;
				processor->Configure(media, mono);
				configure(false, resetGainControl);
			}
			noInputTime = 0.f;
		}
	}

	void renderBlock(float hostSampleRate) {
		bool inL = inputs[IN_L_INPUT].isConnected();
		bool inR = inputs[IN_R_INPUT].isConnected();
		bool frozen = freezeButton || blockFreeze;
		monitorStateChange(inL, inR, frozen, 1.f / kBlockRate);
		float sampleRate = mediaSampleRate();
		int blockSize = (int) sampleRate / kBlockRate;

		// Convert input buffer
		beads::FloatFrame input[kMaxBlockSize] = {};
		{
			inputSrc.setRates(hostSampleRate, sampleRate);
			dsp::Frame<2> inputFrames[kMaxBlockSize];
			int inLen = inputBuffer.size();
			int outLen = blockSize;
			inputSrc.process(inputBuffer.startData(), &inLen, inputFrames, &outLen);
			inputBuffer.startIncr(inLen);

			// We might not fill all of the input buffer if there is a deficiency, but this cannot be avoided due to imprecisions between the input and output SRC.
			for (int i = 0; i < outLen; i++) {
				input[i].l = inputFrames[i].samples[0];
				input[i].r = inputFrames[i].samples[1];
			}
		}

		// Gate events
		bool seedPatched = inputs[SEED_INPUT].isConnected();
		bool freeze[kMaxBlockSize];
		stmlib::GateFlags seed[kMaxBlockSize];
		for (int i = 0; i < blockSize; i++) {
			freeze[i] = frozen;
			previousSeed = seed[i] = stmlib::ExtractGateFlags(previousSeed, seedPatched && blockSeed);
		}
		freezeLight = frozen;
		seedLight = seedPatched && blockSeed && !transitionBlocks;
		blockFreeze = false;
		blockSeed = false;

		beads::FloatFrame output[kMaxBlockSize] = {};
		gainControl->Process(!transitionBlocks, inputGain, !inL, !inR, input, input, blockSize);
		if (transitionBlocks) {
			transitionBlocks--;
		}
		else {
			renderEngine(seedPatched ? seed : NULL, freeze, input, output, blockSize);
		}

		// Convert output buffer
		{
			dsp::Frame<2> outputFrames[kMaxBlockSize];
			for (int i = 0; i < blockSize; i++) {
				float l = clamp(output[i].l, -1.f, 0.999999f);
				float r = clamp(output[i].r, -1.f, 0.999999f);
				// Don't enter the synth mode while a frozen or delayed sound is still playing.
				if (std::fabs(l) > 0.01f || std::fabs(r) > 0.01f) {
					noInputTime = 0.f;
				}
				outputFrames[i].samples[0] = l;
				outputFrames[i].samples[1] = r;
			}

			outputSrc.setRates(sampleRate, hostSampleRate);
			int inLen = blockSize;
			int outLen = outputBuffer.capacity();
			outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
			outputBuffer.endIncr(outLen);
		}
	}

	void setGrainParameter(beads::RandomizedParameter* p, float base, int amountParam, int input) {
		p->base = base;
		p->attenuverter = attenurandomizerLaw(params[amountParam].getValue());
		p->has_cv = inputs[input].isConnected();
		p->cv = clamp(inputs[input].getVoltage() / 5.f, -1.6f, 1.6f);
	}

	void renderEngine(const stmlib::GateFlags* seed, const bool* freeze, const beads::FloatFrame* input, beads::FloatFrame* output, int blockSize) {
		beads::Parameters p;
		setGrainParameter(&p.time, params[TIME_PARAM].getValue(), TIME_AMOUNT_PARAM, TIME_INPUT);
		setGrainParameter(&p.size, params[SIZE_PARAM].getValue(), SIZE_AMOUNT_PARAM, SIZE_INPUT);
		setGrainParameter(&p.shape, params[SHAPE_PARAM].getValue(), SHAPE_AMOUNT_PARAM, SHAPE_INPUT);
		setGrainParameter(&p.pitch, detuneLaw(params[PITCH_PARAM].getValue()), PITCH_AMOUNT_PARAM, PITCH_INPUT);
		// Pitch is in semitones
		p.pitch.cv = clamp(inputs[PITCH_INPUT].getVoltage() * 12.f, -60.f, 60.f);
		p.monophonic_mode = false;

		beads::ModulatedParameter density;
		density.base = params[DENSITY_PARAM].getValue();
		density.cv = inputs[DENSITY_INPUT].isConnected() ? clamp(inputs[DENSITY_INPUT].getVoltage() / 5.f, -1.f, 1.f) : 0.f;

		float assignCv = clamp(inputs[ASSIGN_INPUT].getVoltage() / 5.f, -1.f, 1.f);
		float mix[NUM_MIX_PARAMETERS];
		for (int i = 0; i < NUM_MIX_PARAMETERS; i++) {
			float amount = (assignDestination == 3) ? assignAmounts[i] / 256.f : (assignDestination == i);
			mix[i] = clamp(mixPots[i].main_parameter() * 1.005f + (assignCv - 0.002f) * amount, 0.f, 1.f);
		}

		beads::Processor::MixParameters m;
		m.feedback = mix[MIX_FEEDBACK];
		m.dry_wet = mix[MIX_DRY_WET];
		m.reverb_amount = mix[MIX_REVERB];
		// L carries a mono mix when R is unpatched, which would include the gate.
		m.output_gate = gateOutput && outputs[OUT_R_OUTPUT].isConnected();

		beads::SeedButtonState seedState = beads::SEED_BUTTON_STATE_OFF;
		if (seedLocked)
			seedState = beads::SEED_BUTTON_STATE_LOCKED;
		else if (seedButton)
			seedState = beads::SEED_BUTTON_STATE_ON;

		if (synthMode) {
			synth->Render(seedState, seed, freeze, (beads::Synth::Resolution) quality, density, p, m, output, blockSize);
		}
		else {
			processor->Process(seedState, seed, freeze, density, p, m, input, output, blockSize);
		}
	}

	// Port of Ui::Poll(): long presses fire while held, and combos suppress the release.
	void pollUi(float dt) {
		uiTime += dt;

		for (int i = 0; i < NUM_SWITCHES; i++) {
			bool pressed = params[FREEZE_PARAM + i].getValue() > 0.f;
			if (pressed && !switchPressed[i]) {
				switchPressed[i] = true;
				switchPressTime[i] = 0.f;
				switchIgnoreRelease[i] = false;
				onSwitchPressed(i);
			}
			else if (pressed) {
				switchPressTime[i] += dt;
				if (!switchIgnoreRelease[i] && switchPressTime[i] >= kLongPressDuration[i]) {
					switchIgnoreRelease[i] = true;
					onSwitchReleased(i, switchPressTime[i]);
				}
			}
			else if (switchPressed[i]) {
				switchPressed[i] = false;
				if (!switchIgnoreRelease[i]) {
					switchIgnoreRelease[i] = true;
					onSwitchReleased(i, switchPressTime[i]);
				}
			}
		}

		for (int i = 0; i < NUM_MIX_PARAMETERS; i++) {
			beads::PotController* pot = &mixPots[i];
			pot->ProcessControlRate(params[FEEDBACK_PARAM + i].getValue());
			pot->ProcessUIRate();
			if (pot->editing_hidden_parameter()) {
				if (switchPressed[SWITCH_QUALITY]) {
					// QUALITY + FEEDBACK sets the manual input gain, and must not trigger the long press.
					switchPressTime[SWITCH_QUALITY] = 0.f;
					inputGain = 1 + (uint8_t) (pot->hidden_parameter() * 254.f);
				}
				else if (switchPressed[SWITCH_ASSIGN]) {
					// ASSIGN + mix knob sets that parameter's CV amount.
					assignDestination = 3;
					assignAmounts[i] = (uint8_t) (pot->hidden_parameter() * 255.f);
				}
			}
		}
	}

	// Rack can move the knob, so return a knob that edited a hidden setting to
	// the value in use instead of catching up like the hardware's pot. The saved
	// knob position then matches the sound. Other knobs, which may be under MIDI
	// control or automation, resume tracking where they are.
	void unlockPot(int i) {
		if (mixPots[i].editing_hidden_parameter())
			params[FEEDBACK_PARAM + i].setValue(mixPots[i].main_parameter());
		mixPots[i].Realign();
	}

	void unlockPots() {
		for (int i = 0; i < NUM_MIX_PARAMETERS; i++) {
			unlockPot(i);
		}
	}

	void onSwitchPressed(int s) {
		// SEED + FREEZE locks the seed.
		if (s == SWITCH_FREEZE && switchPressed[SWITCH_SEED]) {
			seedLocked = true;
			seedButton = false;
			switchIgnoreRelease[SWITCH_SEED] = switchIgnoreRelease[SWITCH_FREEZE] = true;
			return;
		}
		// ASSIGN + SEED toggles the gate output on R.
		if (s == SWITCH_SEED && switchPressed[SWITCH_ASSIGN]) {
			gateOutput ^= true;
			unlockPots();
			switchIgnoreRelease[SWITCH_ASSIGN] = switchIgnoreRelease[SWITCH_SEED] = true;
			return;
		}

		switch (s) {
			case SWITCH_FREEZE: {
				freezeButton ^= true;
			} break;
			case SWITCH_SEED: {
				seedLocked = false;
				seedButton = true;
			} break;
			case SWITCH_QUALITY: {
				mixPots[MIX_FEEDBACK].Lock();
			} break;
			case SWITCH_ASSIGN: {
				for (int i = 0; i < NUM_MIX_PARAMETERS; i++) {
					mixPots[i].Lock();
				}
			} break;
		}
	}

	void onSwitchReleased(int s, float pressTime) {
		switch (s) {
			case SWITCH_SEED: {
				if (pressTime >= kLongPressDuration[SWITCH_SEED])
					seedLocked = true;
				seedButton = false;
			} break;
			case SWITCH_QUALITY: {
				if (!mixPots[MIX_FEEDBACK].editing_hidden_parameter()) {
					if (pressTime >= kLongPressDuration[SWITCH_QUALITY]) {
						// Return to automatic gain control.
						inputGain = 0;
						gainControl->Restart();
					}
					else {
						quality = (quality + 3) % 4;
					}
				}
				unlockPot(MIX_FEEDBACK);
			} break;
			case SWITCH_ASSIGN: {
				bool potTouched = false;
				for (int i = 0; i < NUM_MIX_PARAMETERS; i++) {
					potTouched = potTouched || mixPots[i].editing_hidden_parameter();
				}
				unlockPots();
				if (!potTouched) {
					assignDestination = (assignDestination == 3) ? 0 : (assignDestination + 1) % 3;
				}
			} break;
		}
	}

	// Port of Ui::UpdateLeds()
	void updateLights(float dt) {
		int triangle = ((int) (uiTime * 1000.0) & 2047) >> 2;
		triangle = triangle >= 256 ? 511 - triangle : triangle;

		float color[3];
		float brightness;
		bool adjustingManualGain = switchPressed[SWITCH_QUALITY] && mixPots[MIX_FEEDBACK].editing_hidden_parameter();
		if (adjustingManualGain) {
			// Meter from dim green to red
			float value = 0.05f + 0.95f * inputGain / 256.f;
			if (value < 0.25f) {
				color[0] = 0.f;
				color[1] = 1.f;
				brightness = value * 4.f;
			}
			else if (value < 0.5f) {
				color[0] = (value - 0.25f) * 4.f;
				color[1] = 1.f;
				brightness = 1.f;
			}
			else {
				color[0] = 1.f;
				color[1] = 1.f - (value - 0.5f) * 2.f;
				brightness = 1.f;
			}
			color[2] = 0.f;
		}
		else {
			for (int c = 0; c < 3; c++) {
				color[c] = kQualityColors[quality][c] / 255.f;
			}
			// Dim as the frozen buffer is about to be overwritten.
			brightness = synthMode ? 1.f : processor->overwrite_reserve();
		}
		// Rack draws each channel at the square root of its brightness, so this
		// shows each color component scaled by the square root of the LED brightness.
		for (int c = 0; c < 3; c++) {
			lights[QUALITY_LIGHT + c].setBrightness(color[c] * color[c] * brightness);
		}

		lights[FREEZE_LIGHT].setBrightness(freezeLight);
		if (seedLocked) {
			lights[SEED_LIGHT].setBrightness(((triangle * 3 >> 2) + 64) / 255.f);
		}
		else {
			lights[SEED_LIGHT].setBrightness(seedButton || seedLight);
		}

		for (int i = 0; i < NUM_MIX_PARAMETERS; i++) {
			float b = (assignDestination == 3) ? (assignAmounts[i] >> 1) / 255.f : (assignDestination == i) * 128 / 255.f;
			lights[ASSIGN_LIGHT + i].setBrightness(b);
		}

		if (gainControl->adjusting()) {
			// Blink while the automatic gain control is learning the input level.
			lights[LEVEL_LIGHT].setBrightness(((int) (uiTime * 1000.0) & 128) ? 0.5f : 0.f);
		}
		else {
			lights[LEVEL_LIGHT].setSmoothBrightness(gainControl->vu_meter() / 255.f, dt);
		}
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		quality = beads::Processor::MEDIA_DIGITAL;
		assignDestination = MIX_FEEDBACK;
		assignAmounts[MIX_FEEDBACK] = 255;
		assignAmounts[MIX_DRY_WET] = 0;
		assignAmounts[MIX_REVERB] = 128;
		inputGain = 0;
		gateOutput = false;
		seedLocked = true;
		seedButton = false;
		freezeButton = false;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "quality", json_integer(quality));
		json_object_set_new(rootJ, "assignDestination", json_integer(assignDestination));
		json_t* assignAmountsJ = json_array();
		for (int i = 0; i < NUM_MIX_PARAMETERS; i++) {
			json_array_append_new(assignAmountsJ, json_integer(assignAmounts[i]));
		}
		json_object_set_new(rootJ, "assignAmounts", assignAmountsJ);
		json_object_set_new(rootJ, "inputGain", json_integer(inputGain));
		json_object_set_new(rootJ, "gateOutput", json_boolean(gateOutput));
		json_object_set_new(rootJ, "seedLocked", json_boolean(seedLocked));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* qualityJ = json_object_get(rootJ, "quality");
		if (qualityJ)
			quality = clamp((int) json_integer_value(qualityJ), 0, 3);

		json_t* assignDestinationJ = json_object_get(rootJ, "assignDestination");
		if (assignDestinationJ)
			assignDestination = clamp((int) json_integer_value(assignDestinationJ), 0, 3);

		json_t* assignAmountsJ = json_object_get(rootJ, "assignAmounts");
		if (assignAmountsJ) {
			for (int i = 0; i < NUM_MIX_PARAMETERS; i++) {
				json_t* amountJ = json_array_get(assignAmountsJ, i);
				if (amountJ)
					assignAmounts[i] = clamp((int) json_integer_value(amountJ), 0, 255);
			}
		}

		json_t* inputGainJ = json_object_get(rootJ, "inputGain");
		if (inputGainJ)
			inputGain = clamp((int) json_integer_value(inputGainJ), 0, 255);

		json_t* gateOutputJ = json_object_get(rootJ, "gateOutput");
		if (gateOutputJ)
			gateOutput = json_boolean_value(gateOutputJ);

		json_t* seedLockedJ = json_object_get(rootJ, "seedLocked");
		if (seedLockedJ)
			seedLocked = json_boolean_value(seedLockedJ);
	}
};


bool BeadsDensityQuantity::dividerMode() {
	Beads* beads = dynamic_cast<Beads*>(module);
	return beads && beads->seedLocked && beads->inputs[Beads::SEED_INPUT].isConnected();
}


// Context menu equivalents of the button + knob combos
struct BeadsAssignAmountQuantity : Quantity {
	Beads* module;
	int index;

	void setValue(float value) override {
		module->assignAmounts[index] = (uint8_t) std::round(clamp(value, 0.f, 1.f) * 255.f);
		module->assignDestination = 3;
	}
	float getValue() override {
		return module->assignAmounts[index] / 255.f;
	}
	float getDefaultValue() override {
		return 0.f;
	}
	float getDisplayValue() override {
		return getValue() * 100.f;
	}
	void setDisplayValue(float displayValue) override {
		setValue(displayValue / 100.f);
	}
	std::string getLabel() override {
		static const char* labels[] = {"Feedback", "Dry/wet", "Reverb"};
		return labels[index];
	}
	std::string getUnit() override {
		return "%";
	}
};


struct BeadsInputGainQuantity : Quantity {
	Beads* module;

	// Manual gain as computed by GainControl, from 1x to 40x
	static float gainFromSetting(float x) {
		return 1.f + 39.f * x * x * x * x;
	}

	void setValue(float value) override {
		module->inputGain = 1 + (uint8_t) std::round(clamp(value, 0.f, 1.f) * 254.f);
	}
	float getValue() override {
		return module->inputGain ? (module->inputGain - 1) / 254.f : 0.f;
	}
	float getDefaultValue() override {
		return 0.f;
	}
	float getDisplayValue() override {
		return 20.f * std::log10(gainFromSetting(module->inputGain / 255.f));
	}
	void setDisplayValue(float displayValue) override {
		float x = std::pow((std::pow(10.f, displayValue / 20.f) - 1.f) / 39.f, 0.25f);
		setValue((x * 255.f - 1.f) / 254.f);
	}
	int getDisplayPrecision() override {
		return 3;
	}
	std::string getLabel() override {
		return "Manual input gain";
	}
	std::string getUnit() override {
		return " dB";
	}
};


struct BeadsSlider : ui::Slider {
	explicit BeadsSlider(Quantity* quantity) {
		this->quantity = quantity;
		box.size.x = 200.0;
	}
	~BeadsSlider() {
		delete quantity;
	}
};


// The hardware's 8mm cap, as a CKD6 drawn at that size
static const float kButtonScale = mm2px(8.f) / 28.f;

struct BeadsButton : CKD6 {
	BeadsButton() {
		widget::TransformWidget* tw = new widget::TransformWidget;
		tw->scale(kButtonScale);
		fb->removeChild(shadow);
		fb->removeChild(sw);
		tw->addChild(shadow);
		tw->addChild(sw);
		fb->addChild(tw);
		box.size = box.size.mult(kButtonScale);
		fb->box.size = box.size;
		// The framebuffer only renders its children's boxes, so include the
		// shadow, which sits below the cap.
		tw->box.size = tw->getChildrenBoundingBox().getBottomRight().mult(kButtonScale);
	}
};


// Light inside the button, the same proportion of the cap as on Marbles
struct BeadsButtonLight : GreenLight {
	BeadsButtonLight() {
		box.size = Vec(22, 22).mult(kButtonScale);
	}
};


struct BeadsWidget : ModuleWidget {
	explicit BeadsWidget(Beads* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Beads.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<Rogan3PSWhite>(mm2px(Vec(35.555, 24.196)), module, Beads::DENSITY_PARAM));
		addParam(createParamCentered<Rogan3PSWhite>(mm2px(Vec(11.735, 38.496)), module, Beads::TIME_PARAM));
		addParam(createParamCentered<Rogan3PSWhite>(mm2px(Vec(59.385, 38.496)), module, Beads::PITCH_PARAM));
		addParam(createParamCentered<Rogan2PSWhite>(mm2px(Vec(24.435, 60.696)), module, Beads::SIZE_PARAM));
		addParam(createParamCentered<Rogan2PSWhite>(mm2px(Vec(46.685, 60.696)), module, Beads::SHAPE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.935, 60.696)), module, Beads::FEEDBACK_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(64.185, 60.696)), module, Beads::DRY_WET_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(64.185, 76.896)), module, Beads::REVERB_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(18.385, 80.696)), module, Beads::TIME_AMOUNT_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(29.835, 80.696)), module, Beads::SIZE_AMOUNT_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(41.286, 80.696)), module, Beads::SHAPE_AMOUNT_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(52.736, 80.696)), module, Beads::PITCH_AMOUNT_PARAM));
		addParam(createParamCentered<BeadsButton>(mm2px(Vec(8.886, 17.846)), module, Beads::FREEZE_PARAM));
		addParam(createParamCentered<BeadsButton>(mm2px(Vec(62.236, 17.846)), module, Beads::SEED_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(35.555, 45.146)), module, Beads::QUALITY_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(6.936, 76.896)), module, Beads::ASSIGN_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.936, 96.596)), module, Beads::ASSIGN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.386, 96.596)), module, Beads::TIME_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(29.836, 96.596)), module, Beads::SIZE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(41.285, 96.596)), module, Beads::SHAPE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(52.735, 96.596)), module, Beads::PITCH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(64.186, 96.596)), module, Beads::DENSITY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.936, 111.196)), module, Beads::IN_L_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.386, 111.196)), module, Beads::IN_R_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(29.836, 111.196)), module, Beads::FREEZE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(41.285, 111.196)), module, Beads::SEED_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(52.735, 111.196)), module, Beads::OUT_L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(64.186, 111.196)), module, Beads::OUT_R_OUTPUT));

		addChild(createLightCentered<BeadsButtonLight>(mm2px(Vec(8.886, 17.846)), module, Beads::FREEZE_LIGHT));
		addChild(createLightCentered<BeadsButtonLight>(mm2px(Vec(62.236, 17.846)), module, Beads::SEED_LIGHT));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(29.186, 45.146)), module, Beads::QUALITY_LIGHT));
		addChild(createLightCentered<MediumLight<WhiteLight>>(mm2px(Vec(6.936, 69.896)), module, Beads::ASSIGN_LIGHT + 0));
		addChild(createLightCentered<MediumLight<WhiteLight>>(mm2px(Vec(64.185, 69.896)), module, Beads::ASSIGN_LIGHT + 1));
		addChild(createLightCentered<MediumLight<WhiteLight>>(mm2px(Vec(64.185, 86.096)), module, Beads::ASSIGN_LIGHT + 2));
		addChild(createLightCentered<MediumLight<WhiteLight>>(mm2px(Vec(12.686, 104.846)), module, Beads::LEVEL_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		Beads* module = dynamic_cast<Beads*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Quality"));

		// Listed in the order of the panel's quality bars, top to bottom
		static const std::vector<std::string> qualityLabels = {
			"Bright digital",
			"Cold digital",
			"Sunny tape",
			"Scorched cassette",
		};
		for (int i = 0; i < (int) qualityLabels.size(); i++) {
			menu->addChild(createCheckMenuItem(qualityLabels[i], "",
				[=]() {return module->quality == 3 - i;},
				[=]() {module->quality = 3 - i;}
			));
		}

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Assignable CV destination"));

		static const std::vector<std::string> assignLabels = {
			"Feedback",
			"Dry/wet",
			"Reverb",
			"Custom amounts",
		};
		for (int i = 0; i < (int) assignLabels.size(); i++) {
			menu->addChild(createCheckMenuItem(assignLabels[i], "",
				[=]() {return module->assignDestination == i;},
				[=]() {module->assignDestination = i;}
			));
		}
		for (int i = 0; i < Beads::NUM_MIX_PARAMETERS; i++) {
			BeadsAssignAmountQuantity* quantity = new BeadsAssignAmountQuantity;
			quantity->module = module;
			quantity->index = i;
			menu->addChild(new BeadsSlider(quantity));
		}

		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolMenuItem("Automatic input gain", "",
			[=]() {return module->inputGain == 0;},
			[=](bool autoGain) {
				if (autoGain) {
					module->inputGain = 0;
					module->gainControl->Restart();
				}
				else {
					module->inputGain = 1;
				}
			}
		));
		BeadsInputGainQuantity* gainQuantity = new BeadsInputGainQuantity;
		gainQuantity->module = module;
		menu->addChild(new BeadsSlider(gainQuantity));

		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Latch seed", "", &module->seedLocked));
		menu->addChild(createBoolPtrMenuItem("Grain gates on R output", "", &module->gateOutput));
	}
};


Model* modelBeads = createModel<Beads, BeadsWidget>("Beads");
