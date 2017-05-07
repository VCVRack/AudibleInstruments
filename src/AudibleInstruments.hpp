#include "rack.hpp"


using namespace rack;


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
