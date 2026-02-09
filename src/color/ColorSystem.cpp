#include "color/ColorSystem.h"
#include "core/FsNode.h"
#include "core/FsTree.h"
#include "core/PlatformUtils.h"

#include <algorithm>
#include <cmath>
#include <ctime>

namespace fsvng {

// ----------------------------------------------------------------------------
// Default node type colors (from original color.c)
// ----------------------------------------------------------------------------
const RGBcolor ColorSystem::defaultNodeTypeColors[NUM_NODE_TYPES] = {
    {0.0f,    0.0f,    0.0f   },   // NODE_METANODE  (not used)
    {0.627f,  0.627f,  0.627f },   // NODE_DIRECTORY  #A0A0A0
    {1.0f,    1.0f,    0.627f },   // NODE_REGFILE    #FFFFA0
    {1.0f,    1.0f,    1.0f   },   // NODE_SYMLINK    #FFFFFF
    {0.0f,    1.0f,    0.0f   },   // NODE_FIFO       #00FF00
    {1.0f,    0.502f,  0.0f   },   // NODE_SOCKET     #FF8000
    {0.0f,    1.0f,    1.0f   },   // NODE_CHARDEV    #00FFFF
    {0.298f,  0.627f,  1.0f   },   // NODE_BLOCKDEV   #4CA0FF
    {1.0f,    0.0f,    0.0f   },   // NODE_UNKNOWN    #FF0000
};

// ----------------------------------------------------------------------------
// Singleton accessor
// ----------------------------------------------------------------------------
ColorSystem& ColorSystem::instance() {
    static ColorSystem inst;
    return inst;
}

// ----------------------------------------------------------------------------
// init - first-time setup: load defaults then generate the spectrum LUT
// ----------------------------------------------------------------------------
void ColorSystem::init() {
    loadDefaults();
    generateSpectrum();
}

// ----------------------------------------------------------------------------
// loadDefaults - populate config_ with the original fsv default values
// ----------------------------------------------------------------------------
void ColorSystem::loadDefaults() {
    // Node type colors
    for (int i = 0; i < NUM_NODE_TYPES; ++i) {
        config_.byNodetype.colors[i] = defaultNodeTypeColors[i];
    }

    // Timestamp settings: rainbow spectrum, modify time, 1-week window
    config_.byTimestamp.spectrumType  = SPECTRUM_RAINBOW;
    config_.byTimestamp.timestampType = TIMESTAMP_MODIFY;
    config_.byTimestamp.newTime       = std::time(nullptr);
    config_.byTimestamp.oldTime       = config_.byTimestamp.newTime - static_cast<time_t>(7 * 24 * 60 * 60);
    config_.byTimestamp.oldColor      = PlatformUtils::hex2rgb("#0000FF");
    config_.byTimestamp.newColor      = PlatformUtils::hex2rgb("#FF0000");

    // Wildcard pattern groups (original defaults from color.c)
    config_.byWpattern.groups.clear();

    // Archives - red
    {
        WPatternGroup grp;
        grp.color = PlatformUtils::hex2rgb("#FF3333");
        grp.patterns = {"*.arj", "*.gz", "*.lzh", "*.tar", "*.tgz", "*.z", "*.zip", "*.Z"};
        config_.byWpattern.groups.push_back(std::move(grp));
    }

    // Images - magenta
    {
        WPatternGroup grp;
        grp.color = PlatformUtils::hex2rgb("#FF33FF");
        grp.patterns = {"*.gif", "*.jpg", "*.png", "*.ppm", "*.tga", "*.tif", "*.xpm"};
        config_.byWpattern.groups.push_back(std::move(grp));
    }

    // Media - white
    {
        WPatternGroup grp;
        grp.color = PlatformUtils::hex2rgb("#FFFFFF");
        grp.patterns = {"*.au", "*.mov", "*.mp3", "*.mpg", "*.wav"};
        config_.byWpattern.groups.push_back(std::move(grp));
    }

    // Default color for unmatched files
    config_.byWpattern.defaultColor = PlatformUtils::hex2rgb("#FFFFA0");

    // Default mode
    mode_ = COLOR_BY_NODETYPE;
}

// ----------------------------------------------------------------------------
// generateSpectrum - rebuild the cached spectrum LUT from the current config
// ----------------------------------------------------------------------------
void ColorSystem::generateSpectrum() {
    spectrum_.generate(config_.byTimestamp.spectrumType,
                       config_.byTimestamp.oldColor,
                       config_.byTimestamp.newColor);
}

// ----------------------------------------------------------------------------
// setMode - change current coloring mode and recolor the entire tree
// ----------------------------------------------------------------------------
void ColorSystem::setMode(ColorMode mode) {
    mode_ = mode;
    FsNode* root = FsTree::instance().root();
    if (root) {
        assignRecursive(root);
    }
}

// ----------------------------------------------------------------------------
// setConfig - replace the color configuration and optionally the mode
//
// Port of color_set_config from color.c.
// ----------------------------------------------------------------------------
void ColorSystem::setConfig(const ColorConfig& config, ColorMode mode) {
    config_ = config;
    generateSpectrum();

    if (mode != COLOR_NONE) {
        setMode(mode);
    } else {
        setMode(mode_);
    }
}

// ----------------------------------------------------------------------------
// assignRecursive - port of color_assign_recursive from color.c
//
// Walks all children of dnode and assigns a color pointer according to the
// current mode.  Recurses into directories.
// ----------------------------------------------------------------------------
void ColorSystem::assignRecursive(FsNode* dnode) {
    if (!dnode) {
        return;
    }

    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        const RGBcolor* color = nullptr;

        switch (mode_) {
            case COLOR_BY_NODETYPE:
                color = nodeTypeColor(node);
                break;
            case COLOR_BY_TIMESTAMP:
                color = timeColor(node);
                break;
            case COLOR_BY_WPATTERN:
                color = wpatternColor(node);
                break;
            default:
                color = nodeTypeColor(node);
                break;
        }

        node->color = color;

        if (node->isDir()) {
            assignRecursive(node);
        }
    }
}

// ----------------------------------------------------------------------------
// getSpectrumColor - look up a color in the precomputed spectrum
// ----------------------------------------------------------------------------
const RGBcolor& ColorSystem::getSpectrumColor(double x) const {
    return spectrum_.colorAt(x);
}

// ----------------------------------------------------------------------------
// nodeTypeColor - return the configured color for this node's type
// ----------------------------------------------------------------------------
const RGBcolor* ColorSystem::nodeTypeColor(FsNode* node) const {
    if (!node) {
        return &config_.byNodetype.colors[NODE_UNKNOWN];
    }
    return &config_.byNodetype.colors[node->type];
}

// ----------------------------------------------------------------------------
// timeColor - port of time_color from color.c
//
// Directories are always colored by node type.  Other nodes are colored
// according to their selected timestamp and the current spectrum.
// ----------------------------------------------------------------------------
const RGBcolor* ColorSystem::timeColor(FsNode* node) const {
    if (!node) {
        return &config_.byNodetype.colors[NODE_UNKNOWN];
    }

    // Directory override -- always use node type color
    if (node->isDir()) {
        return nodeTypeColor(node);
    }

    // Choose the appropriate timestamp
    time_t nodeTime = 0;
    switch (config_.byTimestamp.timestampType) {
        case TIMESTAMP_ACCESS:
            nodeTime = node->atime;
            break;
        case TIMESTAMP_MODIFY:
            nodeTime = node->mtime;
            break;
        case TIMESTAMP_ATTRIB:
            nodeTime = node->ctime;
            break;
        default:
            nodeTime = node->mtime;
            break;
    }

    // Compute temporal position value (0 = old, 1 = new)
    double timeDiff = std::difftime(config_.byTimestamp.newTime, config_.byTimestamp.oldTime);
    if (timeDiff == 0.0) {
        return &spectrum_.colorAt(0.5);
    }

    double x = std::difftime(nodeTime, config_.byTimestamp.oldTime) / timeDiff;

    if (x < 0.0) {
        // Node is off the spectrum (too old)
        return &spectrum_.underflowColor();
    }

    if (x > 1.0) {
        // Node is off the spectrum (too new)
        return &spectrum_.overflowColor();
    }

    // Return a color somewhere in the spectrum
    return &spectrum_.colorAt(x);
}

// ----------------------------------------------------------------------------
// wpatternColor - port of wpattern_color from color.c
//
// Directories are always colored by node type.  Other nodes are matched
// against the wildcard pattern groups; unmatched files get the default color.
// ----------------------------------------------------------------------------
const RGBcolor* ColorSystem::wpatternColor(FsNode* node) const {
    if (!node) {
        return &config_.byWpattern.defaultColor;
    }

    // Directory override
    if (node->isDir()) {
        return nodeTypeColor(node);
    }

    const std::string& name = node->name;

    // Search for a match in the wildcard pattern groups
    for (const auto& group : config_.byWpattern.groups) {
        for (const auto& pattern : group.patterns) {
            if (PlatformUtils::wildcardMatch(pattern, name)) {
                return &group.color;
            }
        }
    }

    // No match -- return the default color
    return &config_.byWpattern.defaultColor;
}

} // namespace fsvng
