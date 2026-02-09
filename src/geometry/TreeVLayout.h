#pragma once

#include "core/Types.h"
#include "renderer/MeshBuffer.h"

#include <glm/glm.hpp>
#include <vector>

namespace fsvng {

class FsNode;

class TreeVLayout {
public:
    static TreeVLayout& instance();

    void init();
    void draw(const glm::mat4& view, const glm::mat4& projection, bool highDetail);
    void drawForPicking(const glm::mat4& view, const glm::mat4& projection);

    void cameraPanFinished();
    void queueRearrange(FsNode* dnode);

    // Public wrapper for GeometryManager to call
    void reshapePlatformPublic(FsNode* dnode, double r0) { reshapePlatform(dnode, r0); }

    // Constants
    static constexpr double MIN_ARC_WIDTH = 90.0;
    static constexpr double MAX_ARC_WIDTH = 225.0;
    static constexpr double BRANCH_WIDTH = 256.0;
    static constexpr double MIN_CORE_RADIUS = 8192.0;
    static constexpr double CORE_GROW_FACTOR = 1.25;
    static constexpr double CURVE_GRANULARITY = 5.0;
    static constexpr double PLATFORM_HEIGHT = 158.2;
    static constexpr double PLATFORM_SPACING_WIDTH = 512.0;
    static constexpr double LEAF_HEIGHT_MULTIPLIER = 1.0;
    static constexpr double LEAF_NODE_EDGE = 256.0;
    static constexpr double PLATFORM_SPACING_DEPTH = 2048.0;
    static constexpr double LEAF_PADDING = 0.125 * 256.0;   // LEAF_NODE_EDGE
    static constexpr double PLATFORM_PADDING = 0.5 * 512.0;  // PLATFORM_SPACING_WIDTH

    static constexpr int NEED_REARRANGE = 1 << 0;

    double& coreRadius() { return coreRadius_; }

private:
    TreeVLayout() = default;

    void initRecursive(FsNode* dnode);
    void reshapePlatform(FsNode* dnode, double r0);
    void arrangeRecursive(FsNode* dnode, double r0, bool reshapeTree);
    void arrange(bool initialArrange);

    bool drawRecursive(FsNode* dnode, const glm::mat4& view, const glm::mat4& proj,
                       double prevR0, double r0, bool withBranches);
    void buildPlatformMesh(FsNode* dnode, double r0,
                           std::vector<Vertex>& vertices,
                           std::vector<uint32_t>& indices);
    void buildLeafMesh(FsNode* node, double r0, bool fullNode,
                       std::vector<Vertex>& vertices,
                       std::vector<uint32_t>& indices);
    void buildBranchLoop(double loopR,
                         std::vector<Vertex>& vertices,
                         std::vector<uint32_t>& indices);
    void buildInBranch(double r0,
                       std::vector<Vertex>& vertices,
                       std::vector<uint32_t>& indices);
    void buildOutBranch(double r1, double theta0, double theta1,
                        std::vector<Vertex>& vertices,
                        std::vector<uint32_t>& indices);
    void buildFolderMesh(FsNode* dnode, double r0,
                         std::vector<Vertex>& vertices,
                         std::vector<uint32_t>& indices);

    // Build leaf nodes onto a directory and lay them out in rows
    void buildDir(FsNode* dnode, double r0,
                  std::vector<Vertex>& vertices,
                  std::vector<uint32_t>& indices);

    void getCorners(FsNode* node, RTZvec* c0, RTZvec* c1) const;

    double coreRadius_ = MIN_CORE_RADIUS;

    RTZvec cursorPrevC0_{};
    RTZvec cursorPrevC1_{};

    // Point buffers for curved geometry
    std::vector<XYvec> innerEdgeBuf_;
    std::vector<XYvec> outerEdgeBuf_;
};

} // namespace fsvng
