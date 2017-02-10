#include "AudibleInstruments.hpp"


// void ModeLight::step() {
// 	int mode = (int) roundf(getf(value));
// 	switch (mode) {
// 		case 0: color = nvgHSL(0.36, 0.7, 0.7); break;
// 		case 1: color = nvgHSL(0.18, 0.7, 0.7); break;
// 		case 2: color = nvgHSL(0.0, 0.7, 0.7); break;
// 		default: color = nvgRGBAf(0.0, 0.0, 0.0, 0.0); break;
// 	}
// }


struct AudibleInstrumentsPlugin : Plugin {
	AudibleInstrumentsPlugin() {
		slug = "AudibleInstruments";
		name = "Audible Instruments";
		createModel<BraidsWidget>(this, "Braids", "Macro Oscillator");
		createModel<ElementsWidget>(this, "Elements", "Modal Synthesizer");
		createModel<TidesWidget>(this, "Tides", "Tidal Modulator");
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
