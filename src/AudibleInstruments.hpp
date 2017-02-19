#include "rack.hpp"


using namespace rack;

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
			color = colors[COLOR_CYAN];
		else if (v == 1.0)
			color = colors[COLOR_ORANGE];
		else
			color = colors[COLOR_RED];
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
