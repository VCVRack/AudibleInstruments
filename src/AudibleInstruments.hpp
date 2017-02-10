#include "rack.hpp"


using namespace rack;

////////////////////
// knobs
////////////////////

struct AudibleKnob : SpriteKnob {
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
		spriteImage = Image::load("plugins/AudibleInstruments/res/knob_huge_glow.png");
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
		spriteImage = Image::load("plugins/AudibleInstruments/res/knob_large_white.png");
	}
};

struct LargeRedKnob : LargeKnob {
	LargeRedKnob() {
		spriteImage = Image::load("plugins/AudibleInstruments/res/knob_large_red.png");
	}
};

struct LargeGreenKnob : LargeKnob {
	LargeGreenKnob() {
		spriteImage = Image::load("plugins/AudibleInstruments/res/knob_large_green.png");
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
		spriteImage = Image::load("plugins/AudibleInstruments/res/knob_medium_white.png");
	}
};

struct MediumRedKnob : MediumKnob {
	MediumRedKnob() {
		spriteImage = Image::load("plugins/AudibleInstruments/res/knob_medium_red.png");
	}
};

struct MediumGreenKnob : MediumKnob {
	MediumGreenKnob() {
		spriteImage = Image::load("plugins/AudibleInstruments/res/knob_medium_green.png");
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
		spriteImage = Image::load("plugins/AudibleInstruments/res/knob_small_white.png");
	}
};

struct SmallRedKnob : SmallKnob {
	SmallRedKnob() {
		spriteImage = Image::load("plugins/AudibleInstruments/res/knob_small_red.png");
	}
};

struct SmallGreenKnob : SmallKnob {
	SmallGreenKnob() {
		spriteImage = Image::load("plugins/AudibleInstruments/res/knob_small_green.png");
	}
};

struct TinyBlackKnob : AudibleKnob {
	TinyBlackKnob() {
		box.size = Vec(18, 18);
		spriteOffset = Vec(-5, -3);
		spriteSize = Vec(31, 30);
		spriteImage = Image::load("plugins/AudibleInstruments/res/knob_tiny_black.png");
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
		spriteImage = Image::load("plugins/AudibleInstruments/res/button_large_black.png");
	}
};

struct MediumSwitch : virtual Switch {
	MediumSwitch() {
		box.size = Vec(15, 15);
		spriteOffset = Vec(-4, -2);
		spriteSize = Vec(25, 25);
		spriteImage = Image::load("plugins/AudibleInstruments/res/button_medium_black.png");
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
		spriteImage = Image::load("plugins/AudibleInstruments/res/slide_switch.png");
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

struct TripleModeLight : ValueLight {
	void step() {
		float v = roundf(getf(value));
		if (v == 0.0)
			color = SCHEME_CYAN;
		else if (v == 1.0)
			color = SCHEME_ORANGE;
		else
			color = SCHEME_RED;
	}
};

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
