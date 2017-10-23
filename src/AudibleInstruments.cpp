#include "AudibleInstruments.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = "AudibleInstruments";
#ifdef VERSION
	p->version = TOSTRING(VERSION);
#endif
	p->addModel(createModel<BraidsWidget>("AudibleInstruments", "Audible Instruments", "Braids", "Macro Oscillator"));
	p->addModel(createModel<ElementsWidget>("AudibleInstruments", "Audible Instruments", "Elements", "Modal Synthesizer"));
	p->addModel(createModel<TidesWidget>("AudibleInstruments", "Audible Instruments", "Tides", "Tidal Modulator"));
	p->addModel(createModel<SheepWidget>("AudibleInstruments", "Audible Instruments", "Sheep", "Wavetable Oscillator"));
	// p->addModel(createModel<StreamsWidget>("AudibleInstruments", "Audible Instruments", "Streams", "Dual Dynamics Gate"));
	p->addModel(createModel<CloudsWidget>("AudibleInstruments", "Audible Instruments", "Clouds", "Texture Synthesizer"));
	p->addModel(createModel<WarpsWidget>("AudibleInstruments", "Audible Instruments", "Warps", "Meta Modulator"));
	p->addModel(createModel<RingsWidget>("AudibleInstruments", "Audible Instruments", "Rings", "Resonator"));
	p->addModel(createModel<LinksWidget>("AudibleInstruments", "Audible Instruments", "Links", "Multiples"));
	p->addModel(createModel<KinksWidget>("AudibleInstruments", "Audible Instruments", "Kinks", "Utilities"));
	p->addModel(createModel<ShadesWidget>("AudibleInstruments", "Audible Instruments", "Shades", "Mixer"));
	p->addModel(createModel<BranchesWidget>("AudibleInstruments", "Audible Instruments", "Branches", "Bernoulli Gate"));
	p->addModel(createModel<BlindsWidget>("AudibleInstruments", "Audible Instruments", "Blinds", "Quad VC-polarizer"));
	p->addModel(createModel<VeilsWidget>("AudibleInstruments", "Audible Instruments", "Veils", "Quad VCA"));
	p->addModel(createModel<FramesWidget>("AudibleInstruments", "Audible Instruments", "Frames", "Keyframer/Mixer"));
}
