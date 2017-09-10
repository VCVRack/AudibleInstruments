
FLAGS += \
	-fshort-enums -DTEST \
	-I./eurorack \
	-Wno-unused-local-typedefs


SOURCES = $(wildcard src/*.cpp) \
	eurorack/stmlib/utils/random.cc \
	eurorack/stmlib/dsp/atan.cc \
	eurorack/stmlib/dsp/units.cc \
	eurorack/braids/macro_oscillator.cc \
	eurorack/braids/analog_oscillator.cc \
	eurorack/braids/digital_oscillator.cc \
	eurorack/braids/quantizer.cc \
	eurorack/braids/resources.cc \
	eurorack/clouds/dsp/correlator.cc \
	eurorack/clouds/dsp/granular_processor.cc \
	eurorack/clouds/dsp/mu_law.cc \
	eurorack/clouds/dsp/pvoc/frame_transformation.cc \
	eurorack/clouds/dsp/pvoc/phase_vocoder.cc \
	eurorack/clouds/dsp/pvoc/stft.cc \
	eurorack/clouds/resources.cc \
	eurorack/elements/dsp/exciter.cc \
	eurorack/elements/dsp/ominous_voice.cc \
	eurorack/elements/dsp/resonator.cc \
	eurorack/elements/dsp/tube.cc \
	eurorack/elements/dsp/multistage_envelope.cc \
	eurorack/elements/dsp/part.cc \
	eurorack/elements/dsp/string.cc \
	eurorack/elements/dsp/voice.cc \
	eurorack/elements/resources.cc \
	eurorack/rings/dsp/fm_voice.cc \
	eurorack/rings/dsp/part.cc \
	eurorack/rings/dsp/string_synth_part.cc \
	eurorack/rings/dsp/string.cc \
	eurorack/rings/dsp/resonator.cc \
	eurorack/rings/resources.cc \
	eurorack/tides/generator.cc \
	eurorack/tides/resources.cc \
	eurorack/warps/dsp/modulator.cc \
	eurorack/warps/dsp/oscillator.cc \
	eurorack/warps/dsp/vocoder.cc \
	eurorack/warps/dsp/filter_bank.cc \
	eurorack/warps/resources.cc

include ../../plugin.mk


dist: all
ifndef VERSION
	$(error VERSION is not set.)
endif
	mkdir -p dist/AudibleInstruments
	cp LICENSE* dist/AudibleInstruments/
	cp plugin.* dist/AudibleInstruments/
	cp -R res dist/AudibleInstruments/
	cd dist && zip -5 -r AudibleInstruments-$(VERSION)-$(ARCH).zip AudibleInstruments
