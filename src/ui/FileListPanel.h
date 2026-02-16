#pragma once

namespace fsvng {

class FsNode;

class FileListPanel {
public:
    static FileListPanel& instance();

    void draw();
    void showEntry(FsNode* node);
    void showDirectory(FsNode* dirNode);

private:
    FileListPanel() = default;

    FsNode* currentDir_ = nullptr;
    FsNode* selectedNode_ = nullptr;
    FsNode* contextMenuNode_ = nullptr;
    FsNode* contextMenuNode_pending_ = nullptr;
};

} // namespace fsvng
