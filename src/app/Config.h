#pragma once

#include "core/Types.h"
#include "color/ColorSystem.h"
#include <string>
#include <nlohmann/json.hpp>

namespace fsvng {

class Config {
public:
    static Config& instance();

    void load();
    void save();

    // App settings
    std::string lastRootPath;
    std::string defaultPath;   // Cached default scan path
    FsvMode lastMode = FSV_MAPV;

    // Window settings
    int windowWidth = 1280;
    int windowHeight = 800;

    // Color settings
    ColorConfig colorConfig;
    ColorMode colorMode = COLOR_BY_NODETYPE;

    // Theme
    std::string themeName = "classic";

    // Get config file path
    static std::string getConfigPath();

private:
    Config() = default;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

    // Helper: serialize/deserialize RGBcolor
    static std::string colorToHex(const RGBcolor& c);
    static RGBcolor hexToColor(const std::string& hex);
};

} // namespace fsvng
