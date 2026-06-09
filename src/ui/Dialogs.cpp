#include "ui/Dialogs.h"

#include <imgui.h>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "core/FsNode.h"
#include "core/FsTree.h"
#include "core/Types.h"
#include "core/FileOps.h"
#include "ui/MainWindow.h"
#include "ui/DirTreePanel.h"
#include "ui/FileListPanel.h"
#include "ui/StatusBar.h"
#include "geometry/CollapseExpand.h"
#include "camera/Camera.h"
#include "app/Config.h"

namespace fsvng {

// ============================================================================
// openInSystem - launch the platform's default handler for a path
// ============================================================================
static void openInSystem(const std::string& path, bool isDir) {
#ifdef _WIN32
    ShellExecuteA(nullptr, isDir ? "explore" : "open", path.c_str(),
                  nullptr, nullptr, SW_SHOWNORMAL);
#else
    // POSIX: hand off to the desktop opener. fork/exec (not system()) avoids
    // shell-injection from paths with spaces or special characters. We
    // double-fork so the launcher is reparented to init and reaped there,
    // leaving no zombie behind for our process to wait on.
    (void)isDir;
#if defined(__APPLE__)
    const char* opener = "open";
#else
    const char* opener = "xdg-open";
#endif
    pid_t pid = fork();
    if (pid == 0) {
        pid_t inner = fork();
        if (inner == 0) {
            execlp(opener, opener, path.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        _exit(0);
    } else if (pid > 0) {
        waitpid(pid, nullptr, 0);
    }
#endif
}

// ============================================================================
// revealInFileManager - show a path in its containing folder
// ============================================================================
static void revealInFileManager(const std::string& path) {
#ifdef _WIN32
    // /select highlights the item within its parent folder.
    std::string args = "/select,\"" + path + "\"";
    ShellExecuteA(nullptr, "open", "explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
#else
    // Open the parent directory in the system file manager. (Selecting the
    // exact item would require the org.freedesktop.FileManager1 D-Bus call,
    // which not all managers serve; opening the container is universal.)
    std::filesystem::path p(path);
    std::filesystem::path parent = p.parent_path();
    openInSystem(parent.empty() ? std::string(".") : parent.string(), true);
#endif
}

// ============================================================================
// openTerminalAt - launch a terminal emulator with cwd set to a directory
// ============================================================================
static void openTerminalAt(const std::string& dir) {
#ifdef _WIN32
    // "open" on a directory with cmd.exe, starting in that directory.
    ShellExecuteA(nullptr, "open", "cmd.exe", nullptr, dir.c_str(), SW_SHOWNORMAL);
#elif defined(__APPLE__)
    // macOS: Terminal.app opens at the given directory when handed a folder.
    pid_t pid = fork();
    if (pid == 0) {
        pid_t inner = fork();
        if (inner == 0) {
            execlp("open", "open", "-a", "Terminal", dir.c_str(),
                   static_cast<char*>(nullptr));
            _exit(127);
        }
        _exit(0);
    } else if (pid > 0) {
        waitpid(pid, nullptr, 0);
    }
#else
    // Linux: chdir into the target, then try terminals in preference order.
    // Most inherit cwd; execlp returns only on failure, so we fall through.
    pid_t pid = fork();
    if (pid == 0) {
        pid_t inner = fork();
        if (inner == 0) {
            if (chdir(dir.c_str()) != 0) {
                _exit(127);
            }
            const char* envTerm = std::getenv("TERMINAL");
            if (envTerm && envTerm[0] != '\0') {
                execlp(envTerm, envTerm, static_cast<char*>(nullptr));
            }
            execlp("x-terminal-emulator", "x-terminal-emulator", static_cast<char*>(nullptr));
            execlp("gnome-terminal", "gnome-terminal", static_cast<char*>(nullptr));
            execlp("konsole", "konsole", static_cast<char*>(nullptr));
            execlp("xfce4-terminal", "xfce4-terminal", static_cast<char*>(nullptr));
            execlp("xterm", "xterm", static_cast<char*>(nullptr));
            _exit(127);
        }
        _exit(0);
    } else if (pid > 0) {
        waitpid(pid, nullptr, 0);
    }
#endif
}

Dialogs& Dialogs::instance() {
    static Dialogs s;
    return s;
}

void Dialogs::showChangeRoot() {
    showChangeRoot_ = true;
    // Initialize buffer with empty or current path
    std::memset(rootPathBuf_, 0, sizeof(rootPathBuf_));
}

void Dialogs::showSetDefaultPath() {
    showSetDefaultPath_ = true;
    std::memset(defaultPathBuf_, 0, sizeof(defaultPathBuf_));
    // Pre-fill with current default path
    const std::string& dp = Config::instance().defaultPath;
    if (!dp.empty()) {
        size_t len = dp.size();
        if (len >= sizeof(defaultPathBuf_)) len = sizeof(defaultPathBuf_) - 1;
        std::memcpy(defaultPathBuf_, dp.c_str(), len);
    }
}

void Dialogs::showColorConfig() {
    showColorConfig_ = true;
}

void Dialogs::showAbout() {
    showAbout_ = true;
}

void Dialogs::draw() {
    drawChangeRoot();
    drawSetDefaultPath();
    drawColorConfig();
    drawAbout();
    drawProperties();
    drawRename();
    drawNewFolder();
}

void Dialogs::drawChangeRoot() {
    if (!showChangeRoot_) return;

    ImGui::OpenPopup("Change Root");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(450.0f, 0.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Change Root", &showChangeRoot_,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter the new root directory path:");
        ImGui::Separator();

        ImGui::InputText("##RootPath", rootPathBuf_, sizeof(rootPathBuf_));

        ImGui::Spacing();

        if (ImGui::Button("OK", ImVec2(120.0f, 0.0f))) {
            std::string newRoot(rootPathBuf_);
            showChangeRoot_ = false;
            ImGui::CloseCurrentPopup();
            if (!newRoot.empty()) {
                MainWindow::instance().requestScan(newRoot);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
            showChangeRoot_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void Dialogs::drawSetDefaultPath() {
    if (!showSetDefaultPath_) return;

    ImGui::OpenPopup("Set Default Path");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(450.0f, 0.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Set Default Path", &showSetDefaultPath_,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Set the default directory to scan on startup:");
        ImGui::Separator();

        ImGui::InputText("##DefaultPath", defaultPathBuf_, sizeof(defaultPathBuf_));

        ImGui::Spacing();

        if (ImGui::Button("OK", ImVec2(120.0f, 0.0f))) {
            std::string newPath(defaultPathBuf_);
            showSetDefaultPath_ = false;
            ImGui::CloseCurrentPopup();
            if (!newPath.empty()) {
                Config::instance().defaultPath = newPath;
                Config::instance().save();
                MainWindow::instance().requestScan(newPath);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear", ImVec2(120.0f, 0.0f))) {
            Config::instance().defaultPath.clear();
            Config::instance().save();
            showSetDefaultPath_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
            showSetDefaultPath_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void Dialogs::drawColorConfig() {
    if (!showColorConfig_) return;

    ImGui::SetNextWindowSize(ImVec2(400.0f, 500.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Color Configuration", &showColorConfig_)) {
        ImGui::Text("Node Type Colors");
        ImGui::Separator();

        // Display a color editor for each node type
        for (int i = 0; i < NUM_NODE_TYPES; i++) {
            // Placeholder colors - will be connected to ColorSystem in a later phase
            static float colors[NUM_NODE_TYPES][3] = {
                {0.5f, 0.5f, 0.5f},  // Metanode
                {0.3f, 0.6f, 1.0f},  // Directory
                {0.2f, 0.8f, 0.2f},  // Regular file
                {0.8f, 0.8f, 0.2f},  // Symlink
                {0.8f, 0.4f, 0.0f},  // FIFO
                {0.8f, 0.0f, 0.8f},  // Socket
                {1.0f, 0.5f, 0.5f},  // Char device
                {0.6f, 0.3f, 0.1f},  // Block device
                {0.4f, 0.4f, 0.4f},  // Unknown
            };
            ImGui::ColorEdit3(nodeTypeNames[i], colors[i]);
        }

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("Apply", ImVec2(100.0f, 0.0f))) {
            // TODO: Apply color changes to ColorSystem
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(100.0f, 0.0f))) {
            showColorConfig_ = false;
        }
    }
    ImGui::End();
}

void Dialogs::drawAbout() {
    if (!showAbout_) return;

    ImGui::OpenPopup("About fsvng");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("About fsvng", &showAbout_,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("fsvng - 3D File System Visualizer");
        ImGui::Spacing();
        ImGui::Text("A modern C++17 reimplementation of fsv.");
        ImGui::Text("Visualize your filesystem in 3D with");
        ImGui::Text("MapV and TreeV modes.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Based on fsv by Daniel Richard G.");
        ImGui::Spacing();

        if (ImGui::Button("OK", ImVec2(120.0f, 0.0f))) {
            showAbout_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void Dialogs::drawContextMenuPopup(const char* popupId, FsNode* node) {
    if (ImGui::BeginPopup(popupId)) {
        if (node) {
            ImGui::TextDisabled("%s", node->name.c_str());
            ImGui::Separator();

            if (node->isDir()) {
                bool isExpanded = DirTreePanel::instance().isEntryExpanded(node);
                if (!isExpanded && ImGui::MenuItem("Expand")) {
                    CollapseExpand::instance().execute(node, ColExpAction::Expand);
                }
                if (isExpanded && ImGui::MenuItem("Collapse")) {
                    CollapseExpand::instance().execute(node, ColExpAction::CollapseRecursive);
                }
                if (ImGui::MenuItem("Scan as Root")) {
                    MainWindow::instance().requestScan(node->absName());
                }
                ImGui::Separator();
            }

            if (ImGui::MenuItem("Look At")) {
                MainWindow::instance().navigateTo(node);
            }

            if (ImGui::MenuItem("Open")) {
                openInSystem(node->absName(), node->isDir());
            }

            if (ImGui::MenuItem("Reveal in File Manager")) {
                revealInFileManager(node->absName());
            }

            if (ImGui::MenuItem("Open Terminal Here")) {
                // For a file, open a terminal in its containing directory.
                std::string dir = node->isDir()
                    ? node->absName()
                    : std::filesystem::path(node->absName()).parent_path().string();
                openTerminalAt(dir.empty() ? std::string(".") : dir);
            }

            if (ImGui::MenuItem("Copy Path")) {
                std::string path = node->absName();
                ImGui::SetClipboardText(path.c_str());
            }

            ImGui::Separator();

            // ---- Mutating operations --------------------------------------
            // The scanned root is excluded from rename/trash so the watched
            // root path (and rescan target) stays valid.
            bool isRoot = (node == FsTree::instance().rootDir());

            if (node->isDir() && ImGui::MenuItem("New Folder...")) {
                newFolderParent_ = node;
                newFolderBuf_[0] = '\0';
                opError_[0] = '\0';
                showNewFolder_ = true;
            }

            if (ImGui::MenuItem("Rename...", nullptr, false, !isRoot)) {
                renameNode_ = node;
                std::snprintf(renameBuf_, sizeof(renameBuf_), "%s", node->name.c_str());
                opError_[0] = '\0';
                showRename_ = true;
            }

            // Batch trash when this node is part of a multi-selection.
            int selCount = FileListPanel::instance().selectionCount();
            bool batch = (selCount > 1 &&
                          FileListPanel::instance().isPathSelected(node->absName()));
            bool canTrash = FileOps::trashAvailable() && !isRoot;
            char trashLabel[64];
            if (batch) {
                std::snprintf(trashLabel, sizeof(trashLabel),
                              "Move %d Items to Trash", selCount);
            } else {
                std::snprintf(trashLabel, sizeof(trashLabel), "Move to Trash");
            }
            if (ImGui::MenuItem(trashLabel, nullptr, false, canTrash)) {
                if (batch) {
                    MainWindow::instance().trashSelection();
                } else {
                    FileOps::Result r = FileOps::moveToTrash(node->absName());
                    if (r.ok) {
                        MainWindow::instance().rescanCurrentRoot();
                    } else {
                        StatusBar::instance().setMessage("Trash failed: " + r.error, "");
                    }
                }
            }
            if (!canTrash && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Install 'gio' (glib2) to enable Trash");
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Properties")) {
                showProperties_ = true;
                propertiesNode_ = node;
            }
        }

        ImGui::EndPopup();
    }
}

void Dialogs::drawRename() {
    if (!showRename_) return;
    ImGui::OpenPopup("Rename");
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Rename", &showRename_,
                               ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted("New name:");
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool submit = ImGui::InputText("##renamebuf", renameBuf_, sizeof(renameBuf_),
                                       ImGuiInputTextFlags_EnterReturnsTrue);
        if (opError_[0] != '\0') {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", opError_);
        }
        if (ImGui::Button("Rename") || submit) {
            FileOps::Result r{false, "no target"};
            if (renameNode_) {
                r = FileOps::rename(renameNode_->absName(), renameBuf_);
            }
            if (r.ok) {
                showRename_ = false;
                renameNode_ = nullptr;
                MainWindow::instance().rescanCurrentRoot();
                ImGui::CloseCurrentPopup();
            } else {
                std::snprintf(opError_, sizeof(opError_), "%s", r.error.c_str());
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            showRename_ = false;
            renameNode_ = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Dialogs::drawNewFolder() {
    if (!showNewFolder_) return;
    ImGui::OpenPopup("New Folder");
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("New Folder", &showNewFolder_,
                               ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted("Folder name:");
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool submit = ImGui::InputText("##newfolderbuf", newFolderBuf_, sizeof(newFolderBuf_),
                                       ImGuiInputTextFlags_EnterReturnsTrue);
        if (opError_[0] != '\0') {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", opError_);
        }
        if (ImGui::Button("Create") || submit) {
            FileOps::Result r{false, "no target"};
            if (newFolderParent_) {
                r = FileOps::createFolder(newFolderParent_->absName(), newFolderBuf_);
            }
            if (r.ok) {
                showNewFolder_ = false;
                newFolderParent_ = nullptr;
                MainWindow::instance().rescanCurrentRoot();
                ImGui::CloseCurrentPopup();
            } else {
                std::snprintf(opError_, sizeof(opError_), "%s", r.error.c_str());
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            showNewFolder_ = false;
            newFolderParent_ = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Dialogs::drawProperties() {
    if (!showProperties_ || !propertiesNode_) return;

    ImGui::SetNextWindowSize(ImVec2(350.0f, 0.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Properties", &showProperties_)) {
        ImGui::Text("Name: %s", propertiesNode_->name.c_str());
        ImGui::Text("Path: %s", propertiesNode_->absName().c_str());
        ImGui::Separator();

        if (propertiesNode_->isDir()) {
            ImGui::Text("Type: Directory");
            ImGui::Text("Files: %d", propertiesNode_->subtree.counts[NODE_REGFILE]);
            ImGui::Text("Subdirectories: %d", propertiesNode_->subtree.counts[NODE_DIRECTORY]);

            // Show total subtree size
            int64_t totalSize = propertiesNode_->subtree.size;
            if (totalSize >= 1073741824LL) {
                ImGui::Text("Total Size: %.2f GB", totalSize / 1073741824.0);
            } else if (totalSize >= 1048576LL) {
                ImGui::Text("Total Size: %.2f MB", totalSize / 1048576.0);
            } else if (totalSize >= 1024LL) {
                ImGui::Text("Total Size: %.2f KB", totalSize / 1024.0);
            } else {
                ImGui::Text("Total Size: %lld bytes", (long long)totalSize);
            }
        } else {
            ImGui::Text("Type: %s", nodeTypeNames[propertiesNode_->type]);

            int64_t sz = propertiesNode_->size;
            if (sz >= 1073741824LL) {
                ImGui::Text("Size: %.2f GB", sz / 1073741824.0);
            } else if (sz >= 1048576LL) {
                ImGui::Text("Size: %.2f MB", sz / 1048576.0);
            } else if (sz >= 1024LL) {
                ImGui::Text("Size: %.2f KB", sz / 1024.0);
            } else {
                ImGui::Text("Size: %lld bytes", (long long)sz);
            }
        }
    }
    ImGui::End();
}

} // namespace fsvng
