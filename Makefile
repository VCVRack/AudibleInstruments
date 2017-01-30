
ARCH ?= linux
CXXFLAGS = -MMD -fPIC -g -Wall -std=c++11 -O3 -msse -mfpmath=sse -ffast-math -DTEST \
	-I./src -I../../include -I./eurorack \
	-fshort-enums

LDFLAGS =

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


# Linux
ifeq ($(ARCH), linux)
CC = gcc
CXX = g++
CXXFLAGS += -Wno-unused-local-typedefs
LDFLAGS += -shared
TARGET = plugin.so
endif

# Apple
ifeq ($(ARCH), apple)
CC = clang
CXX = clang++
CXXFLAGS += -stdlib=libc++
LDFLAGS += -stdlib=libc++ -shared -undefined dynamic_lookup
TARGET = plugin.dylib
endif

# Windows
ifeq ($(ARCH), windows)
CC = x86_64-w64-mingw32-gcc
CXX = x86_64-w64-mingw32-g++
CXXFLAGS += -D_USE_MATH_DEFINES -Wno-unused-local-typedefs
SOURCES +=
LDFLAGS += -shared -L../../ -lRack
TARGET = plugin.dll
endif


all: $(TARGET)

dist: $(TARGET)
	mkdir -p dist/AudibleInstruments
	cp LICENSE* dist/AudibleInstruments/
	cp plugin.* dist/AudibleInstruments/
	cp -R res dist/AudibleInstruments/

clean:
	rm -rfv build $(TARGET) dist


include ../../Makefile.inc
