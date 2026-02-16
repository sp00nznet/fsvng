#include "app/Config.h"
#include "core/PlatformUtils.h"

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

namespace fsvng {

// ============================================================================
// Singleton accessor
// ============================================================================
Config& Config::instance() {
    static Config inst;
    return inst;
}

// ============================================================================
// getConfigPath - platform-appropriate config file location
// ============================================================================
std::string Config::getConfigPath() {
#ifdef _WIN32
    // %APPDATA%/fsvng/config.json
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        return std::string(appdata) + "\\fsvng\\config.json";
    }
    return "fsvng_config.json";
#elif defined(__APPLE__)
    // ~/Library/Application Support/fsvng/config.json
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/Library/Application Support/fsvng/config.json";
    }
    return "fsvng_config.json";
#else
    // Linux/other: ~/.config/fsvng/config.json
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig && xdgConfig[0] != '\0') {
        return std::string(xdgConfig) + "/fsvng/config.json";
    }
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.config/fsvng/config.json";
    }
    return "fsvng_config.json";
#endif
}

// ============================================================================
// Helper: create parent directories for a path (cross-platform)
// ============================================================================
static void createParentDirs(const std::string& filePath) {
    // Find the last separator.
    std::string dir;
    auto lastSlash = filePath.find_last_of("/\\");
    if (lastSlash == std::string::npos) {
        return; // No directory component.
    }
    dir = filePath.substr(0, lastSlash);

    // Build each component.
    std::string accumulated;
    for (size_t i = 0; i < dir.size(); ++i) {
        char c = dir[i];
        accumulated += c;

        // On Windows, skip the drive letter colon (e.g. "C:")
#ifdef _WIN32
        if (c == ':') {
            continue;
        }
#endif
        if (c == '/' || c == '\\' || i == dir.size() - 1) {
#ifdef _WIN32
            _mkdir(accumulated.c_str());
#else
            mkdir(accumulated.c_str(), 0755);
#endif
        }
    }
}

// ============================================================================
// colorToHex / hexToColor
// ============================================================================
std::string Config::colorToHex(const RGBcolor& c) {
    return PlatformUtils::rgb2hex(c);
}

RGBcolor Config::hexToColor(const std::string& hex) {
    return PlatformUtils::hex2rgb(hex);
}

// ============================================================================
// load - read JSON config from disk; use defaults if file is missing
// ============================================================================
void Config::load() {
    std::string path = getConfigPath();
    std::ifstream ifs(path);

    if (!ifs.is_open()) {
        // File does not exist -- use defaults.  Initialize color config
        // from ColorSystem's load-defaults path.
        ColorSystem::instance().init();
        colorConfig = ColorSystem::instance().getConfig();
        colorMode = ColorSystem::instance().getMode();
        return;
    }

    try {
        nlohmann::json j;
        ifs >> j;
        fromJson(j);
    } catch (const std::exception& e) {
        std::cerr << "fsvng: failed to parse config: " << e.what() << std::endl;
        // Fall back to defaults.
        ColorSystem::instance().init();
        colorConfig = ColorSystem::instance().getConfig();
        colorMode = ColorSystem::instance().getMode();
    }
}

// ============================================================================
// save - write JSON config to disk, creating directories if needed
// ============================================================================
void Config::save() {
    std::string path = getConfigPath();
    createParentDirs(path);

    nlohmann::json j = toJson();

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        std::cerr << "fsvng: failed to write config to " << path << std::endl;
        return;
    }

    ofs << j.dump(4) << std::endl;
}

// ============================================================================
// toJson - serialize all settings to a JSON object
// ============================================================================
nlohmann::json Config::toJson() const {
    nlohmann::json j;

    // App settings
    j["lastRootPath"] = lastRootPath;
    j["defaultPath"] = defaultPath;
    j["lastMode"] = static_cast<int>(lastMode);
    j["themeName"] = themeName;

    // Window settings
    j["window"]["width"] = windowWidth;
    j["window"]["height"] = windowHeight;

    // Color settings
    nlohmann::json& jc = j["color"];
    jc["mode"] = static_cast<int>(colorMode);

    // byNodetype
    nlohmann::json jNodetype = nlohmann::json::array();
    for (int i = 0; i < NUM_NODE_TYPES; ++i) {
        jNodetype.push_back(colorToHex(colorConfig.byNodetype.colors[i]));
    }
    jc["byNodetype"]["colors"] = jNodetype;

    // byTimestamp
    nlohmann::json& jts = jc["byTimestamp"];
    jts["spectrumType"] = static_cast<int>(colorConfig.byTimestamp.spectrumType);
    jts["timestampType"] = static_cast<int>(colorConfig.byTimestamp.timestampType);
    jts["oldTime"] = static_cast<int64_t>(colorConfig.byTimestamp.oldTime);
    jts["newTime"] = static_cast<int64_t>(colorConfig.byTimestamp.newTime);
    jts["oldColor"] = colorToHex(colorConfig.byTimestamp.oldColor);
    jts["newColor"] = colorToHex(colorConfig.byTimestamp.newColor);

    // byWpattern
    nlohmann::json& jwp = jc["byWpattern"];
    nlohmann::json jGroups = nlohmann::json::array();
    for (const auto& grp : colorConfig.byWpattern.groups) {
        nlohmann::json jg;
        jg["color"] = colorToHex(grp.color);
        jg["patterns"] = grp.patterns;
        jGroups.push_back(jg);
    }
    jwp["groups"] = jGroups;
    jwp["defaultColor"] = colorToHex(colorConfig.byWpattern.defaultColor);

    return j;
}

// ============================================================================
// fromJson - deserialize settings, falling back to defaults for missing keys
// ============================================================================
void Config::fromJson(const nlohmann::json& j) {
    // Initialize defaults first so that any missing key keeps its default.
    ColorSystem::instance().init();
    colorConfig = ColorSystem::instance().getConfig();
    colorMode = ColorSystem::instance().getMode();

    // App settings
    if (j.contains("lastRootPath") && j["lastRootPath"].is_string()) {
        lastRootPath = j["lastRootPath"].get<std::string>();
    }
    if (j.contains("defaultPath") && j["defaultPath"].is_string()) {
        defaultPath = j["defaultPath"].get<std::string>();
    }
    if (j.contains("lastMode") && j["lastMode"].is_number_integer()) {
        int m = j["lastMode"].get<int>();
        if (m >= FSV_DISCV && m <= FSV_NONE) {
            lastMode = static_cast<FsvMode>(m);
        }
    }
    if (j.contains("themeName") && j["themeName"].is_string()) {
        themeName = j["themeName"].get<std::string>();
    }

    // Window settings
    if (j.contains("window") && j["window"].is_object()) {
        const auto& jw = j["window"];
        if (jw.contains("width") && jw["width"].is_number_integer()) {
            windowWidth = jw["width"].get<int>();
        }
        if (jw.contains("height") && jw["height"].is_number_integer()) {
            windowHeight = jw["height"].get<int>();
        }
    }

    // Color settings
    if (!j.contains("color") || !j["color"].is_object()) {
        return;
    }
    const auto& jc = j["color"];

    if (jc.contains("mode") && jc["mode"].is_number_integer()) {
        int m = jc["mode"].get<int>();
        if (m >= COLOR_BY_NODETYPE && m <= COLOR_NONE) {
            colorMode = static_cast<ColorMode>(m);
        }
    }

    // byNodetype
    if (jc.contains("byNodetype") && jc["byNodetype"].is_object()) {
        const auto& jnt = jc["byNodetype"];
        if (jnt.contains("colors") && jnt["colors"].is_array()) {
            const auto& arr = jnt["colors"];
            int count = static_cast<int>(arr.size());
            if (count > NUM_NODE_TYPES) count = NUM_NODE_TYPES;
            for (int i = 0; i < count; ++i) {
                if (arr[static_cast<size_t>(i)].is_string()) {
                    colorConfig.byNodetype.colors[i] =
                        hexToColor(arr[static_cast<size_t>(i)].get<std::string>());
                }
            }
        }
    }

    // byTimestamp
    if (jc.contains("byTimestamp") && jc["byTimestamp"].is_object()) {
        const auto& jts = jc["byTimestamp"];

        if (jts.contains("spectrumType") && jts["spectrumType"].is_number_integer()) {
            int st = jts["spectrumType"].get<int>();
            if (st >= SPECTRUM_RAINBOW && st <= SPECTRUM_GRADIENT) {
                colorConfig.byTimestamp.spectrumType = static_cast<SpectrumType>(st);
            }
        }
        if (jts.contains("timestampType") && jts["timestampType"].is_number_integer()) {
            int tt = jts["timestampType"].get<int>();
            if (tt >= TIMESTAMP_ACCESS && tt <= TIMESTAMP_ATTRIB) {
                colorConfig.byTimestamp.timestampType = static_cast<TimeStampType>(tt);
            }
        }
        if (jts.contains("oldTime") && jts["oldTime"].is_number_integer()) {
            colorConfig.byTimestamp.oldTime =
                static_cast<time_t>(jts["oldTime"].get<int64_t>());
        }
        if (jts.contains("newTime") && jts["newTime"].is_number_integer()) {
            colorConfig.byTimestamp.newTime =
                static_cast<time_t>(jts["newTime"].get<int64_t>());
        }
        if (jts.contains("oldColor") && jts["oldColor"].is_string()) {
            colorConfig.byTimestamp.oldColor =
                hexToColor(jts["oldColor"].get<std::string>());
        }
        if (jts.contains("newColor") && jts["newColor"].is_string()) {
            colorConfig.byTimestamp.newColor =
                hexToColor(jts["newColor"].get<std::string>());
        }
    }

    // byWpattern
    if (jc.contains("byWpattern") && jc["byWpattern"].is_object()) {
        const auto& jwp = jc["byWpattern"];

        if (jwp.contains("groups") && jwp["groups"].is_array()) {
            colorConfig.byWpattern.groups.clear();
            for (const auto& jg : jwp["groups"]) {
                if (!jg.is_object()) continue;

                WPatternGroup grp;
                if (jg.contains("color") && jg["color"].is_string()) {
                    grp.color = hexToColor(jg["color"].get<std::string>());
                }
                if (jg.contains("patterns") && jg["patterns"].is_array()) {
                    for (const auto& jp : jg["patterns"]) {
                        if (jp.is_string()) {
                            grp.patterns.push_back(jp.get<std::string>());
                        }
                    }
                }
                colorConfig.byWpattern.groups.push_back(std::move(grp));
            }
        }

        if (jwp.contains("defaultColor") && jwp["defaultColor"].is_string()) {
            colorConfig.byWpattern.defaultColor =
                hexToColor(jwp["defaultColor"].get<std::string>());
        }
    }
}

} // namespace fsvng
