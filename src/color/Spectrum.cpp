#include "color/Spectrum.h"
#include "core/PlatformUtils.h"

#include <algorithm>
#include <cmath>

namespace fsvng {

// ----------------------------------------------------------------------------
// generate - Port of generate_spectrum_colors from color.c
//
// Fills the colors_ array with NUM_SHADES entries.  Each entry is computed
// by mapping i in [0, NUM_SHADES-1] to x in [0.0, 1.0] and calling
// spectrumColor().  Underflow/overflow colors are darkened copies of the
// first/last shade.
// ----------------------------------------------------------------------------
void Spectrum::generate(SpectrumType type, const RGBcolor& oldColor, const RGBcolor& newColor) {
    const RGBcolor* zeroPtr = (type == SPECTRUM_GRADIENT) ? &oldColor : nullptr;
    const RGBcolor* onePtr  = (type == SPECTRUM_GRADIENT) ? &newColor : nullptr;

    for (int i = 0; i < NUM_SHADES; ++i) {
        double x = static_cast<double>(i) / static_cast<double>(NUM_SHADES - 1);
        colors_[static_cast<size_t>(i)] = spectrumColor(type, x, zeroPtr, onePtr);
    }

    // Off-spectrum colors -- darken by 50%
    underflowColor_ = colors_[0];
    underflowColor_.r *= 0.5f;
    underflowColor_.g *= 0.5f;
    underflowColor_.b *= 0.5f;

    overflowColor_ = colors_[NUM_SHADES - 1];
    overflowColor_.r *= 0.5f;
    overflowColor_.g *= 0.5f;
    overflowColor_.b *= 0.5f;
}

// ----------------------------------------------------------------------------
// colorAt - return the precomputed color at position x in [0, 1]
// ----------------------------------------------------------------------------
const RGBcolor& Spectrum::colorAt(double x) const {
    x = std::clamp(x, 0.0, 1.0);
    int i = static_cast<int>(std::floor(x * static_cast<double>(NUM_SHADES - 1)));
    i = std::clamp(i, 0, NUM_SHADES - 1);
    return colors_[static_cast<size_t>(i)];
}

// ----------------------------------------------------------------------------
// spectrumColor - Port of color_spectrum_color from color.c
//
// Returns a color in the given type of spectrum at position x in [0, 1].
// For SPECTRUM_GRADIENT, zeroColor and oneColor must be non-null.
// ----------------------------------------------------------------------------
RGBcolor Spectrum::spectrumColor(SpectrumType type, double x,
                                  const RGBcolor* zeroColor,
                                  const RGBcolor* oneColor) {
    x = std::clamp(x, 0.0, 1.0);

    switch (type) {
        case SPECTRUM_RAINBOW:
            return PlatformUtils::rainbowColor(1.0 - x);

        case SPECTRUM_HEAT:
            return PlatformUtils::heatColor(x);

        case SPECTRUM_GRADIENT: {
            // Linear interpolation between the two boundary colors.
            RGBcolor fallbackZero{0.0f, 0.0f, 1.0f};
            RGBcolor fallbackOne{1.0f, 0.0f, 0.0f};
            const RGBcolor& c0 = zeroColor ? *zeroColor : fallbackZero;
            const RGBcolor& c1 = oneColor  ? *oneColor  : fallbackOne;
            RGBcolor color{};
            color.r = c0.r + static_cast<float>(x) * (c1.r - c0.r);
            color.g = c0.g + static_cast<float>(x) * (c1.g - c0.g);
            color.b = c0.b + static_cast<float>(x) * (c1.b - c0.b);
            return color;
        }

        default:
            break;
    }

    // Fallback (should not be reached).
    return RGBcolor{1.0f, 0.0f, 1.0f};
}

} // namespace fsvng
