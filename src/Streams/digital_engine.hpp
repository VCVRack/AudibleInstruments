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
#include <rack.hpp>
#include "cv_scaler.hpp"
#include "ui.hpp"
#include <streams/processor.h>

namespace streams
{

using namespace rack;

class DigitalEngine
{
public:
    static constexpr int kSampleRate = 31089;

    template <int block_size>
    struct ChannelFrame
    {
        // Parameters
        float shape_knob;
        float mod_knob;
        float level_mod_knob;
        float response_knob;

        bool function_button;

        // Inputs
        float excite_in[block_size];
        float signal_in[block_size];
        float level_adc_in[block_size];

        // Outputs
        float dac_out[block_size];
        float pwm_out[block_size];

        // Lights
        float led_green[4];
        float led_red[4];
    };

    template <int block_size>
    struct Frame
    {
        ChannelFrame<block_size> ch1;
        ChannelFrame<block_size> ch2;
        bool metering_button;
    };

    DigitalEngine()
    {
        Reset();
    }

    void Reset(void)
    {
        adc_.Init();
        cv_scaler_.Init(&adc_);
        processor_[0].Init(0);
        processor_[1].Init(1);
        ui_.Init(&adc_, &cv_scaler_, &processor_[0]);
        pwm_value_[0] = 0;
        pwm_value_[1] = 0;

        for (int i = 0; i < 4; i++)
        {
            led_lpf_[i].reset();
            led_lpf_[i].setLambda(kLambdaLEDs);
        }
    }

    const UiSettings& ui_settings(void)
    {
        return ui_.settings();
    }

    void ApplySettings(const UiSettings& settings)
    {
        ui_.ApplySettings(settings);
    }

    void SyncUI(const DigitalEngine& other)
    {
        ui_.SyncUI(other.ui_);

        for (int i = 0; i < 4; i++)
        {
            led_lpf_[i].reset();
        }
    }

    void Randomize(void)
    {
        UiSettings settings;

        settings.alternate[0] = random::u32() & 1;
        settings.alternate[1] = random::u32() & 1;
        int modulus0 = (settings.alternate[0]) ?
            1 + PROCESSOR_FUNCTION_FILTER_CONTROLLER :
            1 + PROCESSOR_FUNCTION_COMPRESSOR;
        int modulus1 = (settings.alternate[1]) ?
            1 + PROCESSOR_FUNCTION_FILTER_CONTROLLER :
            1 + PROCESSOR_FUNCTION_COMPRESSOR;
        settings.function[0]  = random::u32() % modulus0;
        settings.function[1]  = random::u32() % modulus1;
        settings.monitor_mode = ui_.settings().monitor_mode;
        settings.linked       = false;

        ApplySettings(settings);
    }

    template <int block_size>
    void Process(Frame<block_size>& frame)
    {
        ProcessUI(frame);

        for (int i = 0; i < block_size; i++)
        {
            float ch1_signal_adc = clamp(frame.ch1.signal_in[i], 0.f, kVdda);
            float ch2_signal_adc = clamp(frame.ch2.signal_in[i], 0.f, kVdda);
            float ch1_excite_adc = clamp(frame.ch1.excite_in[i], 0.f, kVdda);
            float ch2_excite_adc = clamp(frame.ch2.excite_in[i], 0.f, kVdda);
            float ch1_level_adc  = clamp(frame.ch1.level_adc_in[i], 0.f, kVdda);
            float ch2_level_adc  = clamp(frame.ch2.level_adc_in[i], 0.f, kVdda);

            adc_.cvs_[0] = std::round(0xFFFF * ch1_excite_adc / kVdda);
            adc_.cvs_[1] = std::round(0xFFFF * ch1_signal_adc / kVdda);
            adc_.cvs_[2] = std::round(0xFFFF * ch1_level_adc  / kVdda);
            adc_.cvs_[3] = std::round(0xFFFF * ch2_excite_adc / kVdda);
            adc_.cvs_[4] = std::round(0xFFFF * ch2_signal_adc / kVdda);
            adc_.cvs_[5] = std::round(0xFFFF * ch2_level_adc  / kVdda);

            uint16_t gain[2] = {0, 0};
            uint16_t frequency[2] = {kPWMPeriod, kPWMPeriod};

            for (int i = 0; i < 2; i++)
            {
                processor_[i].Process(
                    cv_scaler_.audio_sample(i),
                    cv_scaler_.excite_sample(i),
                    &gain[i],
                    &frequency[i]);

                // Filter PWM value
                frequency[i] = kPWMPeriod - frequency[i];
                pwm_value_[i] += (frequency[i] - pwm_value_[i]) >> 4;
            }

            frame.ch1.dac_out[i] =
                kVoltsPerLSB * cv_scaler_.ScaleGain(0, gain[0]);
            frame.ch2.dac_out[i] =
                kVoltsPerLSB * cv_scaler_.ScaleGain(1, gain[1]);

            frame.ch1.pwm_out[i] = (kVdda / kPWMPeriod) * pwm_value_[0];
            frame.ch2.pwm_out[i] = (kVdda / kPWMPeriod) * pwm_value_[1];
        }
    }

protected:
    using float_4 = simd::float_4;

    AdcEmulator adc_;
    CvScaler cv_scaler_;
    Processor processor_[2];
    Ui ui_;
    uint16_t pwm_value_[2];
    dsp::TExponentialFilter<float_4> led_lpf_[4];

    template <int block_size>
    void ProcessUI(Frame<block_size>& frame)
    {
        float timestep = block_size * 1.f / kSampleRate;

        adc_.pots_[0] = std::round(0xFFFF * frame.ch1.shape_knob);
        adc_.pots_[1] = std::round(0xFFFF * frame.ch1.mod_knob);
        adc_.pots_[2] = std::round(0xFFFF * frame.ch2.shape_knob);
        adc_.pots_[3] = std::round(0xFFFF * frame.ch2.mod_knob);

        ui_.switches().SetPin(SWITCH_MODE_1,  frame.ch1.function_button);
        ui_.switches().SetPin(SWITCH_MODE_2,  frame.ch2.function_button);
        ui_.switches().SetPin(SWITCH_MONITOR, frame.metering_button);

        ui_.Poll(timestep * 1e6);
        ui_.DoEvents();

        for (int i = 0; i < 4; i++)
        {
            auto led = float_4(
                ui_.leds().intensity_green(i),
                ui_.leds().intensity_red(i),
                ui_.leds().intensity_green(i + 4),
                ui_.leds().intensity_red(i + 4));
            led = led_lpf_[i].process(timestep, led);

            frame.ch1.led_green[i] = led[0];
            frame.ch1.led_red[i]   = led[1];
            frame.ch2.led_green[i] = led[2];
            frame.ch2.led_red[i]   = led[3];
        }
    }

    static constexpr int kUiPollRate = 4000;
    static constexpr float kVdda = 3.3f;
    static constexpr int kPWMPeriod = 65535;
    static constexpr float kDacVref = 2.5f;
    static constexpr float kVoltsPerLSB = kDacVref / 65536.f;

    // The VU meter flickers when monitoring LEVEL or OUT when there is an
    // audio signal at the LEVEL input. Due to human persistence of vision,
    // the only noticable effect on the hardware module is a slight dimming of
    // the LEDs. However, the flickering is very apparent on the software module
    // due to the low UI refresh rate. We solve this by applying a lowpass
    // filter to the LED brightness. This lambda value is simply hand-tuned
    // to match hardware.
    static constexpr float kLambdaLEDs = 1.5e-3 * kSampleRate;
};

}
