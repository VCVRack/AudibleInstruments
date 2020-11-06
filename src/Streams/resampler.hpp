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

namespace streams
{

template <int num_inputs, int num_outputs, int block_size, int buffer_size>
class SRCResampler
{
public:
    void Init(int outer_sample_rate, int inner_sample_rate, int quality)
    {
        input_src.setRates(outer_sample_rate, inner_sample_rate);
        output_src.setRates(inner_sample_rate, outer_sample_rate);

        input_src.setQuality(quality);
        output_src.setQuality(quality);

        Reset();
    }

    void Reset(void)
    {
        input_src.refreshState();
        output_src.refreshState();

        input_buffer.clear();
        output_buffer.clear();
    }

    using InputFrame = dsp::Frame<num_inputs>;
    using OutputFrame = dsp::Frame<num_outputs>;

    template <typename F>
    OutputFrame Process(InputFrame& input_frame, F callback)
    {
        if (!input_buffer.full())
        {
            input_buffer.push(input_frame);
        }

        if (output_buffer.empty())
        {
            // Resample input buffer
            InputFrame input_frames[block_size] = {};
            int in_len = input_buffer.size();
            int out_len = block_size;
            input_src.process(
                input_buffer.startData(), &in_len,
                input_frames, &out_len);
            input_buffer.startIncr(in_len);

            // Process the resampled signal
            // We might not fill all of the input buffer if there is a
            // deficiency, but this cannot be avoided due to imprecisions
            // between the input and output SRC.
            OutputFrame output_frames[block_size];
            callback(output_frames, input_frames);

            // Resample output buffer
            in_len = block_size;
            out_len = output_buffer.capacity();
            output_src.process(
                output_frames, &in_len,
                output_buffer.endData(), &out_len);
            output_buffer.endIncr(out_len);
        }

        // Set output
        OutputFrame output_frame = {};

        if (!output_buffer.empty())
        {
            output_frame = output_buffer.shift();
        }

        return output_frame;
    }

protected:
    dsp::SampleRateConverter<num_inputs> input_src;
    dsp::SampleRateConverter<num_outputs> output_src;
    dsp::DoubleRingBuffer<InputFrame, buffer_size> input_buffer;
    dsp::DoubleRingBuffer<OutputFrame, buffer_size> output_buffer;
};

template <int num_inputs, int num_outputs, int block_size, int buffer_size>
class InterpolatingResampler
{
public:
    void Init(int outer_sample_rate, int inner_sample_rate, int quality)
    {
        (void)quality;

        ratio_ = inner_sample_rate * 1.f / outer_sample_rate;
        ratio_inverse_ = 1.f / ratio_;
    }

    void Reset(void)
    {
        in_buffer_.clear();
        out_buffer_.clear();
        out_buffer_.endIncr(block_size);
        in_phase_ = 1.f;
        prev_input_ = {};
        prev_output_ = {};
        next_output_ = {};
    }

    using InputFrame = dsp::Frame<num_inputs>;
    using OutputFrame = dsp::Frame<num_outputs>;

    template <typename F>
    OutputFrame Process(InputFrame& input_frame, F callback)
    {
        int pushed = -in_buffer_.size();

        while (in_phase_ <= 1.f)
        {
            // Resample and push into input buffer
            auto input = CrossfadeFrame(prev_input_, input_frame, in_phase_);
            in_buffer_.push(input);
            in_phase_ += ratio_inverse_;
        }

        in_phase_ -= 1.f;
        prev_input_ = input_frame;
        pushed += in_buffer_.size();

        // Process the resampled signal
        while (in_buffer_.size() >= block_size)
        {
            callback(out_buffer_.endData(), in_buffer_.startData());
            in_buffer_.startIncr(block_size);
            out_buffer_.endIncr(block_size);
        }

        // Resample from output buffer
        float phase = clamp(1.f - in_phase_ * ratio_, 0.f, 1.f);

        while (pushed--)
        {
            prev_output_ = next_output_;
            next_output_ = out_buffer_.shift();
        }

        return CrossfadeFrame(prev_output_, next_output_, phase);
    }

protected:
    float ratio_;
    float ratio_inverse_;
    dsp::DoubleRingBuffer<InputFrame, buffer_size> in_buffer_;
    dsp::DoubleRingBuffer<OutputFrame, buffer_size> out_buffer_;
    float in_phase_;
    InputFrame prev_input_;
    OutputFrame prev_output_;
    OutputFrame next_output_;

    template <typename T>
    T Crossfade(T a, T b, float x)
    {
        return a + (b - a) * x;
    }

    InputFrame CrossfadeFrame(InputFrame a, InputFrame b, float x)
    {
        InputFrame result;

        for (int i = 0; i < num_inputs; i++)
        {
            result.samples[i] = Crossfade(a.samples[i], b.samples[i], x);
        }

        return result;
    }

    OutputFrame CrossfadeFrame(OutputFrame a, OutputFrame b, float x)
    {
        OutputFrame result;

        for (int i = 0; i < num_outputs; i++)
        {
            result.samples[i] = Crossfade(a.samples[i], b.samples[i], x);
        }

        return result;
    }
};

}
