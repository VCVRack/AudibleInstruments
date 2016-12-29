#include "Rack.hpp"


using namespace rack;

////////////////////
// helpers
////////////////////

struct AudiblePanel : ModulePanel {
	AudiblePanel() {
		backgroundColor = nvgRGBf(0.85, 0.85, 0.85);
		highlightColor = nvgRGBf(0.90, 0.90, 0.90);
	}
};

////////////////////
// knobs
////////////////////

struct AudibleKnob : Knob {
	AudibleKnob() {
		minIndex = 44;
		maxIndex = -46;
		spriteCount = 120;
	}
};

struct HugeGlowKnob : AudibleKnob {
	HugeGlowKnob() {
		box.size = Vec(91, 91);
		spriteOffset = Vec(-10, -9);
		spriteSize = Vec(110, 110);
		spriteFilename = "plugins/AudibleInstruments/res/knob_huge_glow.png";
		minIndex = 44+5;
		maxIndex = -46-5;
	}
};

struct LargeKnob : AudibleKnob {
	LargeKnob() {
		box.size = Vec(52, 52);
		spriteOffset = Vec(-5, -4);
		spriteSize = Vec(65, 66);
	}
};

struct LargeWhiteKnob : LargeKnob {
	LargeWhiteKnob() {
		spriteFilename = "plugins/AudibleInstruments/res/knob_large_white.png";
	}
};

struct LargeRedKnob : LargeKnob {
	LargeRedKnob() {
		spriteFilename = "plugins/AudibleInstruments/res/knob_large_red.png";
	}
};

struct LargeGreenKnob : LargeKnob {
	LargeGreenKnob() {
		spriteFilename = "plugins/AudibleInstruments/res/knob_large_green.png";
	}
};

struct MediumKnob : AudibleKnob {
	MediumKnob() {
		box.size = Vec(44, 44);
		spriteOffset = Vec(-5, -4);
		spriteSize = Vec(57, 58);
	}
};

struct MediumWhiteKnob : MediumKnob {
	MediumWhiteKnob() {
		spriteFilename = "plugins/AudibleInstruments/res/knob_medium_white.png";
	}
};

struct MediumRedKnob : MediumKnob {
	MediumRedKnob() {
		spriteFilename = "plugins/AudibleInstruments/res/knob_medium_red.png";
	}
};

struct MediumGreenKnob : MediumKnob {
	MediumGreenKnob() {
		spriteFilename = "plugins/AudibleInstruments/res/knob_medium_green.png";
	}
};

struct SmallKnob : AudibleKnob {
	SmallKnob() {
		box.size = Vec(40, 40);
		spriteOffset = Vec(-5, -4);
		spriteSize = Vec(53, 54);
	}
};

struct SmallWhiteKnob : SmallKnob {
	SmallWhiteKnob() {
		spriteFilename = "plugins/AudibleInstruments/res/knob_small_white.png";
	}
};

struct SmallRedKnob : SmallKnob {
	SmallRedKnob() {
		spriteFilename = "plugins/AudibleInstruments/res/knob_small_red.png";
	}
};

struct SmallGreenKnob : SmallKnob {
	SmallGreenKnob() {
		spriteFilename = "plugins/AudibleInstruments/res/knob_small_green.png";
	}
};

struct TinyBlackKnob : AudibleKnob {
	TinyBlackKnob() {
		box.size = Vec(18, 18);
		spriteOffset = Vec(-5, -3);
		spriteSize = Vec(31, 30);
		spriteFilename = "plugins/AudibleInstruments/res/knob_tiny_black.png";
	}
};

////////////////////
// switches
////////////////////

struct LargeSwitch : virtual Switch {
	LargeSwitch() {
		box.size = Vec(27, 27);
		spriteOffset = Vec(-3, -2);
		spriteSize = Vec(36, 36);
		spriteFilename = "plugins/AudibleInstruments/res/button_large_black.png";
	}
};

struct MediumSwitch : virtual Switch {
	MediumSwitch() {
		box.size = Vec(15, 15);
		spriteOffset = Vec(-4, -2);
		spriteSize = Vec(25, 25);
		spriteFilename = "plugins/AudibleInstruments/res/button_medium_black.png";
	}
};

struct LargeMomentarySwitch : LargeSwitch, MomentarySwitch {};
struct MediumMomentarySwitch : MediumSwitch, MomentarySwitch {};
struct LargeToggleSwitch : LargeSwitch, ToggleSwitch {};
struct MediumToggleSwitch : MediumSwitch, ToggleSwitch {};

struct SlideSwitch : Switch {
	SlideSwitch() {
		box.size = Vec(11, 21);
		spriteOffset = Vec(-1, -1);
		spriteSize = Vec(12, 22);
		spriteFilename = "plugins/AudibleInstruments/res/slide_switch.png";
	}

	void step() {
		index = (value == minValue) ? 0 : 1;
	}

	void onDragDrop(Widget *origin) {
		if (origin != this)
			return;

		if (value == minValue) {
			setValue(maxValue);
		}
		else {
			setValue(minValue);
		}
	}
};

////////////////////
// lights
////////////////////

struct ValueLight : virtual Light {
	float *value = NULL;
	void step();
};

struct ModeLight : ValueLight {
	void step();
};

struct SmallLight : virtual Light {
	SmallLight() {
		box.size = Vec(7, 7);
		spriteOffset = Vec(-16, -16);
		spriteSize = Vec(38, 38);
		spriteFilename = "plugins/AudibleInstruments/res/light_small.png";
	}
};

struct MediumLight : virtual Light {
	MediumLight() {
		box.size = Vec(14, 14);
		spriteOffset = Vec(-16, -15);
		spriteSize = Vec(45, 45);
		spriteFilename = "plugins/AudibleInstruments/res/light_medium.png";
	}
};

struct SmallValueLight : SmallLight, ValueLight {};
struct MediumValueLight : MediumLight, ValueLight {};
struct SmallModeLight : SmallLight, ModeLight {};

template <class TLight>
ValueLight *createValueLight(Vec pos, float *value) {
	ValueLight *light = new TLight();
	light->box.pos = pos;
	light->value = value;
	return light;
}

////////////////////
// module widgets
////////////////////

struct BraidsWidget : ModuleWidget {
	BraidsWidget();
};

struct ElementsWidget : ModuleWidget {
	ElementsWidget();
};

struct TidesWidget : ModuleWidget {
	TidesWidget();
};

struct StreamsWidget : ModuleWidget {
	StreamsWidget();
};

struct CloudsWidget : ModuleWidget {
	CloudsWidget();
};

struct WarpsWidget : ModuleWidget {
	WarpsWidget();
};

struct RingsWidget : ModuleWidget {
	RingsWidget();
};

struct LinksWidget : ModuleWidget {
	LinksWidget();
};

struct KinksWidget : ModuleWidget {
	KinksWidget();
};

struct ShadesWidget : ModuleWidget {
	ShadesWidget();
};

struct BranchesWidget : ModuleWidget {
	BranchesWidget();
};

struct BlindsWidget : ModuleWidget {
	BlindsWidget();
};

struct VeilsWidget : ModuleWidget {
	VeilsWidget();
};
