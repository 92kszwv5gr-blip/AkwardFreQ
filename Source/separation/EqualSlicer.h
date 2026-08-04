#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include "DrumSlicer.h"

namespace afq
{
    // Divides a range into exactly N equal-length slices — a classic beat-
    // slicer/chopper, distinct from DrumSlicer's onset-detected hits. Useful
    // when you want mechanically even chops (e.g. a 4-bar loop into 16
    // sixteenth-note slices) rather than transient-following ones, or on
    // material with weak/no transients where onset detection has nothing to
    // grab onto.
    //
    // Reuses DrumSlice as the output type since downstream (DrumRackExporter)
    // doesn't care how the boundaries were chosen.
    class EqualSlicer
    {
    public:
        // `rangeEndSample <= rangeStartSample` slices the whole buffer.
        // `sliceCount` is clamped to [1, 256].
        static std::vector<DrumSlice> slice (int64_t bufferLengthSamples, int sliceCount,
                                              int64_t rangeStartSample = 0, int64_t rangeEndSample = -1);
    };
}
