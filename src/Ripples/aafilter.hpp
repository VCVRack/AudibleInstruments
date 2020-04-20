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

namespace ripples
{

template <typename T>
class AAFilter
{
public:
    void Init(float sample_rate)
    {
        InitFilter(sample_rate);
    }

    T ProcessUp(T in)
    {
        return up_filter_.Process(in);
    }

    T ProcessDown(T in)
    {
        return down_filter_.Process(in);
    }

    int GetOversamplingFactor(void)
    {
        return oversampling_factor_;
    }

protected:
    struct CascadedSOS
    {
        float sample_rate;
        int oversampling_factor;
        int num_sections;
        const SOSCoefficients* coeffs;
    };

    /*[[[cog
    from scipy import signal
    import math

    min_oversampled_rate = 20000 * 6

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

    fp = 20000 # passband corner in Hz
    rp = 0.1 # passband ripple in dB
    rs = 100 # stopband attenuation in dB

    array_name = 'kFilter'
    cascades = []
    max_num_sections = 0

    for fs in common_rates:
        factor = math.ceil(min_oversampled_rate / fs)
        wp = fp / fs
        ws = 0.5

        n, wc = signal.ellipord(wp*2/factor, ws*2/factor, rp, rs)

        # We are using second-order sections, so if the filter order would have
        # been odd, we can bump it up by 1 for 'free'
        n = 2 * int(math.ceil(n / 2))

        # Non-oversampled sampling rates result in 0-order filters, since there
        # is no spectral content above fs/2. Bump these up to order 2 so we
        # get some rolloff.
        n = max(2, n)
        z, p, k = signal.ellip(n, rp, rs, wc, output='zpk')

        if n % 2 == 0:
            # DC gain is -rp for even-order filters, so amplify by rp
            k *= math.pow(10, rp / 20)
        sos = signal.zpk2sos(z, p, k)
        max_num_sections = max(max_num_sections, len(sos))

        cascade = (fs, factor, n, wc, sos)
        cascades.append(cascade)

    cog.outl('static constexpr int kMaxNumSections = {};'
        .format(max_num_sections))
    ]]]*/
    static constexpr int kMaxNumSections = 7;
    //[[[end]]]

    SOSFilter<T, kMaxNumSections> up_filter_;
    SOSFilter<T, kMaxNumSections> down_filter_;
    int oversampling_factor_;

    void InitFilter(float sample_rate)
    {
        if (false) {}
        /*[[[cog
        for cascade in reversed(cascades):
            (fs, factor, order, wc, sos) = cascade
            num_sections = len(sos)
            name = '{:s}{:d}x{:d}'.format(array_name, fs, factor)
            cost = fs * factor * num_sections

            cog.outl('else if ({} <= sample_rate)'.format(fs))
            cog.outl('{')
            cog.outl('    const SOSCoefficients {:s}[{:d}] ='
                ' // n = {:d}, wc = {:f}, cost = {:d}'
                .format(name, num_sections, order, wc, cost))
            cog.outl('    {')
            for sec in sos:
                b = ''.join(['{:.8e},'.format(c).ljust(17) for c in sec[:3]])
                a = ''.join(['{:.8e},'.format(c).ljust(17) for c in sec[4:]])
                cog.outl('        { {' + b + '}, {' + a + '} },')
            cog.outl('    };')
            cog.outl('    up_filter_.Init({}, {});'.format(num_sections, name))
            cog.outl('    down_filter_.Init({}, {});'.format(num_sections, name))
            cog.outl('    oversampling_factor_ = {};'.format(factor))
            cog.outl('}')
        cog.outl('else {{ InitFilter({}); }}'.format(*cascades[0]))
        ]]]*/
        else if (768000 <= sample_rate)
        {
            const SOSCoefficients kFilter768000x1[1] = // n = 2, wc = 0.052083, cost = 768000
            {
                { {1.83197956e-02,  3.66063440e-02,  1.83197956e-02,  }, {-1.60702602e+00, 6.80271956e-01,  } },
            };
            up_filter_.Init(1, kFilter768000x1);
            down_filter_.Init(1, kFilter768000x1);
            oversampling_factor_ = 1;
        }
        else if (705600 <= sample_rate)
        {
            const SOSCoefficients kFilter705600x1[1] = // n = 2, wc = 0.056689, cost = 705600
            {
                { {2.13438638e-02,  4.26550556e-02,  2.13438638e-02,  }, {-1.57253460e+00, 6.57877382e-01,  } },
            };
            up_filter_.Init(1, kFilter705600x1);
            down_filter_.Init(1, kFilter705600x1);
            oversampling_factor_ = 1;
        }
        else if (384000 <= sample_rate)
        {
            const SOSCoefficients kFilter384000x1[1] = // n = 2, wc = 0.104167, cost = 384000
            {
                { {6.09620331e-02,  1.21896769e-01,  6.09620331e-02,  }, {-1.22760212e+00, 4.71422957e-01,  } },
            };
            up_filter_.Init(1, kFilter384000x1);
            down_filter_.Init(1, kFilter384000x1);
            oversampling_factor_ = 1;
        }
        else if (352800 <= sample_rate)
        {
            const SOSCoefficients kFilter352800x1[1] = // n = 2, wc = 0.113379, cost = 352800
            {
                { {6.99874107e-02,  1.39948456e-01,  6.99874107e-02,  }, {-1.16347041e+00, 4.43393682e-01,  } },
            };
            up_filter_.Init(1, kFilter352800x1);
            down_filter_.Init(1, kFilter352800x1);
            oversampling_factor_ = 1;
        }
        else if (192000 <= sample_rate)
        {
            const SOSCoefficients kFilter192000x1[1] = // n = 2, wc = 0.208333, cost = 192000
            {
                { {1.74603587e-01,  3.49188678e-01,  1.74603587e-01,  }, {-5.65216145e-01, 2.63611998e-01,  } },
            };
            up_filter_.Init(1, kFilter192000x1);
            down_filter_.Init(1, kFilter192000x1);
            oversampling_factor_ = 1;
        }
        else if (176400 <= sample_rate)
        {
            const SOSCoefficients kFilter176400x1[1] = // n = 2, wc = 0.226757, cost = 176400
            {
                { {1.95938020e-01,  3.91858763e-01,  1.95938020e-01,  }, {-4.62313019e-01, 2.46047822e-01,  } },
            };
            up_filter_.Init(1, kFilter176400x1);
            down_filter_.Init(1, kFilter176400x1);
            oversampling_factor_ = 1;
        }
        else if (96000 <= sample_rate)
        {
            const SOSCoefficients kFilter96000x2[4] = // n = 8, wc = 0.208333, cost = 768000
            {
                { {1.61637850e-04,  2.48564833e-04,  1.61637850e-04,  }, {-1.55379599e+00, 6.19242969e-01,  } },
                { {1.00000000e+00,  -3.56106191e-03, 1.00000000e+00,  }, {-1.52397985e+00, 7.01779035e-01,  } },
                { {1.00000000e+00,  -7.04269454e-01, 1.00000000e+00,  }, {-1.49925562e+00, 8.20191196e-01,  } },
                { {1.00000000e+00,  -9.36222412e-01, 1.00000000e+00,  }, {-1.51854586e+00, 9.39911675e-01,  } },
            };
            up_filter_.Init(4, kFilter96000x2);
            down_filter_.Init(4, kFilter96000x2);
            oversampling_factor_ = 2;
        }
        else if (88200 <= sample_rate)
        {
            const SOSCoefficients kFilter88200x2[4] = // n = 8, wc = 0.226757, cost = 705600
            {
                { {2.14361684e-04,  3.44618768e-04,  2.14361684e-04,  }, {-1.51452462e+00, 5.91486912e-01,  } },
                { {1.00000000e+00,  1.79381294e-01,  1.00000000e+00,  }, {-1.47183116e+00, 6.80568376e-01,  } },
                { {1.00000000e+00,  -5.38705333e-01, 1.00000000e+00,  }, {-1.43146550e+00, 8.07687680e-01,  } },
                { {1.00000000e+00,  -7.87002288e-01, 1.00000000e+00,  }, {-1.44140131e+00, 9.35689662e-01,  } },
            };
            up_filter_.Init(4, kFilter88200x2);
            down_filter_.Init(4, kFilter88200x2);
            oversampling_factor_ = 2;
        }
        else if (48000 <= sample_rate)
        {
            const SOSCoefficients kFilter48000x3[6] = // n = 12, wc = 0.277778, cost = 864000
            {
                { {1.96007199e-04,  3.15285921e-04,  1.96007199e-04,  }, {-1.49750952e+00, 5.79487424e-01,  } },
                { {1.00000000e+00,  1.64502383e-01,  1.00000000e+00,  }, {-1.43900370e+00, 6.63196513e-01,  } },
                { {1.00000000e+00,  -5.92180251e-01, 1.00000000e+00,  }, {-1.36241892e+00, 7.75058824e-01,  } },
                { {1.00000000e+00,  -9.07488127e-01, 1.00000000e+00,  }, {-1.30223398e+00, 8.69165582e-01,  } },
                { {1.00000000e+00,  -1.04177534e+00, 1.00000000e+00,  }, {-1.26951947e+00, 9.34679234e-01,  } },
                { {1.00000000e+00,  -1.09276235e+00, 1.00000000e+00,  }, {-1.26454687e+00, 9.80322986e-01,  } },
            };
            up_filter_.Init(6, kFilter48000x3);
            down_filter_.Init(6, kFilter48000x3);
            oversampling_factor_ = 3;
        }
        else if (44100 <= sample_rate)
        {
            const SOSCoefficients kFilter44100x3[7] = // n = 14, wc = 0.302343, cost = 926100
            {
                { {2.33467524e-04,  3.85146244e-04,  2.33467524e-04,  }, {-1.46779940e+00, 5.59300587e-01,  } },
                { {1.00000000e+00,  2.84344987e-01,  1.00000000e+00,  }, {-1.39743012e+00, 6.47280334e-01,  } },
                { {1.00000000e+00,  -4.81735913e-01, 1.00000000e+00,  }, {-1.30466696e+00, 7.63828718e-01,  } },
                { {1.00000000e+00,  -8.14458422e-01, 1.00000000e+00,  }, {-1.22921466e+00, 8.60153843e-01,  } },
                { {1.00000000e+00,  -9.63424410e-01, 1.00000000e+00,  }, {-1.18164620e+00, 9.24279595e-01,  } },
                { {1.00000000e+00,  -1.03102512e+00, 1.00000000e+00,  }, {-1.15782377e+00, 9.63657309e-01,  } },
                { {1.00000000e+00,  -1.05757483e+00, 1.00000000e+00,  }, {-1.15253824e+00, 9.89272846e-01,  } },
            };
            up_filter_.Init(7, kFilter44100x3);
            down_filter_.Init(7, kFilter44100x3);
            oversampling_factor_ = 3;
        }
        else if (24000 <= sample_rate)
        {
            const SOSCoefficients kFilter24000x5[4] = // n = 8, wc = 0.333333, cost = 480000
            {
                { {9.93374792e-04,  1.81504524e-03,  9.93374792e-04,  }, {-1.28123502e+00, 4.43830055e-01,  } },
                { {1.00000000e+00,  9.69736619e-01,  1.00000000e+00,  }, {-1.14056361e+00, 5.73274737e-01,  } },
                { {1.00000000e+00,  3.23593812e-01,  1.00000000e+00,  }, {-9.84074266e-01, 7.48267989e-01,  } },
                { {1.00000000e+00,  4.69137219e-02,  1.00000000e+00,  }, {-9.17508757e-01, 9.16260523e-01,  } },
            };
            up_filter_.Init(4, kFilter24000x5);
            down_filter_.Init(4, kFilter24000x5);
            oversampling_factor_ = 5;
        }
        else if (22050 <= sample_rate)
        {
            const SOSCoefficients kFilter22050x6[4] = // n = 8, wc = 0.302343, cost = 529200
            {
                { {6.47358611e-04,  1.15520581e-03,  6.47358611e-04,  }, {-1.35050917e+00, 4.84676642e-01,  } },
                { {1.00000000e+00,  7.82770646e-01,  1.00000000e+00,  }, {-1.24212580e+00, 6.01760550e-01,  } },
                { {1.00000000e+00,  9.46030879e-02,  1.00000000e+00,  }, {-1.12297856e+00, 7.63193697e-01,  } },
                { {1.00000000e+00,  -1.84341946e-01, 1.00000000e+00,  }, {-1.08165394e+00, 9.20980215e-01,  } },
            };
            up_filter_.Init(4, kFilter22050x6);
            down_filter_.Init(4, kFilter22050x6);
            oversampling_factor_ = 6;
        }
        else if (12000 <= sample_rate)
        {
            const SOSCoefficients kFilter12000x10[3] = // n = 6, wc = 0.333333, cost = 360000
            {
                { {3.42306291e-03,  6.53522273e-03,  3.42306291e-03,  }, {-1.13209947e+00, 3.65774415e-01,  } },
                { {1.00000000e+00,  1.42136933e+00,  1.00000000e+00,  }, {-9.55595652e-01, 5.55195466e-01,  } },
                { {1.00000000e+00,  1.05842861e+00,  1.00000000e+00,  }, {-8.35474882e-01, 8.34840828e-01,  } },
            };
            up_filter_.Init(3, kFilter12000x10);
            down_filter_.Init(3, kFilter12000x10);
            oversampling_factor_ = 10;
        }
        else if (11025 <= sample_rate)
        {
            const SOSCoefficients kFilter11025x11[3] = // n = 6, wc = 0.329829, cost = 363825
            {
                { {3.26702718e-03,  6.22983576e-03,  3.26702718e-03,  }, {-1.14130758e+00, 3.70354990e-01,  } },
                { {1.00000000e+00,  1.40863044e+00,  1.00000000e+00,  }, {-9.69538649e-01, 5.57917370e-01,  } },
                { {1.00000000e+00,  1.03994151e+00,  1.00000000e+00,  }, {-8.54328717e-01, 8.35728285e-01,  } },
            };
            up_filter_.Init(3, kFilter11025x11);
            down_filter_.Init(3, kFilter11025x11);
            oversampling_factor_ = 11;
        }
        else if (8000 <= sample_rate)
        {
            const SOSCoefficients kFilter8000x15[3] = // n = 6, wc = 0.333333, cost = 360000
            {
                { {3.42306291e-03,  6.53522273e-03,  3.42306291e-03,  }, {-1.13209947e+00, 3.65774415e-01,  } },
                { {1.00000000e+00,  1.42136933e+00,  1.00000000e+00,  }, {-9.55595652e-01, 5.55195466e-01,  } },
                { {1.00000000e+00,  1.05842861e+00,  1.00000000e+00,  }, {-8.35474882e-01, 8.34840828e-01,  } },
            };
            up_filter_.Init(3, kFilter8000x15);
            down_filter_.Init(3, kFilter8000x15);
            oversampling_factor_ = 15;
        }
        else { InitFilter(8000); }
        //[[[end]]]
    }
};

}
