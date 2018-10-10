#include "AudibleInstruments.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = TOSTRING(SLUG);
	p->version = TOSTRING(VERSION);

	p->addModel(modelBraids);
	p->addModel(modelPlaits);
	p->addModel(modelElements);
	p->addModel(modelTides);
	p->addModel(modelClouds);
	p->addModel(modelWarps);
	p->addModel(modelRings);
	p->addModel(modelLinks);
	p->addModel(modelKinks);
	p->addModel(modelShades);
	p->addModel(modelBranches);
	p->addModel(modelBlinds);
	p->addModel(modelVeils);
	p->addModel(modelFrames);
	// p->addModel(modelPeaks);
	p->addModel(modelMarbles);
	p->addModel(modelStages);
}
