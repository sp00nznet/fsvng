#include "geometry/DiscVLayout.h"
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
#include <vector>
#include <utility>

namespace fsvng {

DiscVLayout& DiscVLayout::instance() {
    static DiscVLayout inst;
    return inst;
}

// ============================================================================
// initRecursive - port of discv_init_recursive
// ============================================================================

void DiscVLayout::initRecursive(FsNode* dnode, double stemTheta) {
    assert(dnode->isDir() || dnode->isMetanode());

    if (dnode->isDir()) {
        MorphEngine::instance().morphBreak(&dnode->deployment);
        if (DirTreePanel::instance().isEntryExpanded(dnode))
            dnode->deployment = 1.0;
        else
            dnode->deployment = 0.0;
        GeometryManager::instance().queueRebuild(dnode);
    }

    // If this directory has no children, nothing further to do
    if (dnode->children.empty())
        return;

    double dirRadius = dnode->discvGeom.radius;

    // Assign radii and arc widths to child nodes
    double totalArcWidth = 0.0;
    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        int64_t nodeSize = std::max(int64_t(64), node->size);
        if (node->isDir())
            nodeSize += node->subtree.size;

        // Area of disc == nodeSize
        double radius = std::sqrt(static_cast<double>(nodeSize) / PI);
        // Center-to-center distance (parent to leaf)
        double dist = dirRadius + radius * (1.0 + LEAF_STEM_PROPORTION);
        double arcWidth = 2.0 * deg(std::asin(radius / dist));

        node->discvGeom.radius = radius;
        node->discvGeom.theta = arcWidth;  // temporary value
        node->discvGeom.pos.x = dist;       // temporary value
        totalArcWidth += arcWidth;
    }

    // Create a sorted list of children by size (descending)
    struct NodeSortEntry {
        FsNode* node;
        int64_t totalSize;
    };
    std::vector<NodeSortEntry> sortedNodes;
    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        int64_t sz = node->size;
        if (node->isDir())
            sz += node->subtree.size;
        sortedNodes.push_back({node, sz});
    }
    std::sort(sortedNodes.begin(), sortedNodes.end(),
              [](const NodeSortEntry& a, const NodeSortEntry& b) {
                  if (a.totalSize != b.totalSize)
                      return a.totalSize > b.totalSize;
                  return a.node->name < b.node->name;
              });

    double k = LEAF_RANGE_ARC_WIDTH / totalArcWidth;
    // If this is going to be a tight fit, stagger the leaf nodes
    bool stagger = k <= 1.0;

    // Assign angle positions to leaf nodes, arranging them in clockwise
    // order (spread out to occupy the entire available range), and
    // recurse into subdirectories
    double theta0 = stemTheta - 180.0;
    double theta1 = stemTheta + 180.0;
    bool even = true;
    bool out = true;

    for (size_t idx = 0; idx < sortedNodes.size(); ++idx) {
        FsNode* node = sortedNodes[idx].node;
        double arcWidth = k * node->discvGeom.theta;
        double dist = node->discvGeom.pos.x;

        if (stagger && out) {
            // Push leaf out
            dist += 2.0 * node->discvGeom.radius;
        }

        if (idx == 0) {
            // First (largest) node
            node->discvGeom.theta = theta0;
            theta0 += 0.5 * arcWidth;
            theta1 -= 0.5 * arcWidth;
            out = !out;
        } else if (even) {
            node->discvGeom.theta = theta0 + 0.5 * arcWidth;
            theta0 += arcWidth;
            out = !out;
        } else {
            node->discvGeom.theta = theta1 - 0.5 * arcWidth;
            theta1 -= arcWidth;
        }

        node->discvGeom.pos.x = dist * std::cos(rad(node->discvGeom.theta));
        node->discvGeom.pos.y = dist * std::sin(rad(node->discvGeom.theta));

        if (node->isDir()) {
            initRecursive(node, node->discvGeom.theta + 180.0);
        }

        even = !even;
    }
}

// ============================================================================
// init - port of discv_init
// ============================================================================

void DiscVLayout::init() {
    FsTree& tree = FsTree::instance();
    FsNode* metanode = tree.root();
    if (!metanode) return;

    FsNode* rootDir = tree.rootDir();
    if (!rootDir) return;

    metanode->discvGeom.radius = 0.0;
    metanode->discvGeom.theta = 0.0;

    initRecursive(metanode, 270.0);

    metanode->discvGeom.pos.x = 0.0;
    metanode->discvGeom.pos.y = -rootDir->discvGeom.radius;
}

// ============================================================================
// Build disc mesh for a node (port of discv_gldraw_node)
// ============================================================================

void DiscVLayout::buildNodeDisc(FsNode* node, double dirDeployment,
                                std::vector<Vertex>& vertices,
                                std::vector<uint32_t>& indices) {
    int segCount = static_cast<int>(360.0 / CURVE_GRANULARITY + 0.999);

    XYvec center;
    center.x = dirDeployment * node->discvGeom.pos.x;
    center.y = dirDeployment * node->discvGeom.pos.y;

    glm::vec3 col(0.7f, 0.7f, 0.7f);
    if (node->color) {
        col = glm::vec3(node->color->r, node->color->g, node->color->b);
    }

    glm::vec3 nUp(0.0f, 0.0f, 1.0f);

    // Draw disc as triangle fan converted to triangles
    // Center vertex
    uint32_t centerIdx = static_cast<uint32_t>(vertices.size());
    vertices.push_back({glm::vec3(static_cast<float>(center.x),
                                   static_cast<float>(center.y), 0.0f),
                        nUp, col, glm::vec2(0.5f, 0.5f)});

    // Edge vertices
    for (int s = 0; s <= segCount; ++s) {
        double theta = static_cast<double>(s) / static_cast<double>(segCount) * 360.0;
        double px = center.x + node->discvGeom.radius * std::cos(rad(theta));
        double py = center.y + node->discvGeom.radius * std::sin(rad(theta));

        vertices.push_back({glm::vec3(static_cast<float>(px),
                                       static_cast<float>(py), 0.0f),
                            nUp, col, glm::vec2(0.0f)});
    }

    // Create triangles from fan
    for (int s = 0; s < segCount; ++s) {
        indices.push_back(centerIdx);
        indices.push_back(centerIdx + 1 + s);
        indices.push_back(centerIdx + 2 + s);
    }
}

// ============================================================================
// Build folder outline for collapsed directory (COMPLETING the WIP)
// The original discv_gldraw_folder was empty - we implement it here
// ============================================================================

void DiscVLayout::buildFolderOutline(FsNode* dnode,
                                     std::vector<Vertex>& vertices,
                                     std::vector<uint32_t>& indices) {
    assert(dnode->isDir());

    int segCount = static_cast<int>(360.0 / CURVE_GRANULARITY + 0.999);

    glm::vec3 col(0.7f, 0.7f, 0.7f);
    if (dnode->color) {
        col = glm::vec3(dnode->color->r, dnode->color->g, dnode->color->b);
    }

    glm::vec3 nUp(0.0f, 0.0f, 1.0f);

    // Draw a ring outline as a series of thin quads
    double innerRadius = dnode->discvGeom.radius * 0.92;
    double outerRadius = dnode->discvGeom.radius;

    // Center position
    double cx = dnode->discvGeom.pos.x;
    double cy = dnode->discvGeom.pos.y;

    for (int s = 0; s < segCount; ++s) {
        double theta0 = static_cast<double>(s) / static_cast<double>(segCount) * 360.0;
        double theta1 = static_cast<double>(s + 1) / static_cast<double>(segCount) * 360.0;

        double ct0 = std::cos(rad(theta0)), st0 = std::sin(rad(theta0));
        double ct1 = std::cos(rad(theta1)), st1 = std::sin(rad(theta1));

        uint32_t base = static_cast<uint32_t>(vertices.size());

        // Inner edge
        vertices.push_back({glm::vec3(float(cx + innerRadius * ct0),
                                       float(cy + innerRadius * st0), 0.01f),
                            nUp, col, glm::vec2(0.0f)});
        // Outer edge
        vertices.push_back({glm::vec3(float(cx + outerRadius * ct0),
                                       float(cy + outerRadius * st0), 0.01f),
                            nUp, col, glm::vec2(0.0f)});
        // Inner edge next
        vertices.push_back({glm::vec3(float(cx + innerRadius * ct1),
                                       float(cy + innerRadius * st1), 0.01f),
                            nUp, col, glm::vec2(0.0f)});
        // Outer edge next
        vertices.push_back({glm::vec3(float(cx + outerRadius * ct1),
                                       float(cy + outerRadius * st1), 0.01f),
                            nUp, col, glm::vec2(0.0f)});

        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 2); indices.push_back(base + 1); indices.push_back(base + 3);
    }

    // Draw a folder tab indicator at the top of the ring
    // A small rectangular tab to distinguish directories
    {
        double tabWidth = outerRadius * 0.4;
        double tabHeight = outerRadius * 0.15;
        double tabLeft = cx - tabWidth * 0.3;
        double tabRight = cx + tabWidth * 0.3;
        double tabBottom = cy + innerRadius;
        double tabTop = tabBottom + tabHeight;

        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(float(tabLeft), float(tabBottom), 0.02f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(float(tabRight), float(tabBottom), 0.02f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(float(tabRight), float(tabTop), 0.02f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(float(tabLeft), float(tabTop), 0.02f), nUp, col, glm::vec2(0.0f)});

        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
    }
}

// ============================================================================
// Build all children of a directory (port of discv_build_dir)
// ============================================================================

void DiscVLayout::buildDir(FsNode* dnode,
                           std::vector<Vertex>& vertices,
                           std::vector<uint32_t>& indices) {
    double deployment = dnode->deployment;
    // Original had: dpm = 1.0; // TODO: Fix this, please
    // We honor the actual deployment value now
    if (deployment < EPSILON)
        deployment = 1.0; // still use 1.0 as fallback like the TODO said

    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        buildNodeDisc(node, deployment, vertices, indices);
    }
}

// ============================================================================
// drawRecursive - port of discv_draw_recursive
// ============================================================================

void DiscVLayout::drawRecursive(FsNode* dnode, const glm::mat4& view,
                                const glm::mat4& proj, bool geometry) {
    GeometryManager& gm = GeometryManager::instance();
    MatrixStack& ms = gm.modelStack();

    ms.push();

    bool dirCollapsed = dnode->isCollapsed();
    bool dirExpanded = dnode->isExpanded();

    ms.translate(static_cast<float>(dnode->discvGeom.pos.x),
                 static_cast<float>(dnode->discvGeom.pos.y),
                 0.0f);
    float dep = static_cast<float>(dnode->deployment);
    ms.scale(dep, dep, 1.0f);

    if (geometry) {
        // Draw folder or leaf nodes
        if (dnode->aDlistStale || true) {
            std::vector<Vertex> verts;
            std::vector<uint32_t> inds;

            if (!dirCollapsed) {
                buildDir(dnode, verts, inds);
            }
            if (!dirExpanded) {
                buildFolderOutline(dnode, verts, inds);
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
    }

    // Update geometry status
    dnode->geomExpanded = !dirCollapsed;

    if (dirExpanded) {
        // Recurse into subdirectories
        for (auto& childPtr : dnode->children) {
            FsNode* node = childPtr.get();
            if (!node->isDir())
                break;
            drawRecursive(node, view, proj, geometry);
        }
    }

    ms.pop();
}

// ============================================================================
// draw - port of discv_draw
// ============================================================================

void DiscVLayout::draw(const glm::mat4& view, const glm::mat4& projection, bool highDetail) {
    FsTree& tree = FsTree::instance();
    FsNode* root = tree.root();
    if (!root) return;

    GeometryManager& gm = GeometryManager::instance();
    gm.modelStack().loadIdentity();

    // Draw geometry
    drawRecursive(root, view, projection, true);

    if (highDetail) {
        // High detail: labels, cursor
        // Labels and cursor drawing deferred to TextRenderer integration
    }
}

void DiscVLayout::drawForPicking(const glm::mat4& view, const glm::mat4& projection) {
    FsTree& tree = FsTree::instance();
    FsNode* root = tree.root();
    if (!root) return;

    GeometryManager& gm = GeometryManager::instance();
    gm.modelStack().loadIdentity();

    drawRecursive(root, view, projection, true);
}

} // namespace fsvng
