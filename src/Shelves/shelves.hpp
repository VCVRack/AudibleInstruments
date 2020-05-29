// Mutable Instruments Shelves emulation for VCV Rack
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

#include <cmath>
#include <algorithm>
#include <rack.hpp>
#include "aafilter.hpp"

namespace shelves
{

using namespace rack;

// Knob ranges
static const float kFreqKnobMin = 20.f;
static const float kFreqKnobMax = 20e3f;
static const float kGainKnobRange = 18.f;
static const float kQKnobMin = 0.5f;
static const float kQKnobMax = 40.f;

// Opamp saturation voltage
static const float kClampVoltage = 10.5f;

// Filter core
static const float kFilterMaxCutoff = kFreqKnobMax;
static const float kFilterR = 100e3f;
static const float kFilterRC = 1.f / (2.f * M_PI * kFilterMaxCutoff);
static const float kFilterC = kFilterRC / kFilterR;

// Frequency CV amplifier
static const float kFreqAmpR = 18e3f;
static const float kFreqAmpC = 560e-12f;
static const float kFreqAmpInputR = 100e3f;
static const float kMinVOct = -kClampVoltage * kFreqAmpInputR / kFreqAmpR;
static const float kFreqKnobVoltage = std::log2f(kFreqKnobMax / kFreqKnobMin);

// The 2164's gain constant is -33mV/dB
static const float kVCAGainConstant = -33e-3f;

inline float QFactorToVoltage(float q_factor)
{
    // Calculate the voltage at the VCA control port for a given Q factor
    return 20.f * -kVCAGainConstant * std::log10(2.f * q_factor);
}

// Q CV amplifier
static const float kQAmpR = 22e3f;
static const float kQAmpC = 560e-12f;
static const float kQAmpGain = -kQAmpR / 150e3f;
static const float kQKnobMinVoltage = QFactorToVoltage(kQKnobMin) / kQAmpGain;
static const float kQKnobMaxVoltage = QFactorToVoltage(kQKnobMax) / kQAmpGain;

// Gain CV amplifier
static const float kGainPerVolt = 10.f * std::log10(2.f); // 3dB per volt
static const float kMaximumGain = 24.f;

// Clipping indicator
static const float kClipLEDThreshold = 7.86f;
static const float kClipInputR = 150e3f;
static const float kClipInputC = 100e-9f;
static const float kClipLEDRiseTime = 2e-3f;
static const float kClipLEDFallTime = 10e-3f;

// Solves an ODE system using the 2nd order Runge-Kutta method
template <typename T, typename F>
inline T StepRK2(float dt, T y, F f)
{
    T k1 = f(y);
    T k2 = f(y + k1 * (dt / 2.f));
    return y + dt * k2;
}

template <int len, typename T, typename F>
inline void StepRK2(float dt, T y[], F f)
{
    T k1[len];
    T k2[len];
    T yi[len];

    f(y, k1);

    for (int i = 0; i < len; i++)
    {
        yi[i] = y[i] + k1[i] * (dt / 2.f);
    }

    f(yi, k2);

    for (int i = 0; i < len; i++)
    {
        y[i] += k2[i] * dt;
    }
}

template <typename T>
class LPFilter
{
public:
    void Init(void)
    {
        voltage_ = 0.f;
    }

    T Process(float timestep, T v_in, T vca_level)
    {
        T rad_per_s = -vca_level / kFilterRC;

        voltage_ = StepRK2(timestep, voltage_, [&](T v_state)
        {
            return rad_per_s * (v_in + v_state);
        });

        voltage_ = simd::clamp(voltage_, -kClampVoltage, kClampVoltage);
        return voltage_;
    }

    T voltage(void)
    {
        return voltage_;
    }

protected:
    T voltage_;
};

template <typename T>
class SVFilter
{
public:
    void Init(void)
    {
        bp_ = 0.f;
        lp_ = 0.f;
        hp_ = 0.f;
    }

    T Process(float timestep, T in, T vca_level, T q_level)
    {
        // Thanks Émilie!
        // https://mutable-instruments.net/archive/documents/svf_analysis.pdf
        //
        // For the bandpass integrator,
        //   dv/dt = -A/RC * hp
        // For the lowpass integrator,
        //   dv/dt = -A/RC * bp
        // with
        //   hp = -(in + lp - 2*Q*bp)

        T rad_per_s = -vca_level / kFilterRC;

        StepRK2<2>(timestep, voltage_, [&](const T v_state[], T v_stepped[])
        {
            T lp = v_state[0];
            T bp = v_state[1];
            T hp = -(in + lp - 2.f * q_level * bp);
            v_stepped[0] = rad_per_s * bp;
            v_stepped[1] = rad_per_s * hp;
        });

        lp_ = simd::clamp(lp_, -kClampVoltage, kClampVoltage);
        bp_ = simd::clamp(bp_, -kClampVoltage, kClampVoltage);
        T out = bp() * -2.f * q_level;
        hp_ = -(in + lp() + out);
        return out;
    }

    T hp(void)
    {
        return hp_;
    }

    T bp(void)
    {
        return bp_;
    }

    T lp(void)
    {
        return lp_;
    }

protected:
    union
    {
        T voltage_[2];
        struct
        {
            T lp_;
            T bp_;
        };
    };
    T hp_;
};

class ShelvesEngine
{
public:
    struct Frame
    {
        // Parameters
        float hs_freq_knob; //  0 to 1 linear
        float hs_gain_knob; // -1 to 1 linear
        float p1_freq_knob; //  0 to 1 linear
        float p1_gain_knob; // -1 to 1 linear
        float p1_q_knob;    //  0 to 1 linear
        float p2_freq_knob; //  0 to 1 linear
        float p2_gain_knob; // -1 to 1 linear
        float p2_q_knob;    //  0 to 1 linear
        float ls_freq_knob; //  0 to 1 linear
        float ls_gain_knob; // -1 to 1 linear

        // Inputs
        float hs_freq_cv;
        float hs_gain_cv;
        float p1_freq_cv;
        float p1_gain_cv;
        float p1_q_cv;
        float p2_freq_cv;
        float p2_gain_cv;
        float p2_q_cv;
        float ls_freq_cv;
        float ls_gain_cv;
        float global_freq_cv;
        float global_gain_cv;
        float main_in;

        bool hs_freq_cv_connected;
        bool hs_gain_cv_connected;
        bool p1_freq_cv_connected;
        bool p1_gain_cv_connected;
        bool p1_q_cv_connected;
        bool p2_freq_cv_connected;
        bool p2_gain_cv_connected;
        bool p2_q_cv_connected;
        bool ls_freq_cv_connected;
        bool ls_gain_cv_connected;
        bool global_freq_cv_connected;
        bool global_gain_cv_connected;

        // Outputs
        float p1_hp_out;
        float p1_bp_out;
        float p1_lp_out;
        float p2_hp_out;
        float p2_bp_out;
        float p2_lp_out;
        float main_out;

        bool p1_hp_out_connected;
        bool p1_bp_out_connected;
        bool p1_lp_out_connected;
        bool p2_hp_out_connected;
        bool p2_bp_out_connected;
        bool p2_lp_out_connected;

        // Lights
        float clip;

        // Options
        bool pre_gain; // True = -6dB, False = 0dB
    };

    ShelvesEngine()
    {
        setSampleRate(1.f);
    }

    void setSampleRate(float sample_rate)
    {
        sample_time_ = 1.f / sample_rate;
        oversampling_ = OversamplingFactor(sample_rate);

        up_filter_[0].Init(sample_rate);
        up_filter_[1].Init(sample_rate);
        up_filter_[2].Init(sample_rate);
        down_filter_[0].Init(sample_rate);
        down_filter_[1].Init(sample_rate);

        low_high_.Init();
        mid_.Init();

        float freq_cut = 1.f / (2.f * M_PI * kFreqAmpR * kFreqAmpC);
        freq_lpf_.reset();
        freq_lpf_.setCutoffFreq(freq_cut / sample_rate);

        float q_cut = 1.f / (2.f * M_PI * kQAmpR * kQAmpC);
        q_lpf_.reset();
        q_lpf_.setCutoffFreq(q_cut / sample_rate);

        float clip_in_cut = 1.f / (2.f * M_PI * kClipInputR * kClipInputC);
        clip_hpf_.reset();
        clip_hpf_.setCutoffFreq(clip_in_cut / sample_rate);

        float rise = 1.f / kClipLEDRiseTime;
        float fall = 1.f / kClipLEDFallTime;
        clip_slew_.reset();
        clip_slew_.setRiseFall(rise, fall);
    }

    void process(Frame& frame)
    {
        auto f_knob = simd::float_4(
            frame.ls_freq_knob,
            frame.p1_freq_knob,
            frame.p2_freq_knob,
            frame.hs_freq_knob);

        auto f_cv = simd::float_4(
            frame.ls_freq_cv,
            frame.p1_freq_cv,
            frame.p2_freq_cv,
            frame.hs_freq_cv);

        bool f_cv_exists =
            frame.hs_freq_cv_connected ||
            frame.p1_freq_cv_connected ||
            frame.p2_freq_cv_connected ||
            frame.ls_freq_cv_connected ||
            frame.global_freq_cv_connected;

        auto q_knob = simd::float_4(
            0.f,
            frame.p1_q_knob,
            frame.p2_q_knob,
            0.f);

        auto q_cv = simd::float_4(
            0.f,
            frame.p1_q_cv,
            frame.p2_q_cv,
            0.f);

        bool q_cv_exists = frame.p1_q_cv_connected || frame.p2_q_cv_connected;

        auto gain_knob = simd::float_4(
            frame.ls_gain_knob,
            frame.p1_gain_knob,
            frame.p2_gain_knob,
            frame.hs_gain_knob);

        auto gain_cv = simd::float_4(
            frame.ls_gain_cv,
            frame.p1_gain_cv,
            frame.p2_gain_cv,
            frame.hs_gain_cv);

        bool gain_cv_exists =
            frame.hs_gain_cv_connected ||
            frame.p1_gain_cv_connected ||
            frame.p2_gain_cv_connected ||
            frame.ls_gain_cv_connected ||
            frame.global_gain_cv_connected;

        f_cv += frame.global_freq_cv;
        gain_cv += frame.global_gain_cv;

        // V/oct
        simd::float_4 v_oct = f_cv + kFreqKnobVoltage * (f_knob - 1.f);
        freq_lpf_.process(v_oct);
        v_oct = freq_lpf_.lowpass();

        // Q CV
        q_cv -= simd::rescale(q_knob,
            0.f, 1.f, kQKnobMinVoltage, kQKnobMaxVoltage);
        q_cv *= -kQAmpGain;
        q_lpf_.process(q_cv);
        q_cv = q_lpf_.lowpass();

        // Gain CV
        simd::float_4 gain_db =
            gain_knob * kGainKnobRange + gain_cv * kGainPerVolt;

        // Stuff input into unused element of Q CV vector
        q_cv[0] = frame.main_in * (frame.pre_gain ? 0.25f : 0.5f);

        float timestep = sample_time_ / oversampling_;

        // If a CV input is not connected, we can perform the expensive
        // exponential calculation here only once instead of inside the loop,
        // since we needn't apply oversampling and anti-aliasing to a low-rate
        // UI control.
        simd::float_4 f_level;
        simd::float_4 q_level;
        simd::float_4 gain_level;

        if (!f_cv_exists)
        {
            f_level = FreqVCALevel(v_oct);
        }

        if (!q_cv_exists)
        {
            q_level = QVCALevel(q_cv);
        }

        if (!gain_cv_exists)
        {
            gain_level = GainVCALevel(gain_db);
        }

        // Outputs
        simd::float_4 out1;
        simd::float_4 out2;
        bool out2_connected =
            frame.p2_hp_out_connected ||
            frame.p2_bp_out_connected ||
            frame.p2_lp_out_connected;

        for (int i = 0; i < oversampling_; i++)
        {
            // Upsample and apply anti-aliasing filters if needed
            if (f_cv_exists)
            {
                v_oct = up_filter_[0].Process(
                    (i == 0) ? (v_oct * oversampling_) : 0.f);
                f_level = FreqVCALevel(v_oct);
            }

            // We can't skip this one since it contains the input signal
            q_cv = up_filter_[1].Process(
                (i == 0) ? (q_cv * oversampling_) : 0.f);
            if (q_cv_exists)
            {
                q_level = QVCALevel(q_cv);
            }

            if (gain_cv_exists)
            {
                gain_db = up_filter_[2].Process(
                    (i == 0) ? (gain_db * oversampling_) : 0.f);
                gain_level = GainVCALevel(gain_db);
            }

            // Unpack input from Q CV vector
            simd::float_4 in =
                _mm_shuffle_ps(q_cv.v, q_cv.v, _MM_SHUFFLE(0, 0, 0, 0));

            // Process VCFs
            low_high_.Process(timestep, in, f_level);
            float low = low_high_.voltage()[0];
            float high = low_high_.voltage()[3];
            simd::float_4 mid = mid_.Process(timestep, in, f_level, q_level);

            // Calculate output
            low *= 1.f - gain_level[0];
            mid *= 1.f - gain_level;
            high = -high + (high + in[0]) * gain_level[3];
            float sum = 2.f * (low + mid[1] + mid[2] + high);

            out1 = simd::float_4(sum, mid_.lp()[1], mid_.bp()[1], mid_.hp()[1]);
            out1 = simd::clamp(out1, -kClampVoltage, kClampVoltage);

            // Pre-downsample anti-alias filtering
            out1 = down_filter_[0].Process(out1);

            if (out2_connected)
            {
                out2 = simd::float_4(0.f, mid_.lp()[2], mid_.bp()[2], mid_.hp()[2]);
                out2 = simd::clamp(out2, -kClampVoltage, kClampVoltage);
                out2 = down_filter_[1].Process(out2);
            }
        }

        frame.main_out = out1[0];

        clip_hpf_.process(out1[0]);
        float clip = 1.f * (std::abs(clip_hpf_.highpass()) > kClipLEDThreshold);
        frame.clip = clip_slew_.process(sample_time_, clip);

        frame.p1_lp_out = out1[1];
        frame.p1_bp_out = out1[2];
        frame.p1_hp_out = out1[3];

        if (out2_connected)
        {
            frame.p2_lp_out = out2[1];
            frame.p2_bp_out = out2[2];
            frame.p2_hp_out = out2[3];
        }
    }

protected:
    float sample_time_;
    int oversampling_;
    UpsamplingAAFilter<simd::float_4> up_filter_[3];
    DownsamplingAAFilter<simd::float_4> down_filter_[2];
    LPFilter<simd::float_4> low_high_;
    SVFilter<simd::float_4> mid_;
    dsp::TRCFilter<simd::float_4> freq_lpf_;
    dsp::TRCFilter<simd::float_4> q_lpf_;
    dsp::TRCFilter<float> clip_hpf_;
    dsp::SlewLimiter clip_slew_;

    template <typename T>
    T FreqVCALevel(T v_oct)
    {
        v_oct = simd::clamp(v_oct, kMinVOct, 0.f);
        return simd::pow(2.f, v_oct);
    }

    template <typename T>
    T QVCALevel(T q_cv)
    {
        q_cv = simd::clamp(q_cv, 0.f, kClampVoltage);
        return simd::pow(10.f, q_cv / kVCAGainConstant / 20.f);
    }

    template <typename T>
    T GainVCALevel(T gain_db)
    {
        gain_db = simd::fmin(gain_db, kMaximumGain);
        return simd::pow(10.f, gain_db / 20.f);
    }
};

}
