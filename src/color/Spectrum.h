#pragma once

#include "core/Types.h"
#include <array>

namespace fsvng {

class Spectrum {
public:
    static constexpr int NUM_SHADES = 1024;

    // Generate spectrum colors for a given type
    void generate(SpectrumType type, const RGBcolor& oldColor = {}, const RGBcolor& newColor = {});

    // Get color at position x in [0,1]
    const RGBcolor& colorAt(double x) const;

    // Get underflow/overflow colors (for out-of-range values)
    const RGBcolor& underflowColor() const { return underflowColor_; }
    const RGBcolor& overflowColor() const { return overflowColor_; }

    // Get a spectrum color for any type (static utility)
    static RGBcolor spectrumColor(SpectrumType type, double x,
                                   const RGBcolor* zeroColor = nullptr,
                                   const RGBcolor* oneColor = nullptr);

private:
    std::array<RGBcolor, NUM_SHADES> colors_{};
    RGBcolor underflowColor_{};
    RGBcolor overflowColor_{};
};

} // namespace fsvng
