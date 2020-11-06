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

#include <rack.hpp>
#include "aafilter.hpp"

namespace streams
{

using namespace rack;

class AnalogEngine
{
public:
    struct ChannelFrame
    {
        // Parameters
        float level_mod_knob;
        float response_knob;

        // Inputs
        float signal_in;
        float level_cv;

        float dac_cv;
        float pwm_cv;

        // Outputs
        float signal_out;
        float adc_out;
    };

    struct Frame
    {
        ChannelFrame ch1;
        ChannelFrame ch2;
    };

    AnalogEngine()
    {
        Reset();
        SetSampleRate(1.f);
    }

    void Reset(void)
    {
        v_out_ = 0.f;
    }

    void SetSampleRate(float sample_rate)
    {
        sample_time_ = 1.f / sample_rate;
        oversampling_ = OversamplingFactor(sample_rate);
        float oversampled_rate = sample_rate * oversampling_;

        up_filter_[0].Init(sample_rate);
        up_filter_[1].Init(sample_rate);
        down_filter_.Init(sample_rate);

        auto cutoff = float_4(kDACFilterCutoff,
                              kDACFilterCutoff,
                              kPWMFilterCutoff,
                              kPWMFilterCutoff);
        rc_lpf_.setCutoffFreq(cutoff / oversampled_rate);
    }

    void Process(Frame& frame)
    {
        auto a_inputs = float_4(frame.ch1.signal_in,
                                frame.ch2.signal_in,
                                frame.ch1.level_cv,
                                frame.ch2.level_cv);
        auto d_inputs = float_4(frame.ch1.dac_cv,
                                frame.ch2.dac_cv,
                                frame.ch1.pwm_cv,
                                frame.ch2.pwm_cv);

        auto level_mod  = float_4(frame.ch1.level_mod_knob,
                                  frame.ch2.level_mod_knob, 0.f, 0.f);
        auto response   = float_4(frame.ch1.response_knob,
                                  frame.ch2.response_knob, 0.f, 0.f);

        float_4 output;
        float timestep = sample_time_ / oversampling_;

        for (int i = 0; i < oversampling_; i++)
        {
            // Upsample and apply anti-aliasing filters
            a_inputs = up_filter_[0].Process(
                (i == 0) ? (a_inputs * oversampling_) : 0.f);
            d_inputs = up_filter_[1].Process(
                (i == 0) ? (d_inputs * oversampling_) : 0.f);

            rc_lpf_.process(d_inputs);
            d_inputs = rc_lpf_.lowpass();

            float_4 signal_in = a_inputs;
            float_4 level_cv = _mm_movehl_ps(a_inputs.v, a_inputs.v);
            float_4 dac_cv = d_inputs;
            auto level = CalculateLevel(dac_cv, level_cv, level_mod, response);
            signal_in *= level;

            float_4 pwm_cv = _mm_movehl_ps(d_inputs.v, d_inputs.v);
            pwm_cv *= kPWMCVInputR / (kPWMCVInputR + kPWMCVOutputR);
            auto rad_per_s = -PinVoltageToLevel(pwm_cv) / kFilterCoreRC;

            // Solve each VCF cell using the backward Euler method.
            float_4 v_in = _mm_movelh_ps(signal_in.v, v_out_.v);
            v_out_ = (v_in + v_out_) * simd::exp(rad_per_s * timestep) - v_in;
            v_out_ = simd::clamp(v_out_, -kClampVoltage, kClampVoltage);

            // Pre-downsample anti-alias filtering
            // output will contain the lower two elements from level and the
            // upper two elements from v_out_
            output =
                _mm_shuffle_ps(level.v, v_out_.v, _MM_SHUFFLE(3, 2, 1, 0));
            output = down_filter_.Process(output);
        }

        frame.ch1.signal_out = output[2];
        frame.ch2.signal_out = output[3];

        // We don't care too much about aliasing in this signal, since it's
        // only fed back to the digital section for VU metering.
        output = LevelToPinVoltage(output);
        frame.ch1.adc_out = output[0];
        frame.ch2.adc_out = output[1];
    }

protected:
    using float_4 = simd::float_4;

    float sample_time_;
    int oversampling_;
    UpsamplingAAFilter<float_4> up_filter_[2];
    DownsamplingAAFilter<float_4> down_filter_;
    dsp::TRCFilter<float_4> rc_lpf_;
    float_4 v_out_;

    // Calculate level from the VCA control pin voltage
    template <typename T>
    T PinVoltageToLevel(T v_control)
    {
        return simd::pow(10.f, v_control / (kVCAGainConstant * 20.f));
    }

    // Calculate VCA control pin voltage from level
    template <typename T>
    T LevelToPinVoltage(T level)
    {
        T volts = kVCAGainConstant * 20.f * simd::log10(level);
        return simd::ifelse(level > 0.f, volts, kClampVoltage);
    }

    // Calculate level from the CV inputs and pots
    template <typename T>
    T CalculateLevel(T dac_cv, T level_cv, T level_mod, T response)
    {
        T power = (kLevelResponseMinR + kLevelResponsePotR) /
                  (kLevelResponseMinR + (kLevelResponsePotR * response));

        T i_level = level_mod * level_cv / kLevelCVInputR;
        T i_dac = dac_cv / (kDACCVOutputR + kDACCVInputR);
        T i_in = i_level + i_dac + kVCAOffsetI;

        T base = -i_in / kLevelRefI;
        T level = simd::pow(base, power);
        level = simd::ifelse(base > 0.f, level, 0.f);
        return simd::fmin(level, kVCAMaxLevel);
    }

    // The 2164's gain constant is -33mV/dB
    static constexpr float kVCAGainConstant = -33e-3f;
    // The 2164's maximum gain is +20dB
    static constexpr float kVCAMaxLevel = 10.f;

    static constexpr float kLevelCVInputR = 100e3f;
    static constexpr float kDACCVOutputR = 11e3f;
    static constexpr float kDACCVInputR = 14e3f;
    static constexpr float kVCAOffsetI = -10.f / 10e6f;
    static constexpr float kPWMCVOutputR = 1.2e3f;
    static constexpr float kPWMCVInputR = 2.5e3f;

    // Level response VCA input current
    static constexpr float kLevelRefI = -10.f / 200e3f;
    // Level response potentiometer and series resistor
    static constexpr float kLevelResponsePotR = 10e3f;
    static constexpr float kLevelResponseMinR = 510.f;

    // Opamp saturation voltage
    static constexpr float kClampVoltage = 10.5f;
    // 1N4148 forward voltage
    static constexpr float kDiodeVoltage = 745e-3f;

    static constexpr float kDACFilterCutoff = 12.7e3f;
    static constexpr float kPWMFilterCutoff = 242.f;

    static constexpr float kFilterCoreR = 100e3f;
    static constexpr float kFilterCoreC = 33e-12f;
    static constexpr float kFilterCoreRC = kFilterCoreR * kFilterCoreC;
};

}
