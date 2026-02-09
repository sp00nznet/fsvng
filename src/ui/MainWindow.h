#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/Types.h"

namespace fsvng {

class FsNode;
class FsScanner;

class MainWindow {
public:
    static MainWindow& instance();
    ~MainWindow();

    void draw();
    void setInitialPath(const std::string& path);

    // Request a scan - runs on a background thread
    void requestScan(const std::string& path);
    void cancelScan();

    bool isScanning() const { return scanning_.load(); }

    // Navigate to a node in the tree panels
    void navigateTo(FsNode* node);
    void navigateBack();
    void navigateToRoot();
    void navigateUp();

    FsNode* getCurrentNode() const { return currentNode_; }

    // Visualization mode
    FsvMode getMode() const { return currentMode_; }
    void setMode(FsvMode mode);

    // Color mode
    ColorMode getColorMode() const { return currentColorMode_; }
    void setColorMode(ColorMode mode);

private:
    MainWindow() = default;
    void setupDockspace();
    void drawProgressOverlay();
    void finishScan();
    void initVisualization();

    // Runs on the background thread
    void scanThreadFunc(const std::string& path);

    bool firstFrame_ = true;
    bool dockspaceInitialized_ = false;
    std::string initialPath_;
    std::string pendingScanPath_;

    // Background scan state
    std::atomic<bool> scanning_{false};
    std::atomic<bool> scanDone_{false};
    std::thread scanThread_;
    std::unique_ptr<FsNode> scanResult_;
    std::shared_ptr<FsScanner> activeScanner_;

    // Thread-safe progress info
    mutable std::mutex progressMutex_;
    std::string progressDir_;
    int progressFiles_ = 0;
    int progressDirs_ = 0;
    std::string scanError_;

    // Navigation history
    std::vector<FsNode*> navHistory_;
    int navHistoryPos_ = -1;
    FsNode* currentNode_ = nullptr;

    // Visualization state
    FsvMode currentMode_ = FSV_MAPV;
    ColorMode currentColorMode_ = COLOR_BY_NODETYPE;
    bool visualizationReady_ = false;
};

} // namespace fsvng
