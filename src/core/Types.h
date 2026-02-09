#pragma once

#include <cmath>
#include <cstdint>
#include <string>

namespace fsvng {

// ============================================================================
// Color types
// ============================================================================

struct RGBcolor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

// ============================================================================
// Vector types
// ============================================================================

struct XYvec {
    double x = 0.0;
    double y = 0.0;
};

struct XYZvec {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct RTvec {
    double r = 0.0;
    double theta = 0.0;
};

struct RTZvec {
    double r = 0.0;
    double theta = 0.0;
    double z = 0.0;
};

// ============================================================================
// Enumerations
// ============================================================================

enum NodeType {
    NODE_METANODE = 0,
    NODE_DIRECTORY,
    NODE_REGFILE,
    NODE_SYMLINK,
    NODE_FIFO,
    NODE_SOCKET,
    NODE_CHARDEV,
    NODE_BLOCKDEV,
    NODE_UNKNOWN,
    NUM_NODE_TYPES
};

enum FsvMode {
    FSV_DISCV = 0,
    FSV_MAPV,
    FSV_TREEV,
    FSV_SPLASH,
    FSV_NONE
};

enum ColorMode {
    COLOR_BY_NODETYPE = 0,
    COLOR_BY_TIMESTAMP,
    COLOR_BY_WPATTERN,
    COLOR_NONE
};

enum TimeStampType {
    TIMESTAMP_ACCESS = 0,
    TIMESTAMP_MODIFY,
    TIMESTAMP_ATTRIB
};

enum SpectrumType {
    SPECTRUM_RAINBOW = 0,
    SPECTRUM_HEAT,
    SPECTRUM_GRADIENT
};

// ============================================================================
// Math constants
// ============================================================================

inline constexpr double LN_2         = 0.69314718055994530942;
inline constexpr double SQRT_2       = 1.41421356237309504880;
inline constexpr double MAGIC_NUMBER = 1.61803398874989484820;  // golden ratio
inline constexpr double PI           = 3.14159265358979323846;
inline constexpr double EPSILON      = 1.0e-6;

// ============================================================================
// Math helpers
// ============================================================================

inline double sqr(double x) {
    return x * x;
}

inline double deg(double radians) {
    return radians * (180.0 / PI);
}

inline double rad(double degrees) {
    return degrees * (PI / 180.0);
}

inline double interpolate(double a, double b, double t) {
    return a + t * (b - a);
}

inline double dotProd(const XYZvec& a, const XYZvec& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline double xyLen(const XYvec& v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

inline double xyzLen(const XYZvec& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

// Distance between two RTZ coordinates (in cylindrical space)
inline double rtzDist(const RTZvec& a, const RTZvec& b) {
    // Convert to Cartesian, compute Euclidean distance
    double ax = a.r * std::cos(a.theta);
    double ay = a.r * std::sin(a.theta);
    double bx = b.r * std::cos(b.theta);
    double by = b.r * std::sin(b.theta);
    return std::sqrt(sqr(ax - bx) + sqr(ay - by) + sqr(a.z - b.z));
}

// ============================================================================
// Node type name arrays
// ============================================================================

inline const char* const nodeTypeNames[] = {
    "Metanode",         // NODE_METANODE
    "Directory",        // NODE_DIRECTORY
    "Regular file",     // NODE_REGFILE
    "Symlink",          // NODE_SYMLINK
    "FIFO/Pipe",        // NODE_FIFO
    "Socket",           // NODE_SOCKET
    "Char device",      // NODE_CHARDEV
    "Block device",     // NODE_BLOCKDEV
    "Unknown"           // NODE_UNKNOWN
};

inline const char* const nodeTypePluralNames[] = {
    "Metanodes",        // NODE_METANODE
    "Directories",      // NODE_DIRECTORY
    "Regular files",    // NODE_REGFILE
    "Symlinks",         // NODE_SYMLINK
    "FIFOs/Pipes",      // NODE_FIFO
    "Sockets",          // NODE_SOCKET
    "Char devices",     // NODE_CHARDEV
    "Block devices",    // NODE_BLOCKDEV
    "Unknown"           // NODE_UNKNOWN
};

} // namespace fsvng
