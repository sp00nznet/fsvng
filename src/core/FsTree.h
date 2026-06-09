#pragma once

#include "FsNode.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fsvng {

// ============================================================================
// FsTree - singleton tree container with O(1) lookup
// ============================================================================

class FsTree {
public:
    static FsTree& instance();

    // Per-scene tree storage. Owned by a Scene so each pane has its own tree
    // and lookup tables; FsTree points at the active one.
    struct SceneState {
        std::unique_ptr<FsNode> root_;
        std::vector<FsNode*> nodeTable_;
        std::unordered_map<std::string, FsNode*> pathTable_;
        unsigned int nextId_ = 0;
    };

    // Re-point FsTree at a scene's state (nullptr restores the default).
    void setSceneState(SceneState* s) { s_ = s ? s : &defaultState_; }
    SceneState* sceneState() { return s_; }

    // Access the metanode (root of the internal tree structure).
    FsNode* root() const;

    // Access the first child of the metanode (the actual scanned directory).
    FsNode* rootDir() const;

    // Replace the tree root.
    void setRoot(std::unique_ptr<FsNode> root);

    // Clear the entire tree.
    void clear();

    // O(1) lookup by node ID.
    FsNode* nodeById(unsigned int id) const;

    // Lookup by absolute path (uses a hash map built during buildNodeTable).
    FsNode* nodeByPath(const std::string& absname) const;

    // Build the lookup tables (called after scan or tree modification).
    void buildNodeTable();

    // Sort children and compute subtree info (replaces setup_fstree_recursive).
    void setupTree();

    // Current number of allocated node IDs.
    unsigned int nodeCount() const { return s_->nextId_; }

    // Allocate the next unique node ID.
    unsigned int allocateId() { return s_->nextId_++; }

private:
    FsTree() = default;
    FsTree(const FsTree&) = delete;
    FsTree& operator=(const FsTree&) = delete;

    // Recursive helper for setupTree.
    void setupRecursive(FsNode* node);

    // Sort a directory node's children: dirs first, then by size desc, then alpha.
    static void sortChildren(FsNode* node);

    SceneState defaultState_;
    SceneState* s_ = &defaultState_;
};

} // namespace fsvng
