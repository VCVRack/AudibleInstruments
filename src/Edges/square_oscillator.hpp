
#ifndef SQUARE_OSCILLATOR_HPP
#define SQUARE_OSCILLATOR_HPP

// ---------------------------------------------------------------------------
// MARK: Square Oscillator
// ---------------------------------------------------------------------------

/// An oscillator that generates a square wave
template <int OVERSAMPLE, int QUALITY, typename T>
struct SquareWaveOscillator {
    /// whether the oscillator is emulating an analog oscillator
    bool analog = false;
    /// TODO: document
    bool soft = false;
    /// whether the oscillator is synced to another oscillator
    bool syncEnabled = false;
    /// Whether ring modulation is enabled
    bool ringModulation = false;
    /// whether note quantization is enabled
    bool isQuantized = false;
    // For optimizing in serial code
    int channels = 0;

    /// the value from the last oscillator synchronization
    T lastSyncValue = 0.f;
    /// the current phase in [0, 1] (2 * pi * phase)
    T phase = 0.f;
    /// the current frequency
    T freq;
    /// the current pulse width in [0, 1]
    T pulseWidth = 0.5f;
    /// the direction of the synchronization
    T syncDirection = 1.f;

    /// a filter for producing an analog effect on the square wave
    dsp::TRCFilter<T> sqrFilter;

    /// the minimum-phase band-limited step generator for preventing aliasing
    dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> sqrMinBlep;

    /// The current value of the square wave
    T sqrValue = 0.f;

    /// Set the pitch of the oscillator to a new value.
    ///
    /// @param pitch the new pitch to set the oscillator to
    ///
    inline void setPitch(T pitch) {
        // quantized the pitch to semitone if enabled
        if (isQuantized) pitch = floor(pitch * 12) / 12.f;
        // set the frequency based on the pitch
        freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30) / 1073741824;
    }

    /// Set the pulse width of the square wave to a new value.
    ///
    /// @param pulseWidth the new pulse width to set the square wave to
    ///
    inline void setPulseWidth(T pulseWidth) {
        const float pwMin = 0.01f;
        this->pulseWidth = simd::clamp(pulseWidth, pwMin, 1.f - pwMin);
    }

    /// Process a sample for given change in time and sync value.
    ///
    /// @param deltaTime the change in time between samples
    /// @param syncValue the value of the oscillator to sync to
    /// @param modulator the value of the oscillator applying ring modulation
    ///
    void process(float deltaTime, T syncValue = 0, T modulator = 1) {
        // Advance phase
        T deltaPhase = simd::clamp(freq * deltaTime, 1e-6f, 0.35f);
        if (soft)  // Reverse direction
            deltaPhase *= syncDirection;
        else  // Reset back to forward
            syncDirection = 1.f;
        phase += deltaPhase;
        // Wrap phase
        phase -= simd::floor(phase);

        // Jump sqr when crossing 0, or 1 if backwards
        T wrapPhase = (syncDirection == -1.f) & 1.f;
        T wrapCrossing = (wrapPhase - (phase - deltaPhase)) / deltaPhase;
        int wrapMask = simd::movemask((0 < wrapCrossing) & (wrapCrossing <= 1.f));
        if (wrapMask) {
            for (int i = 0; i < channels; i++) {
                if (wrapMask & (1 << i)) {
                    T mask = simd::movemaskInverse<T>(1 << i);
                    float p = wrapCrossing[i] - 1.f;
                    T x = mask & (2.f * syncDirection);
                    sqrMinBlep.insertDiscontinuity(p, x);
                }
            }
        }

        // Jump sqr when crossing `pulseWidth`
        T pulseCrossing = (pulseWidth - (phase - deltaPhase)) / deltaPhase;
        int pulseMask = simd::movemask((0 < pulseCrossing) & (pulseCrossing <= 1.f));
        if (pulseMask) {
            for (int i = 0; i < channels; i++) {
                if (pulseMask & (1 << i)) {
                    T mask = simd::movemaskInverse<T>(1 << i);
                    float p = pulseCrossing[i] - 1.f;
                    T x = mask & (-2.f * syncDirection);
                    sqrMinBlep.insertDiscontinuity(p, x);
                }
            }
        }

        // Detect sync
        // Might be NAN or outside of [0, 1) range
        if (syncEnabled) {
            T deltaSync = syncValue - lastSyncValue;
            T syncCrossing = -lastSyncValue / deltaSync;
            lastSyncValue = syncValue;
            T sync = (0.f < syncCrossing) & (syncCrossing <= 1.f) & (syncValue >= 0.f);
            int syncMask = simd::movemask(sync);
            if (syncMask) {
                if (soft) {
                    syncDirection = simd::ifelse(sync, -syncDirection, syncDirection);
                }
                else {
                    T newPhase = simd::ifelse(sync, (1.f - syncCrossing) * deltaPhase, phase);
                    // Insert minBLEP for sync
                    for (int i = 0; i < channels; i++) {
                        if (syncMask & (1 << i)) {
                            T mask = simd::movemaskInverse<T>(1 << i);
                            float p = syncCrossing[i] - 1.f;
                            T x;
                            x = mask & (sqr(newPhase) - sqr(phase));
                            sqrMinBlep.insertDiscontinuity(p, x);
                        }
                    }
                    phase = newPhase;
                }
            }
        }

        // process the square wave value
        sqrValue = sqr(phase);
        sqrValue += sqrMinBlep.process();

        if (analog) {  // apply an analog filter
            sqrFilter.setCutoffFreq(20.f * deltaTime);
            sqrFilter.process(sqrValue);
            sqrValue = sqrFilter.highpass() * 0.95f;
        }

        if (ringModulation) {  // apply ring modulation
            sqrValue *= modulator;
        }
    }

    /// Calculate and return the value of the square wave for given phase.
    ///
    /// @param phase the phase of the wave in [0, 1]
    /// @returns the value of the wave in [0, 1]
    ///
    inline T sqr(T phase) { return simd::ifelse(phase < pulseWidth, 1.f, -1.f); }

    /// Return the value of the square wave.
    ///
    /// @returns the value of the wave in [0, 1]
    ///
    inline T sqr() const { return sqrValue; }
};

#endif  // SQUARE_OSCILLATOR_HPP
