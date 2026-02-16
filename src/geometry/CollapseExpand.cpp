#include "geometry/CollapseExpand.h"
#include "geometry/GeometryManager.h"
#include "core/FsNode.h"
#include "core/FsTree.h"
#include "ui/DirTreePanel.h"
#include "animation/Morph.h"
#include "animation/Animation.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace fsvng {

CollapseExpand& CollapseExpand::instance() {
    static CollapseExpand inst;
    return inst;
}

// ============================================================================
// collapsedDepth - port of collapsed_depth
// Returns the number of collapsed directory levels above the given directory
// ============================================================================

int CollapseExpand::collapsedDepth(FsNode* dnode) const {
    int depth = 0;
    FsNode* upNode = dnode->parent;
    while (upNode != nullptr && upNode->isDir()) {
        if (!upNode->isCollapsed())
            break;
        ++depth;
        upNode = upNode->parent;
    }
    return depth;
}

// ============================================================================
// maxExpandedDepth - port of max_expanded_depth
// Returns the maximum depth to which a directory's subtree has been expanded
// ============================================================================

int CollapseExpand::maxExpandedDepth(FsNode* dnode) const {
    int maxDepth = 0;

    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        if (node->isDir()) {
            if (node->isExpanded()) {
                int subtreeDepth = 1 + maxExpandedDepth(node);
                maxDepth = std::max(maxDepth, subtreeDepth);
            }
        } else {
            break; // children are sorted dirs-first
        }
    }

    return maxDepth;
}

// ============================================================================
// progressCallback - port of colexp_progress_cb
// Step/end callback for collapses/expands
// ============================================================================

void CollapseExpand::progressCallback(Morph* morph) {
    FsNode* dnode = static_cast<FsNode*>(morph->data);
    assert(dnode != nullptr && dnode->isDir());

    // Keep geometry module appraised of collapse/expand progress
    GeometryManager::instance().colexpInProgress(dnode);

    // Keep viewport refreshed
    Animation::instance().requestRedraw();
}

// ============================================================================
// execute - port of colexp()
// Main entry point for collapse/expand operations
// ============================================================================

void CollapseExpand::execute(FsNode* dnode, ColExpAction action) {
    assert(dnode != nullptr && dnode->isDir());

    // Static state shared across recursive calls
    static double colexpTime = 0.0;
    static int depth = 0;
    static int maxDepthStatic = 0;

    GeometryManager& gm = GeometryManager::instance();
    DirTreePanel& dirTree = DirTreePanel::instance();
    MorphEngine& morphEngine = MorphEngine::instance();

    if (depth == 0) {
        // Top-level call: update dir tree and determine max recursion depth
        switch (action) {
            case ColExpAction::CollapseRecursive:
                // Collapse this node and all children in the tree panel
                dirTree.setEntryExpanded(dnode, false);
                // Recursively collapse children in tree panel
                for (auto& childPtr : dnode->children) {
                    FsNode* node = childPtr.get();
                    if (node->isDir())
                        dirTree.setEntryExpanded(node, false);
                }
                maxDepthStatic = maxExpandedDepth(dnode);
                break;

            case ColExpAction::Expand:
                dirTree.setEntryExpanded(dnode, true);
                maxDepthStatic = 0;
                break;

            case ColExpAction::ExpandAny:
                dirTree.setEntryExpanded(dnode, true);
                maxDepthStatic = collapsedDepth(dnode);
                break;

            case ColExpAction::ExpandRecursive:
                dirTree.setEntryExpanded(dnode, true);
                // Recursively expand children in tree panel
                for (auto& childPtr : dnode->children) {
                    FsNode* node = childPtr.get();
                    if (node->isDir())
                        dirTree.setEntryExpanded(node, true);
                }
                // max_depth will be used as a high-water mark
                maxDepthStatic = 0;
                break;
        }

        // Collapse/expand time for current visualization mode
        switch (gm.currentMode()) {
            case FSV_MAPV:
                colexpTime = MAPV_TIME;
                break;
            case FSV_TREEV:
                colexpTime = TREEV_TIME;
                break;
            default:
                colexpTime = MAPV_TIME;
                break;
        }
    }

    // Break any ongoing deployment morphs
    morphEngine.morphBreak(&dnode->deployment);

    // Determine time to wait before collapsing/expanding directory
    int waitCount = 0;
    switch (action) {
        case ColExpAction::CollapseRecursive:
            waitCount = maxDepthStatic - depth;
            break;
        case ColExpAction::ExpandRecursive:
        case ColExpAction::Expand:
            waitCount = depth;
            break;
        case ColExpAction::ExpandAny:
            waitCount = maxDepthStatic - depth;
            break;
    }

    if (waitCount > 0) {
        double waitTime = static_cast<double>(waitCount) * colexpTime;
        morphEngine.morph(&dnode->deployment, MorphType::Linear,
                          dnode->deployment, waitTime);
    }

    // Initiate collapse/expand morph
    switch (action) {
        case ColExpAction::CollapseRecursive:
            morphEngine.morphFull(&dnode->deployment, MorphType::Quadratic,
                                  0.0, colexpTime,
                                  progressCallback, progressCallback,
                                  dnode);
            break;

        case ColExpAction::Expand:
        case ColExpAction::ExpandAny:
        case ColExpAction::ExpandRecursive:
            morphEngine.morphFull(&dnode->deployment, MorphType::InvQuadratic,
                                  1.0, colexpTime,
                                  progressCallback, progressCallback,
                                  dnode);
            break;
    }

    // Recursion
    // geometry_colexp_initiated() is called at differing points because
    // (at least in TreeV mode) notification must always proceed from
    // parent to children, and not the other way around
    switch (action) {
        case ColExpAction::Expand:
            // Initial collapse/expand notify
            gm.colexpInitiated(dnode);
            // EXPAND does not walk the tree
            break;

        case ColExpAction::ExpandAny:
            // Ensure that all parent directories are expanded
            if (dnode->parent != nullptr && dnode->parent->isDir()) {
                ++depth;
                execute(dnode->parent, ColExpAction::ExpandAny);
                --depth;
            }
            // Initial collapse/expand notify
            gm.colexpInitiated(dnode);
            break;

        case ColExpAction::CollapseRecursive:
        case ColExpAction::ExpandRecursive:
            // Initial collapse/expand notify
            gm.colexpInitiated(dnode);
            // Perform action on subdirectories
            ++depth;
            for (auto& childPtr : dnode->children) {
                FsNode* node = childPtr.get();
                if (node->isDir()) {
                    execute(node, action);
                } else {
                    break;
                }
            }
            --depth;
            break;
    }

    if (action == ColExpAction::ExpandRecursive) {
        // Update high-water mark
        maxDepthStatic = std::max(maxDepthStatic, depth);
    }

    if (depth == 0) {
        // Top-level cleanup

        // Determine position of current node w.r.t. the
        // collapsing/expanding directory node
        // Note: In the new architecture, current_node tracking is
        // handled by a higher-level component. The camera movement
        // logic is preserved structurally but the actual camera calls
        // will be wired up during camera integration.

        // If in TreeV mode, the current node is an ancestor of
        // a collapsing/expanding directory, the scrollbars may
        // need updating to reflect a new scroll range
        scrollbarsColexpAdjust_ = false;
        if (gm.currentMode() == FSV_TREEV) {
            // Would check if current_node is ancestor of dnode
            // scrollbarsColexpAdjust_ = true;
        }
    }
}

} // namespace fsvng
