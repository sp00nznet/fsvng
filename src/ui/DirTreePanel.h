#pragma once

#include <set>

namespace fsvng {

class FsNode;

class DirTreePanel {
public:
    static DirTreePanel& instance();

    void draw();

    // Check if a directory entry is expanded in the tree
    bool isEntryExpanded(FsNode* node) const;

    // Expand/collapse entry
    void setEntryExpanded(FsNode* node, bool expanded);

    // Select a node in the tree
    void selectNode(FsNode* node);

    // Clear all expansion state (call before replacing the tree)
    void clearExpanded();

private:
    DirTreePanel() = default;
    void drawNode(FsNode* node);

    FsNode* selectedNode_ = nullptr;
    std::set<FsNode*> expandedNodes_;
};

} // namespace fsvng
