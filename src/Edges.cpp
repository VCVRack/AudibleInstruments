#include "plugin.hpp"
#include "Edges/digital_oscillator.hpp"
#include "Edges/square_oscillator.hpp"

using simd::float_4;

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// the PWM values to cycle between
static const float SQUARE_PWM_VALUES[] = {0.5, 0.66, 0.75, 0.87, 0.95, 1.0};

struct Edges : Module {
    enum ParamIds {
        FREQ1_PARAM,
        XMOD1_PARAM,
        LEVEL1_PARAM,
        FREQ2_PARAM,
        XMOD2_PARAM,
        LEVEL2_PARAM,
        FREQ3_PARAM,
        XMOD3_PARAM,
        LEVEL3_PARAM,
        FREQ4_PARAM,
        LEVEL4_PARAM,
        WAVEFORM1_PARAM,
        WAVEFORM2_PARAM,
        WAVEFORM3_PARAM,
        WAVEFORM4_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        GATE1_INPUT,
        FREQ1_INPUT,
        MOD1_INPUT,
        GATE2_INPUT,
        FREQ2_INPUT,
        MOD2_INPUT,
        GATE3_INPUT,
        FREQ3_INPUT,
        MOD3_INPUT,
        GATE4_INPUT,
        FREQ4_INPUT,
        MOD4_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        WAVEFORM1_OUTPUT,
        WAVEFORM2_OUTPUT,
        WAVEFORM3_OUTPUT,
        WAVEFORM4_OUTPUT,
        MIX_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        GATE1_LIGHT_GREEN, GATE1_LIGHT_RED,
        GATE2_LIGHT_GREEN, GATE2_LIGHT_RED,
        GATE3_LIGHT_GREEN, GATE3_LIGHT_RED,
        GATE4_LIGHT_GREEN, GATE4_LIGHT_RED,
        NUM_LIGHTS
    };

    Edges() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        // setup waveform 1 parameters
        configParam(FREQ1_PARAM, -36.0, 36.0, 0.0, "Channel 1 frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(XMOD1_PARAM, 0.0, 1.0, 0.0, "Channel 1 to Channel 2 hard sync");
        configParam(LEVEL1_PARAM, 0.0, 1.0, 0.0, "Channel 1 level", "%", 0, 100);
        // setup waveform 2 parameters
        configParam(FREQ2_PARAM, -36.0, 36.0, 0.0, "Channel 2 frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(XMOD2_PARAM, 0.0, 1.0, 0.0, "Channel 1 x Channel 2 ring modulation");
        configParam(LEVEL2_PARAM, 0.0, 1.0, 0.0, "Channel 2 level", "%", 0, 100);
        // setup waveform 3 parameters
        configParam(FREQ3_PARAM, -36.0, 36.0, 0.0, "Channel 3 frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(XMOD3_PARAM, 0.0, 1.0, 0.0, "Channel 1 x Channel 3 ring modulation");
        configParam(LEVEL3_PARAM, 0.0, 1.0, 0.0, "Channel 3 level", "%", 0, 100);
        // setup waveform 4 parameters
        configParam(FREQ4_PARAM, -36.0, 36.0, 0.0, "Channel 4 frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(LEVEL4_PARAM, 0.0, 1.0, 0.0, "Channel 4 level", "%", 0, 100);
        // setup waveform selection parameters
        configParam(WAVEFORM1_PARAM, 0.0, 1.0, 0.0, "Channel 1 select");
        configParam(WAVEFORM2_PARAM, 0.0, 1.0, 0.0, "Channel 2 select");
        configParam(WAVEFORM3_PARAM, 0.0, 1.0, 0.0, "Channel 3 select");
        configParam(WAVEFORM4_PARAM, 0.0, 1.0, 0.0, "Channel 4 select");
    }

    /// a type for the square wave oscillator
    typedef SquareWaveOscillator<16, 16, float_4> Square;
    /// a type for the digital oscillator
    typedef DigitalOscillator<float_4> Digital;

    /// channel 1 (square wave)
    Square oscillator1;
    /// channel 2 (square wave)
    Square oscillator2;
    /// channel 3 (square wave)
    Square oscillator3;
    /// channel 4 (digital selectable wave)
    Digital oscillator4;

    /// a flag determining whether waveform 1's gate is open
    bool is_gate1_open = true;
    /// a flag determining whether waveform 2's gate is open
    bool is_gate2_open = true;
    /// a flag determining whether waveform 3's gate is open
    bool is_gate3_open = true;
    /// a flag determining whether waveform 4's gate is open
    bool is_gate4_open = true;

    /// the voltage at 1V/oct port 1
    float_4 voct1_voltage = 0.f;
    /// the voltage at 1V/oct port 2
    float_4 voct2_voltage = 0.f;
    /// the voltage at 1V/oct port 3
    float_4 voct3_voltage = 0.f;
    /// the voltage at 1V/oct port 4
    float voct4_voltage = 0.f;

    /// the mod voltage for mod port 1
    float_4 mod1_voltage = 0.f;
    /// the mod voltage for mod port 2
    float_4 mod2_voltage = 0.f;
    /// the mod voltage for mod port 3
    float_4 mod3_voltage = 0.f;
    /// the mod voltage for mod port 4
    float mod4_voltage = 0.f;

    /// the state of oscillator 1 determined by the button
    int osc1_state = 0;
    /// the state of oscillator 2 determined by the button
    int osc2_state = 0;
    /// the state of oscillator 3 determined by the button
    int osc3_state = 0;

    /// a Schmitt trigger for handling inputs to the waveform select 1 button
    dsp::SchmittTrigger oscillator1Trigger;
    /// a Schmitt trigger for handling inputs to the waveform select 2 button
    dsp::SchmittTrigger oscillator2Trigger;
    /// a Schmitt trigger for handling inputs to the waveform select 3 button
    dsp::SchmittTrigger oscillator3Trigger;
    /// a Schmitt trigger for handling inputs to the waveform select 4 button
    dsp::SchmittTrigger oscillator4Trigger;

    /// Process the gate inputs to determine which waveforms are active.
    /// Note that Gates are normalled, meaning that when a connection is active
    /// at a higher level gate, the signal propagates to lower level gates that
    /// are not connected
    /// TODO: determine whether the 0.7V threshold from hardware specification
    ///       fits the software model
    void process_gate_inputs() {
        // the 0.7V threshold is from the hardware specification for Edges
        is_gate1_open = inputs[GATE1_INPUT].getNormalVoltage(0.7f) >= 0.7f;
        is_gate2_open = inputs[GATE2_INPUT].getNormalVoltage(is_gate1_open * 0.7f) >= 0.7f;
        is_gate3_open = inputs[GATE3_INPUT].getNormalVoltage(is_gate2_open * 0.7f) >= 0.7f;
        is_gate4_open = inputs[GATE4_INPUT].getNormalVoltage(is_gate3_open * 0.7f) >= 0.7f;
        // set LEDs for each of the gates
        lights[GATE1_LIGHT_GREEN].setBrightness(is_gate1_open);
        lights[GATE2_LIGHT_GREEN].setBrightness(is_gate2_open);
        lights[GATE3_LIGHT_GREEN].setBrightness(is_gate3_open);
        lights[GATE4_LIGHT_GREEN].setBrightness(is_gate4_open);
    }

    /// Process the 1V/oct inputs
    void process_voct_inputs() {
        voct1_voltage = inputs[FREQ1_INPUT].getNormalVoltageSimd<float_4>(0, 0);
        voct2_voltage = inputs[FREQ2_INPUT].getNormalVoltageSimd<float_4>(voct1_voltage, 0);
        voct3_voltage = inputs[FREQ3_INPUT].getNormalVoltageSimd<float_4>(voct2_voltage, 0);
        // get the normalled channel 4 input as a float
        auto voct1_voltage_ = inputs[FREQ1_INPUT].getNormalVoltage(0, 0);
        auto voct2_voltage_ = inputs[FREQ2_INPUT].getNormalVoltage(voct1_voltage_, 0);
        auto voct3_voltage_ = inputs[FREQ3_INPUT].getNormalVoltage(voct2_voltage_, 0);
        voct4_voltage = inputs[FREQ4_INPUT].getNormalVoltage(voct3_voltage_, 0);
        // TODO: refactor to use the float_4 structure
        // voct4_voltage = inputs[FREQ4_INPUT].getNormalVoltageSimd<float_4>(voct3_voltage, 0);
    }

    inline void process_mod_inputs() {
        mod1_voltage = inputs[MOD1_INPUT].getNormalVoltageSimd<float_4>(0, 0);
        mod2_voltage = inputs[MOD2_INPUT].getNormalVoltageSimd<float_4>(0, 0);
        mod3_voltage = inputs[MOD3_INPUT].getNormalVoltageSimd<float_4>(0, 0);
        mod4_voltage = inputs[MOD4_INPUT].getNormalVoltage(0, 0);
    }

    void process_buttons() {
        // process a button press for oscillator 1
        if (oscillator1Trigger.process(params[WAVEFORM1_PARAM].getValue())) {
            osc1_state = (osc1_state + 1) % 6;
            if (osc1_state < 5) {
                oscillator1.setPulseWidth(SQUARE_PWM_VALUES[osc1_state]);
            }
        }
        // set PWM for oscillator 1 based on oscillator 4 frequency control
        if (osc1_state == 5) {
            oscillator1.setPulseWidth(inputs[FREQ4_INPUT].getNormalVoltage(10.f) / 10.f);
        }

        // process a button press for oscillator 2
        if (oscillator2Trigger.process(params[WAVEFORM2_PARAM].getValue())) {
            osc2_state = (osc2_state + 1) % 6;
            if (osc2_state < 5) {
                oscillator2.setPulseWidth(SQUARE_PWM_VALUES[osc2_state]);
            }
        }
        // set PWM for oscillator 2 based on oscillator 4 frequency control
        if (osc2_state == 5) {
            oscillator2.setPulseWidth(inputs[FREQ4_INPUT].getNormalVoltage(10.f) / 10.f);
        }

        // process a button press for oscillator 3
        if (oscillator3Trigger.process(params[WAVEFORM3_PARAM].getValue())) {
            osc3_state = (osc3_state + 1) % 6;
            if (osc3_state < 5) {
                oscillator3.setPulseWidth(SQUARE_PWM_VALUES[osc3_state]);
            }
        }
        // set PWM for oscillator 3 based on oscillator 4 frequency control
        if (osc3_state == 5) {
            oscillator3.setPulseWidth(inputs[FREQ4_INPUT].getNormalVoltage(10.f) / 10.f);
        }

        // process a button press for oscillator 4
        if (oscillator4Trigger.process(params[WAVEFORM4_PARAM].getValue())) {
            oscillator4.nextShape();
        }
    }

    void process_xmod_switches() {
        oscillator2.syncEnabled = params[XMOD1_PARAM].getValue();
        oscillator2.ringModulation = params[XMOD2_PARAM].getValue();
        oscillator3.ringModulation = params[XMOD3_PARAM].getValue();
    }

    void process_frequency(float sampleTime, int sampleRate) {
        // set the pitch of oscillator 1
        float_4 pitch1 = params[FREQ1_PARAM].getValue() / 12.f;
        pitch1 += voct1_voltage;
        pitch1 += mod1_voltage;
        oscillator1.setPitch(pitch1);
        // set the pitch of oscillator 2
        float_4 pitch2 = params[FREQ2_PARAM].getValue() / 12.f;
        pitch2 += voct2_voltage;
        pitch2 += mod2_voltage;
        oscillator2.setPitch(pitch2);
        // set the pitch of oscillator 3
        float_4 pitch3 = params[FREQ3_PARAM].getValue() / 12.f;
        pitch3 += voct3_voltage;
        pitch3 += mod3_voltage;
        oscillator3.setPitch(pitch3);
        // set the pitch of oscillator 4
        // get the pitch for the 4th oscillator (12 notes/octave * 1V/octave)
        auto twelve_volt_octave = 12 * (voct4_voltage + mod4_voltage);
        // 61 is the base for C4
        uint16_t pitch4_int = 61 + params[FREQ4_PARAM].getValue() + twelve_volt_octave;
        // get the microtone for the 4th oscillator as 7-bit integer
        uint16_t pitch4_frac = 128 * (params[FREQ4_PARAM].getValue() - static_cast<int>(params[FREQ4_PARAM].getValue()));
        pitch4_frac += 128 * (twelve_volt_octave - static_cast<int>(twelve_volt_octave));
        // set the pitch of oscillator 4
        oscillator4.setPitch((pitch4_int << 7) + pitch4_frac + 64);

        // Process the output voltage of each oscillator
        oscillator1.process(sampleTime);
        // sync oscillator 2 to oscillator 1, ring mod with oscillator 1
        // (hard sync is only applied if oscillator2.syncEnabled is true)
        // (ring mod is only applied if oscillator2.ringModulation is true)
        oscillator2.process(sampleTime, oscillator1.sqr(), oscillator1.sqr());
        // don't sync oscillator 3, ring mod with oscillator 1
        // (ring mod is only applied if oscillator3.ringModulation is true)
        oscillator3.process(sampleTime, 0, oscillator1.sqr());
        oscillator4.process(sampleRate);
    }

    void process_output() {
        // set outputs for channel 1 if connected
        if (outputs[WAVEFORM1_OUTPUT].isConnected())
            outputs[WAVEFORM1_OUTPUT].setVoltageSimd(is_gate1_open * 5.f * oscillator1.sqr(), 0);
        // set outputs for channel 2 if connected
        if (outputs[WAVEFORM2_OUTPUT].isConnected())
            outputs[WAVEFORM2_OUTPUT].setVoltageSimd(is_gate2_open * 5.f * oscillator2.sqr(), 0);
        // set outputs for channel 3 if connected
        if (outputs[WAVEFORM3_OUTPUT].isConnected())
            outputs[WAVEFORM3_OUTPUT].setVoltageSimd(is_gate3_open * 5.f * oscillator3.sqr(), 0);
        // set outputs for channel 4 if connected
        if (outputs[WAVEFORM4_OUTPUT].isConnected())
            outputs[WAVEFORM4_OUTPUT].setVoltageSimd(is_gate4_open * 5.f * oscillator4.getValue(), 0);
        // create the mixed output if connected
        if (outputs[MIX_OUTPUT].isConnected()) {
            auto the_mix  = is_gate1_open * oscillator1.sqr() * !outputs[WAVEFORM1_OUTPUT].isConnected() * params[LEVEL1_PARAM].getValue();
            the_mix      += is_gate2_open * oscillator2.sqr() * !outputs[WAVEFORM2_OUTPUT].isConnected() * params[LEVEL2_PARAM].getValue();
            the_mix      += is_gate3_open * oscillator3.sqr() * !outputs[WAVEFORM3_OUTPUT].isConnected() * params[LEVEL3_PARAM].getValue();
            the_mix      += is_gate4_open * oscillator4.getValue() * !outputs[WAVEFORM4_OUTPUT].isConnected() * params[LEVEL4_PARAM].getValue();
            outputs[MIX_OUTPUT].setVoltageSimd(5.f * the_mix, 0);
        }
    }

    void process(const ProcessArgs &args) override {
        process_gate_inputs();
        process_voct_inputs();
        process_mod_inputs();
        process_buttons();
        process_xmod_switches();
        process_frequency(args.sampleTime, args.sampleRate);
        process_output();
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// A menu item for controlling the quantization feature
template<typename T>
struct EdgesQuantizerItem : MenuItem {
    /// the oscillator to control the quantization of
    T *oscillator;

    /// Respond to a menu action.
    inline void onAction(const event::Action &e) override {
        oscillator->isQuantized = not oscillator->isQuantized;
    }

    /// Perform a step on the menu.
    inline void step() override {
        rightText = oscillator->isQuantized ? "✔" : "";
        MenuItem::step();
    }
};

/// The widget structure that lays out the panel of the module and the UI menus.
struct EdgesWidget : ModuleWidget {
    EdgesWidget(Edges *module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Edges.svg")));
        // add vanity screws
        addChild(createWidget<ScrewSilver>(Vec(15, 0)));
        addChild(createWidget<ScrewSilver>(Vec(270, 0)));
        addChild(createWidget<ScrewSilver>(Vec(15, 365)));
        addChild(createWidget<ScrewSilver>(Vec(270, 365)));
        // add frequency knobs
        addParam(createParam<Rogan1PSWhite>(Vec(122, 54), module, Edges::FREQ1_PARAM));
        addParam(createParam<Rogan1PSWhite>(Vec(122, 114), module, Edges::FREQ2_PARAM));
        addParam(createParam<Rogan1PSWhite>(Vec(122, 174), module, Edges::FREQ3_PARAM));
        addParam(createParam<Rogan1PSWhite>(Vec(122, 234), module, Edges::FREQ4_PARAM));
        // add XMOD switches
        addParam(createParam<CKSS>(Vec(178, 64), module, Edges::XMOD1_PARAM));
        addParam(createParam<CKSS>(Vec(178, 124), module, Edges::XMOD2_PARAM));
        addParam(createParam<CKSS>(Vec(178, 184), module, Edges::XMOD3_PARAM));
        // add level knobs
        addParam(createParam<Rogan1PSGreen>(Vec(245, 54), module, Edges::LEVEL1_PARAM));
        addParam(createParam<Rogan1PSGreen>(Vec(245, 114), module, Edges::LEVEL2_PARAM));
        addParam(createParam<Rogan1PSGreen>(Vec(245, 174), module, Edges::LEVEL3_PARAM));
        addParam(createParam<Rogan1PSGreen>(Vec(245, 234), module, Edges::LEVEL4_PARAM));
        // add waveform selection buttons
        addParam(createParam<TL1105>(Vec(43, 320), module, Edges::WAVEFORM1_PARAM));
        addParam(createParam<TL1105>(Vec(67, 320), module, Edges::WAVEFORM2_PARAM));
        addParam(createParam<TL1105>(Vec(91, 320), module, Edges::WAVEFORM3_PARAM));
        addParam(createParam<TL1105>(Vec(115, 320), module, Edges::WAVEFORM4_PARAM));
        // add inputs for Gate
        addInput(createInput<PJ301MPort>(Vec(20, 62), module, Edges::GATE1_INPUT));
        addInput(createInput<PJ301MPort>(Vec(20, 122), module, Edges::GATE2_INPUT));
        addInput(createInput<PJ301MPort>(Vec(20, 182), module, Edges::GATE3_INPUT));
        addInput(createInput<PJ301MPort>(Vec(20, 242), module, Edges::GATE4_INPUT));
        // add inputs for Frequency (1V/Oct)
        addInput(createInput<PJ301MPort>(Vec(58, 62), module, Edges::FREQ1_INPUT));
        addInput(createInput<PJ301MPort>(Vec(58, 122), module, Edges::FREQ2_INPUT));
        addInput(createInput<PJ301MPort>(Vec(58, 182), module, Edges::FREQ3_INPUT));
        addInput(createInput<PJ301MPort>(Vec(58, 242), module, Edges::FREQ4_INPUT));
        // add inputs for Mod
        addInput(createInput<PJ301MPort>(Vec(90, 62), module, Edges::MOD1_INPUT));
        addInput(createInput<PJ301MPort>(Vec(90, 122), module, Edges::MOD2_INPUT));
        addInput(createInput<PJ301MPort>(Vec(90, 182), module, Edges::MOD3_INPUT));
        addInput(createInput<PJ301MPort>(Vec(90, 242), module, Edges::MOD4_INPUT));
        // add outputs for each waveform
        addOutput(createOutput<PJ301MPort>(Vec(215, 62), module, Edges::WAVEFORM1_OUTPUT));
        addOutput(createOutput<PJ301MPort>(Vec(215, 122), module, Edges::WAVEFORM2_OUTPUT));
        addOutput(createOutput<PJ301MPort>(Vec(215, 182), module, Edges::WAVEFORM3_OUTPUT));
        addOutput(createOutput<PJ301MPort>(Vec(215, 242), module, Edges::WAVEFORM4_OUTPUT));
        // add output for mix (all waveforms mixed)
        addOutput(createOutput<PJ301MPort>(Vec(253, 315), module, Edges::MIX_OUTPUT));
        // add an LED for each waveform selection button
        addChild(createLight<MediumLight<GreenRedLight>>(Vec(46, 300), module, Edges::GATE1_LIGHT_GREEN));
        addChild(createLight<MediumLight<GreenRedLight>>(Vec(70, 300), module, Edges::GATE2_LIGHT_GREEN));
        addChild(createLight<MediumLight<GreenRedLight>>(Vec(94, 300), module, Edges::GATE3_LIGHT_GREEN));
        addChild(createLight<MediumLight<GreenRedLight>>(Vec(118, 300), module, Edges::GATE4_LIGHT_GREEN));
    }

    void appendContextMenu(Menu *menu) override {
        Edges *module = dynamic_cast<Edges*>(this->module);
        assert(module);
        // add the menu for the Quantizer feature
        menu->addChild(construct<MenuLabel>());
        menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Quantizer"));
        menu->addChild(construct<EdgesQuantizerItem<Edges::Square>>(
            &MenuItem::text, "Channel 1",
            &EdgesQuantizerItem<Edges::Square>::oscillator, &module->oscillator1
        ));
        menu->addChild(construct<EdgesQuantizerItem<Edges::Square>>(
            &MenuItem::text, "Channel 2",
            &EdgesQuantizerItem<Edges::Square>::oscillator, &module->oscillator2
        ));
        menu->addChild(construct<EdgesQuantizerItem<Edges::Square>>(
            &MenuItem::text, "Channel 3",
            &EdgesQuantizerItem<Edges::Square>::oscillator, &module->oscillator3
        ));
        menu->addChild(construct<EdgesQuantizerItem<Edges::Digital>>(
            &MenuItem::text, "Channel 4",
            &EdgesQuantizerItem<Edges::Digital>::oscillator, &module->oscillator4
        ));
    }
};


Model *modelEdges = createModel<Edges, EdgesWidget>("Edges");
