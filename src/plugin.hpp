#pragma once
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
extern Model* modelBeads;


/** Mutable Instruments' red/green bicolor LEDs look orange, not yellow, with both dies lit. Rack adds the two
colors to yellow, so this draws their overlap in orange. Reads `lightId` (green) and `lightId + 1` (red), like
GreenRedLight, and looks the same whenever only one die is lit.
*/
struct GreenRedBicolorLight : GrayModuleLightWidget {
	GreenRedBicolorLight() {
		addBaseColor(SCHEME_GREEN);
		addBaseColor(SCHEME_RED);
	}

	void step() override {
		GrayModuleLightWidget::step();
		if (!module)
			return;
		float green = math::clamp(getLight(0)->getBrightness(), 0.f, 1.f);
		float red = math::clamp(getLight(1)->getBrightness(), 0.f, 1.f);
		float orange = std::min(green, red);
		// With only one die lit, Rack's own blend is already correct.
		if (orange <= 0.f)
			return;
		// Blend the dies outside the overlap as Rack does, including ModuleLightWidget's square-root brightness
		// mapping, then screen the overlap on top in orange.
		setBrightnesses({std::sqrt(green - orange), std::sqrt(red - orange)});
		color = color::screen(color, color::alpha(nvgRGB(0xff, 0x80, 0x00), std::sqrt(orange)));
	}
};


template <typename Base>
struct Rogan6PSLight : Base {
	Rogan6PSLight() {
		this->box.size = mm2px(Vec(23.04, 23.04));
		// this->bgColor = nvgRGBA(0, 0, 0, 0);
		this->borderColor = nvgRGBA(0, 0, 0, 0);
	}
};
