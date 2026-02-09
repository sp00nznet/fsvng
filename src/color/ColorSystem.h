#pragma once

#include "core/Types.h"
#include "color/Spectrum.h"
#include <vector>
#include <string>

namespace fsvng {

class FsNode;

struct WPatternGroup {
    RGBcolor color;
    std::vector<std::string> patterns;
};

struct ColorConfig {
    // Color by node type
    struct {
        RGBcolor colors[NUM_NODE_TYPES];
    } byNodetype;

    // Color by timestamp
    struct {
        SpectrumType spectrumType = SPECTRUM_RAINBOW;
        TimeStampType timestampType = TIMESTAMP_MODIFY;
        time_t oldTime = 0;
        time_t newTime = 0;
        RGBcolor oldColor{0.0f, 0.0f, 1.0f};
        RGBcolor newColor{1.0f, 0.0f, 0.0f};
    } byTimestamp;

    // Color by wildcard pattern
    struct {
        std::vector<WPatternGroup> groups;
        RGBcolor defaultColor{1.0f, 1.0f, 0.625f};
    } byWpattern;
};

class ColorSystem {
public:
    static ColorSystem& instance();

    void init();

    ColorMode getMode() const { return mode_; }
    void setMode(ColorMode mode);

    const ColorConfig& getConfig() const { return config_; }
    void setConfig(const ColorConfig& config, ColorMode mode = COLOR_NONE);

    // Assign colors to all nodes in subtree
    void assignRecursive(FsNode* dnode);

    // Get color for spectrum visualization
    const RGBcolor& getSpectrumColor(double x) const;

    // Default colors
    static const RGBcolor defaultNodeTypeColors[NUM_NODE_TYPES];

private:
    ColorSystem() = default;

    const RGBcolor* nodeTypeColor(FsNode* node) const;
    const RGBcolor* timeColor(FsNode* node) const;
    const RGBcolor* wpatternColor(FsNode* node) const;

    void generateSpectrum();
    void loadDefaults();

    ColorMode mode_ = COLOR_BY_NODETYPE;
    ColorConfig config_;
    Spectrum spectrum_;
};

} // namespace fsvng
