SLUG = AudibleInstruments
VERSION = 0.6.3

FLAGS += \
	-DTEST \
	-I./eurorack \
	-Wno-unused-local-typedefs

SOURCES += $(wildcard src/*.cpp)

SOURCES += eurorack/stmlib/utils/random.cc
SOURCES += eurorack/stmlib/dsp/atan.cc
SOURCES += eurorack/stmlib/dsp/units.cc

SOURCES += eurorack/braids/macro_oscillator.cc
SOURCES += eurorack/braids/analog_oscillator.cc
SOURCES += eurorack/braids/digital_oscillator.cc
SOURCES += eurorack/braids/quantizer.cc
SOURCES += eurorack/braids/resources.cc

SOURCES += $(wildcard eurorack/plaits/dsp/*.cc)
SOURCES += $(wildcard eurorack/plaits/dsp/engine/*.cc)
SOURCES += $(wildcard eurorack/plaits/dsp/speech/*.cc)
SOURCES += $(wildcard eurorack/plaits/dsp/physical_modelling/*.cc)
SOURCES += eurorack/plaits/resources.cc

SOURCES += eurorack/clouds/dsp/correlator.cc
SOURCES += eurorack/clouds/dsp/granular_processor.cc
SOURCES += eurorack/clouds/dsp/mu_law.cc
SOURCES += eurorack/clouds/dsp/pvoc/frame_transformation.cc
SOURCES += eurorack/clouds/dsp/pvoc/phase_vocoder.cc
SOURCES += eurorack/clouds/dsp/pvoc/stft.cc
SOURCES += eurorack/clouds/resources.cc

SOURCES += eurorack/elements/dsp/exciter.cc
SOURCES += eurorack/elements/dsp/ominous_voice.cc
SOURCES += eurorack/elements/dsp/resonator.cc
SOURCES += eurorack/elements/dsp/tube.cc
SOURCES += eurorack/elements/dsp/multistage_envelope.cc
SOURCES += eurorack/elements/dsp/part.cc
SOURCES += eurorack/elements/dsp/string.cc
SOURCES += eurorack/elements/dsp/voice.cc
SOURCES += eurorack/elements/resources.cc

SOURCES += eurorack/rings/dsp/fm_voice.cc
SOURCES += eurorack/rings/dsp/part.cc
SOURCES += eurorack/rings/dsp/string_synth_part.cc
SOURCES += eurorack/rings/dsp/string.cc
SOURCES += eurorack/rings/dsp/resonator.cc
SOURCES += eurorack/rings/resources.cc

SOURCES += eurorack/tides/generator.cc
SOURCES += eurorack/tides/resources.cc

SOURCES += eurorack/warps/dsp/modulator.cc
SOURCES += eurorack/warps/dsp/oscillator.cc
SOURCES += eurorack/warps/dsp/vocoder.cc
SOURCES += eurorack/warps/dsp/filter_bank.cc
SOURCES += eurorack/warps/resources.cc

SOURCES += eurorack/frames/keyframer.cc
SOURCES += eurorack/frames/resources.cc
SOURCES += eurorack/frames/poly_lfo.cc

SOURCES += eurorack/peaks/processors.cc
SOURCES += eurorack/peaks/resources.cc
SOURCES += eurorack/peaks/drums/bass_drum.cc
SOURCES += eurorack/peaks/drums/fm_drum.cc
SOURCES += eurorack/peaks/drums/high_hat.cc
SOURCES += eurorack/peaks/drums/snare_drum.cc
SOURCES += eurorack/peaks/modulations/lfo.cc
SOURCES += eurorack/peaks/modulations/multistage_envelope.cc
SOURCES += eurorack/peaks/pulse_processor/pulse_shaper.cc
SOURCES += eurorack/peaks/pulse_processor/pulse_randomizer.cc
SOURCES += eurorack/peaks/number_station/number_station.cc

SOURCES += eurorack/stages/segment_generator.cc
SOURCES += eurorack/stages/ramp_extractor.cc
SOURCES += eurorack/stages/resources.cc

SOURCES += eurorack/stmlib/utils/random.cc
SOURCES += eurorack/stmlib/dsp/atan.cc
SOURCES += eurorack/stmlib/dsp/units.cc
SOURCES += eurorack/marbles/random/t_generator.cc
SOURCES += eurorack/marbles/random/x_y_generator.cc
SOURCES += eurorack/marbles/random/output_channel.cc
SOURCES += eurorack/marbles/random/lag_processor.cc
SOURCES += eurorack/marbles/random/quantizer.cc
SOURCES += eurorack/marbles/ramp/ramp_extractor.cc
SOURCES += eurorack/marbles/resources.cc


DISTRIBUTABLES += $(wildcard LICENSE*) res

RACK_DIR ?= ../..
include $(RACK_DIR)/plugin.mk
