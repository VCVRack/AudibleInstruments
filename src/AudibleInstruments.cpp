#include "AudibleInstruments.hpp"


struct AudibleInstrumentsPlugin : Plugin {
	AudibleInstrumentsPlugin() {
		slug = "AudibleInstruments";
		name = "Audible Instruments";
		createModel<BraidsWidget>(this, "Braids", "Macro Oscillator");
		createModel<ElementsWidget>(this, "Elements", "Modal Synthesizer");
		createModel<TidesWidget>(this, "Tides", "Tidal Modulator");
		createModel<SheepWidget>(this, "Sheep", "Wavetable Oscillator");
		// createModel<StreamsWidget>(this, "Streams", "Dual Dynamics Gate");
		createModel<CloudsWidget>(this, "Clouds", "Texture Synthesizer");
		createModel<WarpsWidget>(this, "Warps", "Meta Modulator");
		createModel<RingsWidget>(this, "Rings", "Resonator");
		createModel<LinksWidget>(this, "Links", "Multiples");
		createModel<KinksWidget>(this, "Kinks", "Utilities");
		createModel<ShadesWidget>(this, "Shades", "Mixer");
		createModel<BranchesWidget>(this, "Branches", "Bernoulli Gate");
		createModel<BlindsWidget>(this, "Blinds", "Quad VC-polarizer");
		createModel<VeilsWidget>(this, "Veils", "Quad VCA");
	}
};


Plugin *init() {
	return new AudibleInstrumentsPlugin();
}
