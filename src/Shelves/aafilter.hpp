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

namespace shelves
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
min_oversampled_rate = audio_bw * 2 * 3

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
    case 8000: return 15;
    case 11025: return 11;
    case 12000: return 10;
    case 22050: return 6;
    case 24000: return 5;
    case 44100: return 3;
    case 48000: return 3;
    case 88200: return 2;
    case 96000: return 2;
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
        case 8000: // o = 15, fp = 3800, fst = 38666, cost = 240000
        {
            const SOSCoefficients kFilter8000x15[2] =
            {
                { {1.44208376e-04,  2.15422675e-04,  1.44208376e-04,  }, {-1.75298317e+00, 7.75007227e-01,  } },
                { {1.00000000e+00,  1.72189731e-01,  1.00000000e+00,  }, {-1.85199502e+00, 9.01687724e-01,  } },
            };
            AAFilter<T>::filter_.Init(2, kFilter8000x15);
            break;
        }
        case 11025: // o = 11, fp = 5236, fst = 38587, cost = 242550
        {
            const SOSCoefficients kFilter11025x11[2] =
            {
                { {3.47236726e-04,  5.94611382e-04,  3.47236726e-04,  }, {-1.66651262e+00, 7.05884392e-01,  } },
                { {1.00000000e+00,  7.58730216e-01,  1.00000000e+00,  }, {-1.77900341e+00, 8.69327961e-01,  } },
            };
            AAFilter<T>::filter_.Init(2, kFilter11025x11);
            break;
        }
        case 12000: // o = 10, fp = 5699, fst = 38000, cost = 240000
        {
            const SOSCoefficients kFilter12000x10[2] =
            {
                { {4.63786610e-04,  8.16220909e-04,  4.63786610e-04,  }, {-1.63450649e+00, 6.81471340e-01,  } },
                { {1.00000000e+00,  9.17818354e-01,  1.00000000e+00,  }, {-1.74936370e+00, 8.57701633e-01,  } },
            };
            AAFilter<T>::filter_.Init(2, kFilter12000x10);
            break;
        }
        case 22050: // o = 6, fp = 10473, fst = 40425, cost = 396900
        {
            const SOSCoefficients kFilter22050x6[3] =
            {
                { {1.95909107e-04,  3.07811266e-04,  1.95909107e-04,  }, {-1.58181808e+00, 6.40141057e-01,  } },
                { {1.00000000e+00,  1.34444168e-01,  1.00000000e+00,  }, {-1.58691814e+00, 7.40684153e-01,  } },
                { {1.00000000e+00,  -4.56209108e-01, 1.00000000e+00,  }, {-1.64635749e+00, 9.03421507e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter22050x6);
            break;
        }
        case 24000: // o = 5, fp = 11399, fst = 36000, cost = 360000
        {
            const SOSCoefficients kFilter24000x5[3] =
            {
                { {3.60375579e-04,  6.11714197e-04,  3.60375579e-04,  }, {-1.50089044e+00, 5.82797128e-01,  } },
                { {1.00000000e+00,  5.06808919e-01,  1.00000000e+00,  }, {-1.48367876e+00, 6.99513376e-01,  } },
                { {1.00000000e+00,  -8.08861216e-02, 1.00000000e+00,  }, {-1.52492835e+00, 8.87536413e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter24000x5);
            break;
        }
        case 44100: // o = 3, fp = 20000, fst = 37433, cost = 529200
        {
            const SOSCoefficients kFilter44100x3[4] =
            {
                { {6.47358611e-04,  1.15520581e-03,  6.47358611e-04,  }, {-1.35050917e+00, 4.84676642e-01,  } },
                { {1.00000000e+00,  7.82770646e-01,  1.00000000e+00,  }, {-1.24212580e+00, 6.01760550e-01,  } },
                { {1.00000000e+00,  9.46030879e-02,  1.00000000e+00,  }, {-1.12297856e+00, 7.63193697e-01,  } },
                { {1.00000000e+00,  -1.84341946e-01, 1.00000000e+00,  }, {-1.08165394e+00, 9.20980215e-01,  } },
            };
            AAFilter<T>::filter_.Init(4, kFilter44100x3);
            break;
        }
        case 48000: // o = 3, fp = 20000, fst = 41333, cost = 576000
        {
            const SOSCoefficients kFilter48000x3[4] =
            {
                { {4.56315687e-04,  7.94441994e-04,  4.56315687e-04,  }, {-1.40446545e+00, 5.18222739e-01,  } },
                { {1.00000000e+00,  6.11274299e-01,  1.00000000e+00,  }, {-1.31956356e+00, 6.25927896e-01,  } },
                { {1.00000000e+00,  -1.00659178e-01, 1.00000000e+00,  }, {-1.22823335e+00, 7.76420985e-01,  } },
                { {1.00000000e+00,  -3.75767056e-01, 1.00000000e+00,  }, {-1.20548228e+00, 9.25277956e-01,  } },
            };
            AAFilter<T>::filter_.Init(4, kFilter48000x3);
            break;
        }
        case 88200: // o = 2, fp = 20000, fst = 52133, cost = 529200
        {
            const SOSCoefficients kFilter88200x2[3] =
            {
                { {6.91751141e-04,  1.23689749e-03,  6.91751141e-04,  }, {-1.40714871e+00, 5.20902227e-01,  } },
                { {1.00000000e+00,  8.42431018e-01,  1.00000000e+00,  }, {-1.35717505e+00, 6.56002263e-01,  } },
                { {1.00000000e+00,  2.97097489e-01,  1.00000000e+00,  }, {-1.36759134e+00, 8.70920336e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter88200x2);
            break;
        }
        case 96000: // o = 2, fp = 20000, fst = 57333, cost = 576000
        {
            const SOSCoefficients kFilter96000x2[3] =
            {
                { {5.02504803e-04,  8.78421990e-04,  5.02504803e-04,  }, {-1.45413648e+00, 5.51330003e-01,  } },
                { {1.00000000e+00,  6.85942380e-01,  1.00000000e+00,  }, {-1.42143582e+00, 6.77242054e-01,  } },
                { {1.00000000e+00,  1.15756990e-01,  1.00000000e+00,  }, {-1.44850505e+00, 8.78995879e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter96000x2);
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
        case 8000: // o = 15, fp = 3800, fst = 4000, cost = 960000
        {
            const SOSCoefficients kFilter8000x15[8] =
            {
                { {1.27849152e-05,  -1.15294016e-05, 1.27849152e-05,  }, {-1.89076082e+00, 8.94920241e-01,  } },
                { {1.00000000e+00,  -1.81550212e+00, 1.00000000e+00,  }, {-1.90419428e+00, 9.15590704e-01,  } },
                { {1.00000000e+00,  -1.91311657e+00, 1.00000000e+00,  }, {-1.92211660e+00, 9.43157527e-01,  } },
                { {1.00000000e+00,  -1.93984732e+00, 1.00000000e+00,  }, {-1.93701740e+00, 9.66048056e-01,  } },
                { {1.00000000e+00,  -1.95004731e+00, 1.00000000e+00,  }, {-1.94692651e+00, 9.81207030e-01,  } },
                { {1.00000000e+00,  -1.95451979e+00, 1.00000000e+00,  }, {-1.95288929e+00, 9.90199673e-01,  } },
                { {1.00000000e+00,  -1.95654696e+00, 1.00000000e+00,  }, {-1.95649904e+00, 9.95393001e-01,  } },
                { {1.00000000e+00,  -1.95734415e+00, 1.00000000e+00,  }, {-1.95907829e+00, 9.98656952e-01,  } },
            };
            AAFilter<T>::filter_.Init(8, kFilter8000x15);
            break;
        }
        case 11025: // o = 11, fp = 5236, fst = 5512, cost = 970200
        {
            const SOSCoefficients kFilter11025x11[8] =
            {
                { {1.59399541e-05,  -5.45523304e-06, 1.59399541e-05,  }, {-1.85152256e+00, 8.59147179e-01,  } },
                { {1.00000000e+00,  -1.66827517e+00, 1.00000000e+00,  }, {-1.86567107e+00, 8.86607422e-01,  } },
                { {1.00000000e+00,  -1.84052903e+00, 1.00000000e+00,  }, {-1.88464921e+00, 9.23416484e-01,  } },
                { {1.00000000e+00,  -1.88895850e+00, 1.00000000e+00,  }, {-1.90052671e+00, 9.54145238e-01,  } },
                { {1.00000000e+00,  -1.90758521e+00, 1.00000000e+00,  }, {-1.91115958e+00, 9.74577353e-01,  } },
                { {1.00000000e+00,  -1.91577845e+00, 1.00000000e+00,  }, {-1.91763851e+00, 9.86729328e-01,  } },
                { {1.00000000e+00,  -1.91949726e+00, 1.00000000e+00,  }, {-1.92169110e+00, 9.93757870e-01,  } },
                { {1.00000000e+00,  -1.92096059e+00, 1.00000000e+00,  }, {-1.92481123e+00, 9.98179459e-01,  } },
            };
            AAFilter<T>::filter_.Init(8, kFilter11025x11);
            break;
        }
        case 12000: // o = 10, fp = 5699, fst = 6000, cost = 960000
        {
            const SOSCoefficients kFilter12000x10[8] =
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
            AAFilter<T>::filter_.Init(8, kFilter12000x10);
            break;
        }
        case 22050: // o = 6, fp = 10473, fst = 11025, cost = 1058400
        {
            const SOSCoefficients kFilter22050x6[8] =
            {
                { {3.67003458e-05,  3.08516252e-05,  3.67003458e-05,  }, {-1.72921734e+00, 7.53994379e-01,  } },
                { {1.00000000e+00,  -1.04633213e+00, 1.00000000e+00,  }, {-1.73301180e+00, 8.01279004e-01,  } },
                { {1.00000000e+00,  -1.49728136e+00, 1.00000000e+00,  }, {-1.73817883e+00, 8.65169236e-01,  } },
                { {1.00000000e+00,  -1.64018498e+00, 1.00000000e+00,  }, {-1.74263646e+00, 9.18956353e-01,  } },
                { {1.00000000e+00,  -1.69729414e+00, 1.00000000e+00,  }, {-1.74585766e+00, 9.54949897e-01,  } },
                { {1.00000000e+00,  -1.72280865e+00, 1.00000000e+00,  }, {-1.74827060e+00, 9.76444779e-01,  } },
                { {1.00000000e+00,  -1.73447030e+00, 1.00000000e+00,  }, {-1.75063420e+00, 9.88907702e-01,  } },
                { {1.00000000e+00,  -1.73907302e+00, 1.00000000e+00,  }, {-1.75392950e+00, 9.96761482e-01,  } },
            };
            AAFilter<T>::filter_.Init(8, kFilter22050x6);
            break;
        }
        case 24000: // o = 5, fp = 11399, fst = 12000, cost = 960000
        {
            const SOSCoefficients kFilter24000x5[8] =
            {
                { {5.41421251e-05,  6.11551260e-05,  5.41421251e-05,  }, {-1.67503641e+00, 7.10371798e-01,  } },
                { {1.00000000e+00,  -7.40935436e-01, 1.00000000e+00,  }, {-1.66871015e+00, 7.66060345e-01,  } },
                { {1.00000000e+00,  -1.30326567e+00, 1.00000000e+00,  }, {-1.66021936e+00, 8.41290550e-01,  } },
                { {1.00000000e+00,  -1.49333046e+00, 1.00000000e+00,  }, {-1.65322192e+00, 9.04610823e-01,  } },
                { {1.00000000e+00,  -1.57100117e+00, 1.00000000e+00,  }, {-1.64887008e+00, 9.46976897e-01,  } },
                { {1.00000000e+00,  -1.60602637e+00, 1.00000000e+00,  }, {-1.64694927e+00, 9.72274830e-01,  } },
                { {1.00000000e+00,  -1.62210241e+00, 1.00000000e+00,  }, {-1.64717215e+00, 9.86942309e-01,  } },
                { {1.00000000e+00,  -1.62845914e+00, 1.00000000e+00,  }, {-1.64981608e+00, 9.96186562e-01,  } },
            };
            AAFilter<T>::filter_.Init(8, kFilter24000x5);
            break;
        }
        case 44100: // o = 3, fp = 20000, fst = 24100, cost = 793800
        {
            const SOSCoefficients kFilter44100x3[6] =
            {
                { {2.68627470e-04,  4.49235868e-04,  2.68627470e-04,  }, {-1.45093297e+00, 5.48077112e-01,  } },
                { {1.00000000e+00,  3.56445341e-01,  1.00000000e+00,  }, {-1.37442858e+00, 6.39226382e-01,  } },
                { {1.00000000e+00,  -4.09182122e-01, 1.00000000e+00,  }, {-1.27479281e+00, 7.60081618e-01,  } },
                { {1.00000000e+00,  -7.45642800e-01, 1.00000000e+00,  }, {-1.19642609e+00, 8.60924455e-01,  } },
                { {1.00000000e+00,  -8.92243997e-01, 1.00000000e+00,  }, {-1.15251661e+00, 9.30694207e-01,  } },
                { {1.00000000e+00,  -9.48436919e-01, 1.00000000e+00,  }, {-1.14204907e+00, 9.79130351e-01,  } },
            };
            AAFilter<T>::filter_.Init(6, kFilter44100x3);
            break;
        }
        case 48000: // o = 3, fp = 20000, fst = 28000, cost = 720000
        {
            const SOSCoefficients kFilter48000x3[5] =
            {
                { {2.57287527e-04,  4.26397322e-04,  2.57287527e-04,  }, {-1.46657488e+00, 5.58547936e-01,  } },
                { {1.00000000e+00,  3.12318565e-01,  1.00000000e+00,  }, {-1.39841450e+00, 6.48946069e-01,  } },
                { {1.00000000e+00,  -4.43959552e-01, 1.00000000e+00,  }, {-1.31299240e+00, 7.70865691e-01,  } },
                { {1.00000000e+00,  -7.61106497e-01, 1.00000000e+00,  }, {-1.25520703e+00, 8.77567308e-01,  } },
                { {1.00000000e+00,  -8.77468526e-01, 1.00000000e+00,  }, {-1.24463600e+00, 9.61716067e-01,  } },
            };
            AAFilter<T>::filter_.Init(5, kFilter48000x3);
            break;
        }
        case 88200: // o = 2, fp = 20000, fst = 68200, cost = 529200
        {
            const SOSCoefficients kFilter88200x2[3] =
            {
                { {6.91751141e-04,  1.23689749e-03,  6.91751141e-04,  }, {-1.40714871e+00, 5.20902227e-01,  } },
                { {1.00000000e+00,  8.42431018e-01,  1.00000000e+00,  }, {-1.35717505e+00, 6.56002263e-01,  } },
                { {1.00000000e+00,  2.97097489e-01,  1.00000000e+00,  }, {-1.36759134e+00, 8.70920336e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter88200x2);
            break;
        }
        case 96000: // o = 2, fp = 20000, fst = 76000, cost = 576000
        {
            const SOSCoefficients kFilter96000x2[3] =
            {
                { {5.02504803e-04,  8.78421990e-04,  5.02504803e-04,  }, {-1.45413648e+00, 5.51330003e-01,  } },
                { {1.00000000e+00,  6.85942380e-01,  1.00000000e+00,  }, {-1.42143582e+00, 6.77242054e-01,  } },
                { {1.00000000e+00,  1.15756990e-01,  1.00000000e+00,  }, {-1.44850505e+00, 8.78995879e-01,  } },
            };
            AAFilter<T>::filter_.Init(3, kFilter96000x2);
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
