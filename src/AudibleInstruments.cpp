#include "AudibleInstruments.hpp"


void ValueLight::step() {
	float v = getf(value);
	if (v > 0) {
		color = nvgHSL(0.36, 0.7, 0.7);
		color.a = v; // May be larger than 1
	}
	else if (v < 0) {
		color = nvgHSL(0.0, 0.7, 0.7);
		color.a = -v;
	}
	else {
		color = nvgRGBAf(1.0, 1.0, 1.0, 0.0);
	}
}


void ModeLight::step() {
	int mode = (int) roundf(getf(value));
	switch (mode) {
		case 0: color = nvgHSL(0.36, 0.7, 0.7); break;
		case 1: color = nvgHSL(0.18, 0.7, 0.7); break;
		case 2: color = nvgHSL(0.0, 0.7, 0.7); break;
		default: color = nvgRGBAf(0.0, 0.0, 0.0, 0.0); break;
	}
}



Plugin *init() {
	Plugin *plugin = createPlugin("Audible Instruments", "Audible Instruments");
	createModel<BraidsWidget>(plugin, "Braids", "Macro Oscillator");
	createModel<ElementsWidget>(plugin, "Elements", "Modal Synthesizer");
	createModel<TidesWidget>(plugin, "Tides", "Tidal Modulator");
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
	return plugin;
}
