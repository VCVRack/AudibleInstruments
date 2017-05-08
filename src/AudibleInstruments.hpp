#include "rack.hpp"


using namespace rack;


struct TripleModeLight : ValueLight {
	void step() {
		int mode = (int)roundf(getf(value));
		if (mode == 0)
			color = nvgRGB(0x22, 0xe6, 0xef);
		else if (mode == 1)
			color = nvgRGB(0xf2, 0xb1, 0x20);
		else
			color = nvgRGB(0xed, 0x2c, 0x24);
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
