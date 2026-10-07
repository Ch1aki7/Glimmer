#pragma once
#include "Glimmer/Asset/Asset.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace gl {
    // Persistent visual specification; never changes hydrology or terrain generation.
    struct WaterSurfaceAppearance {
        uint32_t Version = 1;
        bool Enabled = true;
        bool NormalDirectX = false;
        uint32_t MeshQuality = 1; // uniform 32 / 64 / 128 cells per chunk
        AssetHandle NormalTexture{0}, FoamTexture{0}; // zero selects deterministic built-ins
        float Absorption = 1.0f, RefractionPixels = 5.0f, SedimentTint = 0.4f;
        float NormalStrength = 0.22f, WaveLength = 12.0f, FlowStrength = 0.2f;
        float FoamStrength = 0.35f, ShoreFoam = 0.12f, ShoreWidth = 0.15f;
        float Roughness = 0.12f, ShoreWetness = 0.6f;
        bool operator==(const WaterSurfaceAppearance& r) const {
            return Version==r.Version && Enabled==r.Enabled && NormalDirectX==r.NormalDirectX
                && MeshQuality==r.MeshQuality && NormalTexture==r.NormalTexture && FoamTexture==r.FoamTexture
                && Absorption==r.Absorption && RefractionPixels==r.RefractionPixels && SedimentTint==r.SedimentTint
                && NormalStrength==r.NormalStrength && WaveLength==r.WaveLength && FlowStrength==r.FlowStrength
                && FoamStrength==r.FoamStrength && ShoreFoam==r.ShoreFoam && ShoreWidth==r.ShoreWidth && Roughness==r.Roughness && ShoreWetness==r.ShoreWetness;
        }
        bool operator!=(const WaterSurfaceAppearance& r) const { return !(*this==r); }
    };
    inline std::string ValidateWaterSurfaceAppearance(const WaterSurfaceAppearance& a) {
        if(a.Version!=1) return "Unsupported WaterSurface version.";
        if(a.MeshQuality>2) return "Water mesh quality must be 0, 1 or 2.";
        auto valid=[](float v,float lo,float hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
        if(!valid(a.Absorption,0,10)||!valid(a.RefractionPixels,0,32)||!valid(a.SedimentTint,0,4)
            ||!valid(a.NormalStrength,0,2)||!valid(a.WaveLength,0.25f,256)||!valid(a.FlowStrength,0,2)
            ||!valid(a.FoamStrength,0,1)||!valid(a.ShoreFoam,0,1)||!valid(a.ShoreWidth,0.005f,4)
            ||!valid(a.Roughness,0.04f,1)||!valid(a.ShoreWetness,0,1)) return "Water appearance has non-finite or out-of-range values.";
        return {};
    }
}
