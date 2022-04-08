#include <rack.hpp>


using namespace rack;


extern Plugin* pluginInstance;

extern Model* modelBraids;
extern Model* modelPlaits;
extern Model* modelElements;
extern Model* modelTides;
extern Model* modelTides2;
extern Model* modelClouds;
extern Model* modelWarps;
extern Model* modelRings;
extern Model* modelLinks;
extern Model* modelKinks;
extern Model* modelShades;
extern Model* modelBranches;
extern Model* modelBlinds;
extern Model* modelVeils;
extern Model* modelFrames;
extern Model* modelStages;
extern Model* modelMarbles;
extern Model* modelRipples;
extern Model* modelShelves;
extern Model* modelStreams;
extern Model *modelPeaks;

template <typename Base>
struct Rogan6PSLight : Base {
	Rogan6PSLight() {
		this->box.size = mm2px(Vec(23.04, 23.04));
		// this->bgColor = nvgRGBA(0, 0, 0, 0);
		this->borderColor = nvgRGBA(0, 0, 0, 0);
	}
};
