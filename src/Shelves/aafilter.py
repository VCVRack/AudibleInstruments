# Anti-aliasing filter design
# Copyright (C) 2020 Tyler Coy
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

from scipy import signal
import math
import cog

# fs: base sampling rate in Hz
# os: oversampling factor
# fpass: passband corner in Hz
# fstop: stopband corner in Hz
# rpass: passband ripple in dB
# rstop: stopband attenuation in dB
def design(fs, os, fpass, fstop, rpass, rstop):
    wp = fpass / (fs * os)
    ws = min(0.5, fstop / (fs * os))

    n, wc = signal.ellipord(wp*2, ws*2, rpass, rstop)

    # We are using second-order sections, so if the filter order would have
    # been odd, we can bump it up by 1 for 'free'
    n = 2 * int(math.ceil(n / 2))

    # Non-oversampled sampling rates result in 0-order filters, since there
    # is no spectral content above fs/2. Bump these up to order 2 so we
    # get some rolloff.
    n = max(2, n)
    z, p, k = signal.ellip(n, rpass, rstop, wc, output='zpk')

    if n % 2 == 0:
        # DC gain is -rpass for even-order filters, so amplify by rpass
        k *= math.pow(10, rpass / 20)
    sos = signal.zpk2sos(z, p, k)

    cascade = FilterDescription((fs, os, n, wc/2, ws, sos))
    return cascade

class FilterDescription(tuple):
    def __init__(self, desc):
        (fs, os, n, wc, ws, sos) = desc
        self.sample_rate = fs
        self.oversampling = os
        self.order = n
        self.wpass = wc
        self.wstop = ws
        self.fpass = wc * fs * os
        self.fstop = ws * fs * os
        self.sections = sos

def print_filter_cases(filters):
    for f in filters:
        fs = f.sample_rate
        factor = f.oversampling
        num_sections = len(f.sections)
        name = 'kFilter{}x{}'.format(fs, factor)
        cost = fs * factor * num_sections

        cog.outl('case {}: // o = {}, fp = {}, fst = {}, cost = {}'
            .format(fs, factor, int(f.fpass), int(f.fstop), cost))
        cog.outl('{')

        cog.outl('    const SOSCoefficients {}[{}] ='
            .format(name, num_sections))
        cog.outl('    {')
        print_coeff = lambda c: '{:.8e},'.format(c).ljust(17)
        for s in f.sections:
            b = ''.join([print_coeff(c) for c in s[:3]])
            a = ''.join([print_coeff(c) for c in s[4:]])
            cog.outl('        { {' + b + '}, {' + a + '} },')
        cog.outl('    };')

        cog.outl('    AAFilter<T>::filter_.Init({}, {});'
            .format(num_sections, name))

        cog.outl('    break;')
        cog.outl('}')
