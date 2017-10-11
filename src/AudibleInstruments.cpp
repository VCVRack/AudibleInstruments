#include "AudibleInstruments.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	plugin->slug = "AudibleInstruments";
	plugin->name = "Audible Instruments";
	plugin->homepageUrl = "https://github.com/VCVRack/AudibleInstruments";
	createModel<BraidsWidget>(plugin, "Braids", "vBraids");
	createModel<ElementsWidget>(plugin, "Elements", "vElements");
	createModel<TidesWidget>(plugin, "Tides", "vTides");
	createModel<SheepWidget>(plugin, "Sheep", "vSheep");
	// createModel<StreamsWidget>(plugin, "Streams", "vStreams");
	createModel<CloudsWidget>(plugin, "Clouds", "vClouds");
	createModel<WarpsWidget>(plugin, "Warps", "vWarps");
	createModel<RingsWidget>(plugin, "Rings", "vRings");
	createModel<LinksWidget>(plugin, "Links", "vLinks");
	createModel<KinksWidget>(plugin, "Kinks", "vKinks");
	createModel<ShadesWidget>(plugin, "Shades", "vShades");
	createModel<BranchesWidget>(plugin, "Branches", "vBranches");
	createModel<BlindsWidget>(plugin, "Blinds", "vBlinds");
	createModel<VeilsWidget>(plugin, "Veils", "vVeils");
	// createModel<FramesWidget>(plugin, "Frames", "Keyframer/Mixer");
}
