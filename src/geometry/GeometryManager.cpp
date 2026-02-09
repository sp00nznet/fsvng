#include "geometry/GeometryManager.h"
#include "geometry/MapVLayout.h"
#include "geometry/TreeVLayout.h"
#include "geometry/DiscVLayout.h"
#include "core/FsNode.h"
#include "core/FsTree.h"
#include "ui/DirTreePanel.h"
#include "renderer/Renderer.h"
#include "renderer/MeshBuffer.h"
#include "renderer/ShaderProgram.h"
#include "animation/Morph.h"
#include "animation/Animation.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cfloat>
#include <cassert>

namespace fsvng {

// ============================================================================
// MatrixStack implementation
// ============================================================================

void MatrixStack::translate(float x, float y, float z) {
    current_ = glm::translate(current_, glm::vec3(x, y, z));
}

void MatrixStack::rotate(float angle, float x, float y, float z) {
    current_ = glm::rotate(current_, glm::radians(angle), glm::vec3(x, y, z));
}

void MatrixStack::scale(float x, float y, float z) {
    current_ = glm::scale(current_, glm::vec3(x, y, z));
}

// ============================================================================
// GeometryManager singleton
// ============================================================================

GeometryManager& GeometryManager::instance() {
    static GeometryManager inst;
    return inst;
}

void GeometryManager::init(FsvMode mode) {
    mode_ = mode;

    // Reset draw stages
    lowDrawStage_ = 0;
    highDrawStage_ = 0;

    FsTree& tree = FsTree::instance();
    FsNode* root = tree.root();
    if (!root) return;

    // Set metanode deployment to 1.0
    root->deployment = 1.0;
    queueRebuild(root);

    switch (mode) {
        case FSV_DISCV:
            DiscVLayout::instance().init();
            break;
        case FSV_MAPV:
            MapVLayout::instance().init();
            break;
        case FSV_TREEV:
            TreeVLayout::instance().init();
            break;
        default:
            break;
    }
}

void GeometryManager::draw(const glm::mat4& view, const glm::mat4& projection, bool highDetail) {
    switch (mode_) {
        case FSV_DISCV:
            DiscVLayout::instance().draw(view, projection, highDetail);
            break;
        case FSV_MAPV:
            MapVLayout::instance().draw(view, projection, highDetail);
            break;
        case FSV_TREEV:
            TreeVLayout::instance().draw(view, projection, highDetail);
            break;
        default:
            break;
    }
}

void GeometryManager::drawForPicking(const glm::mat4& view, const glm::mat4& projection) {
    switch (mode_) {
        case FSV_DISCV:
            DiscVLayout::instance().drawForPicking(view, projection);
            break;
        case FSV_MAPV:
            MapVLayout::instance().drawForPicking(view, projection);
            break;
        case FSV_TREEV:
            TreeVLayout::instance().drawForPicking(view, projection);
            break;
        default:
            break;
    }
}

void GeometryManager::queueRebuild(FsNode* dnode) {
    if (!dnode) return;
    dnode->aDlistStale = true;
    dnode->bDlistStale = true;
    dnode->cDlistStale = true;
    queueUncachedDraw();
}

void GeometryManager::queueUncachedDraw() {
    lowDrawStage_ = 0;
    highDrawStage_ = 0;
}

void GeometryManager::cameraPanFinished() {
    switch (mode_) {
        case FSV_DISCV:
            // Not yet implemented in original
            break;
        case FSV_MAPV:
            MapVLayout::instance().cameraPanFinished();
            break;
        case FSV_TREEV:
            TreeVLayout::instance().cameraPanFinished();
            break;
        default:
            break;
    }
}

void GeometryManager::colexpInitiated(FsNode* dnode) {
    if (!dnode || !dnode->isDir()) return;

    // A newly expanding directory in TreeV mode will probably
    // need (re)shaping (it may be appearing for the first time,
    // or its inner radius may have changed)
    if (dnode->isCollapsed() && mode_ == FSV_TREEV) {
        TreeVLayout::instance().reshapePlatformPublic(dnode,
            treevPlatformR0(dnode));
    }
}

void GeometryManager::colexpInProgress(FsNode* dnode) {
    if (!dnode || !dnode->isDir()) return;

    // Check geometry status against deployment. If they don't concur
    // properly, then directory geometry has to be rebuilt
    if (dnode->geomExpanded != (dnode->deployment > EPSILON)) {
        queueRebuild(dnode);
    } else {
        queueUncachedDraw();
    }

    if (mode_ == FSV_TREEV) {
        // Take care of shifting angles
        TreeVLayout::instance().queueRearrange(dnode);
    }
}

void GeometryManager::freeAll() {
    highlightNode_ = nullptr;
    lowDrawStage_ = 0;
    highDrawStage_ = 0;
}

bool GeometryManager::shouldHighlight(FsNode* node) const {
    if (!node) return false;

    if (!node->isDir())
        return true;

    switch (mode_) {
        case FSV_DISCV:
            return true;
        case FSV_MAPV:
            return node->isCollapsed();
        case FSV_TREEV:
            return treevIsLeaf(node);
        default:
            return false;
    }
}

// ============================================================================
// MapV helpers
// ============================================================================

double GeometryManager::mapvNodeZ0(FsNode* node) const {
    double z = 0.0;
    FsNode* upNode = node->parent;
    while (upNode != nullptr) {
        z += upNode->mapvGeom.height;
        upNode = upNode->parent;
    }
    return z;
}

double GeometryManager::mapvMaxExpandedHeight(FsNode* dnode) const {
    assert(dnode->isDir());

    if (DirTreePanel::instance().isEntryExpanded(dnode)) {
        double maxHeight = 0.0;
        for (auto& childPtr : dnode->children) {
            FsNode* node = childPtr.get();
            double height = node->mapvGeom.height;
            if (node->isDir()) {
                height += mapvMaxExpandedHeight(node);
                maxHeight = std::max(maxHeight, height);
            } else {
                maxHeight = std::max(maxHeight, height);
                break; // non-dirs are sorted last, first one is largest
            }
        }
        return maxHeight;
    }

    return 0.0;
}

// ============================================================================
// TreeV helpers
// ============================================================================

bool GeometryManager::treevIsLeaf(FsNode* node) const {
    if (node->isDir()) {
        if (DirTreePanel::instance().isEntryExpanded(node))
            return false;
    }
    return true;
}

double GeometryManager::treevPlatformR0(FsNode* dnode) const {
    if (dnode->isMetanode())
        return treevCoreRadius_;

    double r0 = 0.0;
    FsNode* upNode = dnode->parent;
    while (upNode != nullptr) {
        r0 += TreeVLayout::PLATFORM_SPACING_DEPTH;
        r0 += upNode->treevGeom.platform.depth;
        upNode = upNode->parent;
    }
    r0 += treevCoreRadius_;

    return r0;
}

double GeometryManager::treevPlatformTheta(FsNode* dnode) const {
    assert(!treevIsLeaf(dnode) || dnode->isMetanode());

    double theta = 0.0;
    FsNode* upNode = dnode;
    while (upNode != nullptr) {
        theta += upNode->treevGeom.platform.theta;
        upNode = upNode->parent;
    }
    return theta;
}

double GeometryManager::treevMaxLeafHeight(FsNode* dnode) const {
    assert(!treevIsLeaf(dnode));

    double maxHeight = 0.0;
    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        if (treevIsLeaf(node)) {
            maxHeight = std::max(maxHeight, node->treevGeom.leaf.height);
        }
    }
    return maxHeight;
}

void GeometryManager::treevGetExtentsRecursive(FsNode* dnode, RTvec* c0, RTvec* c1,
                                                double r0, double theta) const {
    assert(dnode->isDir());

    double subtreeR0 = r0 + dnode->treevGeom.platform.depth + TreeVLayout::PLATFORM_SPACING_DEPTH;

    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        if (!node->isDir())
            break;
        if (!treevIsLeaf(node)) {
            treevGetExtentsRecursive(node, c0, c1, subtreeR0,
                theta + node->treevGeom.platform.theta);
        }
    }

    c0->r = std::min(c0->r, r0);
    c0->theta = std::min(c0->theta, theta - dnode->treevGeom.platform.arc_width);
    c1->r = std::max(c1->r, r0 + dnode->treevGeom.platform.depth);
    c1->theta = std::max(c1->theta, theta + dnode->treevGeom.platform.arc_width);
}

void GeometryManager::treevGetExtents(FsNode* dnode, RTvec* extC0, RTvec* extC1) const {
    assert(!treevIsLeaf(dnode));

    RTvec c0, c1;
    c0.r = DBL_MAX;
    c0.theta = DBL_MAX;
    c1.r = -DBL_MAX;
    c1.theta = -DBL_MAX;

    treevGetExtentsRecursive(dnode, &c0, &c1,
        treevPlatformR0(dnode), treevPlatformTheta(dnode));

    if (extC0 != nullptr) *extC0 = c0;
    if (extC1 != nullptr) *extC1 = c1;
}

// ============================================================================
// DiscV helpers
// ============================================================================

XYvec GeometryManager::discvNodePos(FsNode* node) const {
    XYvec pos{};
    pos.x = 0.0;
    pos.y = 0.0;

    FsNode* upNode = node;
    while (upNode != nullptr) {
        pos.x += upNode->discvGeom.pos.x;
        pos.y += upNode->discvGeom.pos.y;
        upNode = upNode->parent;
    }

    return pos;
}

} // namespace fsvng
