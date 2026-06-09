#pragma once

#include <vector>

namespace fsvng {

class FsNode;

class FileListPanel {
public:
    static FileListPanel& instance();

    // Per-scene file-list state. Owned by a Scene; panel points at active one.
    struct SceneState {
        FsNode* currentDir_ = nullptr;
        FsNode* selectedNode_ = nullptr;
        FsNode* contextMenuNode_ = nullptr;
        FsNode* contextMenuNode_pending_ = nullptr;
        std::vector<FsNode*> sorted_;
        int sortColumn_ = 1;       // default: Name
        bool sortAscending_ = true;
        bool sortDirty_ = true;
    };
    void setSceneState(SceneState* s) { s_ = s ? s : &defaultState_; }
    SceneState* sceneState() { return s_; }

    void draw();
    void showEntry(FsNode* node);
    void showDirectory(FsNode* dirNode);
    FsNode* currentDir() const { return s_->currentDir_; }

private:
    FileListPanel() = default;

    // Rebuild sorted_ from currentDir_ according to the active sort column.
    void rebuildSorted();

    SceneState defaultState_;
    SceneState* s_ = &defaultState_;
};

} // namespace fsvng
