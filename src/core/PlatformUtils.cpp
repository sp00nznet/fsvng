#include "PlatformUtils.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#endif

namespace fsvng {
namespace PlatformUtils {

// ============================================================================
// getTime - high-resolution monotonic clock, returns seconds as double
// ============================================================================
double getTime() {
    using Clock = std::chrono::high_resolution_clock;
    static const auto startTime = Clock::now();
    auto now = Clock::now();
    std::chrono::duration<double> elapsed = now - startTime;
    return elapsed.count();
}

// ============================================================================
// getUserName / getGroupName
// ============================================================================
std::string getUserName(uint32_t uid) {
#ifdef _WIN32
    (void)uid;
    return "User";
#else
    struct passwd* pw = getpwuid(static_cast<uid_t>(uid));
    if (pw && pw->pw_name) {
        return std::string(pw->pw_name);
    }
    return std::to_string(uid);
#endif
}

std::string getGroupName(uint32_t gid) {
#ifdef _WIN32
    (void)gid;
    return "Users";
#else
    struct group* gr = getgrgid(static_cast<gid_t>(gid));
    if (gr && gr->gr_name) {
        return std::string(gr->gr_name);
    }
    return std::to_string(gid);
#endif
}

// ============================================================================
// formatNumber - format int64 with comma separators
// ============================================================================
std::string formatNumber(int64_t number) {
    if (number == 0) {
        return "0";
    }

    bool negative = (number < 0);
    // Use unsigned to handle INT64_MIN correctly.
    uint64_t n = negative ? static_cast<uint64_t>(-(number + 1)) + 1u
                          : static_cast<uint64_t>(number);

    // Build digit groups from least significant.
    std::string result;
    int digitCount = 0;
    while (n > 0) {
        if (digitCount > 0 && digitCount % 3 == 0) {
            result += ',';
        }
        result += static_cast<char>('0' + static_cast<int>(n % 10));
        n /= 10;
        digitCount++;
    }

    if (negative) {
        result += '-';
    }

    std::reverse(result.begin(), result.end());
    return result;
}

// ============================================================================
// abbrevSize - abbreviate byte size (B, kB, MB, GB, TB, PB, EB)
// ============================================================================
std::string abbrevSize(int64_t size) {
    static const char* suffixes[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB"};
    static const int numSuffixes = static_cast<int>(sizeof(suffixes) / sizeof(suffixes[0]));

    if (size < 0) {
        return "-" + abbrevSize(-size);
    }

    double s = static_cast<double>(size);
    int idx = 0;
    while (s >= 1024.0 && idx < numSuffixes - 1) {
        s /= 1024.0;
        idx++;
    }

    char buf[64];
    if (idx == 0) {
        // Bytes - no decimal.
        std::snprintf(buf, sizeof(buf), "%d %s", static_cast<int>(s), suffixes[idx]);
    } else if (s < 10.0) {
        std::snprintf(buf, sizeof(buf), "%.2f %s", s, suffixes[idx]);
    } else if (s < 100.0) {
        std::snprintf(buf, sizeof(buf), "%.1f %s", s, suffixes[idx]);
    } else {
        std::snprintf(buf, sizeof(buf), "%.0f %s", s, suffixes[idx]);
    }
    return std::string(buf);
}

// ============================================================================
// rgb2hex / hex2rgb
// ============================================================================
std::string rgb2hex(const RGBcolor& color) {
    int r = static_cast<int>(std::round(std::clamp(color.r, 0.0f, 1.0f) * 255.0f));
    int g = static_cast<int>(std::round(std::clamp(color.g, 0.0f, 1.0f) * 255.0f));
    int b = static_cast<int>(std::round(std::clamp(color.b, 0.0f, 1.0f) * 255.0f));
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    return std::string(buf);
}

RGBcolor hex2rgb(const std::string& hexColor) {
    RGBcolor color{};
    const char* str = hexColor.c_str();
    if (*str == '#') str++;

    unsigned int r = 0, g = 0, b = 0;
    if (std::sscanf(str, "%02x%02x%02x", &r, &g, &b) == 3) {
        color.r = static_cast<float>(r) / 255.0f;
        color.g = static_cast<float>(g) / 255.0f;
        color.b = static_cast<float>(b) / 255.0f;
    }
    return color;
}

// ============================================================================
// rainbowColor - rainbow spectrum from original common.c
//
// Maps x in [0, 1] through a 6-segment color wheel:
//   0.0 = red -> yellow -> green -> cyan -> blue -> magenta -> 1.0 = red
// ============================================================================
RGBcolor rainbowColor(double x) {
    // Clamp x to [0, 1].
    x = std::clamp(x, 0.0, 1.0);

    // Scale to [0, 6) for six color segments.
    double t = x * 6.0;
    int segment = static_cast<int>(std::floor(t));
    double frac = t - segment;

    // Wrap segment 6 back to 0.
    if (segment >= 6) {
        segment = 0;
        frac = 0.0;
    }

    RGBcolor c{};
    switch (segment) {
        case 0: // red -> yellow
            c.r = 1.0f;
            c.g = static_cast<float>(frac);
            c.b = 0.0f;
            break;
        case 1: // yellow -> green
            c.r = static_cast<float>(1.0 - frac);
            c.g = 1.0f;
            c.b = 0.0f;
            break;
        case 2: // green -> cyan
            c.r = 0.0f;
            c.g = 1.0f;
            c.b = static_cast<float>(frac);
            break;
        case 3: // cyan -> blue
            c.r = 0.0f;
            c.g = static_cast<float>(1.0 - frac);
            c.b = 1.0f;
            break;
        case 4: // blue -> magenta
            c.r = static_cast<float>(frac);
            c.g = 0.0f;
            c.b = 1.0f;
            break;
        case 5: // magenta -> red
            c.r = 1.0f;
            c.g = 0.0f;
            c.b = static_cast<float>(1.0 - frac);
            break;
        default:
            c.r = 1.0f;
            c.g = 0.0f;
            c.b = 0.0f;
            break;
    }
    return c;
}

// ============================================================================
// heatColor - heat spectrum from original common.c
//
// Maps x in [0, 1]: black -> red -> yellow -> white
// ============================================================================
RGBcolor heatColor(double x) {
    x = std::clamp(x, 0.0, 1.0);

    RGBcolor c{};
    if (x < 1.0 / 3.0) {
        // Black -> Red
        double t = x * 3.0;
        c.r = static_cast<float>(t);
        c.g = 0.0f;
        c.b = 0.0f;
    } else if (x < 2.0 / 3.0) {
        // Red -> Yellow
        double t = (x - 1.0 / 3.0) * 3.0;
        c.r = 1.0f;
        c.g = static_cast<float>(t);
        c.b = 0.0f;
    } else {
        // Yellow -> White
        double t = (x - 2.0 / 3.0) * 3.0;
        c.r = 1.0f;
        c.g = 1.0f;
        c.b = static_cast<float>(t);
    }
    return c;
}

// ============================================================================
// wildcardMatch - simple glob matching with * and ?
// ============================================================================
bool wildcardMatch(const std::string& pattern, const std::string& str) {
    // Two-pointer technique with star backtracking.
    size_t pLen = pattern.size();
    size_t sLen = str.size();
    size_t pi = 0, si = 0;
    size_t starPi = std::string::npos;
    size_t starSi = 0;

    while (si < sLen) {
        if (pi < pLen && (pattern[pi] == '?' || pattern[pi] == str[si])) {
            // Characters match or pattern has '?'
            pi++;
            si++;
        } else if (pi < pLen && pattern[pi] == '*') {
            // Record star position, try matching zero characters first.
            starPi = pi;
            starSi = si;
            pi++;
        } else if (starPi != std::string::npos) {
            // Mismatch but we have a star to fall back to.
            pi = starPi + 1;
            starSi++;
            si = starSi;
        } else {
            // No match.
            return false;
        }
    }

    // Consume trailing '*' in pattern.
    while (pi < pLen && pattern[pi] == '*') {
        pi++;
    }

    return pi == pLen;
}

// ============================================================================
// formatTime - format time_t as human-readable string (thread-safe)
// ============================================================================
std::string formatTime(time_t t) {
    if (t == 0) {
        return "(unknown)";
    }

    struct tm tmBuf;
#ifdef _WIN32
    errno_t err = localtime_s(&tmBuf, &t);
    if (err != 0) {
        return "(invalid)";
    }
#else
    struct tm* result = localtime_r(&t, &tmBuf);
    if (!result) {
        return "(invalid)";
    }
#endif

    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf);
    return std::string(buf);
}

} // namespace PlatformUtils
} // namespace fsvng
