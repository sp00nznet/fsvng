#pragma once

#include "Types.h"

#include <cstdint>
#include <ctime>
#include <string>

namespace fsvng {
namespace PlatformUtils {

    // Get current time as double seconds (high-resolution monotonic clock).
    double getTime();

    // Get user name from UID (POSIX) or return placeholder (Windows).
    std::string getUserName(uint32_t uid);

    // Get group name from GID (POSIX) or return placeholder (Windows).
    std::string getGroupName(uint32_t gid);

    // Format an int64 with comma separators (e.g., 1,234,567).
    std::string formatNumber(int64_t number);

    // Abbreviate byte size to human-readable form (e.g., "1.5 MB").
    std::string abbrevSize(int64_t size);

    // Convert RGBcolor to hex string "#RRGGBB".
    std::string rgb2hex(const RGBcolor& color);

    // Convert hex string "#RRGGBB" or "RRGGBB" to RGBcolor.
    RGBcolor hex2rgb(const std::string& hexColor);

    // Rainbow color spectrum. x in [0, 1].
    RGBcolor rainbowColor(double x);

    // Heat color spectrum. x in [0, 1].
    RGBcolor heatColor(double x);

    // Cross-platform wildcard matching (supports * and ?).
    bool wildcardMatch(const std::string& pattern, const std::string& str);

    // Format a time_t as a human-readable string.
    std::string formatTime(time_t t);

} // namespace PlatformUtils
} // namespace fsvng
