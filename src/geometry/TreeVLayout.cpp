#include "geometry/TreeVLayout.h"
#include "geometry/GeometryManager.h"
#include "core/FsNode.h"
#include "core/FsTree.h"
#include "ui/DirTreePanel.h"
#include "renderer/Renderer.h"
#include "renderer/ShaderProgram.h"
#include "animation/Morph.h"
#include "animation/Animation.h"
#include "ui/ThemeManager.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cassert>

namespace fsvng {

// Branch color
static const RGBcolor branchColor = { 0.5f, 0.0f, 0.0f };

TreeVLayout& TreeVLayout::instance() {
    static TreeVLayout inst;
    return inst;
}

// ============================================================================
// reshapePlatform - THE MAPLE-DERIVED CUBIC
// Port of treev_reshape_platform from geometry.c
// ============================================================================

void TreeVLayout::reshapePlatform(FsNode* dnode, double r0) {
    static constexpr double edge05 = 0.5 * LEAF_NODE_EDGE;
    static constexpr double edge15 = 1.5 * LEAF_NODE_EDGE;
    static const double w = PLATFORM_SPACING_WIDTH;
    static const double w_2 = w * w;
    static const double w_3 = w_2 * w;
    static const double w_4 = w_2 * w_2;

    // Estimated area, based on number of immediate children
    int n = static_cast<int>(dnode->children.size());
    double k = edge15 * std::ceil(std::sqrt(static_cast<double>(std::max(1, n)))) + edge05;
    double area = k * k;

    // Maple-derived solution to cubic equation
    // d^3 + (2r+w)*d^2 + (2wr - 2A - w)*d - 2Ar = 0
    double A = area;
    double A_2 = A * A;
    double A_3 = A * A_2;
    double r = r0;
    double r_2 = r * r;
    double r_3 = r * r_2;
    double r_4 = r_2 * r_2;

    double ka = 72.0 * (A * r - w * (A + r)) - 64.0 * r_3 + 48.0 * r_2 * w
                - 36.0 * w_2 + 24.0 * r * w_2 - 8.0 * w_3;

    double T1 = 72.0 * A * w_2 - 132.0 * A * r * w_2 - 240.0 * A * w * r_3
                + 120.0 * A * w_2 * r_2 - 24.0 * A_2 * w * r - 60.0 * w_3 * r;
    double T2 = 12.0 * (w_2 * r_2 + A_2 * w_2 - w_4 * r + w_4 * r_2
                + A * w_3 + w_3);
    double T3 = 48.0 * (w_2 * r_4 - w_2 * r_3 - w_3 * r_3)
                + 96.0 * (A_3 + w_3 * r_2);
    double T4 = 192.0 * A * r_4 + 156.0 * A_2 * r_2 + 3.0 * w_4
                + 144.0 * A_2 * w + 264.0 * A * w * r_2;

    double kb = 12.0 * std::sqrt(std::abs(T1 + T2 + T3 + T4));
    double kc = std::cos(std::atan2(kb, ka) / 3.0);
    double kd = std::cbrt(std::hypot(ka, kb));

    // Bring it all together
    double d = (-w - 2.0 * r) / 3.0
             + ((8.0 * r_2 - 4.0 * w * r + 2.0 * w_2) / 3.0 + 4.0 * A + 2.0 * w) * kc / kd
             + kc * kd / 6.0;
    double theta = 180.0 * (d + w) / (PI * (r + d));

    double depth = d;
    double arcWidth = theta;

    // Adjust depth upward to accommodate an integral number of rows
    depth += (edge15 - std::fmod(depth - edge05, edge15)) + edge05;

    // Final arc width must be at least large enough to yield an
    // inner edge length that is two leaf node edges long
    double minArcWidth = (180.0 * (2.0 * LEAF_NODE_EDGE + PLATFORM_SPACING_WIDTH) / PI) / r0;

    dnode->treevGeom.platform.arc_width = std::max(minArcWidth, arcWidth);
    dnode->treevGeom.platform.depth = depth;

    // Directory will need rebuilding
    GeometryManager::instance().queueRebuild(dnode);
}

// ============================================================================
// arrangeRecursive - port of treev_arrange_recursive
// ============================================================================

void TreeVLayout::arrangeRecursive(FsNode* dnode, double r0, bool reshapeTree) {
    assert(dnode->isDir() || dnode->isMetanode());

    GeometryManager& gm = GeometryManager::instance();

    if (!reshapeTree && !(dnode->flags & NEED_REARRANGE))
        return;

    if (reshapeTree && dnode->isDir()) {
        if (gm.treevIsLeaf(dnode)) {
            // Ensure directory leaf gets repositioned
            gm.queueRebuild(dnode);
            return;
        } else {
            // Reshape directory platform
            reshapePlatform(dnode, r0);
        }
    }

    // Recurse into expanded subdirectories, and obtain the overall
    // arc width of the subtree
    double subtreeR0 = r0 + dnode->treevGeom.platform.depth + PLATFORM_SPACING_DEPTH;
    double subtreeArcWidth = 0.0;

    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        if (!node->isDir())
            break;
        arrangeRecursive(node, subtreeR0, reshapeTree);
        double arcWidth = node->deployment
            * std::max(node->treevGeom.platform.arc_width,
                       node->treevGeom.platform.subtree_arc_width);
        node->treevGeom.platform.theta = arcWidth; // temporary value
        subtreeArcWidth += arcWidth;
    }
    dnode->treevGeom.platform.subtree_arc_width = subtreeArcWidth;

    // Spread the subdirectories, sweeping counterclockwise
    double theta = -0.5 * subtreeArcWidth;
    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        if (!node->isDir())
            break;
        double arcWidth = node->treevGeom.platform.theta;
        node->treevGeom.platform.theta = theta + 0.5 * arcWidth;
        theta += arcWidth;
    }

    // Clear the "need rearrange" flag
    dnode->flags &= static_cast<uint16_t>(~NEED_REARRANGE);
}

// ============================================================================
// arrange - port of treev_arrange
// ============================================================================

void TreeVLayout::arrange(bool initialArrange) {
    // Start from rootDir (skip metanode level so children fill the first ring)
    FsNode* rootDir = FsTree::instance().rootDir();
    if (!rootDir) return;

    double rootR0 = coreRadius_ + PLATFORM_SPACING_DEPTH;
    arrangeRecursive(rootDir, rootR0, initialArrange);
    rootDir->treevGeom.platform.arc_width = MAX_ARC_WIDTH;

    // Check that the tree's total arc width is within bounds
    for (;;) {
        double subtreeArc = rootDir->treevGeom.platform.subtree_arc_width;
        if (subtreeArc > MAX_ARC_WIDTH) {
            // Grow core radius
            coreRadius_ *= CORE_GROW_FACTOR;
            rootR0 = coreRadius_ + PLATFORM_SPACING_DEPTH;
            arrangeRecursive(rootDir, rootR0, true);
            rootDir->treevGeom.platform.arc_width = MAX_ARC_WIDTH;
        } else if (subtreeArc < MIN_ARC_WIDTH &&
                   coreRadius_ > MIN_CORE_RADIUS) {
            // Shrink core radius
            coreRadius_ = std::max(MIN_CORE_RADIUS, coreRadius_ / CORE_GROW_FACTOR);
            rootR0 = coreRadius_ + PLATFORM_SPACING_DEPTH;
            arrangeRecursive(rootDir, rootR0, true);
            rootDir->treevGeom.platform.arc_width = MAX_ARC_WIDTH;
        } else {
            break;
        }
    }
}

// ============================================================================
// initRecursive - port of treev_init_recursive
// ============================================================================

void TreeVLayout::initRecursive(FsNode* dnode) {
    assert(dnode->isDir() || dnode->isMetanode());

    if (dnode->isDir()) {
        MorphEngine::instance().morphBreak(&dnode->deployment);
        if (DirTreePanel::instance().isEntryExpanded(dnode))
            dnode->deployment = 1.0;
        else
            dnode->deployment = 0.0;
        GeometryManager::instance().queueRebuild(dnode);
    }

    dnode->flags = 0;

    // Assign heights to leaf nodes using log scale to handle extreme size ranges
    // (0-byte files next to multi-GB files). Log scale gives a reasonable visual
    // ratio: log2(64)=6 vs log2(10GB)=33, instead of sqrt which gives 8 vs 103K.
    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        int64_t size = std::max(int64_t(64), node->size);
        if (node->isDir()) {
            size += node->subtree.size;
            node->treevGeom.platform.height = PLATFORM_HEIGHT;
            initRecursive(node);
        }
        double logHeight = std::log2(static_cast<double>(size));
        node->treevGeom.leaf.height = logHeight * LEAF_HEIGHT_MULTIPLIER * 16.0;
    }
}

// ============================================================================
// init - port of treev_init
// ============================================================================

void TreeVLayout::init() {
    FsTree& tree = FsTree::instance();
    FsNode* metanode = tree.root();
    if (!metanode) return;

    FsNode* rootDir = tree.rootDir();
    if (!rootDir) return;

    // Allocate point buffers
    int numPoints = static_cast<int>(std::ceil(360.0 / CURVE_GRANULARITY)) + 1;
    innerEdgeBuf_.resize(numPoints);
    outerEdgeBuf_.resize(numPoints);

    coreRadius_ = MIN_CORE_RADIUS;

    // Set up metanode as invisible center (depth=0)
    // theta=0 because draw skips metanode, so its rotation shouldn't be in the chain
    metanode->treevGeom.platform.theta = 0.0;
    metanode->treevGeom.platform.depth = 0.0;
    metanode->treevGeom.platform.arc_width = MAX_ARC_WIDTH;
    metanode->treevGeom.platform.height = 0.0;
    metanode->deployment = 1.0;

    // Set up rootDir as the effective center so its children fill the first ring.
    // (Metanode always has exactly 1 child - rootDir - which wastes a ring level.)
    rootDir->treevGeom.platform.theta = 0.0;
    rootDir->treevGeom.platform.height = 0.0;
    rootDir->treevGeom.leaf.theta = 0.0;
    rootDir->treevGeom.leaf.distance = 0.0;
    rootDir->deployment = 1.0;

    // Initialize from rootDir (skip metanode level)
    initRecursive(rootDir);

    // Arrange from rootDir - its r0 matches treevPlatformR0(rootDir)
    double rootR0 = coreRadius_ + PLATFORM_SPACING_DEPTH;
    arrangeRecursive(rootDir, rootR0, true);

    // Override rootDir's arc_width to fill the entire ring
    rootDir->treevGeom.platform.arc_width = MAX_ARC_WIDTH;

    // Core radius adjustment loop
    for (;;) {
        double subtreeArc = rootDir->treevGeom.platform.subtree_arc_width;
        if (subtreeArc > MAX_ARC_WIDTH) {
            coreRadius_ *= CORE_GROW_FACTOR;
            rootR0 = coreRadius_ + PLATFORM_SPACING_DEPTH;
            arrangeRecursive(rootDir, rootR0, true);
            rootDir->treevGeom.platform.arc_width = MAX_ARC_WIDTH;
        } else if (subtreeArc < MIN_ARC_WIDTH &&
                   coreRadius_ > MIN_CORE_RADIUS) {
            coreRadius_ = std::max(MIN_CORE_RADIUS, coreRadius_ / CORE_GROW_FACTOR);
            rootR0 = coreRadius_ + PLATFORM_SPACING_DEPTH;
            arrangeRecursive(rootDir, rootR0, true);
            rootDir->treevGeom.platform.arc_width = MAX_ARC_WIDTH;
        } else {
            break;
        }
    }

    // Store core radius in GeometryManager
    GeometryManager::instance().treevCoreRadius_ = coreRadius_;

    // Initial cursor state
    getCorners(rootDir, &cursorPrevC0_, &cursorPrevC1_);
    cursorPrevC0_.r *= 0.875;
    cursorPrevC0_.theta -= rootDir->treevGeom.platform.arc_width;
    cursorPrevC0_.z = 0.0;
    cursorPrevC1_.r *= 1.125;
    cursorPrevC1_.theta += rootDir->treevGeom.platform.arc_width;
    cursorPrevC1_.z = rootDir->treevGeom.platform.height;
}

// ============================================================================
// cameraPanFinished - port of treev_camera_pan_finished
// ============================================================================

void TreeVLayout::cameraPanFinished() {
    // Save cursor position - in production this would use the current_node
    // For now, stub
}

// ============================================================================
// queueRearrange - port of treev_queue_rearrange
// ============================================================================

void TreeVLayout::queueRearrange(FsNode* dnode) {
    assert(dnode->isDir());

    FsNode* upNode = dnode;
    while (upNode != nullptr) {
        upNode->flags |= static_cast<uint16_t>(NEED_REARRANGE);

        // Branch geometry has to be rebuilt (display list B)
        upNode->bDlistStale = true;

        upNode = upNode->parent;
    }

    GeometryManager::instance().queueUncachedDraw();
}

// ============================================================================
// getCorners - port of treev_get_corners
// ============================================================================

void TreeVLayout::getCorners(FsNode* node, RTZvec* c0, RTZvec* c1) const {
    GeometryManager& gm = GeometryManager::instance();

    if (gm.treevIsLeaf(node)) {
        // Absolute position of center of leaf node bottom
        RTZvec pos;
        pos.r = gm.treevPlatformR0(node->parent) + node->treevGeom.leaf.distance;
        pos.theta = gm.treevPlatformTheta(node->parent) + node->treevGeom.leaf.theta;
        pos.z = node->parent->treevGeom.platform.height;

        // Calculate corners of leaf node
        double leafArcWidth = (180.0 * LEAF_NODE_EDGE / PI) / pos.r;
        c0->r = pos.r - 0.5 * LEAF_NODE_EDGE;
        c0->theta = pos.theta - 0.5 * leafArcWidth;
        c0->z = pos.z;
        c1->r = pos.r + 0.5 * LEAF_NODE_EDGE;
        c1->theta = pos.theta + 0.5 * leafArcWidth;
        c1->z = pos.z + node->treevGeom.leaf.height;

        // Push corners outward a bit
        double paddingArcWidth = (180.0 * LEAF_PADDING / PI) / pos.r;
        c0->r -= LEAF_PADDING;
        c0->theta -= paddingArcWidth;
        c0->z -= 0.5 * LEAF_PADDING;
        c1->r += LEAF_PADDING;
        c1->theta += paddingArcWidth;
        c1->z += 0.5 * LEAF_PADDING;
    } else {
        // Position of center of inner edge of platform
        RTZvec pos;
        pos.r = gm.treevPlatformR0(node);
        pos.theta = gm.treevPlatformTheta(node);

        // Calculate corners of platform region
        c0->r = pos.r;
        c0->theta = pos.theta - 0.5 * node->treevGeom.platform.arc_width;
        c0->z = 0.0;
        c1->r = pos.r + node->treevGeom.platform.depth;
        c1->theta = pos.theta + 0.5 * node->treevGeom.platform.arc_width;
        c1->z = node->treevGeom.platform.height;

        // Push corners outward. Sides already encompass spacing regions
        c0->r -= PLATFORM_PADDING;
        c1->r += PLATFORM_PADDING;
    }
}

// ============================================================================
// Mesh building: platform (port of treev_gldraw_platform)
// ============================================================================

void TreeVLayout::buildPlatformMesh(FsNode* dnode, double r0,
                                    std::vector<Vertex>& vertices,
                                    std::vector<uint32_t>& indices) {
    assert(dnode->isDir());

    double r1 = r0 + dnode->treevGeom.platform.depth;
    int segCount = static_cast<int>(std::ceil(
        dnode->treevGeom.platform.arc_width / CURVE_GRANULARITY));
    if (segCount < 1) segCount = 1;
    double segArcWidth = dnode->treevGeom.platform.arc_width / static_cast<double>(segCount);

    // Ensure buffers are large enough
    if (innerEdgeBuf_.size() < static_cast<size_t>(segCount + 1)) {
        innerEdgeBuf_.resize(segCount + 1);
        outerEdgeBuf_.resize(segCount + 1);
    }

    // Calculate and cache inner/outer edge vertices
    double theta = -0.5 * dnode->treevGeom.platform.arc_width;
    for (int s = 0; s <= segCount; ++s) {
        double sinTheta = std::sin(rad(theta));
        double cosTheta = std::cos(rad(theta));

        XYvec p0, p1;
        p0.x = r0 * cosTheta;
        p0.y = r0 * sinTheta;
        p1.x = r1 * cosTheta;
        p1.y = r1 * sinTheta;

        if (s == 0) {
            // Leading edge offset
            double dx = -sinTheta * (0.5 * PLATFORM_SPACING_WIDTH);
            double dy = cosTheta * (0.5 * PLATFORM_SPACING_WIDTH);
            p0.x += dx; p0.y += dy;
            p1.x += dx; p1.y += dy;
        } else if (s == segCount) {
            // Trailing edge offset
            double dx = sinTheta * (0.5 * PLATFORM_SPACING_WIDTH);
            double dy = -cosTheta * (0.5 * PLATFORM_SPACING_WIDTH);
            p0.x += dx; p0.y += dy;
            p1.x += dx; p1.y += dy;
        }

        innerEdgeBuf_[s] = p0;
        outerEdgeBuf_[s] = p1;
        theta += segArcWidth;
    }

    float z1 = static_cast<float>(dnode->treevGeom.platform.height);

    glm::vec3 col(0.7f, 0.7f, 0.7f);
    if (dnode->color) {
        col = glm::vec3(dnode->color->r, dnode->color->g, dnode->color->b);
    }

    // Helper lambda to add a quad
    auto addQuad = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, glm::vec3 n) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({v0, n, col, glm::vec2(0.0f)});
        vertices.push_back({v1, n, col, glm::vec2(0.0f)});
        vertices.push_back({v2, n, col, glm::vec2(0.0f)});
        vertices.push_back({v3, n, col, glm::vec2(0.0f)});
        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
    };

    // Draw inner edge
    for (int s = 0; s < segCount; ++s) {
        float px0 = static_cast<float>(innerEdgeBuf_[s].x);
        float py0 = static_cast<float>(innerEdgeBuf_[s].y);
        float px1 = static_cast<float>(innerEdgeBuf_[s + 1].x);
        float py1 = static_cast<float>(innerEdgeBuf_[s + 1].y);
        glm::vec3 n0(static_cast<float>(-innerEdgeBuf_[s].x / r0),
                     static_cast<float>(-innerEdgeBuf_[s].y / r0), 0.0f);
        glm::vec3 n1(static_cast<float>(-innerEdgeBuf_[s+1].x / r0),
                     static_cast<float>(-innerEdgeBuf_[s+1].y / r0), 0.0f);
        glm::vec3 nAvg = glm::normalize(0.5f * (n0 + n1));

        addQuad(glm::vec3(px0, py0, 0.0f), glm::vec3(px0, py0, z1),
                glm::vec3(px1, py1, z1), glm::vec3(px1, py1, 0.0f), nAvg);
    }

    // Draw outer edge
    for (int s = segCount; s > 0; --s) {
        float px0 = static_cast<float>(outerEdgeBuf_[s].x);
        float py0 = static_cast<float>(outerEdgeBuf_[s].y);
        float px1 = static_cast<float>(outerEdgeBuf_[s - 1].x);
        float py1 = static_cast<float>(outerEdgeBuf_[s - 1].y);
        glm::vec3 n0(static_cast<float>(outerEdgeBuf_[s].x / r1),
                     static_cast<float>(outerEdgeBuf_[s].y / r1), 0.0f);
        glm::vec3 n1(static_cast<float>(outerEdgeBuf_[s-1].x / r1),
                     static_cast<float>(outerEdgeBuf_[s-1].y / r1), 0.0f);
        glm::vec3 nAvg = glm::normalize(0.5f * (n0 + n1));

        addQuad(glm::vec3(px0, py0, 0.0f), glm::vec3(px0, py0, z1),
                glm::vec3(px1, py1, z1), glm::vec3(px1, py1, 0.0f), nAvg);
    }

    // Draw leading edge face
    {
        float ip0x = static_cast<float>(innerEdgeBuf_[0].x);
        float ip0y = static_cast<float>(innerEdgeBuf_[0].y);
        float op0x = static_cast<float>(outerEdgeBuf_[0].x);
        float op0y = static_cast<float>(outerEdgeBuf_[0].y);
        glm::vec3 n(static_cast<float>(ip0y / r0), static_cast<float>(-ip0x / r0), 0.0f);
        n = glm::normalize(n);
        addQuad(glm::vec3(ip0x, ip0y, 0.0f), glm::vec3(op0x, op0y, 0.0f),
                glm::vec3(op0x, op0y, z1), glm::vec3(ip0x, ip0y, z1), n);
    }

    // Draw trailing edge face
    {
        float ipNx = static_cast<float>(innerEdgeBuf_[segCount].x);
        float ipNy = static_cast<float>(innerEdgeBuf_[segCount].y);
        float opNx = static_cast<float>(outerEdgeBuf_[segCount].x);
        float opNy = static_cast<float>(outerEdgeBuf_[segCount].y);
        glm::vec3 n(static_cast<float>(-ipNy / r0), static_cast<float>(ipNx / r0), 0.0f);
        n = glm::normalize(n);
        addQuad(glm::vec3(ipNx, ipNy, z1), glm::vec3(opNx, opNy, z1),
                glm::vec3(opNx, opNy, 0.0f), glm::vec3(ipNx, ipNy, 0.0f), n);
    }

    // Draw top face
    glm::vec3 nUp(0.0f, 0.0f, 1.0f);
    for (int s = 0; s < segCount; ++s) {
        float ip0x = static_cast<float>(innerEdgeBuf_[s].x);
        float ip0y = static_cast<float>(innerEdgeBuf_[s].y);
        float op0x = static_cast<float>(outerEdgeBuf_[s].x);
        float op0y = static_cast<float>(outerEdgeBuf_[s].y);
        float ip1x = static_cast<float>(innerEdgeBuf_[s + 1].x);
        float ip1y = static_cast<float>(innerEdgeBuf_[s + 1].y);
        float op1x = static_cast<float>(outerEdgeBuf_[s + 1].x);
        float op1y = static_cast<float>(outerEdgeBuf_[s + 1].y);

        addQuad(glm::vec3(ip0x, ip0y, z1), glm::vec3(op0x, op0y, z1),
                glm::vec3(op1x, op1y, z1), glm::vec3(ip1x, ip1y, z1), nUp);
    }
}

// ============================================================================
// Mesh building: leaf node (port of treev_gldraw_leaf)
// ============================================================================

void TreeVLayout::buildLeafMesh(FsNode* node, double r0, bool fullNode,
                                std::vector<Vertex>& vertices,
                                std::vector<uint32_t>& indices) {
    double edge, height;

    if (fullNode) {
        edge = LEAF_NODE_EDGE;
        height = node->treevGeom.leaf.height;
        if (node->isDir())
            height *= (1.0 - node->deployment);
    } else {
        edge = 0.875 * LEAF_NODE_EDGE;
        height = LEAF_NODE_EDGE / 64.0;
    }

    // Set up corners, centered around (r0+distance, 0, 0)
    XYvec corners[4];
    corners[0].x = r0 + node->treevGeom.leaf.distance - 0.5 * edge;
    corners[0].y = -0.5 * edge;
    corners[1].x = corners[0].x + edge;
    corners[1].y = corners[0].y;
    corners[2].x = corners[1].x;
    corners[2].y = corners[0].y + edge;
    corners[3].x = corners[0].x;
    corners[3].y = corners[2].y;

    // Bottom and top
    float z0f = static_cast<float>(node->parent ? node->parent->treevGeom.platform.height : 0.0);
    float z1f = z0f + static_cast<float>(height);

    double sinTheta = std::sin(rad(node->treevGeom.leaf.theta));
    double cosTheta = std::cos(rad(node->treevGeom.leaf.theta));

    // Rotate corners into position
    XYvec rotated[4];
    for (int i = 0; i < 4; ++i) {
        rotated[i].x = corners[i].x * cosTheta - corners[i].y * sinTheta;
        rotated[i].y = corners[i].x * sinTheta + corners[i].y * cosTheta;
    }

    glm::vec3 col(0.7f, 0.7f, 0.7f);
    if (node->color) {
        col = glm::vec3(node->color->r, node->color->g, node->color->b);
    }

    auto addQuad = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, glm::vec3 n) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({v0, n, col, glm::vec2(0.0f)});
        vertices.push_back({v1, n, col, glm::vec2(0.0f)});
        vertices.push_back({v2, n, col, glm::vec2(0.0f)});
        vertices.push_back({v3, n, col, glm::vec2(0.0f)});
        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
    };

    // Top face
    glm::vec3 nUp(0.0f, 0.0f, 1.0f);
    addQuad(
        glm::vec3(static_cast<float>(rotated[0].x), static_cast<float>(rotated[0].y), z1f),
        glm::vec3(static_cast<float>(rotated[1].x), static_cast<float>(rotated[1].y), z1f),
        glm::vec3(static_cast<float>(rotated[2].x), static_cast<float>(rotated[2].y), z1f),
        glm::vec3(static_cast<float>(rotated[3].x), static_cast<float>(rotated[3].y), z1f),
        nUp);

    if (!fullNode) {
        // Draw an "X" marker - we skip lines in mesh mode, just draw the top face
        return;
    }

    // Side faces
    float st = static_cast<float>(sinTheta);
    float ct = static_cast<float>(cosTheta);

    // Front face (side 0->1)
    glm::vec3 nFront(st, -ct, 0.0f);
    addQuad(
        glm::vec3(static_cast<float>(rotated[0].x), static_cast<float>(rotated[0].y), z1f),
        glm::vec3(static_cast<float>(rotated[0].x), static_cast<float>(rotated[0].y), z0f),
        glm::vec3(static_cast<float>(rotated[1].x), static_cast<float>(rotated[1].y), z0f),
        glm::vec3(static_cast<float>(rotated[1].x), static_cast<float>(rotated[1].y), z1f),
        nFront);

    // Right face (side 1->2)
    glm::vec3 nRight(ct, st, 0.0f);
    addQuad(
        glm::vec3(static_cast<float>(rotated[1].x), static_cast<float>(rotated[1].y), z1f),
        glm::vec3(static_cast<float>(rotated[1].x), static_cast<float>(rotated[1].y), z0f),
        glm::vec3(static_cast<float>(rotated[2].x), static_cast<float>(rotated[2].y), z0f),
        glm::vec3(static_cast<float>(rotated[2].x), static_cast<float>(rotated[2].y), z1f),
        nRight);

    // Rear face (side 2->3)
    glm::vec3 nRear(-st, ct, 0.0f);
    addQuad(
        glm::vec3(static_cast<float>(rotated[2].x), static_cast<float>(rotated[2].y), z1f),
        glm::vec3(static_cast<float>(rotated[2].x), static_cast<float>(rotated[2].y), z0f),
        glm::vec3(static_cast<float>(rotated[3].x), static_cast<float>(rotated[3].y), z0f),
        glm::vec3(static_cast<float>(rotated[3].x), static_cast<float>(rotated[3].y), z1f),
        nRear);

    // Left face (side 3->0)
    glm::vec3 nLeftFace(-ct, -st, 0.0f);
    addQuad(
        glm::vec3(static_cast<float>(rotated[3].x), static_cast<float>(rotated[3].y), z1f),
        glm::vec3(static_cast<float>(rotated[3].x), static_cast<float>(rotated[3].y), z0f),
        glm::vec3(static_cast<float>(rotated[0].x), static_cast<float>(rotated[0].y), z0f),
        glm::vec3(static_cast<float>(rotated[0].x), static_cast<float>(rotated[0].y), z1f),
        nLeftFace);
}

// ============================================================================
// Build folder mesh for collapsed directory leaf (port of treev_gldraw_folder)
// ============================================================================

void TreeVLayout::buildFolderMesh(FsNode* dnode, double r0,
                                  std::vector<Vertex>& vertices,
                                  std::vector<uint32_t>& indices) {
    assert(dnode->isDir());

    static constexpr double X1 = -0.4375 * LEAF_NODE_EDGE;
    static constexpr double X2 = 0.375 * LEAF_NODE_EDGE;
    static constexpr double X3 = 0.4375 * LEAF_NODE_EDGE;
    static constexpr double Y1 = -0.4375 * LEAF_NODE_EDGE;
    static constexpr double Y5 = 0.4375 * LEAF_NODE_EDGE;
    static constexpr double Y2 = Y1 + (2.0 - MAGIC_NUMBER) * LEAF_NODE_EDGE;
    static constexpr double Y3 = Y2 + 0.0625 * LEAF_NODE_EDGE;
    static constexpr double Y4 = Y5 - 0.0625 * LEAF_NODE_EDGE;

    static const XYvec folderPoints[] = {
        { X1, Y1 }, { X2, Y1 }, { X2, Y2 }, { X3, Y3 },
        { X3, Y4 }, { X2, Y5 }, { X1, Y5 }
    };

    double folderR = r0 + dnode->treevGeom.leaf.distance;
    double sinTheta = std::sin(rad(dnode->treevGeom.leaf.theta));
    double cosTheta = std::cos(rad(dnode->treevGeom.leaf.theta));
    float z = static_cast<float>((1.0 - dnode->deployment) * dnode->treevGeom.leaf.height
              + (dnode->parent ? dnode->parent->treevGeom.platform.height : 0.0));

    glm::vec3 col(0.7f, 0.7f, 0.7f);
    if (dnode->color) {
        col = glm::vec3(dnode->color->r, dnode->color->g, dnode->color->b);
    }

    glm::vec3 nUp(0.0f, 0.0f, 1.0f);
    float lineWidth = static_cast<float>(LEAF_NODE_EDGE * 0.02);

    // Draw folder outline as thin quads
    for (int i = 0; i < 7; ++i) {
        int j = (i + 1) % 7;
        double px0 = folderR + folderPoints[i].x;
        double py0 = folderPoints[i].y;
        double px1 = folderR + folderPoints[j].x;
        double py1 = folderPoints[j].y;

        // Rotate into position
        float rx0 = static_cast<float>(px0 * cosTheta - py0 * sinTheta);
        float ry0 = static_cast<float>(px0 * sinTheta + py0 * cosTheta);
        float rx1 = static_cast<float>(px1 * cosTheta - py1 * sinTheta);
        float ry1 = static_cast<float>(px1 * sinTheta + py1 * cosTheta);

        float dx = rx1 - rx0;
        float dy = ry1 - ry0;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6f) continue;
        float nx = -dy / len * lineWidth;
        float ny = dx / len * lineWidth;

        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(rx0 + nx, ry0 + ny, z), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(rx0 - nx, ry0 - ny, z), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(rx1 + nx, ry1 + ny, z), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(rx1 - nx, ry1 - ny, z), nUp, col, glm::vec2(0.0f)});

        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 2); indices.push_back(base + 1); indices.push_back(base + 3);
    }
}

// ============================================================================
// Build branch loop (port of treev_gldraw_loop)
// ============================================================================

void TreeVLayout::buildBranchLoop(double loopR,
                                  std::vector<Vertex>& vertices,
                                  std::vector<uint32_t>& indices) {
    int segCount = static_cast<int>(360.0 / CURVE_GRANULARITY + 0.5);
    double loopR0 = loopR - 0.5 * BRANCH_WIDTH;
    double loopR1 = loopR + 0.5 * BRANCH_WIDTH;

    glm::vec3 col(branchColor.r, branchColor.g, branchColor.b);
    glm::vec3 nUp(0.0f, 0.0f, 1.0f);

    // Build quad strip as triangles
    for (int s = 0; s < segCount; ++s) {
        double theta0 = 360.0 * static_cast<double>(s) / static_cast<double>(segCount);
        double theta1 = 360.0 * static_cast<double>(s + 1) / static_cast<double>(segCount);
        double st0 = std::sin(rad(theta0)), ct0 = std::cos(rad(theta0));
        double st1 = std::sin(rad(theta1)), ct1 = std::cos(rad(theta1));

        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(float(loopR0 * ct0), float(loopR0 * st0), 0.0f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(float(loopR1 * ct0), float(loopR1 * st0), 0.0f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(float(loopR0 * ct1), float(loopR0 * st1), 0.0f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(float(loopR1 * ct1), float(loopR1 * st1), 0.0f), nUp, col, glm::vec2(0.0f)});

        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 2); indices.push_back(base + 1); indices.push_back(base + 3);
    }
}

// ============================================================================
// Build inner branch (port of treev_gldraw_inbranch)
// ============================================================================

void TreeVLayout::buildInBranch(double r0,
                                std::vector<Vertex>& vertices,
                                std::vector<uint32_t>& indices) {
    float c0x = static_cast<float>(r0 - 0.5 * PLATFORM_SPACING_DEPTH);
    float c0y = static_cast<float>(-0.5 * BRANCH_WIDTH);
    float c1x = static_cast<float>(r0);
    float c1y = static_cast<float>(0.5 * BRANCH_WIDTH);

    glm::vec3 col(branchColor.r, branchColor.g, branchColor.b);
    glm::vec3 nUp(0.0f, 0.0f, 1.0f);

    uint32_t base = static_cast<uint32_t>(vertices.size());
    vertices.push_back({glm::vec3(c0x, c0y, 0.0f), nUp, col, glm::vec2(0.0f)});
    vertices.push_back({glm::vec3(c1x, c0y, 0.0f), nUp, col, glm::vec2(0.0f)});
    vertices.push_back({glm::vec3(c1x, c1y, 0.0f), nUp, col, glm::vec2(0.0f)});
    vertices.push_back({glm::vec3(c0x, c1y, 0.0f), nUp, col, glm::vec2(0.0f)});

    indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
    indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
}

// ============================================================================
// Build outer branch (port of treev_gldraw_outbranch)
// ============================================================================

void TreeVLayout::buildOutBranch(double r1, double theta0, double theta1,
                                 std::vector<Vertex>& vertices,
                                 std::vector<uint32_t>& indices) {
    assert(theta1 >= theta0);

    glm::vec3 col(branchColor.r, branchColor.g, branchColor.b);
    glm::vec3 nUp(0.0f, 0.0f, 1.0f);

    double arcR = r1 + 0.5 * PLATFORM_SPACING_DEPTH;
    double arcR0 = arcR - 0.5 * BRANCH_WIDTH;
    double arcR1 = arcR + 0.5 * BRANCH_WIDTH;

    // Draw branch stem
    {
        float sx0 = static_cast<float>(r1);
        float sy0 = static_cast<float>(-0.5 * BRANCH_WIDTH);
        float sx1 = static_cast<float>(arcR);
        float sy1 = static_cast<float>(0.5 * BRANCH_WIDTH);

        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(sx0, sy0, 0.0f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(sx1, sy0, 0.0f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(sx1, sy1, 0.0f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(sx0, sy1, 0.0f), nUp, col, glm::vec2(0.0f)});

        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
    }

    // Draw arc
    double arcWidth = theta1 - theta0;
    if (arcWidth < EPSILON)
        return;

    double suppArcWidth = (180.0 * BRANCH_WIDTH / PI) / arcR0;
    int segCount = static_cast<int>(std::ceil((arcWidth + suppArcWidth) / CURVE_GRANULARITY));
    if (segCount < 1) segCount = 1;
    double segArcWidth = (arcWidth + suppArcWidth) / static_cast<double>(segCount);

    double theta = theta0 - 0.5 * suppArcWidth;
    for (int s = 0; s < segCount; ++s) {
        double t0 = theta + s * segArcWidth;
        double t1 = theta + (s + 1) * segArcWidth;
        double st0 = std::sin(rad(t0)), ct0 = std::cos(rad(t0));
        double st1 = std::sin(rad(t1)), ct1 = std::cos(rad(t1));

        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(float(arcR0 * ct0), float(arcR0 * st0), 0.0f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(float(arcR1 * ct0), float(arcR1 * st0), 0.0f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(float(arcR0 * ct1), float(arcR0 * st1), 0.0f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(float(arcR1 * ct1), float(arcR1 * st1), 0.0f), nUp, col, glm::vec2(0.0f)});

        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 2); indices.push_back(base + 1); indices.push_back(base + 3);
    }
}

// ============================================================================
// buildDir - arrange and draw leaf nodes on a directory (port of treev_build_dir)
// ============================================================================

void TreeVLayout::buildDir(FsNode* dnode, double r0,
                           std::vector<Vertex>& vertices,
                           std::vector<uint32_t>& indices) {
    static constexpr double edge05 = 0.5 * LEAF_NODE_EDGE;
    static constexpr double edge15 = 1.5 * LEAF_NODE_EDGE;

    assert(dnode->isDir());

    // Build rows of leaf nodes, going from the inner edge outward
    // (this requires laying down nodes in reverse order)
    int remainingNodeCount = static_cast<int>(dnode->children.size());
    RTvec pos;
    pos.r = r0 + LEAF_NODE_EDGE;

    // Iterate children in reverse
    int childIdx = static_cast<int>(dnode->children.size()) - 1;
    while (childIdx >= 0) {
        FsNode* node = dnode->children[childIdx].get();

        // Calculate available arc length of row
        double arcLen = (PI / 180.0) * pos.r * dnode->treevGeom.platform.arc_width
                        - PLATFORM_SPACING_WIDTH;
        // Number of nodes this row can accommodate
        int rowNodeCount = static_cast<int>(std::floor((arcLen - edge05) / edge15));
        if (rowNodeCount < 1) rowNodeCount = 1;
        // Arc width between adjacent leaf nodes
        double interArcWidth = (180.0 * edge15 / PI) / pos.r;

        // Lay out nodes in this row, sweeping clockwise
        pos.theta = 0.5 * interArcWidth
                    * static_cast<double>(std::min(rowNodeCount, remainingNodeCount) - 1);

        for (int n = 0; (n < rowNodeCount) && (childIdx >= 0); ++n) {
            node = dnode->children[childIdx].get();
            node->treevGeom.leaf.theta = pos.theta;
            node->treevGeom.leaf.distance = pos.r - r0;

            // Build leaf mesh - full node for non-dirs, footprint for collapsed dirs
            buildLeafMesh(node, r0, !node->isDir(), vertices, indices);

            pos.theta -= interArcWidth;
            --childIdx;
        }

        remainingNodeCount -= rowNodeCount;
        pos.r += edge15;
    }

    // Official directory depth
    pos.r -= edge05;
    dnode->treevGeom.platform.depth = pos.r - r0;

    // Draw underlying directory platform
    buildPlatformMesh(dnode, r0, vertices, indices);
}

// ============================================================================
// drawRecursive - port of treev_draw_recursive
// ============================================================================

bool TreeVLayout::drawRecursive(FsNode* dnode, const glm::mat4& view,
                                const glm::mat4& proj,
                                double prevR0, double r0, bool withBranches) {
    assert(dnode->isDir() || dnode->isMetanode());

    GeometryManager& gm = GeometryManager::instance();
    MatrixStack& ms = gm.modelStack();

    ms.push();

    bool dirCollapsed = dnode->isCollapsed();
    bool dirExpanded = dnode->isExpanded();

    if (!dirCollapsed) {
        if (!dirExpanded) {
            // Directory is partially deployed
            // Draw the shrinking/growing leaf
            {
                std::vector<Vertex> verts;
                std::vector<uint32_t> inds;
                buildLeafMesh(dnode, prevR0, true, verts, inds);
                buildFolderMesh(dnode, prevR0, verts, inds);

                if (!verts.empty()) {
                    float nodeGlow = ThemeManager::instance().currentTheme().baseEmissive + dnode->glowIntensity;
                    ShaderProgram& shader = Renderer::instance().getNodeShader();
                    shader.use();
                    shader.setMat4("uModel", ms.top());
                    shader.setMat4("uView", view);
                    shader.setMat4("uProjection", proj);
                    shader.setFloat("uGlowIntensity", nodeGlow);

                    MeshBuffer mesh;
                    mesh.upload(verts, inds);
                    mesh.draw(GL_TRIANGLES);
                }
            }

            // Platform should shrink to/grow from corresponding leaf position
            double leafR = prevR0 + dnode->treevGeom.leaf.distance;
            double leafTheta = dnode->treevGeom.leaf.theta;
            ms.rotate(static_cast<float>(leafTheta), 0.0f, 0.0f, 1.0f);
            ms.translate(static_cast<float>(leafR), 0.0f, 0.0f);
            float dep = static_cast<float>(dnode->deployment);
            ms.scale(dep, dep, dep);
            ms.translate(static_cast<float>(-leafR), 0.0f, 0.0f);
            ms.rotate(static_cast<float>(-leafTheta), 0.0f, 0.0f, 1.0f);
        }

        ms.rotate(static_cast<float>(dnode->treevGeom.platform.theta), 0.0f, 0.0f, 1.0f);
    }

    // Draw directory in either leaf or platform form
    if (dnode->aDlistStale || true) {
        std::vector<Vertex> verts;
        std::vector<uint32_t> inds;

        if (dirCollapsed) {
            // Leaf form
            buildLeafMesh(dnode, prevR0, true, verts, inds);
            buildFolderMesh(dnode, prevR0, verts, inds);
        } else if (dnode->isDir()) {
            // Platform form with leaf children
            buildDir(dnode, r0, verts, inds);
        }

        if (!verts.empty()) {
            float nodeGlow = ThemeManager::instance().currentTheme().baseEmissive + dnode->glowIntensity;
            ShaderProgram& shader = Renderer::instance().getNodeShader();
            shader.use();
            shader.setMat4("uModel", ms.top());
            shader.setMat4("uView", view);
            shader.setMat4("uProjection", proj);
            shader.setFloat("uGlowIntensity", nodeGlow);

            MeshBuffer mesh;
            mesh.upload(verts, inds);
            mesh.draw(GL_TRIANGLES);
        }

        dnode->aDlistStale = false;
    }

    FsNode* firstNode = nullptr;
    FsNode* lastNode = nullptr;

    if (!dirCollapsed) {
        // Recurse into subdirectories
        double subtreeR0 = r0 + dnode->treevGeom.platform.depth + PLATFORM_SPACING_DEPTH;
        for (auto& childPtr : dnode->children) {
            FsNode* node = childPtr.get();
            if (!node->isDir())
                break;
            if (drawRecursive(node, view, proj, r0, subtreeR0, withBranches)) {
                if (firstNode == nullptr)
                    firstNode = node;
                lastNode = node;
            }
        }
    }

    if (dirExpanded && withBranches) {
        // Draw interconnecting branches
        std::vector<Vertex> branchVerts;
        std::vector<uint32_t> branchInds;

        if (dnode->isMetanode()) {
            buildBranchLoop(r0, branchVerts, branchInds);
            buildOutBranch(r0, 0.0, 0.0, branchVerts, branchInds);
        } else {
            buildInBranch(r0, branchVerts, branchInds);
            if (firstNode != nullptr) {
                double t0 = std::min(0.0, firstNode->treevGeom.platform.theta);
                double t1 = std::max(0.0, lastNode->treevGeom.platform.theta);
                buildOutBranch(r0 + dnode->treevGeom.platform.depth,
                               t0, t1, branchVerts, branchInds);
            }
        }

        if (!branchVerts.empty()) {
            float nodeGlow = ThemeManager::instance().currentTheme().baseEmissive + dnode->glowIntensity;
            ShaderProgram& shader = Renderer::instance().getNodeShader();
            shader.use();
            shader.setMat4("uModel", ms.top());
            shader.setMat4("uView", view);
            shader.setMat4("uProjection", proj);
            shader.setFloat("uGlowIntensity", nodeGlow);

            MeshBuffer mesh;
            mesh.upload(branchVerts, branchInds);
            mesh.draw(GL_TRIANGLES);
        }

        dnode->bDlistStale = false;
    }

    // Update geometry status
    dnode->geomExpanded = !dirCollapsed;

    ms.pop();

    return dirExpanded;
}

// ============================================================================
// draw - port of treev_draw
// ============================================================================

void TreeVLayout::draw(const glm::mat4& view, const glm::mat4& projection, bool highDetail) {
    FsNode* rootDir = FsTree::instance().rootDir();
    if (!rootDir) return;

    GeometryManager& gm = GeometryManager::instance();
    gm.modelStack().loadIdentity();

    // Arrange if needed
    if (gm.lowDrawStage() == 0 || gm.highDrawStage() == 0) {
        arrange(false);
        gm.treevCoreRadius_ = coreRadius_;
    }

    // Draw starting from rootDir (skip metanode so children fill first ring)
    double rootR0 = coreRadius_ + PLATFORM_SPACING_DEPTH;
    drawRecursive(rootDir, view, projection, 0.0, rootR0, true);

    if (highDetail) {
        // Labels and cursor drawing deferred to TextRenderer integration
    }
}

void TreeVLayout::drawForPicking(const glm::mat4& view, const glm::mat4& projection) {
    FsNode* rootDir = FsTree::instance().rootDir();
    if (!rootDir) return;

    GeometryManager& gm = GeometryManager::instance();
    gm.modelStack().loadIdentity();

    double rootR0 = coreRadius_ + PLATFORM_SPACING_DEPTH;
    drawRecursive(rootDir, view, projection, 0.0, rootR0, false);
}

} // namespace fsvng
