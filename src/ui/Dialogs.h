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
    // Draw context menu popup items (call from within a window that opened the popup)
    void drawContextMenuPopup(const char* popupId, FsNode* node);

private:
    Dialogs() = default;

    void drawChangeRoot();
    void drawSetDefaultPath();
    void drawColorConfig();
    void drawAbout();
    void drawProperties();

    bool showChangeRoot_ = false;
    bool showSetDefaultPath_ = false;
    bool showColorConfig_ = false;
    bool showAbout_ = false;
    bool showProperties_ = false;

    FsNode* propertiesNode_ = nullptr;
    char rootPathBuf_[512] = {};
    char defaultPathBuf_[512] = {};
};

} // namespace fsvng
