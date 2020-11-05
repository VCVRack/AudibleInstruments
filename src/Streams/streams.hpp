// Mutable Instruments Streams emulation for VCV Rack
// Copyright (C) 2020 Tyler Coy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <cmath>
#include "rack.hpp"
#include "resampler.hpp"
#include "analog_engine.hpp"
#include "digital_engine.hpp"

namespace streams
{

using namespace rack;

class StreamsEngine
{
public:
    struct ChannelFrame
    {
        // Parameters
        float shape_knob;
        float mod_knob;
        float level_mod_knob;
        float response_knob;

        bool function_button;

        // Inputs
        float excite_in;
        float signal_in;
        float level_cv;

        bool signal_in_connected;
        bool level_cv_connected;

        // Outputs
        float signal_out;

        // Lights
        float led_green[4];
        float led_red[4];
    };

    struct Frame
    {
        ChannelFrame ch1;
        ChannelFrame ch2;
        bool metering_button;
        bool lights_updated;
    };

    StreamsEngine()
    {
        Reset();
        SetSampleRate(1.f);
    }

    void Reset(void)
    {
        adc_lpf_.reset();
        resampler_.Reset();
        analog_engine_.Reset();
        digital_engine_.Reset();
        adc_feedback_[0] = 0.f;
        adc_feedback_[1] = 0.f;
    }

    void SetSampleRate(float sample_rate)
    {
        adc_lpf_.setCutoffFreq(kAdcFilterCutoff / sample_rate);
        resampler_.Init(sample_rate, DigitalEngine::kSampleRate, 0);
        analog_engine_.SetSampleRate(sample_rate);
    }

    void Randomize(void)
    {
        digital_engine_.Randomize();
    }

    void ApplySettings(const UiSettings& settings)
    {
        digital_engine_.ApplySettings(settings);
    }

    const UiSettings& ui_settings(void)
    {
        return digital_engine_.ui_settings();
    }

    void SyncUI(const StreamsEngine& other)
    {
        digital_engine_.SyncUI(other.digital_engine_);
    }

    void Process(Frame& frame)
    {
        frame.lights_updated = false;

        float ch1_signal_in = frame.ch1.signal_in_connected ?
                              frame.ch1.signal_in : kSignalInNormalV;
        float ch2_signal_in = frame.ch2.signal_in_connected ?
                              frame.ch2.signal_in : kSignalInNormalV;

        float ch1_level_cv  = frame.ch1.level_cv_connected ?
                              frame.ch1.level_cv : kLevelNormalV;
        float ch2_level_cv  = frame.ch2.level_cv_connected ?
                              frame.ch2.level_cv : kLevelNormalV;

        Rsmp::InputFrame d_input;
        d_input.samples[0] = ch1_signal_in;
        d_input.samples[1] = ch2_signal_in;
        d_input.samples[2] = frame.ch1.excite_in;
        d_input.samples[3] = frame.ch2.excite_in;
        d_input.samples[4] = adc_feedback_[0];
        d_input.samples[5] = adc_feedback_[1];

        auto adc_input = simd::float_4::load(d_input.samples);
        adc_lpf_.process(adc_input);
        adc_input = adc_lpf_.lowpass();
        adc_input = kAdcFilterOffset + adc_input * kAdcFilterGain;
        adc_input.store(d_input.samples);

        DigitalEngine::Frame<kBlockSize> d_frame;

        Rsmp::OutputFrame d_output = resampler_.Process(d_input,
        [&](Rsmp::OutputFrame* output, const Rsmp::InputFrame* input)
        {
            d_frame.ch1.shape_knob          = frame.ch1.shape_knob;
            d_frame.ch1.mod_knob            = frame.ch1.mod_knob;
            d_frame.ch1.level_mod_knob      = frame.ch1.level_mod_knob;
            d_frame.ch1.response_knob       = frame.ch1.response_knob;
            d_frame.ch2.shape_knob          = frame.ch2.shape_knob;
            d_frame.ch2.mod_knob            = frame.ch2.mod_knob;
            d_frame.ch2.level_mod_knob      = frame.ch2.level_mod_knob;
            d_frame.ch2.response_knob       = frame.ch2.response_knob;

            d_frame.ch1.function_button     = frame.ch1.function_button;
            d_frame.ch2.function_button     = frame.ch2.function_button;
            d_frame.metering_button         = frame.metering_button;

            for (int i = 0; i < kBlockSize; i++)
            {
                d_frame.ch1.signal_in[i]    = input[i].samples[0];
                d_frame.ch2.signal_in[i]    = input[i].samples[1];
                d_frame.ch1.excite_in[i]    = input[i].samples[2];
                d_frame.ch2.excite_in[i]    = input[i].samples[3];
                d_frame.ch1.level_adc_in[i] = input[i].samples[4];
                d_frame.ch2.level_adc_in[i] = input[i].samples[5];
            }

            digital_engine_.Process(d_frame);

            for (int i = 0; i < kBlockSize; i++)
            {
                output[i].samples[0] = d_frame.ch1.dac_out[i];
                output[i].samples[1] = d_frame.ch1.pwm_out[i];
                output[i].samples[2] = d_frame.ch2.dac_out[i];
                output[i].samples[3] = d_frame.ch2.pwm_out[i];
            }

            frame.ch1.led_green[0] = d_frame.ch1.led_green[0];
            frame.ch1.led_green[1] = d_frame.ch1.led_green[1];
            frame.ch1.led_green[2] = d_frame.ch1.led_green[2];
            frame.ch1.led_green[3] = d_frame.ch1.led_green[3];
            frame.ch1.led_red[0]   = d_frame.ch1.led_red[0];
            frame.ch1.led_red[1]   = d_frame.ch1.led_red[1];
            frame.ch1.led_red[2]   = d_frame.ch1.led_red[2];
            frame.ch1.led_red[3]   = d_frame.ch1.led_red[3];
            frame.ch2.led_green[0] = d_frame.ch2.led_green[0];
            frame.ch2.led_green[1] = d_frame.ch2.led_green[1];
            frame.ch2.led_green[2] = d_frame.ch2.led_green[2];
            frame.ch2.led_green[3] = d_frame.ch2.led_green[3];
            frame.ch2.led_red[0]   = d_frame.ch2.led_red[0];
            frame.ch2.led_red[1]   = d_frame.ch2.led_red[1];
            frame.ch2.led_red[2]   = d_frame.ch2.led_red[2];
            frame.ch2.led_red[3]   = d_frame.ch2.led_red[3];
            frame.lights_updated   = true;
        });

        AnalogEngine::Frame a_frame;

        a_frame.ch1.level_mod_knob      = frame.ch1.level_mod_knob;
        a_frame.ch1.response_knob       = frame.ch1.response_knob;
        a_frame.ch2.level_mod_knob      = frame.ch2.level_mod_knob;
        a_frame.ch2.response_knob       = frame.ch2.response_knob;

        a_frame.ch1.signal_in           = ch1_signal_in;
        a_frame.ch1.level_cv            = ch1_level_cv;
        a_frame.ch2.signal_in           = ch2_signal_in;
        a_frame.ch2.level_cv            = ch2_level_cv;

        a_frame.ch1.dac_cv              = d_output.samples[0];
        a_frame.ch1.pwm_cv              = d_output.samples[1];
        a_frame.ch2.dac_cv              = d_output.samples[2];
        a_frame.ch2.pwm_cv              = d_output.samples[3];

        analog_engine_.Process(a_frame);

        frame.ch1.signal_out = a_frame.ch1.signal_out;
        frame.ch2.signal_out = a_frame.ch2.signal_out;

        adc_feedback_[0] = a_frame.ch1.adc_out;
        adc_feedback_[1] = a_frame.ch2.adc_out;
    }

protected:
    static constexpr int kBlockSize = 16;
    static constexpr float kAdcFilterCutoff = 1.f / (2 * M_PI * 20e3f * 1e-9f);
    static constexpr float kAdcFilterGain = -20e3f / 100e3f;
    static constexpr float kAdcFilterOffset = -10.f * -20e3f / 120e3f;
    static constexpr float kSignalInNormalV = 5.f;
    static constexpr float kLevelNormalV = 8.f;

    dsp::TRCFilter<simd::float_4> adc_lpf_;
    using Rsmp = InterpolatingResampler<6, 4, kBlockSize, 256>;
    Rsmp resampler_;
    AnalogEngine analog_engine_;
    DigitalEngine digital_engine_;
    float adc_feedback_[2];
};

}
