// Copyright 2015 Emilie Gillet.
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
// VCV Rack: the parts of stmlib e3bd7c9 (pichenettes/stmlib) that Beads needs
// and that are missing from the stmlib pinned by eurorack/. Copied unchanged.
// Delete this file once eurorack/stmlib is updated.

#ifndef BEADS_STMLIB_COMPAT_H_
#define BEADS_STMLIB_COMPAT_H_

#include "stmlib/stmlib.h"

namespace stmlib {

// From stmlib/dsp/dsp.h
inline float SmoothStep(float value) {
  return value * value * (3.0f - 2.0f * value);
}

// From stmlib/dsp/hysteresis_quantizer.h
class HysteresisQuantizer2 {
 public:
  HysteresisQuantizer2() { }
  ~HysteresisQuantizer2() { }

  void Init(int num_steps, float hysteresis, bool symmetric) {
    num_steps_ = num_steps;
    hysteresis_ = hysteresis;

    scale_ = static_cast<float>(symmetric ? num_steps - 1 : num_steps);
    offset_ = symmetric ? 0.0f : -0.5f;

    quantized_value_ = 0;
  }

  inline int Process(float value) {
    return Process(0, value);
  }

  inline int Process(int base, float value) {
    value *= scale_;
    value += offset_;
    value += static_cast<float>(base);

    float hysteresis_sign = value > static_cast<float>(quantized_value_)
        ? -1.0f
        : +1.0f;
    int q = static_cast<int>(value + hysteresis_sign * hysteresis_ + 0.5f);
    CONSTRAIN(q, 0, num_steps_ - 1);
    quantized_value_ = q;
    return q;
  }

  template<typename T>
  const T& Lookup(const T* array, float value) {
    return array[Process(value)];
  }

  inline int num_steps() const {
    return num_steps_;
  }

  inline int quantized_value() const {
    return quantized_value_;
  }

 private:
  int num_steps_;
  float hysteresis_;

  float scale_;
  float offset_;

  int quantized_value_;

  DISALLOW_COPY_AND_ASSIGN(HysteresisQuantizer2);
};

}  // namespace stmlib

#endif  // BEADS_STMLIB_COMPAT_H_
