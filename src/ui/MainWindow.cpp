#include "ui/MainWindow.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <iostream>

#include "ui/MenuBar.h"
#include "ui/Toolbar.h"
#include "ui/DirTreePanel.h"
#include "ui/FileListPanel.h"
#include "ui/ViewportPanel.h"
#include "ui/StatusBar.h"
#include "ui/Dialogs.h"
#include "core/FsTree.h"
#include "core/FsNode.h"
#include "core/FsScanner.h"
#include "color/ColorSystem.h"
#include "renderer/Renderer.h"
#include "geometry/GeometryManager.h"
#include "geometry/CollapseExpand.h"
#include "camera/Camera.h"
#include "ui/PulseEffect.h"

namespace fsvng {

MainWindow& MainWindow::instance() {
    static MainWindow s;
    return s;
}

MainWindow::~MainWindow() {
    if (scanThread_.joinable()) {
        if (activeScanner_) {
            activeScanner_->cancelRequested.store(true);
        }
        scanThread_.join();
    }
}

void MainWindow::setInitialPath(const std::string& path) {
    initialPath_ = path;
}

void MainWindow::requestScan(const std::string& path) {
    if (scanning_.load()) return;
    pendingScanPath_ = path;
}

void MainWindow::cancelScan() {
    if (activeScanner_) {
        activeScanner_->cancelRequested.store(true);
    }
}

void MainWindow::navigateTo(FsNode* node) {
    if (!node) return;

    // Don't re-navigate to same node
    if (node == currentNode_) return;

    // Truncate forward history
    if (navHistoryPos_ + 1 < (int)navHistory_.size()) {
        navHistory_.resize(navHistoryPos_ + 1);
    }
    // Save current node to history before navigating
    if (currentNode_) {
        navHistory_.push_back(currentNode_);
        navHistoryPos_ = (int)navHistory_.size() - 1;
    }

    currentNode_ = node;

    // Update UI panels
    DirTreePanel::instance().selectNode(node);
    if (node->isDir()) {
        FileListPanel::instance().showDirectory(node);

        // Auto-expand the directory so its contents are visible in the 3D view
        if (visualizationReady_ && !DirTreePanel::instance().isEntryExpanded(node)) {
            CollapseExpand::instance().execute(node, ColExpAction::Expand);
        }
    }

    // Point camera at the node
    if (visualizationReady_) {
        Camera::instance().lookAt(node);
    }
}

void MainWindow::navigateBack() {
    if (navHistoryPos_ < 0 || navHistory_.empty()) return;

    FsNode* prevNode = navHistory_[navHistoryPos_];
    navHistoryPos_--;

    currentNode_ = prevNode;
    DirTreePanel::instance().selectNode(prevNode);
    if (prevNode->isDir()) {
        FileListPanel::instance().showDirectory(prevNode);
    }
    if (visualizationReady_) {
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
    if (!currentNode_ || !currentNode_->parent) return;
    FsNode* parent = currentNode_->parent;
    // Don't navigate to the metanode
    if (parent->isMetanode()) return;
    navigateTo(parent);
}

void MainWindow::navigateToFirstChild() {
    if (!currentNode_ || currentNode_->children.empty()) return;
    navigateTo(currentNode_->children[0].get());
}

void MainWindow::navigateToNextSibling() {
    if (!currentNode_ || !currentNode_->parent) return;
    auto& siblings = currentNode_->parent->children;
    for (size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i].get() == currentNode_) {
            if (i + 1 < siblings.size()) {
                navigateTo(siblings[i + 1].get());
            }
            return;
        }
    }
}

void MainWindow::navigateToPrevSibling() {
    if (!currentNode_ || !currentNode_->parent) return;
    auto& siblings = currentNode_->parent->children;
    for (size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i].get() == currentNode_) {
            if (i > 0) {
                navigateTo(siblings[i - 1].get());
            }
            return;
        }
    }
}

void MainWindow::toggleExpandCurrent() {
    if (!currentNode_ || !currentNode_->isDir() || !visualizationReady_) return;
    if (DirTreePanel::instance().isEntryExpanded(currentNode_)) {
        CollapseExpand::instance().execute(currentNode_, ColExpAction::CollapseRecursive);
    } else {
        CollapseExpand::instance().execute(currentNode_, ColExpAction::Expand);
    }
}

void MainWindow::setMode(FsvMode mode) {
    if (mode == currentMode_ && visualizationReady_) return;
    currentMode_ = mode;

    if (FsTree::instance().rootDir()) {
        initVisualization();
    }
}

void MainWindow::setColorMode(ColorMode mode) {
    if (mode == currentColorMode_) return;
    currentColorMode_ = mode;
    ColorSystem::instance().setMode(mode);

    FsNode* root = FsTree::instance().root();
    if (root) {
        ColorSystem::instance().assignRecursive(root);
        GeometryManager::instance().queueUncachedDraw();
    }
}

void MainWindow::initVisualization() {
    FsNode* rootDir = FsTree::instance().rootDir();
    if (!rootDir) return;

    // Ensure root directory is expanded so geometry layouts show children
    DirTreePanel::instance().setEntryExpanded(rootDir, true);

    // Initialize geometry for current mode
    GeometryManager::instance().init(currentMode_);

    // Initialize camera for current mode
    Camera::instance().init(currentMode_, true);

    // Point camera at root
    Camera::instance().lookAt(rootDir);

    visualizationReady_ = true;
}

void MainWindow::scanThreadFunc(const std::string& path) {
    try {
        auto tree = activeScanner_->scan(path, [this](const std::string& dir, const ScanStats& stats) {
            std::lock_guard<std::mutex> lock(progressMutex_);
            progressDir_ = dir;
            progressFiles_ = stats.nodeCounts[NODE_REGFILE];
            progressDirs_ = stats.nodeCounts[NODE_DIRECTORY];
        });

        if (activeScanner_->cancelRequested.load()) {
            // Scan was cancelled - discard result
            scanResult_.reset();
        } else {
            scanResult_ = std::move(tree);
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(progressMutex_);
        scanError_ = e.what();
    } catch (...) {
        std::lock_guard<std::mutex> lock(progressMutex_);
        scanError_ = "Unknown error during scan";
    }

    scanDone_.store(true);
}

void MainWindow::finishScan() {
    if (scanThread_.joinable()) {
        scanThread_.join();
    }

    scanning_.store(false);
    scanDone_.store(false);
    activeScanner_.reset();

    std::string error;
    {
        std::lock_guard<std::mutex> lock(progressMutex_);
        error = std::move(scanError_);
        scanError_.clear();
    }

    if (!error.empty()) {
        StatusBar::instance().setMessage("Scan error: " + error, "");
        scanResult_.reset();
        return;
    }

    if (!scanResult_) {
        StatusBar::instance().setMessage("Scan cancelled", "");
        return;
    }

    // Clear UI state before replacing tree
    PulseEffect::instance().reset();
    DirTreePanel::instance().selectNode(nullptr);
    DirTreePanel::instance().clearExpanded();
    FileListPanel::instance().showDirectory(nullptr);
    currentNode_ = nullptr;
    visualizationReady_ = false;
    GeometryManager::instance().freeAll();

    FsTree::instance().setRoot(std::move(scanResult_));
    FsTree::instance().setupTree();
    ColorSystem::instance().init();
    ColorSystem::instance().setMode(currentColorMode_);
    FsNode* root = FsTree::instance().root();
    if (root) {
        ColorSystem::instance().assignRecursive(root);
    }

    // Clear navigation history
    navHistory_.clear();
    navHistoryPos_ = -1;

    FsNode* rootDir = FsTree::instance().rootDir();
    if (rootDir) {
        currentNode_ = rootDir;
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
            std::lock_guard<std::mutex> lock(progressMutex_);
            dir = progressDir_;
            files = progressFiles_;
            dirs = progressDirs_;
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

void MainWindow::draw() {
    // Check if background scan just completed
    if (scanDone_.load()) {
        finishScan();
    }

    // Start a deferred scan on the background thread
    if (!pendingScanPath_.empty() && !scanning_.load()) {
        std::string path = std::move(pendingScanPath_);
        pendingScanPath_.clear();

        DirTreePanel::instance().selectNode(nullptr);
        DirTreePanel::instance().clearExpanded();
        FileListPanel::instance().showDirectory(nullptr);

        {
            std::lock_guard<std::mutex> lock(progressMutex_);
            progressDir_ = path;
            progressFiles_ = 0;
            progressDirs_ = 0;
            scanError_.clear();
        }

        scanning_.store(true);
        scanDone_.store(false);

        if (scanThread_.joinable()) {
            scanThread_.join();
        }
        activeScanner_ = std::make_shared<FsScanner>();
        scanThread_ = std::thread(&MainWindow::scanThreadFunc, this, path);
    }

    // Do initial scan on first frame
    if (firstFrame_) {
        if (!initialPath_.empty() && !scanning_.load()) {
            requestScan(initialPath_);
        }
    }

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

    float toolbarHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f;
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
    ViewportPanel::instance().draw();
    StatusBar::instance().draw();
    Dialogs::instance().draw();

    if (scanning_.load()) {
        drawProgressOverlay();
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
    ImGui::DockBuilderDockWindow("Viewport", rightId);

    ImGui::DockBuilderFinish(dockspaceId);
    dockspaceInitialized_ = true;
}

} // namespace fsvng
