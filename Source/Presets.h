#pragma once
#include "EqDsp.h"
#include <vector>

namespace swelleq {

struct PresetBand
{
    BandType type;
    float freq, gain, q;
    int slope = 1;
};

struct FactoryPreset
{
    const char* category;
    const char* name;
    const char* hint;        // une phrase : à quoi il sert
    float output;            // dB
    std::vector<PresetBand> bands;
};

// Banque d'usine ; l'indice 0 est « Neutre » (tout à plat).
const std::vector<FactoryPreset>& factoryPresets();

} // namespace swelleq
