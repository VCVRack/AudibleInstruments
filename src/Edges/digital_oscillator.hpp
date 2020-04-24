
#include "wavetables.hpp"
#include <algorithm>

#ifndef DIGITAL_OSCILLATOR
#define DIGITAL_OSCILLATOR

// ---------------------------------------------------------------------------
// MARK: 24-bit floating point
// ---------------------------------------------------------------------------

/// A 24-bit floating point number
struct uint24_t {
  /// the integral piece of the number
  uint16_t integral;
  /// the fractional piece of the number
  uint8_t fractional;
};

/// Return the sum of two uint24_t values.
///
/// @param left the left operand
/// @param right the right operand
/// @returns the sum of the left and right operands
///
static inline uint24_t operator+(uint24_t left, uint24_t right) {
  uint24_t result;
  uint32_t leftv = (static_cast<uint32_t>(left.integral) << 8) + left.fractional;
  uint32_t rightv = (static_cast<uint32_t>(right.integral) << 8) + right.fractional;
  uint32_t sum = leftv + rightv;
  result.integral = sum >> 8;
  result.fractional = sum & 0xff;
  return result;
}

/// Perform a right shift operation in place.
///
/// @param value a reference to the uint24_t to shift right in place
/// @returns a reference to value
///
static inline uint24_t& operator>>=(uint24_t& value, uint8_t num_shifts) {
  while (num_shifts--) {
    uint32_t av = static_cast<uint32_t>(value.integral) << 8;
    av += value.fractional;
    av >>= 1;
    value.integral = av >> 8;
    value.fractional = av & 0xff;
  }
  return value;
}

// ---------------------------------------------------------------------------
// MARK: Interpolation
// ---------------------------------------------------------------------------

/// Mix two 8-bit values.
///
/// @param a the first value to mix
/// @param b the second value to mix
/// @param balance the mix between a (0) and b (255)
///
static inline uint8_t mix(uint16_t a, uint16_t b, uint8_t balance) {
  return (a * (255 - balance) + b * balance) >> 8;
}

/// Interpolate between a sample from a wave table.
///
/// @param table a pointer to the table to lookup samples in
/// @param phase the current phase in the wave table
///
static inline uint8_t interpolate(const uint8_t* table, uint16_t phase) {
  auto index = phase >> 7;
  return mix(table[index], table[index + 1], phase & 0xff);
}

/// Interpolate between two wave tables.
///
/// @param table_a the first wave table
/// @param table_b the second wave table
/// @param phase the phase in the wave table
/// @param gain_a the gain for the first wave
/// @param gain_b the gain for the second wave
/// @returns a value interpolated between the two wave tables at the given phase
///
static inline uint16_t interpolate(
  const uint8_t* table_a,
  const uint8_t* table_b,
  uint16_t phase,
  uint8_t gain_a,
  uint8_t gain_b
) {
  uint16_t result = 0;
  result += interpolate(table_a, phase) * gain_a;
  result += interpolate(table_b, phase) * gain_b;
  return result;
}

// ---------------------------------------------------------------------------
// MARK: Digital Oscillator
// ---------------------------------------------------------------------------

/// The wave shapes for the digital oscillator
enum OscillatorShape {
  OSC_SINE = 0,
  OSC_TRIANGLE,
  OSC_NES_TRIANGLE,
  OSC_PITCHED_NOISE,
  OSC_NES_NOISE_LONG,
  OSC_NES_NOISE_SHORT,
  NUM_DIGITAL_OSC  // the total number of oscillators in the enumeration
};

/// TODO: document
static const uint8_t kMaxZone = 7;
/// TODO: document
static const int16_t kOctave = 12 * 128;
/// TODO: document
static const int16_t kPitchTableStart = 116 * 128;

/// the native sample rate of the digital oscillator
static const float SAMPLE_RATE = 48000.f;

/// A digital oscillator with triangle, NES triangle, NES noise, hold & sample
/// noise, and sine wave shapes.
template<typename T>
class DigitalOscillator {
 public:
    /// whether note quantization is enabled
    bool isQuantized = false;
    /// Initialize a new digital oscillator.
    DigitalOscillator() { intialize(); }

    /// Destroy an existing digital oscillator.
    ~DigitalOscillator() { }

    /// Initialize the digital oscillator
    void intialize() {
        // set the pitch to the default value
        setPitch(60 << 7);
        // reset the wave shape to the first value
        shape_ = OSC_SINE;
        // set the gate to true (i.e., open)
        setGate(true);
        // reset the random number seed to 1
        rng_state_ = 1;
    }

    /// Set the wave shape to a new value.
    ///
    /// @param shape_ the wave shape to select
    ///
    inline void setShape(OscillatorShape shape) { shape_ = shape; }

    /// Select the next wave shape.
    inline void nextShape() {
        auto next_shape = (static_cast<int>(shape_) + 1) % NUM_DIGITAL_OSC;
        shape_ = static_cast<OscillatorShape>(next_shape);
    }

    /// Set the pitch to a new value.
    ///
    /// @param pitch_ the pitch of the oscillator
    ///
    inline void setPitch(int16_t pitch) {
        if (isQuantized) pitch = pitch & 0b1111111110000000;
        pitch_ = pitch;
    }

    /// Set the gate to a new value.
    ///
    /// @param gate_ the gate of the oscillator
    ///
    inline void setGate(bool gate) { gate_ = gate; }

    /// Set the CV Pulse Width to a new value.
    ///
    /// @param cv_pw_ the CV parameter for the pulse width
    ///
    inline void set_cv_pw(uint8_t cv_pw) { cv_pw_ = cv_pw; }

    /// Render a sample from the oscillator using current parameters.
    void process(int sampleRate) {
        if (gate_) {
            computePhaseIncrement(sampleRate);
            (this->*getRenderFunction(shape_))();
        } else {
            renderSilence();
        }
    }

    /// Get the value from the oscillator in the range [-1.0, 1.0]
    inline T getValue() const {
        // divide the 12-bit value by 4096.0 to normalize in [0.0, 1.0]
        // multiply by 2 and subtract 1 to get the value in [-1.0, 1.0]
        return 2 * (value / 4096.0) - 1;
    }

 private:
    /// a type for calling and storing render functions for the wave shapes
    typedef void (DigitalOscillator::*RenderFunction)();

    /// Get the render function for the given shape.
    ///
    /// @param shape the wave shape to return the render function for
    /// @returns the render function for the given wave shape
    ///
    static inline RenderFunction getRenderFunction(OscillatorShape shape) {
        // create a static constant array of the functions
        static const RenderFunction function_table[] = {
            &DigitalOscillator::renderSine,
            &DigitalOscillator::renderBandlimitedTriangle,
            &DigitalOscillator::renderBandlimitedTriangle,
            &DigitalOscillator::renderNoise,
            &DigitalOscillator::renderNoiseNES,
            &DigitalOscillator::renderNoiseNES
        };
        // lookup the wave using the shape enumeration
        return function_table[shape];
    }

    /// the current shape of the wave produced by the oscillator
    OscillatorShape shape_ = OSC_TRIANGLE;
    /// the current pitch of the oscillator
    int16_t pitch_ = 60 << 7;
    /// the 12TET quantized note based on the oscillator pitch
    uint8_t note_ = 0;
    /// whether the gate is open
    bool gate_ = true;

    /// the phase the oscillator is currently at
    uint24_t phase_;
    /// the change inn phase at the current processing step
    uint24_t phase_increment_;
    /// The random number generator state for generating random noise
    uint16_t rng_state_ = 1;
    /// A sample from the sine wave to use for the random noise generators
    uint16_t sample_ = 0;
    /// The auxiliary phase for the sine wave bit crusher
    uint16_t aux_phase_ = 0;
    /// The CV PW value for the sine wave bit crusher
    uint8_t cv_pw_ = 0;

    /// The output value from the oscillator
    uint16_t value = 0;

    /// Compute the increment in phased for the current step.
    ///
    /// @param sampleRate the sample rate to output audio at
    ///
    void computePhaseIncrement(int sampleRate) {
        int16_t ref_pitch = pitch_ - kPitchTableStart;
        uint8_t num_shifts = shape_ >= OSC_PITCHED_NOISE ? 0 : 1;
        while (ref_pitch < 0) {
            ref_pitch += kOctave;
            ++num_shifts;
        }

        uint24_t increment;
        uint16_t pitch_lookup_index_integral = ref_pitch >> 4;
        uint8_t pitch_lookup_index_fractional = ref_pitch << 4;

        uint16_t increment16 = lut_res_oscillator_increments[pitch_lookup_index_integral];
        uint16_t increment16_next = lut_res_oscillator_increments[pitch_lookup_index_integral + 1];
        // set the integral and fractional of the increment
        uint32_t increment16_diff = increment16_next - increment16;
        uint32_t pitch32 = pitch_lookup_index_fractional;
        uint16_t integral_diff = (increment16_diff * pitch32) >> 8;
        // set the integral based o the current sample rate
        increment.integral = (SAMPLE_RATE / sampleRate) * increment16 + integral_diff;
        increment.fractional = 0;
        increment >>= num_shifts;

        // shift the 15-bit pitch over 7 bits to produce a byte, 12 is the min value
        note_ = std::max(pitch_ >> 7, 12);
        phase_increment_ = increment;
    }

    /// Run the sample loop with given callback for the loop body.
    ///
    /// @param render_fn a callback function that accepts the phase and phase
    ///                  increment as parameters
    ///
    template<typename Callable>
    inline void renderWrapper(Callable render_fn) {
        uint24_t phase;
        uint24_t phase_increment;
        phase_increment.integral = phase_increment_.integral;
        phase_increment.fractional = phase_increment_.fractional;
        phase.integral = phase_.integral;
        phase.fractional = phase_.fractional;
        render_fn(phase, phase_increment);
        phase_.integral = phase.integral;
        phase_.fractional = phase.fractional;
    }

    /// Render silence from the oscillator.
    inline void renderSilence() { value = 0; }

    /// Render a sine wave from the oscillator.
    void renderSine() {
        uint16_t aux_phase_increment = lut_res_bitcrusher_increments[cv_pw_];
        renderWrapper([&, this](uint24_t& phase, uint24_t& phase_increment) {
            phase = phase + phase_increment;
            aux_phase_ += aux_phase_increment;
            if (aux_phase_ < aux_phase_increment || !aux_phase_increment) {
                sample_ = interpolate(wav_res_bandlimited_triangle_6, phase.integral) << 8;
            }
            value = sample_ >> 4;
        });
    }

    /// Render a triangle wave from the oscillator.
    void renderBandlimitedTriangle() {
        uint8_t balance_index = ((note_ - 12) << 4) | ((note_ - 12) >> 4);
        uint8_t gain_2 = balance_index & 0xf0;
        uint8_t gain_1 = ~gain_2;

        uint8_t wave_index = balance_index & 0xf;
        uint8_t base_resource_id = (shape_ == OSC_NES_TRIANGLE)
            ? WAV_RES_BANDLIMITED_NES_TRIANGLE_0
            : WAV_RES_BANDLIMITED_TRIANGLE_0;

        const uint8_t* wave_1 = waveform_table[base_resource_id + wave_index];
        wave_index = std::min<uint8_t>(wave_index + 1, kMaxZone);
        const uint8_t* wave_2 = waveform_table[base_resource_id + wave_index];

        renderWrapper([&](uint24_t& phase, uint24_t& phase_increment) {
            phase = phase + phase_increment;
            uint16_t sample = interpolate(wave_1, wave_2, phase.integral, gain_1, gain_2);
            value = sample >> 4;
        });
    }

    /// Render NES noise from the oscillator.
    void renderNoiseNES() {
        uint16_t rng_state = rng_state_;
        uint16_t sample = sample_;
        renderWrapper([&, this](uint24_t& phase, uint24_t& phase_increment) {
            phase = phase + phase_increment;
            if (phase.integral < phase_increment.integral) {
                uint8_t tap = rng_state >> 1;
                if (shape_ == OSC_NES_NOISE_SHORT) {
                    tap >>= 5;
                }
                uint8_t random_bit = (rng_state ^ tap) & 1;
                rng_state >>= 1;
                if (random_bit) {
                    rng_state |= 0x4000;
                    sample = 0x0300;
                } else {
                    sample = 0x0cff;
                }
            }
            value = sample;
        });
        rng_state_ = rng_state;
        sample_ = sample;
    }

    /// Render sample and hold noise from the oscillator.
    void renderNoise() {
        uint16_t rng_state = rng_state_;
        uint16_t sample = sample_;
        renderWrapper([&](uint24_t& phase, uint24_t& phase_increment) {
            phase = phase + phase_increment;
            if (phase.integral < phase_increment.integral) {
                rng_state = (rng_state >> 1) ^ (-(rng_state & 1) & 0xb400);
                sample = rng_state & 0x0fff;
                sample = 512 + ((sample * 3) >> 2);
            }
            value = sample;
        });
        rng_state_ = rng_state;
        sample_ = sample;
    }
};

#endif  // DIGITAL_OSCILLATOR
