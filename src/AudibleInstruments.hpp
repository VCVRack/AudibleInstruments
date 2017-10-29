#include "rack.hpp"


using namespace rack;


extern Plugin *plugin;

////////////////////
// module widgets
////////////////////

struct BraidsWidget : ModuleWidget {
	BraidsWidget();
	Menu *createContextMenu() override;
};

struct ElementsWidget : ModuleWidget {
	ElementsWidget();
	Menu *createContextMenu() override;
};

struct TidesWidget : ModuleWidget {
	TidesWidget();
};

struct SheepWidget : TidesWidget {
	SheepWidget();
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
	Menu *createContextMenu() override;
};

struct BlindsWidget : ModuleWidget {
	BlindsWidget();
};

struct VeilsWidget : ModuleWidget {
	VeilsWidget();
};

struct FramesWidget : ModuleWidget {
	FramesWidget();
	Menu *createContextMenu() override;
};
