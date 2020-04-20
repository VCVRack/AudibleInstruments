#include "plugin.hpp"
#include "Ripples/ripples.hpp"


struct Ripples : Module {
	enum ParamIds {
		RES_PARAM,
		FREQ_PARAM,
		FM_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		RES_INPUT,
		FREQ_INPUT,
		FM_INPUT,
		IN_INPUT,
		GAIN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		BP2_OUTPUT,
		LP2_OUTPUT,
		LP4_OUTPUT,
		LP4VCA_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	RipplesEngine engines[16];

	Ripples() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RES_PARAM, 0.f, 1.f, 0.f, "Resonance", "%", 0, 100);
		configParam(FREQ_PARAM, std::log2(RipplesEngine::kFreqKnobMin), std::log2(RipplesEngine::kFreqKnobMax), std::log2(RipplesEngine::kFreqKnobMax), "Frequency", " Hz", 2.f);
		configParam(FM_PARAM, -1.f, 1.f, 0.f, "Frequency modulation", "%", 0, 100);

		onSampleRateChange();
	}

	void onReset() override {
		onSampleRateChange();
	}

	void onSampleRateChange() override {
		// TODO In Rack v2, replace with args.sampleRate
		for (int c = 0; c < 16; c++) {
			engines[c].setSampleRate(APP->engine->getSampleRate());
		}
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[IN_INPUT].getChannels(), 1);

		// Reuse the same frame object for multiple engines because the params aren't touched.
		RipplesEngine::Frame frame;
		frame.res_knob = params[RES_PARAM].getValue();
		frame.freq_knob = rescale(params[FREQ_PARAM].getValue(), std::log2(RipplesEngine::kFreqKnobMin), std::log2(RipplesEngine::kFreqKnobMax), 0.f, 1.f);
		frame.fm_knob = params[FM_PARAM].getValue();
		frame.gain_cv_present = inputs[GAIN_INPUT].isConnected();

		for (int c = 0; c < channels; c++) {
			frame.res_cv = inputs[RES_INPUT].getPolyVoltage(c);
			frame.freq_cv = inputs[FREQ_INPUT].getPolyVoltage(c);
			frame.fm_cv = inputs[FM_INPUT].getPolyVoltage(c);
			frame.input = inputs[IN_INPUT].getVoltage(c);
			frame.gain_cv = inputs[GAIN_INPUT].getPolyVoltage(c);

			engines[c].process(frame);

			// Although rare, using extreme parameters, I've been able to produce nonfinite floats with the filter model, so detect them and reset the state.
			if (!std::isfinite(frame.bp2)) {
				// A reset() method would be nice, but we can just reinitialize it.
				engines[c] = RipplesEngine();
				engines[c].setSampleRate(args.sampleRate);
			}
			else {
				outputs[BP2_OUTPUT].setVoltage(frame.bp2, c);
				outputs[LP2_OUTPUT].setVoltage(frame.lp2, c);
				outputs[LP4_OUTPUT].setVoltage(frame.lp4, c);
				outputs[LP4VCA_OUTPUT].setVoltage(frame.lp4vca, c);
			}
		}

		outputs[BP2_OUTPUT].setChannels(channels);
		outputs[LP2_OUTPUT].setChannels(channels);
		outputs[LP4_OUTPUT].setChannels(channels);
		outputs[LP4VCA_OUTPUT].setChannels(channels);
	}
};


struct RipplesWidget : ModuleWidget {
	RipplesWidget(Ripples* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Ripples.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<Rogan2PSRed>(mm2px(Vec(8.872, 20.877)), module, Ripples::RES_PARAM));
		addParam(createParamCentered<Rogan3PSWhite>(mm2px(Vec(20.307, 42.468)), module, Ripples::FREQ_PARAM));
		addParam(createParamCentered<Rogan2PSGreen>(mm2px(Vec(31.732 + 0.1, 64.059)), module, Ripples::FM_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.227, 86.909)), module, Ripples::RES_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.297, 86.909)), module, Ripples::FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(32.367, 86.909)), module, Ripples::FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.227, 98.979)), module, Ripples::IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.227, 111.05)), module, Ripples::GAIN_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.297, 98.979)), module, Ripples::BP2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.367, 98.979)), module, Ripples::LP2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.297, 111.05)), module, Ripples::LP4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.367, 111.05)), module, Ripples::LP4VCA_OUTPUT));
	}
};


Model* modelRipples = createModel<Ripples, RipplesWidget>("Ripples");