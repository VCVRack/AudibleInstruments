// Copyright 2013 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// User interface.

#pragma once

#include <algorithm>

#include "stmlib/stmlib.h"
#include "src/Streams/event_queue.hpp"

#include "src/Streams/audio_cv_meter.hpp"
#include "src/Streams/cv_scaler.hpp"
#include "streams/processor.h"

#include "src/Streams/adc.hpp"
#include "src/Streams/leds.hpp"
#include "src/Streams/switches.hpp"

namespace streams
{

enum DisplayMode
{
    DISPLAY_MODE_FUNCTION,
    DISPLAY_MODE_MONITOR,
    DISPLAY_MODE_MONITOR_FUNCTION
};

enum MonitorMode
{
    MONITOR_MODE_EXCITE_IN,
    MONITOR_MODE_VCA_CV,
    MONITOR_MODE_AUDIO_IN,
    MONITOR_MODE_OUTPUT,
    MONITOR_MODE_LAST
};

enum Switch
{
    SWITCH_MODE_1,
    SWITCH_MODE_2,
    SWITCH_MONITOR,
    SWITCH_LAST
};

struct UiSettings
{
    uint8_t function[kNumChannels];
    uint8_t alternate[kNumChannels];
    uint8_t monitor_mode;
    uint8_t linked;
    uint8_t padding[2];
};

class Processor;

class Ui
{
public:
    Ui() { }
    ~Ui() { }

    void Init(AdcEmulator* adc, CvScaler* cv_scaler, Processor* processor,
        UiSettings* settings = nullptr)
    {
        queue_.Init();
        time_us_ = 0;
        leds_.Init();
        switches_.Init();
        adc_ = adc;
        cv_scaler_ = cv_scaler;
        processor_ = processor;

        for (int i = 0; i < kNumPots; i++)
        {
            pot_value_[i] = 0;
            pot_threshold_[i] = 0;
        }

        for (int i = 0; i < kNumSwitches; i++)
        {
            press_time_[i] = 0;
        }

        if (settings)
        {
            ApplySettings(*settings);
        }
        else
        {
            // Flash is not formatted. Initialize.
            for (uint8_t i = 0; i < kNumChannels; ++i)
            {
                ui_settings_.function[i] = PROCESSOR_FUNCTION_ENVELOPE;
                ui_settings_.alternate[i] = false;
            }

            ui_settings_.monitor_mode = MONITOR_MODE_OUTPUT;
            ui_settings_.linked = false;

            ApplySettings(ui_settings_);
        }

        for (uint8_t i = 0; i < kNumChannels; ++i)
        {
            meter_[i].Init();
            display_mode_[i] = DISPLAY_MODE_MONITOR;
        }

        secret_handshake_counter_ = 0;
    }

    void Poll(uint32_t timestep_us)
    {
        queue_.StepTime(timestep_us);
        time_us_ += timestep_us;

        switches_.Debounce();

        for (uint8_t i = 0; i < kNumSwitches; ++i)
        {
            if (switches_.just_pressed(i))
            {
                queue_.AddEvent(CONTROL_SWITCH, i, 0);
                press_time_[i] = (time_us_ / 1000);
            }

            if (switches_.pressed(i) && press_time_[i] != 0)
            {
                int32_t pressed_time = (time_us_ / 1000) - press_time_[i];

                if (pressed_time > kLongPressDuration)
                {
                    queue_.AddEvent(CONTROL_SWITCH, i, pressed_time);
                    press_time_[i] = 0;
                }
            }

            if (switches_.released(i) && press_time_[i] != 0)
            {
                queue_.AddEvent(
                    CONTROL_SWITCH,
                    i,
                    (time_us_ / 1000) - press_time_[i] + 1);
                press_time_[i] = 0;
            }
        }

        for (uint8_t i = 0; i < kNumPots; ++i)
        {
            int32_t value = adc_->pot(i);
            int32_t current_value = pot_value_[i];

            if (value >= current_value + pot_threshold_[i] ||
                value <= current_value - pot_threshold_[i] ||
                !pot_threshold_[i])
            {
                Event e;
                e.control_id = i;
                e.data = value;
                queue_.AddEvent(CONTROL_POT, i, e.data);
                pot_value_[i] = value;
                pot_threshold_[i] = 256;
            }
        }

        PaintLeds(timestep_us);
    }

    void DoEvents()
    {
        bool refresh = false;

        while (queue_.available())
        {
            Event e = queue_.PullEvent();

            if (e.control_type == CONTROL_SWITCH)
            {
                if (e.data == 0)
                {
                    OnSwitchPressed(e);
                }
                else
                {
                    OnSwitchReleased(e);
                }
            }
            else if (e.control_type == CONTROL_POT)
            {
                OnPotMoved(e);
            }

            refresh = true;
        }

        if (queue_.idle_time() / 1000 > 1000)
        {
            queue_.Touch();

            if (display_mode_[0] == DISPLAY_MODE_MONITOR_FUNCTION &&
                display_mode_[1] == DISPLAY_MODE_MONITOR_FUNCTION)
            {
                display_mode_[0] = display_mode_[1] = DISPLAY_MODE_MONITOR;
            }

            refresh = true;
        }

        // Recompute processor parameters if necessary.
        if (refresh)
        {
            for (uint8_t i = 0; i < kNumChannels; ++i)
            {
                processor_[i].Configure();
            }
        }
    }

    void FlushEvents()
    {
        queue_.Flush();
    }

    LedsEmulator& leds()
    {
        return leds_;
    }

    SwitchesEmulator& switches()
    {
        return switches_;
    }

    void ToggleLink(uint8_t follower_channel)
    {
        bool state = !linked();

        for (uint8_t i = 0; i < kNumChannels; ++i)
        {
            display_mode_[i] = DISPLAY_MODE_FUNCTION;
            processor_[i].set_linked(state);
        }

        Link(1 - follower_channel);
        SaveState();
    }

    void SetLinked(bool state)
    {
        if (state != linked())
        {
            ToggleLink(1);
        }
    }

    bool linked()
    {
        return processor_[0].linked();
    }

    const UiSettings& settings()
    {
        return ui_settings_;
    }

    void ApplySettings(const UiSettings& settings)
    {
        bool ch2_changed = settings.function[1]  != ui_settings_.function[1] ||
                           settings.alternate[1] != ui_settings_.alternate[1];
        bool channels_eq = settings.function[0]  == settings.function[1] &&
                           settings.alternate[0] == settings.alternate[1];
        bool unlink = ch2_changed && !channels_eq;

        ui_settings_ = settings;

        if (unlink)
        {
            ui_settings_.linked = false;
        }
        else if (ui_settings_.linked)
        {
            ui_settings_.function[1] = ui_settings_.function[0];
            ui_settings_.alternate[1] = ui_settings_.alternate[0];
        }

        monitor_mode_ = static_cast<MonitorMode>(ui_settings_.monitor_mode);

        for (uint8_t i = 0; i < kNumChannels; ++i)
        {
            processor_[i].set_alternate(ui_settings_.alternate[i]);
            processor_[i].set_linked(ui_settings_.linked);
            processor_[i].set_function(
                static_cast<ProcessorFunction>(ui_settings_.function[i]));
        }
    }

    DisplayMode display_mode(int channel)
    {
        return display_mode_[channel];
    }

    MonitorMode monitor_mode(void)
    {
        return monitor_mode_;
    }

    void SyncUI(const Ui& other)
    {
        ApplySettings(other.ui_settings_);

        for (int i = 0; i < kNumChannels; i++)
        {
            press_time_[i] = other.press_time_[i];
            display_mode_[i] = other.display_mode_[i];
            pot_value_[i] = other.pot_value_[i];
            pot_threshold_[i] = other.pot_threshold_[i];
            meter_[i] = other.meter_[i];
        }

        time_us_ = other.time_us_;
        switches_ = other.switches_;
        monitor_mode_ = other.monitor_mode_;
        ui_settings_ = other.ui_settings_;
        secret_handshake_counter_ = other.secret_handshake_counter_;
    }

private:
    // Synchronize UI state of channels 1 & 2.

    void Link(uint8_t index)
    {
        if (processor_[0].linked())
        {
            for (uint8_t i = 0; i < kNumChannels; ++i)
            {
                if (i != index)
                {
                    display_mode_[i] = display_mode_[index];
                    processor_[i].set_function(processor_[index].function());
                    processor_[i].set_alternate(processor_[index].alternate());
                }
            }
        }
    }

    void OnSwitchPressed(const Event& e)
    {
        switch (e.control_id)
        {
            case SWITCH_MONITOR:
            {
                if (display_mode_[0] == DISPLAY_MODE_MONITOR &&
                    display_mode_[1] == DISPLAY_MODE_MONITOR)
                {
                    display_mode_[0] = display_mode_[1] = DISPLAY_MODE_MONITOR_FUNCTION;
                }
                else if (display_mode_[0] == DISPLAY_MODE_MONITOR_FUNCTION &&
                    display_mode_[1] == DISPLAY_MODE_MONITOR_FUNCTION)
                {
                    monitor_mode_ = static_cast<MonitorMode>(monitor_mode_ + 1);

                    if (monitor_mode_ == MONITOR_MODE_LAST)
                    {
                        monitor_mode_ = static_cast<MonitorMode>(0);
                    }

                    SaveState();
                }
                else
                {
                    display_mode_[0] = display_mode_[1] = DISPLAY_MODE_MONITOR;
                }
            }
            break;

            default:
                break;
        }
    }

    void OnSwitchReleased(const Event& e)
    {
        // Detect secret handshake for easter egg...
        uint8_t secret_handshake_code = e.control_id;
        secret_handshake_code |= e.data >= kLongPressDuration ? 2 : 0;

        if ((secret_handshake_counter_ & 3) == secret_handshake_code)
        {
            ++secret_handshake_counter_;

            if (secret_handshake_counter_ == 16)
            {
                for (uint8_t i = 0; i < kNumChannels; ++i)
                {
                    processor_[i].set_alternate(false);
                    processor_[i].set_function(PROCESSOR_FUNCTION_LORENZ_GENERATOR);
                }

                SaveState();
                secret_handshake_counter_ = 0;
                return;
            }
        }
        else
        {
            secret_handshake_counter_ = 0;
        }

        if (e.data >= kLongPressDuration)
        {
            // Handle long presses.
            switch (e.control_id)
            {
                case SWITCH_MONITOR:
                    break;

                case SWITCH_MODE_1:
                case SWITCH_MODE_2:
                {
                    processor_[e.control_id].set_alternate(
                        !processor_[e.control_id].alternate());

                    if (processor_[e.control_id].function() >
                        PROCESSOR_FUNCTION_COMPRESSOR)
                    {
                        processor_[e.control_id].set_function(PROCESSOR_FUNCTION_ENVELOPE);
                    }

                    display_mode_[e.control_id] = DISPLAY_MODE_FUNCTION;
                    int32_t other = 1 - e.control_id;

                    if (display_mode_[other] == DISPLAY_MODE_MONITOR_FUNCTION)
                    {
                        display_mode_[other] = DISPLAY_MODE_MONITOR;
                    }

                    Link(e.control_id);
                    SaveState();
                }
                break;
            }
        }
        else
        {
            switch (e.control_id)
            {
                case SWITCH_MODE_1:
                case SWITCH_MODE_2:
                {
                    if (display_mode_[e.control_id] == DISPLAY_MODE_FUNCTION)
                    {
                        ProcessorFunction index = processor_[e.control_id].function();
                        index = static_cast<ProcessorFunction>(index + 1);
                        ProcessorFunction limit = processor_[e.control_id].alternate()
                            ? PROCESSOR_FUNCTION_FILTER_CONTROLLER
                            : PROCESSOR_FUNCTION_COMPRESSOR;

                        if (index > limit)
                        {
                            index = static_cast<ProcessorFunction>(0);
                        }

                        processor_[e.control_id].set_function(index);
                    }
                    else
                    {
                        display_mode_[e.control_id] = DISPLAY_MODE_FUNCTION;
                        int32_t other = 1 - e.control_id;

                        if (display_mode_[other] == DISPLAY_MODE_MONITOR_FUNCTION)
                        {
                            display_mode_[other] = DISPLAY_MODE_MONITOR;
                        }
                    }

                    Link(e.control_id);
                    SaveState();
                }
                break;

                default:
                    break;
            }
        }
    }

    void OnPotMoved(const Event& e)
    {
        processor_[0].set_global(e.control_id, e.data);
        processor_[1].set_global(e.control_id, e.data);
        processor_[e.control_id >> 1].set_parameter(e.control_id & 1, e.data);

        for (uint8_t i = 0; i < kNumChannels; ++i)
        {
            processor_[i].Configure();
        }
    }

    void SaveState()
    {
        ui_settings_.monitor_mode = monitor_mode_;
        ui_settings_.linked = processor_[0].linked();
        ui_settings_.function[0] = processor_[0].function();
        ui_settings_.function[1] = processor_[1].function();
        ui_settings_.alternate[0] = processor_[0].alternate();
        ui_settings_.alternate[1] = processor_[1].alternate();
    }

    void PaintLeds(uint32_t timestep_us)
    {
        leds_.Clear();

        for (uint8_t i = 0; i < kNumChannels; ++i)
        {
            uint8_t bank = i * 4;

            switch (display_mode_[i])
            {
                case DISPLAY_MODE_FUNCTION:
                {
                    bool alternate = processor_[i].alternate();
                    uint8_t intensity = 255;

                    if (processor_[i].linked())
                    {
                        uint8_t phase = (time_us_ / 1000) >> 1;
                        phase += i * 128;
                        phase = phase < 128 ? phase : (255 - phase);
                        intensity = (phase * 224 >> 7) + 32;
                        intensity = intensity * intensity >> 8;
                    }

                    uint8_t function = processor_[i].function();

                    if (function == PROCESSOR_FUNCTION_FILTER_CONTROLLER)
                    {
                        for (uint8_t j = 0; j < 4; ++j)
                        {
                            leds_.set(bank + j,
                                alternate ? intensity : 0,
                                alternate ? 0 : intensity);
                        }
                    }
                    else if (function < PROCESSOR_FUNCTION_LORENZ_GENERATOR)
                    {
                        leds_.set(
                            bank + function,
                            alternate ? intensity : 0,
                            alternate ? 0 : intensity);
                    }
                    else
                    {
                        uint8_t index = (processor_[i].last_gain() >> 4) * 5 >> 4;

                        if (index > 3)
                        {
                            index = 3;
                        }

                        int16_t color = processor_[i].last_frequency();
                        color = color - 128;
                        color *= 2;

                        if (color < 0)
                        {
                            if (color < -127)
                            {
                                color = -127;
                            }

                            leds_.set(bank + index, 255 + (color * 2), 255);
                        }
                        else
                        {
                            if (color > 127)
                            {
                                color = 127;
                            }

                            leds_.set(bank + index, 255, 255 - (color * 2));
                        }
                    }
                }
                break;

                case DISPLAY_MODE_MONITOR_FUNCTION:
                {
                    uint8_t position = static_cast<uint8_t>(monitor_mode_);
                    leds_.set(position * 2, 255, 0);
                    leds_.set(position * 2 + 1, 255, 0);
                }
                break;

                case DISPLAY_MODE_MONITOR:
                    PaintMonitor(i, timestep_us);
                    break;
            }
        }
    }

    void PaintMonitor(uint8_t channel, uint32_t timestep_us)
    {
        switch (monitor_mode_)
        {
            case MONITOR_MODE_EXCITE_IN:
                PaintAdaptive(channel, cv_scaler_->excite_sample(channel), 0, timestep_us);
                break;

            case MONITOR_MODE_AUDIO_IN:
                PaintAdaptive(channel, cv_scaler_->audio_sample(channel), 0, timestep_us);
                break;

            case MONITOR_MODE_VCA_CV:
                leds_.PaintPositiveBar(channel, 32768 + cv_scaler_->gain_sample(channel));
                break;

            case MONITOR_MODE_OUTPUT:
                if (processor_[channel].function() == PROCESSOR_FUNCTION_COMPRESSOR)
                {
                    leds_.PaintNegativeBar(channel, processor_[channel].gain_reduction());
                }
                else
                {
                    PaintAdaptive(
                        channel,
                        cv_scaler_->audio_sample(channel),
                        cv_scaler_->gain_sample(channel),
                        timestep_us);
                }

                break;

            default:
                break;
        }
    }

    void PaintAdaptive(uint8_t channel, int32_t sample, int32_t gain,
        uint32_t timestep_us)
    {
        meter_[channel].Process(sample, timestep_us);

        if (meter_[channel].cv())
        {
            sample = sample * lut_2164_gain[-gain >> 9] >> 15;
            leds_.PaintCv(channel, sample * 5 >> 2);
        }
        else
        {
            leds_.PaintPositiveBar(channel, wav_db[meter_[channel].peak() >> 7] + gain);
        }
    }

    EventQueue<16> queue_;
    uint32_t time_us_;

    AdcEmulator* adc_;
    CvScaler* cv_scaler_;
    Processor* processor_;
    LedsEmulator leds_;
    SwitchesEmulator switches_;
    uint32_t press_time_[kNumSwitches];
    DisplayMode display_mode_[kNumChannels];

    MonitorMode monitor_mode_;

    UiSettings ui_settings_;

    uint8_t secret_handshake_counter_;

    int32_t pot_value_[kNumPots];
    int32_t pot_threshold_[kNumPots];

    AudioCvMeter meter_[kNumChannels];

    const int32_t kLongPressDuration = 1000;

    DISALLOW_COPY_AND_ASSIGN(Ui);
};

}  // namespace streams

