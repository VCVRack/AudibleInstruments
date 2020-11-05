// Anti-aliasing filters for common sample rates
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

#include "sos.hpp"

namespace streams
{

/*[[[cog
import math
import aafilter

# We design our filters to keep aliasing out of this band
audio_bw = 20000

# We assume the client process generates no frequency content above this
# multiple of the original bandwidth
max_bw_mult = 3

rpass = 0.1 # Maximum passband ripple in dB
rstop = 100 # Minimum stopband attenuation in dB

# Generate filters for these sampling rates
common_rates = [
    8000,
    11025, 12000,
    22050, 24000,
    44100, 48000,
    88200, 96000,
    176400, 192000,
    352800, 384000,
    705600, 768000
]

# Oversample to at least this frequency
min_oversampled_rate = audio_bw * 2 * 2

up_filters = list()
down_filters = list()
oversampling_factors = dict()
max_num_sections = 0

# For each sample rate, design a pair of upsampling and downsampling filters.
# For the upsampling filter, the stopband must be placed such that the client's
# multiplied bandwidth won't reach into the aliased audio band. For the
# downsampling filter, the stopband must be placed such that all foldover falls
# above the audio band.
for fs in common_rates:
    os = math.ceil(min_oversampled_rate / fs)
    oversampling_factors[fs] = os
    fpass = min(audio_bw, 0.475 * fs)
    critical_bw = fpass if fpass >= audio_bw else fs / 2
    up_fstop   = min(fs * os / 2, (fs * os - critical_bw) / max_bw_mult)
    down_fstop = min(fs * os / 2, fs - critical_bw)

    up   = aafilter.design(fs, os, fpass, up_fstop,   rpass, rstop)
    down = aafilter.design(fs, os, fpass, down_fstop, rpass, rstop)
    max_num_sections = max(max_num_sections, len(up.sections), len(down.sections))
    up_filters.append(up)
    down_filters.append(down)

cog.outl('static constexpr int kMaxNumSections = {};'
    .format(max_num_sections))
]]]*/
static constexpr int kMaxNumSections = 8;
//[[[end]]]

inline int SampleRateID(float sample_rate)
{
    if (false) {}
    /*[[[cog
    for fs in sorted(common_rates, reverse=True):
        cog.outl('else if ({} <= sample_rate) return {};'.format(fs, fs))
    cog.outl('else return {};'.format(min(common_rates)))
    ]]]*/
    else if (768000 <= sample_rate) return 768000;
    else if (705600 <= sample_rate) return 705600;
    else if (384000 <= sample_rate) return 384000;
    else if (352800 <= sample_rate) return 352800;
    else if (192000 <= sample_rate) return 192000;
    else if (176400 <= sample_rate) return 176400;
    else if (96000 <= sample_rate) return 96000;
    else if (88200 <= sample_rate) return 88200;
    else if (48000 <= sample_rate) return 48000;
    else if (44100 <= sample_rate) return 44100;
    else if (24000 <= sample_rate) return 24000;
    else if (22050 <= sample_rate) return 22050;
    else if (12000 <= sample_rate) return 12000;
    else if (11025 <= sample_rate) return 11025;
    else if (8000 <= sample_rate) return 8000;
    else return 8000;
    //[[[end]]]
}

inline int OversamplingFactor(float sample_rate)
{
    switch (SampleRateID(sample_rate))
    {
    default:
    /*[[[cog
    for fs in sorted(common_rates):
        cog.outl('case {}: return {};'.format(fs, oversampling_factors[fs]))
    ]]]*/
    case 8000: return 10;
    case 11025: return 8;
    case 12000: return 7;
    case 22050: return 4;
    case 24000: return 4;
    case 44100: return 2;
    case 48000: return 2;
    case 88200: return 1;
    case 96000: return 1;
    case 176400: return 1;
    case 192000: return 1;
    case 352800: return 1;
    case 384000: return 1;
    case 705600: return 1;
    case 768000: return 1;
    //[[[end]]]
    }
}

template <typename T>
class AAFilter
{
public:
    void Init(float sample_rate)
    {
        InitFilter(sample_rate);
    }

    T Process(T in)
    {
        return filter_.Process(in);
    }

protected:
    SOSFilter<T, kMaxNumSections> filter_;

    virtual void InitFilter(float sample_rate) = 0;
};

template <typename T>
class UpsamplingAAFilter : public AAFilter<T>
{
    void InitFilter(float sample_rate) override
    {
        switch (SampleRateID(sample_rate))
        {
        default:
        /*[[[cog
        aafilter.print_filter_cases(up_filters)
        ]]]*/
        case 8000: // o = 10, fp = 3799, fst = 25333, cost = 160000
        {
            const SOSCoefficients kFilter8000x10[2] =
            {
                { {4.63786610e-04,  8.16220909e-04,  4.63786610e-04,  }, {-1.63450649e+00, 6.81471340e-01,  } },
                { {1.00000000e+00,  9.17818354e-01,  1.00000000e+00,  }, {-1.74936370e+00, 8.57701633e-01,  } },
            };
            AAFilter<T>::filter_.Init(2, kFilter8000x10);
            break;
        }
        case 11025: // o = 8, fp = 5236, fst = 27562, cost = 264600
        {
            const SOSCoefficients kFilter11025x8[3] =
            {
                { {8.58405971e-05,  1.10355095e-04,  8.58405971e-05,  }, {-1.68369279e+00, 7.17693063e-01,  } },
                { {1.00000000e+00,  -4.51272752e-01, 1.00000000e+00,  }, {-1.70761645e+00, 7.97046177e-01,  } },
                { {1.00000000e+00,  -9.69385103e-01, 1.00000000e+00,  }, {-1.77709771e+00, 9.25148961e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter11025x8);
            break;
        }
        case 12000: // o = 7, fp = 5700, fst = 26000, cost = 252000
        {
            const SOSCoefficients kFilter12000x7[3] =
            {
                { {1.23289409e-04,  1.76631634e-04,  1.23289409e-04,  }, {-1.63990095e+00, 6.83607830e-01,  } },
                { {1.00000000e+00,  -1.84350251e-01, 1.00000000e+00,  }, {-1.65709238e+00, 7.72217183e-01,  } },
                { {1.00000000e+00,  -7.46080513e-01, 1.00000000e+00,  }, {-1.72410914e+00, 9.15596208e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter12000x7);
            break;
        }
        case 22050: // o = 4, fp = 10473, fst = 25725, cost = 264600
        {
            const SOSCoefficients kFilter22050x4[3] =
            {
                { {8.28104239e-04,  1.49680255e-03,  8.28104239e-04,  }, {-1.37972564e+00, 5.03689463e-01,  } },
                { {1.00000000e+00,  9.23962985e-01,  1.00000000e+00,  }, {-1.31894849e+00, 6.44142088e-01,  } },
                { {1.00000000e+00,  3.95355727e-01,  1.00000000e+00,  }, {-1.31864199e+00, 8.66452582e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter22050x4);
            break;
        }
        case 24000: // o = 4, fp = 11400, fst = 28000, cost = 288000
        {
            const SOSCoefficients kFilter24000x4[3] =
            {
                { {8.28104239e-04,  1.49680255e-03,  8.28104239e-04,  }, {-1.37972564e+00, 5.03689463e-01,  } },
                { {1.00000000e+00,  9.23962985e-01,  1.00000000e+00,  }, {-1.31894849e+00, 6.44142088e-01,  } },
                { {1.00000000e+00,  3.95355727e-01,  1.00000000e+00,  }, {-1.31864199e+00, 8.66452582e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter24000x4);
            break;
        }
        case 44100: // o = 2, fp = 20000, fst = 22733, cost = 529200
        {
            const SOSCoefficients kFilter44100x2[6] =
            {
                { {1.79111485e-03,  3.36261548e-03,  1.79111485e-03,  }, {-1.13743427e+00, 3.66260569e-01,  } },
                { {1.00000000e+00,  1.20719512e+00,  1.00000000e+00,  }, {-9.11565008e-01, 5.12543165e-01,  } },
                { {1.00000000e+00,  6.02914008e-01,  1.00000000e+00,  }, {-6.39374195e-01, 6.90602186e-01,  } },
                { {1.00000000e+00,  2.52534955e-01,  1.00000000e+00,  }, {-4.37984152e-01, 8.26937992e-01,  } },
                { {1.00000000e+00,  7.75467885e-02,  1.00000000e+00,  }, {-3.22061575e-01, 9.15513587e-01,  } },
                { {1.00000000e+00,  6.28451771e-03,  1.00000000e+00,  }, {-2.73474858e-01, 9.74748983e-01,  } },
            };
            AAFilter<T>::filter_.Init(6, kFilter44100x2);
            break;
        }
        case 48000: // o = 2, fp = 20000, fst = 25333, cost = 480000
        {
            const SOSCoefficients kFilter48000x2[5] =
            {
                { {1.56483717e-03,  2.92030174e-03,  1.56483717e-03,  }, {-1.17455774e+00, 3.85298764e-01,  } },
                { {1.00000000e+00,  1.15074177e+00,  1.00000000e+00,  }, {-9.70672689e-01, 5.26603999e-01,  } },
                { {1.00000000e+00,  5.31582897e-01,  1.00000000e+00,  }, {-7.26313285e-01, 7.02981269e-01,  } },
                { {1.00000000e+00,  1.94109007e-01,  1.00000000e+00,  }, {-5.54517652e-01, 8.45646275e-01,  } },
                { {1.00000000e+00,  5.47965468e-02,  1.00000000e+00,  }, {-4.79572665e-01, 9.52220684e-01,  } },
            };
            AAFilter<T>::filter_.Init(5, kFilter48000x2);
            break;
        }
        case 88200: // o = 1, fp = 20000, fst = 22733, cost = 529200
        {
            const SOSCoefficients kFilter88200x1[6] =
            {
                { {1.79111485e-03,  3.36261548e-03,  1.79111485e-03,  }, {-1.13743427e+00, 3.66260569e-01,  } },
                { {1.00000000e+00,  1.20719512e+00,  1.00000000e+00,  }, {-9.11565008e-01, 5.12543165e-01,  } },
                { {1.00000000e+00,  6.02914008e-01,  1.00000000e+00,  }, {-6.39374195e-01, 6.90602186e-01,  } },
                { {1.00000000e+00,  2.52534955e-01,  1.00000000e+00,  }, {-4.37984152e-01, 8.26937992e-01,  } },
                { {1.00000000e+00,  7.75467885e-02,  1.00000000e+00,  }, {-3.22061575e-01, 9.15513587e-01,  } },
                { {1.00000000e+00,  6.28451771e-03,  1.00000000e+00,  }, {-2.73474858e-01, 9.74748983e-01,  } },
            };
            AAFilter<T>::filter_.Init(6, kFilter88200x1);
            break;
        }
        case 96000: // o = 1, fp = 20000, fst = 25333, cost = 480000
        {
            const SOSCoefficients kFilter96000x1[5] =
            {
                { {1.56483717e-03,  2.92030174e-03,  1.56483717e-03,  }, {-1.17455774e+00, 3.85298764e-01,  } },
                { {1.00000000e+00,  1.15074177e+00,  1.00000000e+00,  }, {-9.70672689e-01, 5.26603999e-01,  } },
                { {1.00000000e+00,  5.31582897e-01,  1.00000000e+00,  }, {-7.26313285e-01, 7.02981269e-01,  } },
                { {1.00000000e+00,  1.94109007e-01,  1.00000000e+00,  }, {-5.54517652e-01, 8.45646275e-01,  } },
                { {1.00000000e+00,  5.47965468e-02,  1.00000000e+00,  }, {-4.79572665e-01, 9.52220684e-01,  } },
            };
            AAFilter<T>::filter_.Init(5, kFilter96000x1);
            break;
        }
        case 176400: // o = 1, fp = 20000, fst = 52133, cost = 529200
        {
            const SOSCoefficients kFilter176400x1[3] =
            {
                { {6.91751141e-04,  1.23689749e-03,  6.91751141e-04,  }, {-1.40714871e+00, 5.20902227e-01,  } },
                { {1.00000000e+00,  8.42431018e-01,  1.00000000e+00,  }, {-1.35717505e+00, 6.56002263e-01,  } },
                { {1.00000000e+00,  2.97097489e-01,  1.00000000e+00,  }, {-1.36759134e+00, 8.70920336e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter176400x1);
            break;
        }
        case 192000: // o = 1, fp = 20000, fst = 57333, cost = 576000
        {
            const SOSCoefficients kFilter192000x1[3] =
            {
                { {5.02504803e-04,  8.78421990e-04,  5.02504803e-04,  }, {-1.45413648e+00, 5.51330003e-01,  } },
                { {1.00000000e+00,  6.85942380e-01,  1.00000000e+00,  }, {-1.42143582e+00, 6.77242054e-01,  } },
                { {1.00000000e+00,  1.15756990e-01,  1.00000000e+00,  }, {-1.44850505e+00, 8.78995879e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter192000x1);
            break;
        }
        case 352800: // o = 1, fp = 20000, fst = 110933, cost = 1058400
        {
            const SOSCoefficients kFilter352800x1[3] =
            {
                { {7.63562466e-05,  9.37911276e-05,  7.63562466e-05,  }, {-1.69760825e+00, 7.28764991e-01,  } },
                { {1.00000000e+00,  -5.40096033e-01, 1.00000000e+00,  }, {-1.72321786e+00, 8.05120281e-01,  } },
                { {1.00000000e+00,  -1.04012920e+00, 1.00000000e+00,  }, {-1.79287839e+00, 9.28245030e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter352800x1);
            break;
        }
        case 384000: // o = 1, fp = 20000, fst = 121333, cost = 1152000
        {
            const SOSCoefficients kFilter384000x1[3] =
            {
                { {6.23104401e-05,  6.94740629e-05,  6.23104401e-05,  }, {-1.72153665e+00, 7.48079159e-01,  } },
                { {1.00000000e+00,  -6.96283878e-01, 1.00000000e+00,  }, {-1.74951535e+00, 8.19207305e-01,  } },
                { {1.00000000e+00,  -1.16050137e+00, 1.00000000e+00,  }, {-1.81879173e+00, 9.33631596e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter384000x1);
            break;
        }
        case 705600: // o = 1, fp = 20000, fst = 228533, cost = 1411200
        {
            const SOSCoefficients kFilter705600x1[2] =
            {
                { {1.08339911e-04,  1.50243615e-04,  1.08339911e-04,  }, {-1.77824462e+00, 7.96098482e-01,  } },
                { {1.00000000e+00,  -5.03405956e-02, 1.00000000e+00,  }, {-1.87131112e+00, 9.11379528e-01,  } },
            };
            AAFilter<T>::filter_.Init(2, kFilter705600x1);
            break;
        }
        case 768000: // o = 1, fp = 20000, fst = 249333, cost = 1536000
        {
            const SOSCoefficients kFilter768000x1[2] =
            {
                { {8.80491172e-05,  1.13851506e-04,  8.80491172e-05,  }, {-1.79584317e+00, 8.11038264e-01,  } },
                { {1.00000000e+00,  -2.19769620e-01, 1.00000000e+00,  }, {-1.88421935e+00, 9.18189356e-01,  } },
            };
            AAFilter<T>::filter_.Init(2, kFilter768000x1);
            break;
        }
        //[[[end]]]
        }
    }
};

template <typename T>
class DownsamplingAAFilter : public AAFilter<T>
{
    void InitFilter(float sample_rate) override
    {
        switch (SampleRateID(sample_rate))
        {
        default:
        /*[[[cog
        aafilter.print_filter_cases(down_filters)
        ]]]*/
        case 8000: // o = 10, fp = 3799, fst = 4000, cost = 640000
        {
            const SOSCoefficients kFilter8000x10[8] =
            {
                { {1.74724987e-05,  -2.65793181e-06, 1.74724987e-05,  }, {-1.83684224e+00, 8.46022748e-01,  } },
                { {1.00000000e+00,  -1.60455772e+00, 1.00000000e+00,  }, {-1.85073181e+00, 8.75957566e-01,  } },
                { {1.00000000e+00,  -1.80816772e+00, 1.00000000e+00,  }, {-1.86939499e+00, 9.16147406e-01,  } },
                { {1.00000000e+00,  -1.86608225e+00, 1.00000000e+00,  }, {-1.88504252e+00, 9.49754529e-01,  } },
                { {1.00000000e+00,  -1.88843627e+00, 1.00000000e+00,  }, {-1.89555097e+00, 9.72128817e-01,  } },
                { {1.00000000e+00,  -1.89828300e+00, 1.00000000e+00,  }, {-1.90199243e+00, 9.85446639e-01,  } },
                { {1.00000000e+00,  -1.90275515e+00, 1.00000000e+00,  }, {-1.90608719e+00, 9.93153182e-01,  } },
                { {1.00000000e+00,  -1.90451538e+00, 1.00000000e+00,  }, {-1.90935079e+00, 9.98002792e-01,  } },
            };
            AAFilter<T>::filter_.Init(8, kFilter8000x10);
            break;
        }
        case 11025: // o = 8, fp = 5236, fst = 5512, cost = 705600
        {
            const SOSCoefficients kFilter11025x8[8] =
            {
                { {2.28458309e-05,  6.85495861e-06,  2.28458309e-05,  }, {-1.79651426e+00, 8.10683491e-01,  } },
                { {1.00000000e+00,  -1.41042952e+00, 1.00000000e+00,  }, {-1.80827124e+00, 8.47262510e-01,  } },
                { {1.00000000e+00,  -1.70583736e+00, 1.00000000e+00,  }, {-1.82413424e+00, 8.96543400e-01,  } },
                { {1.00000000e+00,  -1.79296606e+00, 1.00000000e+00,  }, {-1.83751071e+00, 9.37903530e-01,  } },
                { {1.00000000e+00,  -1.82697868e+00, 1.00000000e+00,  }, {-1.84658043e+00, 9.65516086e-01,  } },
                { {1.00000000e+00,  -1.84202935e+00, 1.00000000e+00,  }, {-1.85227474e+00, 9.81981088e-01,  } },
                { {1.00000000e+00,  -1.84887890e+00, 1.00000000e+00,  }, {-1.85613708e+00, 9.91518889e-01,  } },
                { {1.00000000e+00,  -1.85157727e+00, 1.00000000e+00,  }, {-1.85962400e+00, 9.97525116e-01,  } },
            };
            AAFilter<T>::filter_.Init(8, kFilter11025x8);
            break;
        }
        case 12000: // o = 7, fp = 5700, fst = 6000, cost = 672000
        {
            const SOSCoefficients kFilter12000x7[8] =
            {
                { {2.79174308e-05,  1.56664250e-05,  2.79174308e-05,  }, {-1.76770492e+00, 7.86069996e-01,  } },
                { {1.00000000e+00,  -1.25883414e+00, 1.00000000e+00,  }, {-1.77670663e+00, 8.27280516e-01,  } },
                { {1.00000000e+00,  -1.62177587e+00, 1.00000000e+00,  }, {-1.78888474e+00, 8.82894847e-01,  } },
                { {1.00000000e+00,  -1.73200235e+00, 1.00000000e+00,  }, {-1.79920477e+00, 9.29653670e-01,  } },
                { {1.00000000e+00,  -1.77543744e+00, 1.00000000e+00,  }, {-1.80628315e+00, 9.60912839e-01,  } },
                { {1.00000000e+00,  -1.79473107e+00, 1.00000000e+00,  }, {-1.81087559e+00, 9.79568474e-01,  } },
                { {1.00000000e+00,  -1.80352658e+00, 1.00000000e+00,  }, {-1.81426259e+00, 9.90380916e-01,  } },
                { {1.00000000e+00,  -1.80699414e+00, 1.00000000e+00,  }, {-1.81775362e+00, 9.97192369e-01,  } },
            };
            AAFilter<T>::filter_.Init(8, kFilter12000x7);
            break;
        }
        case 22050: // o = 4, fp = 10473, fst = 11025, cost = 705600
        {
            const SOSCoefficients kFilter22050x4[8] =
            {
                { {9.74314780e-05,  1.37711747e-04,  9.74314780e-05,  }, {-1.59261637e+00, 6.47353056e-01,  } },
                { {1.00000000e+00,  -2.94219878e-01, 1.00000000e+00,  }, {-1.56519364e+00, 7.15705529e-01,  } },
                { {1.00000000e+00,  -9.81960881e-01, 1.00000000e+00,  }, {-1.52839642e+00, 8.07626243e-01,  } },
                { {1.00000000e+00,  -1.23949615e+00, 1.00000000e+00,  }, {-1.49778996e+00, 8.84625474e-01,  } },
                { {1.00000000e+00,  -1.34882542e+00, 1.00000000e+00,  }, {-1.47786684e+00, 9.35956597e-01,  } },
                { {1.00000000e+00,  -1.39893677e+00, 1.00000000e+00,  }, {-1.46698417e+00, 9.66536770e-01,  } },
                { {1.00000000e+00,  -1.42210887e+00, 1.00000000e+00,  }, {-1.46262788e+00, 9.84243409e-01,  } },
                { {1.00000000e+00,  -1.43130155e+00, 1.00000000e+00,  }, {-1.46352911e+00, 9.95397324e-01,  } },
            };
            AAFilter<T>::filter_.Init(8, kFilter22050x4);
            break;
        }
        case 24000: // o = 4, fp = 11400, fst = 12000, cost = 768000
        {
            const SOSCoefficients kFilter24000x4[8] =
            {
                { {9.74314780e-05,  1.37711747e-04,  9.74314780e-05,  }, {-1.59261637e+00, 6.47353056e-01,  } },
                { {1.00000000e+00,  -2.94219878e-01, 1.00000000e+00,  }, {-1.56519364e+00, 7.15705529e-01,  } },
                { {1.00000000e+00,  -9.81960881e-01, 1.00000000e+00,  }, {-1.52839642e+00, 8.07626243e-01,  } },
                { {1.00000000e+00,  -1.23949615e+00, 1.00000000e+00,  }, {-1.49778996e+00, 8.84625474e-01,  } },
                { {1.00000000e+00,  -1.34882542e+00, 1.00000000e+00,  }, {-1.47786684e+00, 9.35956597e-01,  } },
                { {1.00000000e+00,  -1.39893677e+00, 1.00000000e+00,  }, {-1.46698417e+00, 9.66536770e-01,  } },
                { {1.00000000e+00,  -1.42210887e+00, 1.00000000e+00,  }, {-1.46262788e+00, 9.84243409e-01,  } },
                { {1.00000000e+00,  -1.43130155e+00, 1.00000000e+00,  }, {-1.46352911e+00, 9.95397324e-01,  } },
            };
            AAFilter<T>::filter_.Init(8, kFilter24000x4);
            break;
        }
        case 44100: // o = 2, fp = 20000, fst = 24100, cost = 441000
        {
            const SOSCoefficients kFilter44100x2[5] =
            {
                { {2.47147477e-03,  4.68008071e-03,  2.47147477e-03,  }, {-1.08909166e+00, 3.42723010e-01,  } },
                { {1.00000000e+00,  1.29826448e+00,  1.00000000e+00,  }, {-8.40340328e-01, 5.00534399e-01,  } },
                { {1.00000000e+00,  7.43776254e-01,  1.00000000e+00,  }, {-5.49893538e-01, 6.91539899e-01,  } },
                { {1.00000000e+00,  4.24723977e-01,  1.00000000e+00,  }, {-3.48795082e-01, 8.41459476e-01,  } },
                { {1.00000000e+00,  2.89331378e-01,  1.00000000e+00,  }, {-2.57028674e-01, 9.51166241e-01,  } },
            };
            AAFilter<T>::filter_.Init(5, kFilter44100x2);
            break;
        }
        case 48000: // o = 2, fp = 20000, fst = 28000, cost = 480000
        {
            const SOSCoefficients kFilter48000x2[5] =
            {
                { {1.56483717e-03,  2.92030174e-03,  1.56483717e-03,  }, {-1.17455774e+00, 3.85298764e-01,  } },
                { {1.00000000e+00,  1.15074177e+00,  1.00000000e+00,  }, {-9.70672689e-01, 5.26603999e-01,  } },
                { {1.00000000e+00,  5.31582897e-01,  1.00000000e+00,  }, {-7.26313285e-01, 7.02981269e-01,  } },
                { {1.00000000e+00,  1.94109007e-01,  1.00000000e+00,  }, {-5.54517652e-01, 8.45646275e-01,  } },
                { {1.00000000e+00,  5.47965468e-02,  1.00000000e+00,  }, {-4.79572665e-01, 9.52220684e-01,  } },
            };
            AAFilter<T>::filter_.Init(5, kFilter48000x2);
            break;
        }
        case 88200: // o = 1, fp = 20000, fst = 44100, cost = 88200
        {
            const SOSCoefficients kFilter88200x1[1] =
            {
                { {4.47760494e-01,  8.95513661e-01,  4.47760494e-01,  }, {5.33267789e-01,  2.57766861e-01,  } },
            };
            AAFilter<T>::filter_.Init(1, kFilter88200x1);
            break;
        }
        case 96000: // o = 1, fp = 20000, fst = 48000, cost = 96000
        {
            const SOSCoefficients kFilter96000x1[1] =
            {
                { {4.08937060e-01,  8.17865642e-01,  4.08937060e-01,  }, {3.98731881e-01,  2.37007882e-01,  } },
            };
            AAFilter<T>::filter_.Init(1, kFilter96000x1);
            break;
        }
        case 176400: // o = 1, fp = 20000, fst = 88200, cost = 176400
        {
            const SOSCoefficients kFilter176400x1[1] =
            {
                { {1.95938020e-01,  3.91858763e-01,  1.95938020e-01,  }, {-4.62313019e-01, 2.46047822e-01,  } },
            };
            AAFilter<T>::filter_.Init(1, kFilter176400x1);
            break;
        }
        case 192000: // o = 1, fp = 20000, fst = 96000, cost = 192000
        {
            const SOSCoefficients kFilter192000x1[1] =
            {
                { {1.74603587e-01,  3.49188678e-01,  1.74603587e-01,  }, {-5.65216145e-01, 2.63611998e-01,  } },
            };
            AAFilter<T>::filter_.Init(1, kFilter192000x1);
            break;
        }
        case 352800: // o = 1, fp = 20000, fst = 176400, cost = 352800
        {
            const SOSCoefficients kFilter352800x1[1] =
            {
                { {6.99874107e-02,  1.39948456e-01,  6.99874107e-02,  }, {-1.16347041e+00, 4.43393682e-01,  } },
            };
            AAFilter<T>::filter_.Init(1, kFilter352800x1);
            break;
        }
        case 384000: // o = 1, fp = 20000, fst = 192000, cost = 384000
        {
            const SOSCoefficients kFilter384000x1[1] =
            {
                { {6.09620331e-02,  1.21896769e-01,  6.09620331e-02,  }, {-1.22760212e+00, 4.71422957e-01,  } },
            };
            AAFilter<T>::filter_.Init(1, kFilter384000x1);
            break;
        }
        case 705600: // o = 1, fp = 20000, fst = 352800, cost = 705600
        {
            const SOSCoefficients kFilter705600x1[1] =
            {
                { {2.13438638e-02,  4.26550556e-02,  2.13438638e-02,  }, {-1.57253460e+00, 6.57877382e-01,  } },
            };
            AAFilter<T>::filter_.Init(1, kFilter705600x1);
            break;
        }
        case 768000: // o = 1, fp = 20000, fst = 384000, cost = 768000
        {
            const SOSCoefficients kFilter768000x1[1] =
            {
                { {1.83197956e-02,  3.66063440e-02,  1.83197956e-02,  }, {-1.60702602e+00, 6.80271956e-01,  } },
            };
            AAFilter<T>::filter_.Init(1, kFilter768000x1);
            break;
        }
        //[[[end]]]
        }
    }
};

}
