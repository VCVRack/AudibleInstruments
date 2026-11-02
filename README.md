
# Audible Instruments

Based on [Mutable Instruments](https://mutable-instruments.net/) Eurorack modules.

## Modules

### Macro Oscillator
Based on [Braids](https://mutable-instruments.net/modules/braids), [Manual](https://mutable-instruments.net/modules/braids/manual/)
- Sync input doesn't work
- More settings could be supported

### Modal Synthesizer
Based on [Elements](https://mutable-instruments.net/modules/elements), [Manual](https://mutable-instruments.net/modules/elements/manual/)

### Tidal Modulator
Based on [Tides](https://mutable-instruments.net/modules/tides), [Manual](https://mutable-instruments.net/modules/tides/manual/)

Connecting CLOCK enables PLL operation; the frequency control selects a ratio with unity at its center. High/Low outputs indicate the ends of attack/release. Older patches retain their original behavior through the **Legacy clock handling** and **Legacy inverted High/Low gates** context-menu options.

### Random Sampler
Based on [Marbles](https://pichenettes.github.io/mutable-instruments-documentation/modules/marbles/manual/).

The context menu includes independent Y controls and **T gate length** / **T gate length randomness** for t1/t3; t2 stays at 50% duty. **Jitter/Steps CV inputs reset T/X** repurposes those two CV inputs for firmware-style resets while leaving the knobs active. Send reset before the next clock. Internally clocked X follows t reset. As on hardware, a t output patched into X CLOCK is recognized after several matching edges, and X then follows that output's internal clock. Older patches retain their original gate-length setting. Hold a button for 2 s for its long press, as on hardware: DEJA VU locks the loop (the LED pulses), T MODE selects the second bank of t models (the LED blinks), and X RANGE selects the scale. As on hardware, Y quantizes to octaves; **Y scale** can instead follow X's scale. Older patches load with **Legacy Y follows X range and knobs**, which reproduces earlier versions' Y output.

### Wavetable Oscillator
Based on [Sheep](https://mutable-instruments.net/modules/tides/firmware/) (Tides alternative firmware)

### Texture Synthesizer
Based on [Clouds](https://mutable-instruments.net/modules/clouds), [Manual](https://mutable-instruments.net/modules/clouds/manual/)
- edit buttons and lights
- freeze button
- right-click context menus to replace menu diving

The **Sample memory** context submenu saves and loads four audio-buffer slots, including quality and playback mode. Saved slots are embedded in patches and presets; loading a slot freezes playback. Save/load requests run at the next audio-processing block, so resume the engine if the menu reports a pending request. The live unsaved buffer is not automatically captured when saving a patch.

New instances detect rising edges at TRIGGER. Older patches retain **Legacy trigger-as-gate behavior**, which can be disabled in the context menu.

### Meta Modulator
Based on [Warps](https://mutable-instruments.net/modules/warps), [Manual](https://mutable-instruments.net/modules/warps/manual/)

The level knobs attenuate their CV inputs using the firmware's squared response and hardware input scaling. Unpatched inputs use the firmware's internal normalization. Internal oscillator pitch control remains additive. Older patches retain **Legacy additive level CV** in the context menu.

### Resonator
Based on [Rings](https://mutable-instruments.net/modules/rings), [Manual](https://mutable-instruments.net/modules/rings/manual/)

### Keyframer/Mixer
Based on [Frames](https://mutable-instruments.net/modules/frames), [Manual](https://mutable-instruments.net/modules/frames/manual/)

FRAME STEP emits 5 V, 1 ms pulses when crossing keyframes, with the firmware's rearming behavior. Select **Sequencer** in the context menu to advance through stored frames with pulses on FRAME; turn MODULATION fully clockwise for standard 5 V clocks.

### Segment Generator
Based on [Stages](https://pichenettes.github.io/mutable-instruments-documentation/modules/stages/manual/).

All modes receive the firmware's extended slider-plus-CV range, including negative voltages for slew/sample-and-hold and additional oscillator/envelope rate control. Outputs saturate at the default firmware DAC limits (approximately +/-8.125 V). Older patches retain **Legacy limited CV range** in the context menu.

### Multiples
Based on [Links](https://mutable-instruments.net/modules/links), [Manual](https://mutable-instruments.net/modules/links/manual/)

The 3:1 section averages with a fixed gain of 1/3. Older patches retain **Legacy 3:1 summing (no attenuation)**; disable it in the context menu for hardware behavior.

### Utilities
Based on [Kinks](https://mutable-instruments.net/modules/kinks), [Manual](https://mutable-instruments.net/modules/kinks/manual/)

### Mixer
Based on [Shades](https://mutable-instruments.net/modules/shades), [Manual](https://mutable-instruments.net/modules/shades/manual/)

### Bernoulli Gate
Based on [Branches](https://mutable-instruments.net/modules/branches), [Manual](https://mutable-instruments.net/modules/branches/manual/)

### Quad VC-polarizer
Based on [Blinds](https://mutable-instruments.net/modules/blinds), [Manual](https://mutable-instruments.net/modules/blinds/manual/)

### Quad VCA
Based on [Veils](https://mutable-instruments.net/modules/veils), [Manual](https://mutable-instruments.net/modules/veils/manual/)

### Liquid Filter
Based on [Ripples](https://mutable-instruments.net/modules/ripples), [Manual](https://mutable-instruments.net/modules/ripples/manual/)
- Virtual analog model provided by [Alright Devices](https://www.alrightdevices.com/) after successful crowdfunding.

### EQ Filter
Based on [Shelves](https://mutable-instruments.net/modules/shelves), [Manual](https://mutable-instruments.net/modules/shelves/manual/)
- Virtual analog model provided by [Alright Devices](https://www.alrightdevices.com/) after successful crowdfunding.


## Not yet ported

### [Peaks](https://mutable-instruments.net/modules/peaks)
[Manual](https://mutable-instruments.net/modules/peaks/manual/)

### [Streams](https://mutable-instruments.net/modules/streams)
[Manual](https://mutable-instruments.net/modules/streams/manual/)
- analog, but it's possible to fake the filter with schematic analysis, need to buy/borrow a unit

### [Yarns](https://mutable-instruments.net/modules/yarns)
[Manual](https://mutable-instruments.net/modules/yarns/manual/)
- functionality covered by the MIDI Interface from core, will not port

### [Ears](https://mutable-instruments.net/modules/ears)
[Manual](https://mutable-instruments.net/modules/ears/manual/)
- a microphone, will not port

### [Edges](https://mutable-instruments.net/modules/edges)
[Manual](https://mutable-instruments.net/modules/edges/manual/)

### [Grids](https://mutable-instruments.net/modules/grids)
[Manual](https://mutable-instruments.net/modules/grids/manual/)

