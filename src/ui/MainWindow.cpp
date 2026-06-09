#include "ui/MainWindow.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <stdexcept>
#include <iostream>

#include "ui/MenuBar.h"
#include "ui/Toolbar.h"
#include "ui/DirTreePanel.h"
#include "ui/FileListPanel.h"
#include "ui/PreviewPanel.h"
#include "ui/ViewportPanel.h"
#include "ui/StatusBar.h"
#include "ui/Dialogs.h"
#include "core/FsTree.h"
#include "core/FsNode.h"
#include "core/FsScanner.h"
#include "core/FileOps.h"
#include "core/PlatformUtils.h"
#include "color/ColorSystem.h"
#include "renderer/Renderer.h"
#include "geometry/GeometryManager.h"
#include "geometry/CollapseExpand.h"
#include "camera/Camera.h"
#include "ui/PulseEffect.h"
#include "scene/Scene.h"
#include "scene/SceneManager.h"

namespace fsvng {

MainWindow& MainWindow::instance() {
    static MainWindow s;
    return s;
}

MainWindow::~MainWindow() {
    if (s_->scanThread_.joinable()) {
        if (s_->activeScanner_) {
            s_->activeScanner_->cancelRequested.store(true);
        }
        s_->scanThread_.join();
    }
}

void MainWindow::setInitialPath(const std::string& path) {
    initialPath_ = path;
}

void MainWindow::requestScan(const std::string& path) {
    if (s_->scanning_.load()) return;
    s_->pendingScanPath_ = path;
}

void MainWindow::cancelScan() {
    if (s_->activeScanner_) {
        s_->activeScanner_->cancelRequested.store(true);
    }
}

void MainWindow::navigateTo(FsNode* node) {
    if (!node) return;

    // Don't re-navigate to same node
    if (node == s_->currentNode_) return;

    // Truncate forward history
    if (s_->navHistoryPos_ + 1 < (int)s_->navHistory_.size()) {
        s_->navHistory_.resize(s_->navHistoryPos_ + 1);
    }
    // Save current node to history before navigating
    if (s_->currentNode_) {
        s_->navHistory_.push_back(s_->currentNode_);
        s_->navHistoryPos_ = (int)s_->navHistory_.size() - 1;
    }

    s_->currentNode_ = node;

    // Update UI panels
    DirTreePanel::instance().selectNode(node);
    if (node->isDir()) {
        FileListPanel::instance().showDirectory(node);

        // Auto-expand the directory so its contents are visible in the 3D view
        if (s_->visualizationReady_ && !DirTreePanel::instance().isEntryExpanded(node)) {
            CollapseExpand::instance().execute(node, ColExpAction::Expand);
        }
    }

    // Point camera at the node
    if (s_->visualizationReady_) {
        Camera::instance().lookAt(node);
    }
}

void MainWindow::navigateBack() {
    if (s_->navHistoryPos_ < 0 || s_->navHistory_.empty()) return;

    FsNode* prevNode = s_->navHistory_[s_->navHistoryPos_];
    s_->navHistoryPos_--;

    s_->currentNode_ = prevNode;
    DirTreePanel::instance().selectNode(prevNode);
    if (prevNode->isDir()) {
        FileListPanel::instance().showDirectory(prevNode);
    }
    if (s_->visualizationReady_) {
        Camera::instance().lookAt(prevNode);
    }
}

void MainWindow::navigateToRoot() {
    FsNode* rootDir = FsTree::instance().rootDir();
    if (rootDir) {
        navigateTo(rootDir);
    }
}

void MainWindow::navigateUp() {
    if (!s_->currentNode_ || !s_->currentNode_->parent) return;
    FsNode* parent = s_->currentNode_->parent;
    // Don't navigate to the metanode
    if (parent->isMetanode()) return;
    navigateTo(parent);
}

void MainWindow::navigateToFirstChild() {
    if (!s_->currentNode_ || s_->currentNode_->children.empty()) return;
    navigateTo(s_->currentNode_->children[0].get());
}

void MainWindow::navigateToNextSibling() {
    if (!s_->currentNode_ || !s_->currentNode_->parent) return;
    auto& siblings = s_->currentNode_->parent->children;
    for (size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i].get() == s_->currentNode_) {
            if (i + 1 < siblings.size()) {
                navigateTo(siblings[i + 1].get());
            }
            return;
        }
    }
}

void MainWindow::navigateToPrevSibling() {
    if (!s_->currentNode_ || !s_->currentNode_->parent) return;
    auto& siblings = s_->currentNode_->parent->children;
    for (size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i].get() == s_->currentNode_) {
            if (i > 0) {
                navigateTo(siblings[i - 1].get());
            }
            return;
        }
    }
}

void MainWindow::toggleExpandCurrent() {
    if (!s_->currentNode_ || !s_->currentNode_->isDir() || !s_->visualizationReady_) return;
    if (DirTreePanel::instance().isEntryExpanded(s_->currentNode_)) {
        CollapseExpand::instance().execute(s_->currentNode_, ColExpAction::CollapseRecursive);
    } else {
        CollapseExpand::instance().execute(s_->currentNode_, ColExpAction::Expand);
    }
}

void MainWindow::setMode(FsvMode mode) {
    if (mode == s_->currentMode_ && s_->visualizationReady_) return;
    s_->currentMode_ = mode;

    if (FsTree::instance().rootDir()) {
        initVisualization();
    }
}

void MainWindow::setColorMode(ColorMode mode) {
    if (mode == s_->currentColorMode_) return;
    s_->currentColorMode_ = mode;
    ColorSystem::instance().setMode(mode);

    FsNode* root = FsTree::instance().root();
    if (root) {
        ColorSystem::instance().assignRecursive(root);
        GeometryManager::instance().queueUncachedDraw();
    }
}

void MainWindow::setShowHidden(bool show) {
    if (show == s_->showHidden_) return;
    s_->showHidden_ = show;
    // Re-scan the current root so the change takes effect immediately.
    if (!s_->currentRootPath_.empty()) {
        requestScan(s_->currentRootPath_);
    }
}

namespace {
std::string toLowerStr(const std::string& s) {
    std::string out(s.size(), '\0');
    std::transform(s.begin(), s.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}
} // namespace

void MainWindow::applySearch(FsNode* node, const std::string& lowerQuery, bool hasWildcard) {
    for (auto& childPtr : node->children) {
        FsNode* c = childPtr.get();
        bool match = false;
        if (!lowerQuery.empty()) {
            std::string name = toLowerStr(c->name);
            match = hasWildcard ? PlatformUtils::wildcardMatch(lowerQuery, name)
                                : (name.find(lowerQuery) != std::string::npos);
        }
        c->searchMatch = match;
        if (match) ++s_->searchMatchCount_;
        applySearch(c, lowerQuery, hasWildcard);
    }
}

void MainWindow::setSearchQuery(const std::string& query) {
    s_->searchQuery_ = query;
    refreshSearch();
}

void MainWindow::refreshSearch() {
    s_->searchMatchCount_ = 0;
    FsNode* root = FsTree::instance().root();
    if (!root) return;
    std::string lower = toLowerStr(s_->searchQuery_);
    bool hasWild = lower.find('*') != std::string::npos ||
                   lower.find('?') != std::string::npos;
    applySearch(root, lower, hasWild);
}

void MainWindow::captureViewState() {
    s_->restoreCurrentPath_ = s_->currentNode_ ? s_->currentNode_->absName() : "";
    FsNode* fld = FileListPanel::instance().currentDir();
    s_->restoreFileListPath_ = fld ? fld->absName() : "";
    s_->restoreExpandedPaths_ = DirTreePanel::instance().expandedPaths();
    Camera::instance().saveState();
    s_->viewRestorePending_ = true;
}

void MainWindow::rescanCurrentRoot() {
    if (!s_->currentRootPath_.empty()) {
        s_->fsChangePending_ = false;
        captureViewState();
        requestScan(s_->currentRootPath_);
    }
}

void MainWindow::pollWatcher() {
    uint64_t c = s_->watcher_.changeCount();
    if (c != s_->lastSeenChange_) {
        s_->lastSeenChange_ = c;
        s_->fsChangePending_ = true;
        s_->lastChangeTime_ = PlatformUtils::getTime();
    }

    // Auto-refresh: rescan once changes have settled (debounce) and no scan
    // is already running. Re-scanning resets navigation, so this is opt-in.
    if (s_->autoRefresh_ && s_->fsChangePending_ && !s_->scanning_.load()) {
        if (PlatformUtils::getTime() - s_->lastChangeTime_ > 0.8) {
            rescanCurrentRoot();
        }
    }
}

void MainWindow::initVisualization(bool resetCamera) {
    FsNode* rootDir = FsTree::instance().rootDir();
    if (!rootDir) return;

    // Ensure root directory is expanded so geometry layouts show children
    DirTreePanel::instance().setEntryExpanded(rootDir, true);

    // Initialize geometry for current mode
    GeometryManager::instance().init(s_->currentMode_);

    if (resetCamera) {
        // Initialize camera for current mode and point it at the root.
        Camera::instance().init(s_->currentMode_, true);
        Camera::instance().lookAt(rootDir);
    }

    // Re-apply any active search to the freshly-built tree.
    if (!s_->searchQuery_.empty()) {
        refreshSearch();
    }

    // (Re)start the filesystem watcher on the new root.
    s_->fsChangePending_ = false;
    s_->lastSeenChange_ = 0;
    s_->watcher_.start(s_->currentRootPath_);

    s_->visualizationReady_ = true;
}

void MainWindow::scanThreadFunc(const std::string& path) {
    try {
        auto tree = s_->activeScanner_->scan(path, [this](const std::string& dir, const ScanStats& stats) {
            std::lock_guard<std::mutex> lock(s_->progressMutex_);
            s_->progressDir_ = dir;
            s_->progressFiles_ = stats.nodeCounts[NODE_REGFILE];
            s_->progressDirs_ = stats.nodeCounts[NODE_DIRECTORY];
        });

        if (s_->activeScanner_->cancelRequested.load()) {
            // Scan was cancelled - discard result
            s_->scanResult_.reset();
        } else {
            s_->scanResult_ = std::move(tree);
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(s_->progressMutex_);
        s_->scanError_ = e.what();
    } catch (...) {
        std::lock_guard<std::mutex> lock(s_->progressMutex_);
        s_->scanError_ = "Unknown error during scan";
    }

    s_->scanDone_.store(true);
}

void MainWindow::finishScan() {
    if (s_->scanThread_.joinable()) {
        s_->scanThread_.join();
    }

    s_->scanning_.store(false);
    s_->scanDone_.store(false);
    s_->activeScanner_.reset();

    std::string error;
    {
        std::lock_guard<std::mutex> lock(s_->progressMutex_);
        error = std::move(s_->scanError_);
        s_->scanError_.clear();
    }

    if (!error.empty()) {
        StatusBar::instance().setMessage("Scan error: " + error, "");
        s_->scanResult_.reset();
        return;
    }

    if (!s_->scanResult_) {
        StatusBar::instance().setMessage("Scan cancelled", "");
        return;
    }

    // Clear UI state before replacing tree
    PulseEffect::instance().reset();
    DirTreePanel::instance().selectNode(nullptr);
    DirTreePanel::instance().clearExpanded();
    FileListPanel::instance().showDirectory(nullptr);
    s_->currentNode_ = nullptr;
    s_->visualizationReady_ = false;
    GeometryManager::instance().freeAll();

    FsTree::instance().setRoot(std::move(s_->scanResult_));
    FsTree::instance().setupTree();
    ColorSystem::instance().init();
    ColorSystem::instance().setMode(s_->currentColorMode_);
    FsNode* root = FsTree::instance().root();
    if (root) {
        ColorSystem::instance().assignRecursive(root);
    }

    // Clear navigation history
    s_->navHistory_.clear();
    s_->navHistoryPos_ = -1;

    FsNode* rootDir = FsTree::instance().rootDir();
    if (!rootDir) {
        return;
    }

    if (s_->viewRestorePending_) {
        // Restore the prior navigation/expansion/camera by re-resolving paths
        // against the freshly-rebuilt tree. Anything that no longer exists
        // (e.g. a deleted node) gracefully falls back to the root.
        s_->viewRestorePending_ = false;

        DirTreePanel::instance().restoreExpanded(s_->restoreExpandedPaths_);

        FsNode* cur = FsTree::instance().nodeByPath(s_->restoreCurrentPath_);
        if (!cur) cur = rootDir;
        s_->currentNode_ = cur;
        DirTreePanel::instance().selectNode(cur);

        FsNode* fld = FsTree::instance().nodeByPath(s_->restoreFileListPath_);
        FileListPanel::instance().showDirectory(fld ? fld : cur);

        int totalFiles = rootDir->subtree.counts[NODE_REGFILE];
        int totalDirs = rootDir->subtree.counts[NODE_DIRECTORY];
        char buf[256];
        snprintf(buf, sizeof(buf), "%d directories, %d files", totalDirs, totalFiles);
        StatusBar::instance().setMessage(rootDir->name, buf);

        initVisualization(/*resetCamera=*/false);
        Camera::instance().restoreState();
    } else {
        s_->currentNode_ = rootDir;
        DirTreePanel::instance().selectNode(rootDir);
        FileListPanel::instance().showDirectory(rootDir);

        int totalFiles = rootDir->subtree.counts[NODE_REGFILE];
        int totalDirs = rootDir->subtree.counts[NODE_DIRECTORY];
        char buf[256];
        snprintf(buf, sizeof(buf), "%d directories, %d files", totalDirs, totalFiles);
        StatusBar::instance().setMessage(rootDir->name, buf);

        // Initialize visualization
        initVisualization();
    }
}

void MainWindow::drawProgressOverlay() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 center = viewport->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(450.0f, 0.0f));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 16.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.18f, 0.95f));

    if (ImGui::Begin("##ScanProgress", nullptr, flags)) {
        ImGui::Text("Scanning filesystem...");
        ImGui::Spacing();

        float time = (float)ImGui::GetTime();
        float progress = time - (float)(int)time;
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), "");

        ImGui::Spacing();

        std::string dir;
        int files, dirs;
        {
            std::lock_guard<std::mutex> lock(s_->progressMutex_);
            dir = s_->progressDir_;
            files = s_->progressFiles_;
            dirs = s_->progressDirs_;
        }

        if (dir.size() > 55) {
            dir = "..." + dir.substr(dir.size() - 52);
        }

        ImGui::Text("Directories: %d  |  Files: %d", dirs, files);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", dir.c_str());

        ImGui::Spacing();
        float buttonWidth = 120.0f;
        float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - buttonWidth) * 0.5f);
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0.0f))) {
            cancelScan();
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void MainWindow::serviceScene() {
    // Check if background scan just completed
    if (s_->scanDone_.load()) {
        finishScan();
    }

    // Detect filesystem changes (and auto-refresh if enabled).
    pollWatcher();

    // Start a deferred scan on the background thread
    if (!s_->pendingScanPath_.empty() && !s_->scanning_.load()) {
        std::string path = std::move(s_->pendingScanPath_);
        s_->pendingScanPath_.clear();
        s_->currentRootPath_ = path;

        DirTreePanel::instance().selectNode(nullptr);
        DirTreePanel::instance().clearExpanded();
        FileListPanel::instance().showDirectory(nullptr);

        {
            std::lock_guard<std::mutex> lock(s_->progressMutex_);
            s_->progressDir_ = path;
            s_->progressFiles_ = 0;
            s_->progressDirs_ = 0;
            s_->scanError_.clear();
        }

        s_->scanning_.store(true);
        s_->scanDone_.store(false);

        if (s_->scanThread_.joinable()) {
            s_->scanThread_.join();
        }
        s_->activeScanner_ = std::make_shared<FsScanner>();
        s_->activeScanner_->showHidden = s_->showHidden_;
        s_->scanThread_ = std::thread(&MainWindow::scanThreadFunc, this, path);
    }
}

void MainWindow::draw() {
    SceneManager& sm = SceneManager::instance();
    Scene* focused = sm.focused();

    // A cross-pane move asked every pane to refresh; do it before servicing.
    if (rescanAllPending_) {
        rescanAllPending_ = false;
        for (int i = 0; i < sm.sceneCount(); ++i) {
            sm.activate(sm.sceneAt(i));
            rescanCurrentRoot();
        }
        sm.activate(focused);
    }

    // Initial scan on first frame (focused/first pane).
    if (firstFrame_ && !initialPath_.empty()) {
        sm.activate(focused);
        if (!s_->scanning_.load()) {
            requestScan(initialPath_);
        }
    }

    // Service every pane's background work (scan completion, deferred scans,
    // live-watch), then return to the focused pane for the UI below.
    for (int i = 0; i < sm.sceneCount(); ++i) {
        sm.activate(sm.sceneAt(i));
        serviceScene();
    }
    sm.activate(focused);

    // Create a fullscreen dockspace
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    // Toolbar is two rows tall (controls + breadcrumb/address bar).
    float toolbarHeight = ImGui::GetFrameHeightWithSpacing() * 2.0f + 8.0f;
    float statusHeight = ImGui::GetFrameHeightWithSpacing();

    ImVec2 dockPos(viewport->WorkPos.x, viewport->WorkPos.y + toolbarHeight);
    ImVec2 dockSize(viewport->WorkSize.x, viewport->WorkSize.y - toolbarHeight - statusHeight);

    ImGui::SetNextWindowPos(dockPos);
    ImGui::SetNextWindowSize(dockSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    if (firstFrame_) {
        setupDockspace();
        firstFrame_ = false;
    }

    ImGui::End();

    MenuBar::instance().draw();
    Toolbar::instance().draw();
    DirTreePanel::instance().draw();
    FileListPanel::instance().draw();
    PreviewPanel::instance().draw();
    drawViewportGrid();
    StatusBar::instance().draw();
    Dialogs::instance().draw();

    if (s_->scanning_.load()) {
        drawProgressOverlay();
    }
}

void MainWindow::drawViewportGrid() {
    SceneManager& sm = SceneManager::instance();
    Scene* focused = sm.focused();
    int n = sm.sceneCount();
    if (n < 1) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport");

    ImVec2 avail = ImGui::GetContentRegionAvail();
    int cols = (n <= 1) ? 1 : 2;
    int rows = (n <= 2) ? 1 : 2;
    float cw = avail.x / cols;
    float ch = avail.y / rows;

    Scene* newFocus = focused;
    for (int i = 0; i < n; ++i) {
        Scene* sc = sm.sceneAt(i);
        int col = i % cols;
        if (col > 0) ImGui::SameLine();

        sm.activate(sc);
        char id[32];
        std::snprintf(id, sizeof(id), "pane%d", i);
        bool clicked = sc->viewport.drawPane(id, ImVec2(cw, ch), sc == focused);
        if (clicked) newFocus = sc;
    }

    ImGui::End();
    ImGui::PopStyleVar();

    if (newFocus != focused) {
        // Settle the outgoing pane's camera so its in-flight morphs don't run
        // under the newly-focused pane during the next Animation::tick.
        sm.activate(focused);
        Camera::instance().panFinish();
        sm.setFocused(newFocus);
    }
    sm.activate(newFocus);
}

void MainWindow::newPane() {
    SceneManager& sm = SceneManager::instance();
    std::string root = s_->currentRootPath_;  // focused pane's current root
    Scene* sc = sm.addScene();
    if (!sc) return;  // at capacity
    sm.setFocused(sc);
    sm.activate(sc);
    if (!root.empty()) {
        requestScan(root);
    }
}

void MainWindow::closeFocusedPane() {
    SceneManager& sm = SceneManager::instance();
    if (sm.sceneCount() <= 1) return;  // keep at least one pane
    sm.closeFocused();
}

void MainWindow::moveAcrossPanes(const std::string& srcPath,
                                 const std::string& destDirPath) {
    FileOps::Result r = FileOps::moveInto(srcPath, destDirPath);
    if (r.ok) {
        // Refresh every pane (source loses the entry, dest gains it). Deferred
        // to the next frame so we don't re-scan mid-render. Each pane restores
        // its own view by path.
        rescanAllPending_ = true;
        StatusBar::instance().setMessage("Moved to " + destDirPath, "");
    } else {
        StatusBar::instance().setMessage("Move failed: " + r.error, "");
    }
}

void MainWindow::setupDockspace() {
    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

    ImGuiID leftId, rightId;
    ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.25f, &leftId, &rightId);

    ImGuiID leftTopId, leftBottomId;
    ImGui::DockBuilderSplitNode(leftId, ImGuiDir_Up, 0.5f, &leftTopId, &leftBottomId);

    ImGui::DockBuilderDockWindow("Directory Tree", leftTopId);
    ImGui::DockBuilderDockWindow("File List", leftBottomId);
    ImGui::DockBuilderDockWindow("Preview", leftBottomId);
    ImGui::DockBuilderDockWindow("Viewport", rightId);

    ImGui::DockBuilderFinish(dockspaceId);
    dockspaceInitialized_ = true;
}

} // namespace fsvng
