### 2.0.0 (in development)
- Add port labels.
- Rearrange context menus for clarity and consistency.
- Restore Meta Modulator level-CV attenuation, Keyframer/Mixer Frame Step pulses and sequencer mode, and the full Segment Generator CV range with firmware output limits. Older patches retain selectable legacy level-CV and CV-range behavior.
- Restore Multiples' 3:1 averaging, Tidal Modulator gate polarity and clock/PLL handling, and Texture Synthesizer rising-edge triggers. Older patches retain selectable legacy behavior.
- Add Random Sampler gate length/randomization and optional Jitter/Steps reset inputs.
- Add four Texture Synthesizer sample-memory slots in the context menu, embedded in patches and presets.
- Fix the Plaits slope oscillator's moving-breakpoint correction in the eurorack dependency, preventing extreme samples during fast PWM at low pitch.
- Random Sampler: show negative X/Y output voltages in red, default X range to ±5 V, quantize Y to octaves (Y scale can follow X instead), and recognize T outputs patched into X CLOCK, all as on hardware. Older patches keep their previous Y output through a legacy option.
- Fix the Marbles quantizer's out-of-bounds read at higher Y steps in the eurorack dependency.

### 1.5.0 (2020-11-07)
- Add Streams via fundraiser.

### 1.4.0 (2020-10-22)
- Add "Ominous voice" (easter egg) mode to Elements.
- Make Elements polyphonic.

### 1.3.1 (2020-07-14)
- Rewrite and re-panel Branches.
- Make Branches polyphonic.

### 1.3.0 (2020-05-29)
- Add EQ Filter via fundraiser.

### 1.2.1 (2020-04-23)
- Multiples
	- Make polyphonic.

### 1.2.0 (2020-04-20)
- Add Liquid Filter via fundraiser.

### 1.1.0 (2020-04-16)
- Macro Oscillator
	- Change range of frequency parameter to [-4, 4] octaves in order to match hardware.
	- Clean up parameter quantities.
- Macro Oscillator 2
	- Make polyphonic.
	- Use separate "invisible" parameters for LPG response and decay, so they can be MIDI-mapped and generally behave better.
	- Clean up parameter quantities.

### 1.0.2 (2020-02-29)
- Add ModularGrid URLs to manifest.

### 1.0.1 (2020-02-27)
- Fix tags and module descriptions.

### 1.0.0 (2019-06-13)
- Port to v1.

### 0.6.3 (2018-10-10)
- Add Segment Generator.

### 0.6.2 (2018-10-09)
- Add Random Sampler from Audible Instruments Preview.

### 0.6.1 (2018-09-12)
- Add Macro Oscillator 2 from Audible Instruments Preview.
