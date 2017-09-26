#include <string.h>
#include "AudibleInstruments.hpp"
#include "dsp.hpp"
#include "braids/macro_oscillator.h"


struct Braids : Module {
	enum ParamIds {
		FINE_PARAM,
		COARSE_PARAM,
		FM_PARAM,
		TIMBRE_PARAM,
		MODULATION_PARAM,
		COLOR_PARAM,
		SHAPE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TRIG_INPUT,
		PITCH_INPUT,
		FM_INPUT,
		TIMBRE_INPUT,
		COLOR_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};

	braids::MacroOscillator *osc;
	SampleRateConverter<1> src;
	DoubleRingBuffer<Frame<1>, 256> outputBuffer;
	bool lastTrig = false;

	Braids();
	~Braids();
	void step();
	void setShape(int shape);
};


Braids::Braids() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);

	osc = new braids::MacroOscillator();
	memset(osc, 0, sizeof(*osc));
	osc->Init();
}

Braids::~Braids() {
	delete osc;
}

void Braids::step() {
	// Trigger
	bool trig = getf(inputs[TRIG_INPUT]) >= 1.0;
	if (!lastTrig && trig) {
		osc->Strike();
	}
	lastTrig = trig;

	// Render frames
	if (outputBuffer.empty()) {
		// Set shape
		int shape = roundf(params[SHAPE_PARAM]);
		osc->set_shape((braids::MacroOscillatorShape) shape);

		// Set timbre/modulation
		float timbre = params[TIMBRE_PARAM] + params[MODULATION_PARAM] * getf(inputs[TIMBRE_INPUT]) / 5.0;
		float modulation = params[COLOR_PARAM] + getf(inputs[COLOR_INPUT]) / 5.0;
		int16_t param1 = rescalef(clampf(timbre, 0.0, 1.0), 0.0, 1.0, 0, INT16_MAX);
		int16_t param2 = rescalef(clampf(modulation, 0.0, 1.0), 0.0, 1.0, 0, INT16_MAX);
		osc->set_parameters(param1, param2);

		// Set pitch
		float pitch = getf(inputs[PITCH_INPUT]) + params[COARSE_PARAM] + params[FINE_PARAM] / 12.0 + params[FM_PARAM] * getf(inputs[FM_INPUT]);
		int16_t p = clampf((pitch * 12.0 + 60) * 128, 0, INT16_MAX);
		osc->set_pitch(p);

		// TODO: add a sync input buffer (must be sample rate converted)
		uint8_t sync_buffer[24] = {};

		int16_t render_buffer[24];
		osc->Render(sync_buffer, render_buffer, 24);

		// Sample rate convert
		Frame<1> in[24];
		for (int i = 0; i < 24; i++) {
			in[i].samples[0] = render_buffer[i] / 32768.0;
		}
		src.setRatio(gSampleRate / 96000.0);

		int inLen = 24;
		int outLen = outputBuffer.capacity();
		src.process(in, &inLen, outputBuffer.endData(), &outLen);
		outputBuffer.endIncr(outLen);
	}

	// Output
	if (!outputBuffer.empty()) {
		Frame<1> f = outputBuffer.shift();
		setf(outputs[OUT_OUTPUT], 5.0 * f.samples[0]);
	}
}


static const char *algo_values[] = {
	"CSAW",
	"/\\-_",
	"//-_",
	"FOLD",
	"uuuu",
	"SUB-",
	"SUB/",
	"SYN-",
	"SYN/",
	"//x3",
	"-_x3",
	"/\\x3",
	"SIx3",
	"RING",
	"////",
	"//uu",
	"TOY*",
	"ZLPF",
	"ZPKF",
	"ZBPF",
	"ZHPF",
	"VOSM",
	"VOWL",
	"VFOF",
	"HARM",
	"FM  ",
	"FBFM",
	"WTFM",
	"PLUK",
	"BOWD",
	"BLOW",
	"FLUT",
	"BELL",
	"DRUM",
	"KICK",
	"CYMB",
	"SNAR",
	"WTBL",
	"WMAP",
	"WLIN",
	"WTx4",
	"NOIS",
	"TWNQ",
	"CLKN",
	"CLOU",
	"PRTC",
	"QPSK",
	"    ",
};

struct BraidsDisplay : TransparentWidget {
	float *value;
	std::shared_ptr<Font> font;

	BraidsDisplay() {
		font = Font::load(assetPlugin(plugin, "res/hdad-segment14-1.002/Segment14.ttf"));
	}

	void draw(NVGcontext *vg) {
		int shape = roundf(getf(value));

		// Background
		NVGcolor backgroundColor = nvgRGB(0x38, 0x38, 0x38);
		NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0.0, 0.0, box.size.x, box.size.y, 5.0);
		nvgFillColor(vg, backgroundColor);
		nvgFill(vg);
		nvgStrokeWidth(vg, 1.0);
		nvgStrokeColor(vg, borderColor);
		nvgStroke(vg);

		nvgFontSize(vg, 36);
		nvgFontFaceId(vg, font->handle);
		nvgTextLetterSpacing(vg, 2.5);

		Vec textPos = Vec(10, 48);
		NVGcolor textColor = nvgRGB(0xaf, 0xd2, 0x2c);
		nvgFillColor(vg, nvgTransRGBA(textColor, 16));
		nvgText(vg, textPos.x, textPos.y, "~~~~", NULL);
		nvgFillColor(vg, textColor);
		nvgText(vg, textPos.x, textPos.y, algo_values[shape], NULL);
	}
};


BraidsWidget::BraidsWidget() {
	Braids *module = new Braids();
	setModule(module);
	box.size = Vec(15*16, 380);

	{
		Panel *panel = new LightPanel();
		panel->backgroundImage = Image::load(assetPlugin(plugin, "res/Braids.png"));
		panel->box.size = box.size;
		addChild(panel);
	}

	{
		BraidsDisplay *display = new BraidsDisplay();
		display->box.pos = Vec(14, 53);
		display->box.size = Vec(148, 56);
		display->value = &module->params[Braids::SHAPE_PARAM];
		addChild(display);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(210, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(210, 365)));

	addParam(createParam<Rogan2SGray>(Vec(177, 60), module, Braids::SHAPE_PARAM, 0.0, braids::MACRO_OSC_SHAPE_LAST-2, 0.0));

	addParam(createParam<Rogan2PSWhite>(Vec(20, 139), module, Braids::FINE_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Rogan2PSWhite>(Vec(98, 139), module, Braids::COARSE_PARAM, -2.0, 2.0, 0.0));
	addParam(createParam<Rogan2PSWhite>(Vec(177, 139), module, Braids::FM_PARAM, -1.0, 1.0, 0.0));

	addParam(createParam<Rogan2PSGreen>(Vec(20, 218), module, Braids::TIMBRE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Rogan2PSGreen>(Vec(98, 218), module, Braids::MODULATION_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Rogan2PSRed>(Vec(177, 218), module, Braids::COLOR_PARAM, 0.0, 1.0, 0.5));

	addInput(createInput<PJ3410Port>(Vec(7, 313), module, Braids::TRIG_INPUT));
	addInput(createInput<PJ3410Port>(Vec(45, 313), module, Braids::PITCH_INPUT));
	addInput(createInput<PJ3410Port>(Vec(82, 313), module, Braids::FM_INPUT));
	addInput(createInput<PJ3410Port>(Vec(120, 313), module, Braids::TIMBRE_INPUT));
	addInput(createInput<PJ3410Port>(Vec(157, 313), module, Braids::COLOR_INPUT));
	addOutput(createOutput<PJ3410Port>(Vec(202, 313), module, Braids::OUT_OUTPUT));
}
