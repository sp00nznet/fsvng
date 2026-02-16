#pragma once

#include <glm/glm.hpp>
#include <imgui.h>
#include <string>
#include <vector>

#include "core/Types.h"

namespace fsvng {

struct Theme {
    std::string id;
    std::string displayName;

    // Viewport
    glm::vec3 viewportBg{0.08f, 0.08f, 0.10f};

    // ImGui accent colors
    glm::vec3 accentPrimary{0.26f, 0.59f, 0.98f};
    glm::vec3 accentSecondary{0.20f, 0.20f, 0.20f};
    glm::vec3 textColor{1.0f, 1.0f, 1.0f};

    // Lighting
    glm::vec3 lightPos{0.0f, 10000.0f, 10000.0f};
    glm::vec3 ambient{0.2f, 0.2f, 0.2f};
    glm::vec3 diffuse{0.5f, 0.5f, 0.5f};

    // Labels
    ImU32 labelColor = IM_COL32(255, 255, 255, 220);
    ImU32 labelShadow = IM_COL32(0, 0, 0, 180);

    // Per-theme node type colors (same indices as NodeType enum)
    RGBcolor nodeTypeColors[NUM_NODE_TYPES] = {};

    // Glow params (disabled for now - reserved for future use)
    glm::vec3 glowColor{0.0f, 0.0f, 0.0f};
    float baseEmissive = 0.0f;
    float rimIntensity = 0.0f;
    float rimPower = 3.0f;

    // Pulse params (disabled for now)
    bool pulseEnabled = false;
    float pulseSpawnInterval = 2.0f;
    float pulseSpeed = 3.0f;
    float pulsePeakIntensity = 0.6f;
    float pulseFadeWidth = 2.0f;
};

class ThemeManager {
public:
    static ThemeManager& instance();

    void init();

    const Theme& currentTheme() const { return themes_[currentIndex_]; }
    int currentIndex() const { return currentIndex_; }
    const std::vector<Theme>& themes() const { return themes_; }

    void setThemeByIndex(int index);
    void setThemeById(const std::string& id);

    void applyImGuiStyle() const;
    void applyNodeColors() const;

private:
    ThemeManager() = default;
    void buildThemes();

    std::vector<Theme> themes_;
    int currentIndex_ = 0;
};

} // namespace fsvng
