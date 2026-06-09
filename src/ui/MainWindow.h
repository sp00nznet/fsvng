#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/Types.h"
#include "core/FsWatcher.h"

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

    bool isScanning() const { return s_->scanning_.load(); }

    // Navigate to a node in the tree panels
    void navigateTo(FsNode* node);
    void navigateBack();
    void navigateToRoot();
    void navigateUp();
    void navigateToFirstChild();
    void navigateToNextSibling();
    void navigateToPrevSibling();
    void toggleExpandCurrent();

    FsNode* getCurrentNode() const { return s_->currentNode_; }

    // Visualization mode
    FsvMode getMode() const { return s_->currentMode_; }
    void setMode(FsvMode mode);

    // Color mode
    ColorMode getColorMode() const { return s_->currentColorMode_; }
    void setColorMode(ColorMode mode);

    // Hidden (dotfile) visibility. Toggling rescans the current root.
    bool getShowHidden() const { return s_->showHidden_; }
    void setShowHidden(bool show);

    // Search/filter: highlight nodes whose name matches the query. Empty
    // query clears all highlights. Supports substring (case-insensitive) and
    // glob (* and ?) when the query contains a wildcard character.
    void setSearchQuery(const std::string& query);
    const std::string& getSearchQuery() const { return s_->searchQuery_; }
    int getSearchMatchCount() const { return s_->searchMatchCount_; }
    // Re-evaluate the current query against the live tree (after a rescan).
    void refreshSearch();

    // Live filesystem watching. When auto-refresh is on, detected changes
    // trigger a debounced rescan; otherwise hasPendingChanges() lets the UI
    // surface a manual "Refresh" affordance.
    bool getAutoRefresh() const { return s_->autoRefresh_; }
    void setAutoRefresh(bool on) { s_->autoRefresh_ = on; }
    bool hasPendingChanges() const { return s_->fsChangePending_; }
    void rescanCurrentRoot();

    // Multi-pane: open a new pane (on the focused pane's root) / close focused.
    void newPane();
    void closeFocusedPane();

    // Move a path (from any pane) into destDirPath, then refresh all panes.
    // Used by cross-pane drag-and-drop.
    void moveAcrossPanes(const std::string& srcPath, const std::string& destDirPath);

    // Move the focused pane's File List selection to Trash, then refresh.
    void trashSelection();

    // Per-scene application state. Owned by a Scene so each pane has its own
    // navigation, scan thread, watcher, search and view mode. Non-copyable
    // (holds a thread/mutex/watcher) — only ever referenced via pointer.
    struct SceneState {
        std::string pendingScanPath_;
        std::string currentRootPath_;
        bool showHidden_ = false;

        // Search/filter
        std::string searchQuery_;
        int searchMatchCount_ = 0;

        // Live-watch
        FsWatcher watcher_;
        uint64_t lastSeenChange_ = 0;
        bool autoRefresh_ = false;
        bool fsChangePending_ = false;
        double lastChangeTime_ = 0.0;

        // View-restore
        bool viewRestorePending_ = false;
        std::string restoreCurrentPath_;
        std::string restoreFileListPath_;
        std::vector<std::string> restoreExpandedPaths_;

        // Background scan
        std::atomic<bool> scanning_{false};
        std::atomic<bool> scanDone_{false};
        std::thread scanThread_;
        std::unique_ptr<FsNode> scanResult_;
        std::shared_ptr<FsScanner> activeScanner_;

        // Thread-safe progress info
        std::mutex progressMutex_;
        std::string progressDir_;
        int progressFiles_ = 0;
        int progressDirs_ = 0;
        std::string scanError_;

        // Navigation
        std::vector<FsNode*> navHistory_;
        int navHistoryPos_ = -1;
        FsNode* currentNode_ = nullptr;

        // Visualization
        FsvMode currentMode_ = FSV_MAPV;
        ColorMode currentColorMode_ = COLOR_BY_NODETYPE;
        bool visualizationReady_ = false;
    };
    void setSceneState(SceneState* s) { s_ = s ? s : &defaultState_; }
    SceneState* sceneState() { return s_; }

private:
    MainWindow() = default;
    void setupDockspace();
    void drawProgressOverlay();
    void finishScan();
    void initVisualization(bool resetCamera = true);

    // Capture the current navigation/expansion/camera so the next scan can
    // restore it instead of resetting to the root.
    void captureViewState();

    // Runs on the background thread
    void scanThreadFunc(const std::string& path);

    void applySearch(FsNode* node, const std::string& lowerQuery, bool hasWildcard);
    void pollWatcher();

    // Background housekeeping for the active scene (scan completion, deferred
    // scans, watcher polling). Called once per scene per frame.
    void serviceScene();
    // Draw all panes' viewports in a grid; manages focus.
    void drawViewportGrid();

    // Window-level (not per-scene) state.
    bool firstFrame_ = true;
    bool dockspaceInitialized_ = false;
    bool rescanAllPending_ = false;  // cross-pane move requested a full refresh
    std::string initialPath_;

    SceneState defaultState_;
    SceneState* s_ = &defaultState_;
};

} // namespace fsvng
