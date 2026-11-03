#pragma once

// Select stmlib's portable implementations before disabling the firmware's
// TEST-only grain-stealing printf in the audio callback.
#include "stmlib/dsp/dsp.h"
#pragma push_macro("TEST")
#undef TEST
#include "beads/dsp/synth.h"
#pragma pop_macro("TEST")

static_assert(sizeof(beads::Item<beads::RESOLUTION_12_BIT, 2>) == 3,
    "12-bit stereo samples must pack to 3 bytes");
static_assert(sizeof(beads::Item<beads::RESOLUTION_12_BIT, 1>) == 3,
    "12-bit mono samples must pack to 3 bytes");
