#include "AudibleInstruments.hpp"
#include <string.h>
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
	int bufferFrame = 0;
	int16_t buffer[24] = {};
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
	// TODO Sample rate convert from 96000Hz
	setf(outputs[OUT_OUTPUT], 5.0*(buffer[bufferFrame] / 32768.0));

	// Trigger
	bool trig = getf(inputs[TRIG_INPUT]) >= 1.0;
	if (!lastTrig && trig) {
		osc->Strike();
	}
	lastTrig = trig;

	if (++bufferFrame >= 24) {
		bufferFrame = 0;
		// Set shape
		int shape = roundf(params[SHAPE_PARAM]);
		osc->set_shape((braids::MacroOscillatorShape) shape);

		// Set timbre/modulation
		float timbre = params[TIMBRE_PARAM] + params[MODULATION_PARAM] * getf(inputs[TIMBRE_INPUT]) / 5.0;
		float modulation = params[COLOR_PARAM] + getf(inputs[COLOR_INPUT]) / 5.0;
		int16_t param1 = mapf(clampf(timbre, 0.0, 1.0), 0.0, 1.0, 0, INT16_MAX);
		int16_t param2 = mapf(clampf(modulation, 0.0, 1.0), 0.0, 1.0, 0, INT16_MAX);
		osc->set_parameters(param1, param2);

		// Set pitch
		float pitch = getf(inputs[PITCH_INPUT]) + params[COARSE_PARAM] + params[FINE_PARAM] / 12.0 + params[FM_PARAM] * getf(inputs[FM_INPUT]);
		int16_t p = clampf((pitch * 12.0 + 60) * 128, 0, INT16_MAX);
		osc->set_pitch(p);

		uint8_t sync_buffer[24] = {};
		osc->Render(sync_buffer, buffer, 24);
	}
}


static const char *algo_values[] = {
	"CSAW",
	"/\\-_",
	"//-_",
	"FOLD",
	"uuuu",
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
	void draw(NVGcontext *vg) {
		int shape = roundf(getf(value));

		int font = loadFont("plugins/AudibleInstruments/res/hdad-segment14-1.002/Segment14.ttf");
		nvgFontSize(vg, 36);
		nvgFontFaceId(vg, font);
		nvgTextLetterSpacing(vg, 2.5);

		NVGcolor color = nvgRGB(0xaf, 0xd2, 0x2c);
		nvgFillColor(vg, nvgTransRGBA(color, 16));
		nvgText(vg, box.pos.x, box.pos.y, "~~~~", NULL);
		nvgFillColor(vg, color);
		nvgText(vg, box.pos.x, box.pos.y, algo_values[shape], NULL);
	}
};


BraidsWidget::BraidsWidget() : ModuleWidget(new Braids()) {
	box.size = Vec(15*16, 380);

	{
		AudiblePanel *panel = new AudiblePanel();
		panel->imageFilename = "plugins/AudibleInstruments/res/Braids.png";
		panel->box.size = box.size;
		addChild(panel);
	}

	{
		BraidsDisplay *display = new BraidsDisplay();
		display->box.pos = Vec(24, 101);
		display->value = &module->params[Braids::SHAPE_PARAM];
		addChild(display);
	}

	addChild(createScrew(Vec(15, 0)));
	addChild(createScrew(Vec(210, 0)));
	addChild(createScrew(Vec(15, 365)));
	addChild(createScrew(Vec(210, 365)));

	addParam(createParam<MediumWhiteKnob>(Vec(187-10, 71-11), module, Braids::SHAPE_PARAM, 0.0, braids::MACRO_OSC_SHAPE_LAST-2, 0.0));

	addParam(createParam<MediumWhiteKnob>(Vec(30-10, 150-11), module, Braids::FINE_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<MediumWhiteKnob>(Vec(108-10, 150-11), module, Braids::COARSE_PARAM, -2.0, 2.0, 0.0));
	addParam(createParam<MediumWhiteKnob>(Vec(187-10, 150-11), module, Braids::FM_PARAM, -1.0, 1.0, 0.0));

	addParam(createParam<MediumGreenKnob>(Vec(30-10, 229-11), module, Braids::TIMBRE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<MediumGreenKnob>(Vec(108-10, 229-11), module, Braids::MODULATION_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<MediumRedKnob>(Vec(187-10, 229-11), module, Braids::COLOR_PARAM, 0.0, 1.0, 0.5));

	addInput(createInput(Vec(12, 318), module, Braids::TRIG_INPUT));
	addInput(createInput(Vec(50, 318), module, Braids::PITCH_INPUT));
	addInput(createInput(Vec(87, 318), module, Braids::FM_INPUT));
	addInput(createInput(Vec(125, 318), module, Braids::TIMBRE_INPUT));
	addInput(createInput(Vec(162, 318), module, Braids::COLOR_INPUT));
	addOutput(createOutput(Vec(207, 318), module, Braids::OUT_OUTPUT));
}
