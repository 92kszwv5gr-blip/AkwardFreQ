#pragma once

#include <unordered_map>
#include "RegionTypes.h"
#include "../Params.h"

namespace afq
{
    // Per-LayerType score multiplier (default 1.0 if a type is absent from the
    // map) that nudges Tier B/C classification toward layer types more common in
    // a given genre. This only breaks close ties — LayerClassifier still requires
    // minClassifierConfidence to be met after biasing, so it can't invent a layer
    // that isn't supported by the actual audio features.
    std::unordered_map<LayerType, float> genreBias (params::GenrePreset genre);
}
