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
// Driver for the Mode and Range switches.

#pragma once

#include <algorithm>

#include "stmlib/stmlib.h"

namespace streams
{

const uint8_t kNumSwitches = 3;

class SwitchesEmulator
{
public:
    SwitchesEmulator() { }
    ~SwitchesEmulator() { }

    void Init()
    {
        std::fill(&switch_state_[0], &switch_state_[kNumSwitches], 0xff);
        std::fill(&pins_[0], &pins_[kNumSwitches], true);
    }

    void SetPin(uint8_t index, bool state)
    {
        pins_[index] = !state;
    }

    void Debounce()
    {
        for (uint8_t i = 0; i < 3; ++i)
        {
            switch_state_[i] = (switch_state_[i] << 1) | pins_[i];
        }
    }

    inline bool released(uint8_t index) const
    {
        return switch_state_[index] == 0x7f;
    }

    inline bool just_pressed(uint8_t index) const
    {
        return switch_state_[index] == 0x80;
    }

    inline bool pressed(uint8_t index) const
    {
        return switch_state_[index] == 0x00;
    }

private:
    uint8_t switch_state_[kNumSwitches];
    bool pins_[kNumSwitches];
};

}  // namespace streams
