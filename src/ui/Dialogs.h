#pragma once

namespace fsvng {

class FsNode;

class Dialogs {
public:
    static Dialogs& instance();
    void draw();

    void showChangeRoot();
    void showSetDefaultPath();
    void showColorConfig();
    void showAbout();
    void showContextMenu(FsNode* node);

private:
    Dialogs() = default;

    void drawChangeRoot();
    void drawSetDefaultPath();
    void drawColorConfig();
    void drawAbout();
    void drawContextMenu();

    bool showChangeRoot_ = false;
    bool showSetDefaultPath_ = false;
    bool showColorConfig_ = false;
    bool showAbout_ = false;
    bool showContextMenu_ = false;

    FsNode* contextMenuNode_ = nullptr;
    char rootPathBuf_[512] = {};
    char defaultPathBuf_[512] = {};
};

} // namespace fsvng
