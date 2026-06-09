#pragma once

#include <set>
#include <string>
#include <vector>

namespace fsvng {

class FsNode;

class DirTreePanel {
public:
    static DirTreePanel& instance();

    // Per-scene tree-panel state (selection + expansion). Owned by a Scene;
    // the panel points at the active one.
    struct SceneState {
        FsNode* selectedNode_ = nullptr;
        FsNode* contextMenuNode_ = nullptr;
        FsNode* contextMenuNode_pending_ = nullptr;
        std::set<FsNode*> expandedNodes_;
    };
    void setSceneState(SceneState* s) { s_ = s ? s : &defaultState_; }
    SceneState* sceneState() { return s_; }

    void draw();

    // Check if a directory entry is expanded in the tree
    bool isEntryExpanded(FsNode* node) const;

    // Expand/collapse entry
    void setEntryExpanded(FsNode* node, bool expanded);

    // Select a node in the tree
    void selectNode(FsNode* node);

    // Clear all expansion state (call before replacing the tree)
    void clearExpanded();

    // Snapshot/restore expanded directories by absolute path. Used to keep
    // the tree's open state across a rescan that rebuilds all FsNode pointers.
    std::vector<std::string> expandedPaths() const;
    void restoreExpanded(const std::vector<std::string>& paths);

private:
    DirTreePanel() = default;
    void drawNode(FsNode* node);

    SceneState defaultState_;
    SceneState* s_ = &defaultState_;
};

} // namespace fsvng
