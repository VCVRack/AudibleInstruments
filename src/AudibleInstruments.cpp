#include "AudibleInstruments.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = "AudibleInstruments";
#ifdef VERSION
	p->version = TOSTRING(VERSION);
#endif

	p->addModel(createModel<BraidsWidget>("Audible Instruments", "Braids", "Macro Oscillator"));
	p->addModel(createModel<ElementsWidget>("Audible Instruments", "Elements", "Modal Synthesizer"));
	p->addModel(createModel<TidesWidget>("Audible Instruments", "Tides", "Tidal Modulator"));
	p->addModel(createModel<SheepWidget>("Audible Instruments", "Sheep", "Wavetable Oscillator"));
	// p->addModel(createModel<StreamsWidget>("Audible Instruments", "Streams", "Dual Dynamics Gate"));
	p->addModel(createModel<CloudsWidget>("Audible Instruments", "Clouds", "Texture Synthesizer"));
	p->addModel(createModel<WarpsWidget>("Audible Instruments", "Warps", "Meta Modulator"));
	p->addModel(createModel<RingsWidget>("Audible Instruments", "Rings", "Resonator"));
	p->addModel(createModel<LinksWidget>("Audible Instruments", "Links", "Multiples"));
	p->addModel(createModel<KinksWidget>("Audible Instruments", "Kinks", "Utilities"));
	p->addModel(createModel<ShadesWidget>("Audible Instruments", "Shades", "Mixer"));
	p->addModel(createModel<BranchesWidget>("Audible Instruments", "Branches", "Bernoulli Gate"));
	p->addModel(createModel<BlindsWidget>("Audible Instruments", "Blinds", "Quad VC-polarizer"));
	p->addModel(createModel<VeilsWidget>("Audible Instruments", "Veils", "Quad VCA"));
	p->addModel(createModel<FramesWidget>("Audible Instruments", "Frames", "Keyframer/Mixer"));
}
