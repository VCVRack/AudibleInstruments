#include "AudibleInstruments.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	plugin->slug = "AudibleInstruments";
	plugin->name = "Audible Instruments";
	plugin->homepageUrl = "https://github.com/VCVRack/AudibleInstruments";
	createModel<BraidsWidget>(plugin, "Braids", "Macro Oscillator");
	createModel<ElementsWidget>(plugin, "Elements", "Modal Synthesizer");
	createModel<TidesWidget>(plugin, "Tides", "Tidal Modulator");
	createModel<SheepWidget>(plugin, "Sheep", "Wavetable Oscillator");
	// createModel<StreamsWidget>(plugin, "Streams", "Dual Dynamics Gate");
	createModel<CloudsWidget>(plugin, "Clouds", "Texture Synthesizer");
	createModel<WarpsWidget>(plugin, "Warps", "Meta Modulator");
	createModel<RingsWidget>(plugin, "Rings", "Resonator");
	createModel<LinksWidget>(plugin, "Links", "Multiples");
	createModel<KinksWidget>(plugin, "Kinks", "Utilities");
	createModel<ShadesWidget>(plugin, "Shades", "Mixer");
	createModel<BranchesWidget>(plugin, "Branches", "Bernoulli Gate");
	createModel<BlindsWidget>(plugin, "Blinds", "Quad VC-polarizer");
	createModel<VeilsWidget>(plugin, "Veils", "Quad VCA");
	createModel<FramesWidget>(plugin, "Frames", "Keyframer/Mixer");
}
