#include "AudibleInstruments.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = TOSTRING(SLUG);
	p->version = TOSTRING(VERSION);

	p->addModel(modelBraidsWidget);
	p->addModel(modelElementsWidget);
	p->addModel(modelTidesWidget);
	p->addModel(modelCloudsWidget);
	p->addModel(modelWarpsWidget);
	p->addModel(modelRingsWidget);
	p->addModel(modelLinksWidget);
	p->addModel(modelKinksWidget);
	p->addModel(modelShadesWidget);
	p->addModel(modelBranchesWidget);
	p->addModel(modelBlindsWidget);
	p->addModel(modelVeilsWidget);
	p->addModel(modelFramesWidget);
}
