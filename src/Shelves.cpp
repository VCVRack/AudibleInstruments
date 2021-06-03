#include "plugin.hpp"
#include "Shelves/shelves.hpp"


static const float freqMin = std::log2(shelves::kFreqKnobMin);
static const float freqMax = std::log2(shelves::kFreqKnobMax);
static const float freqInit = (freqMin + freqMax) / 2;
static const float gainMin = -shelves::kGainKnobRange;
static const float gainMax = shelves::kGainKnobRange;
static const float qMin = std::log2(shelves::kQKnobMin);
static const float qMax = std::log2(shelves::kQKnobMax);
static const float qInit = (qMin + qMax) / 2;


struct Shelves : Module {
	enum ParamIds {
		HS_FREQ_PARAM,
		HS_GAIN_PARAM,
		P1_FREQ_PARAM,
		P1_GAIN_PARAM,
		P1_Q_PARAM,
		P2_FREQ_PARAM,
		P2_GAIN_PARAM,
		P2_Q_PARAM,
		LS_FREQ_PARAM,
		LS_GAIN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		HS_FREQ_INPUT,
		HS_GAIN_INPUT,
		P1_FREQ_INPUT,
		P1_GAIN_INPUT,
		P1_Q_INPUT,
		P2_FREQ_INPUT,
		P2_GAIN_INPUT,
		P2_Q_INPUT,
		LS_FREQ_INPUT,
		LS_GAIN_INPUT,
		FREQ_INPUT,
		GAIN_INPUT,
		IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		P1_HP_OUTPUT,
		P1_BP_OUTPUT,
		P1_LP_OUTPUT,
		P2_HP_OUTPUT,
		P2_BP_OUTPUT,
		P2_LP_OUTPUT,
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CLIP_LIGHT,
		NUM_LIGHTS
	};

	shelves::ShelvesEngine engines[16];
	bool preGain;

	Shelves() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(HS_FREQ_PARAM, freqMin, freqMax, freqInit, "High-shelf frequency", " Hz", 2.f);
		configParam(P1_FREQ_PARAM, freqMin, freqMax, freqInit, "Parametric 1 frequency", " Hz", 2.f);
		configParam(P2_FREQ_PARAM, freqMin, freqMax, freqInit, "Parametric 2 frequency", " Hz", 2.f);
		configParam(LS_FREQ_PARAM, freqMin, freqMax, freqInit, "Low-shelf frequency", " Hz", 2.f);

		configParam(HS_GAIN_PARAM, gainMin, gainMax, 0.f, "High-shelf gain", " dB");
		configParam(P1_GAIN_PARAM, gainMin, gainMax, 0.f, "Parametric 1 gain", " dB");
		configParam(P2_GAIN_PARAM, gainMin, gainMax, 0.f, "Parametric 2 gain", " dB");
		configParam(LS_GAIN_PARAM, gainMin, gainMax, 0.f, "Low-shelf gain", " dB");

		configParam(P1_Q_PARAM, qMin, qMax, qInit, "Parametric 1 quality", "", 2.f);
		configParam(P2_Q_PARAM, qMin, qMax, qInit, "Parametric 2 quality", "", 2.f);

		onReset();
	}

	void onReset() override {
		preGain = false;
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
		shelves::ShelvesEngine::Frame frame;
		frame.pre_gain = preGain;

		frame.hs_freq_knob = rescale(params[HS_FREQ_PARAM].getValue(), freqMin, freqMax, 0.f, 1.f);
		frame.p1_freq_knob = rescale(params[P1_FREQ_PARAM].getValue(), freqMin, freqMax, 0.f, 1.f);
		frame.p2_freq_knob = rescale(params[P2_FREQ_PARAM].getValue(), freqMin, freqMax, 0.f, 1.f);
		frame.ls_freq_knob = rescale(params[LS_FREQ_PARAM].getValue(), freqMin, freqMax, 0.f, 1.f);

		frame.hs_gain_knob = params[HS_GAIN_PARAM].getValue() / shelves::kGainKnobRange;
		frame.p1_gain_knob = params[P1_GAIN_PARAM].getValue() / shelves::kGainKnobRange;
		frame.p2_gain_knob = params[P2_GAIN_PARAM].getValue() / shelves::kGainKnobRange;
		frame.ls_gain_knob = params[LS_GAIN_PARAM].getValue() / shelves::kGainKnobRange;

		frame.p1_q_knob = rescale(params[P1_Q_PARAM].getValue(), qMin, qMax, 0.f, 1.f);
		frame.p2_q_knob = rescale(params[P2_Q_PARAM].getValue(), qMin, qMax, 0.f, 1.f);

		frame.hs_freq_cv_connected = inputs[HS_FREQ_INPUT].isConnected();
		frame.hs_gain_cv_connected = inputs[HS_GAIN_INPUT].isConnected();
		frame.p1_freq_cv_connected = inputs[P1_FREQ_INPUT].isConnected();
		frame.p1_gain_cv_connected = inputs[P1_GAIN_INPUT].isConnected();
		frame.p1_q_cv_connected = inputs[P1_Q_INPUT].isConnected();
		frame.p2_freq_cv_connected = inputs[P2_FREQ_INPUT].isConnected();
		frame.p2_gain_cv_connected = inputs[P2_GAIN_INPUT].isConnected();
		frame.p2_q_cv_connected = inputs[P2_Q_INPUT].isConnected();
		frame.ls_freq_cv_connected = inputs[LS_FREQ_INPUT].isConnected();
		frame.ls_gain_cv_connected = inputs[LS_GAIN_INPUT].isConnected();
		frame.global_freq_cv_connected = inputs[FREQ_INPUT].isConnected();
		frame.global_gain_cv_connected = inputs[GAIN_INPUT].isConnected();

		frame.p1_hp_out_connected = outputs[P1_HP_OUTPUT].isConnected();
		frame.p1_bp_out_connected = outputs[P1_BP_OUTPUT].isConnected();
		frame.p1_lp_out_connected = outputs[P1_LP_OUTPUT].isConnected();
		frame.p2_hp_out_connected = outputs[P2_HP_OUTPUT].isConnected();
		frame.p2_bp_out_connected = outputs[P2_BP_OUTPUT].isConnected();
		frame.p2_lp_out_connected = outputs[P2_LP_OUTPUT].isConnected();

		float clipLight = 0.f;

		for (int c = 0; c < channels; c++) {
			frame.main_in = inputs[IN_INPUT].getVoltage(c);
			frame.hs_freq_cv = inputs[HS_FREQ_INPUT].getPolyVoltage(c);
			frame.hs_gain_cv = inputs[HS_GAIN_INPUT].getPolyVoltage(c);
			frame.p1_freq_cv = inputs[P1_FREQ_INPUT].getPolyVoltage(c);
			frame.p1_gain_cv = inputs[P1_GAIN_INPUT].getPolyVoltage(c);
			frame.p1_q_cv = inputs[P1_Q_INPUT].getPolyVoltage(c);
			frame.p2_freq_cv = inputs[P2_FREQ_INPUT].getPolyVoltage(c);
			frame.p2_gain_cv = inputs[P2_GAIN_INPUT].getPolyVoltage(c);
			frame.p2_q_cv = inputs[P2_Q_INPUT].getPolyVoltage(c);
			frame.ls_freq_cv = inputs[LS_FREQ_INPUT].getPolyVoltage(c);
			frame.ls_gain_cv = inputs[LS_GAIN_INPUT].getPolyVoltage(c);
			frame.global_freq_cv = inputs[FREQ_INPUT].getPolyVoltage(c);
			frame.global_gain_cv = inputs[GAIN_INPUT].getPolyVoltage(c);

			engines[c].process(frame);

			outputs[P1_HP_OUTPUT].setVoltage(frame.p1_hp_out, c);
			outputs[P1_BP_OUTPUT].setVoltage(frame.p1_bp_out, c);
			outputs[P1_LP_OUTPUT].setVoltage(frame.p1_lp_out, c);
			outputs[P2_HP_OUTPUT].setVoltage(frame.p2_hp_out, c);
			outputs[P2_BP_OUTPUT].setVoltage(frame.p2_bp_out, c);
			outputs[P2_LP_OUTPUT].setVoltage(frame.p2_lp_out, c);
			outputs[OUT_OUTPUT].setVoltage(frame.main_out, c);
			clipLight += frame.clip;
		}

		outputs[P1_HP_OUTPUT].setChannels(channels);
		outputs[P1_BP_OUTPUT].setChannels(channels);
		outputs[P1_LP_OUTPUT].setChannels(channels);
		outputs[P2_HP_OUTPUT].setChannels(channels);
		outputs[P2_BP_OUTPUT].setChannels(channels);
		outputs[P2_LP_OUTPUT].setChannels(channels);
		outputs[OUT_OUTPUT].setChannels(channels);
		lights[CLIP_LIGHT].setSmoothBrightness(clipLight, args.sampleTime);
	}

	json_t* dataToJson() override {
		json_t* root_j = json_object();
		json_object_set_new(root_j, "preGain", json_boolean(preGain));
		return root_j;
	}

	void dataFromJson(json_t* root_j) override {
		json_t* preGainJ = json_object_get(root_j, "preGain");
		if (preGainJ)
			preGain = json_boolean_value(preGainJ);
	}
};


struct ShelvesWidget : ModuleWidget {
	ShelvesWidget(Shelves* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Shelves.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(41.582, 19.659)), module, Shelves::HS_FREQ_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(65.699, 19.659)), module, Shelves::HS_GAIN_PARAM));
		addParam(createParamCentered<Rogan1PSRed>(mm2px(Vec(41.582, 43.473)), module, Shelves::P1_FREQ_PARAM));
		addParam(createParamCentered<Rogan1PSRed>(mm2px(Vec(65.699, 43.473)), module, Shelves::P1_GAIN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.632, 48.111)), module, Shelves::P1_Q_PARAM));
		addParam(createParamCentered<Rogan1PSGreen>(mm2px(Vec(41.582, 67.286)), module, Shelves::P2_FREQ_PARAM));
		addParam(createParamCentered<Rogan1PSGreen>(mm2px(Vec(65.699, 67.286)), module, Shelves::P2_GAIN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.632, 63.447)), module, Shelves::P2_Q_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(41.582, 91.099)), module, Shelves::LS_FREQ_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(mm2px(Vec(65.699, 91.099)), module, Shelves::LS_GAIN_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 17.329)), module, Shelves::HS_FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.619, 17.329)), module, Shelves::HS_GAIN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 33.824)), module, Shelves::P1_FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.619, 33.824)), module, Shelves::P1_GAIN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 48.111)), module, Shelves::P1_Q_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 77.733)), module, Shelves::P2_FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.619, 77.733)), module, Shelves::P2_GAIN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 63.447)), module, Shelves::P2_Q_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 94.228)), module, Shelves::LS_FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.619, 94.228)), module, Shelves::LS_GAIN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.983, 109.475)), module, Shelves::FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.619, 109.475)), module, Shelves::GAIN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(41.565, 109.475)), module, Shelves::IN_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(84.418, 17.329)), module, Shelves::P1_HP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(84.418, 32.725)), module, Shelves::P1_BP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(84.431, 48.111)), module, Shelves::P1_LP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(84.431, 63.447)), module, Shelves::P2_HP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(84.418, 78.832)), module, Shelves::P2_BP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(84.418, 94.228)), module, Shelves::P2_LP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(65.682, 109.475)), module, Shelves::OUT_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(53.629, 109.475)), module, Shelves::CLIP_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		Shelves* module = dynamic_cast<Shelves*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("Pad input by -6dB", &module->preGain));
	}
};


Model* modelShelves = createModel<Shelves, ShelvesWidget>("Shelves");
