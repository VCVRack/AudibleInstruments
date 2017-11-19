#include "AudibleInstruments.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = "AudibleInstruments";
#ifdef VERSION
	p->version = TOSTRING(VERSION);
#endif

	p->addModel(createModel<BraidsWidget>("Audible Instruments", "Braids", "Macro Oscillator", OSCILLATOR_TAG, WAVESHAPER_TAG));
	p->addModel(createModel<ElementsWidget>("Audible Instruments", "Elements", "Modal Synthesizer", REVERB_TAG));
	p->addModel(createModel<TidesWidget>("Audible Instruments", "Tides", "Tidal Modulator", LFO_TAG, OSCILLATOR_TAG, WAVESHAPER_TAG, FUNCTION_GENERATOR_TAG));
	p->addModel(createModel<CloudsWidget>("Audible Instruments", "Clouds", "Texture Synthesizer", GRANULAR_TAG, REVERB_TAG));
	p->addModel(createModel<WarpsWidget>("Audible Instruments", "Warps", "Meta Modulator", RING_MODULATOR_TAG, WAVESHAPER_TAG));
	p->addModel(createModel<RingsWidget>("Audible Instruments", "Rings", "Resonator"));
	p->addModel(createModel<LinksWidget>("Audible Instruments", "Links", "Multiples", MULTIPLE_TAG, MIXER_TAG));
	p->addModel(createModel<KinksWidget>("Audible Instruments", "Kinks", "Utilities", UTILITY_TAG));
	p->addModel(createModel<ShadesWidget>("Audible Instruments", "Shades", "Mixer", MIXER_TAG));
	p->addModel(createModel<BranchesWidget>("Audible Instruments", "Branches", "Bernoulli Gate", RANDOM_TAG, DUAL_TAG));
	p->addModel(createModel<BlindsWidget>("Audible Instruments", "Blinds", "Quad VC-polarizer", MIXER_TAG, ATTENUATOR_TAG));
	p->addModel(createModel<VeilsWidget>("Audible Instruments", "Veils", "Quad VCA", MIXER_TAG));
	p->addModel(createModel<FramesWidget>("Audible Instruments", "Frames", "Keyframer/Mixer", MIXER_TAG, ATTENUATOR_TAG, LFO_TAG));
}
