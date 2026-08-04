#include "GenreBias.h"

namespace afq
{
    std::unordered_map<LayerType, float> genreBias (params::GenrePreset genre)
    {
        using G = params::GenrePreset;
        switch (genre)
        {
            case G::GoaTrance:
                return { { LayerType::SynthLead, 1.15f }, { LayerType::Atmosphere, 1.10f } };

            case G::PsyTrance:
                return { { LayerType::Stabs, 1.10f }, { LayerType::Zap, 1.10f } };

            case G::ForestPsy:
                return { { LayerType::FX, 1.15f }, { LayerType::Glitch, 1.10f }, { LayerType::Percussion, 1.10f } };

            case G::ProgressivePsyTrance:
                return { { LayerType::Atmosphere, 1.15f }, { LayerType::SynthLead, 1.10f } };

            case G::MinimalPsyTrance:
                return { { LayerType::Percussion, 1.15f }, { LayerType::HiHat, 1.05f },
                         { LayerType::Atmosphere, 0.90f }, { LayerType::Stabs, 0.90f } };

            case G::Generic:
            default:
                return {};
        }
    }
}
