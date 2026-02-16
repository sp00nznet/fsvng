#pragma once

#include "Types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fsvng {

// ============================================================================
// Geometry parameter structs - one per visualization mode
// ============================================================================

struct DiscVGeomParams {
    double radius = 0.0;
    double theta = 0.0;
    XYvec pos{};
};

struct MapVGeomParams {
    XYvec c0{};   // lower-left corner
    XYvec c1{};   // upper-right corner
    double height = 0.0;
};

struct TreeVGeomParams {
    struct {
        double distance = 0.0;
        double theta = 0.0;
        double height = 0.0;
    } leaf;
    struct {
        double theta = 0.0;
        double depth = 0.0;
        double arc_width = 0.0;
        double height = 0.0;
        double subtree_arc_width = 0.0;
    } platform;
};

// ============================================================================
// FsNode - unified filesystem node
// ============================================================================

class FsNode {
public:
    // Base fields (from NodeDesc)
    NodeType type = NODE_UNKNOWN;
    unsigned int id = 0;
    std::string name;
    int64_t size = 0;
    int64_t sizeAlloc = 0;
    uint32_t userId = 0;
    uint32_t groupId = 0;
    uint16_t perms = 0;
    uint16_t flags = 0;
    time_t atime = 0;
    time_t mtime = 0;
    time_t ctime = 0;
    const RGBcolor* color = nullptr;

    // Geometry params - one struct per visualization mode
    DiscVGeomParams discvGeom{};
    MapVGeomParams mapvGeom{};
    TreeVGeomParams treevGeom{};

    // Transient glow intensity (set by PulseEffect each frame, not persisted)
    float glowIntensity = 0.0f;

    // Directory-specific fields (from DirNodeDesc)
    double deployment = 0.0;  // 0 = collapsed, 1 = expanded

    struct {
        int64_t size = 0;
        unsigned int counts[NUM_NODE_TYPES] = {};
    } subtree;

    bool geomExpanded = false;
    bool aDlistStale = true;
    bool bDlistStale = true;
    bool cDlistStale = true;

    // Tree structure
    FsNode* parent = nullptr;
    std::vector<std::unique_ptr<FsNode>> children;

    // --- Inline helpers ---

    bool isDir() const { return type == NODE_DIRECTORY; }
    bool isMetanode() const { return type == NODE_METANODE; }
    bool isCollapsed() const { return deployment < EPSILON; }
    bool isExpanded() const { return deployment > (1.0 - EPSILON); }

    size_t childCount() const { return children.size(); }

    // MapV helper methods (replacing macros from original)
    double mapvWidth() const { return mapvGeom.c1.x - mapvGeom.c0.x; }
    double mapvDepth() const { return mapvGeom.c1.y - mapvGeom.c0.y; }
    double mapvCenterX() const { return 0.5 * (mapvGeom.c0.x + mapvGeom.c1.x); }
    double mapvCenterY() const { return 0.5 * (mapvGeom.c0.y + mapvGeom.c1.y); }

    // --- Methods implemented in .cpp ---

    // Build absolute path by traversing parent pointers
    std::string absName() const;

    // Add a child node; sets child's parent pointer. Returns raw pointer.
    FsNode* addChild(std::unique_ptr<FsNode> child);
};

} // namespace fsvng
