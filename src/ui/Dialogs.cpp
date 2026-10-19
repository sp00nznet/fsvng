#include "ui/Dialogs.h"

#include <imgui.h>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include "app/App.h"
#endif

#include "core/FsNode.h"
#include "core/Types.h"
#include "ui/MainWindow.h"
#include "ui/DirTreePanel.h"
#include "geometry/CollapseExpand.h"
#include "camera/Camera.h"
#include "app/Config.h"

namespace fsvng {

#ifdef _WIN32
static void showWindowsContextMenu(const std::string& path);
#endif

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

#ifdef _WIN32
    // Deferred Windows context menu (must be called outside ImGui popup)
    if (!windowsContextMenuPath_.empty()) {
        std::string path = std::move(windowsContextMenuPath_);
        windowsContextMenuPath_.clear();
        showWindowsContextMenu(path);
    }
#endif
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
        ImGui::Text("fsvng %s - 3D File System Visualizer", FSVNG_VERSION);
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

#ifdef _WIN32
static void showWindowsContextMenu(const std::string& path) {
    // Get the HWND from SDL
    SDL_Window* sdlWin = App::instance().getWindow();
    if (!sdlWin) return;

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(sdlWin, &wmInfo)) return;
    HWND hwnd = wmInfo.info.win.window;

    // Normalize path: convert forward slashes to backslashes for Windows Shell API
    std::string winPath = path;
    for (char& c : winPath) {
        if (c == '/') c = '\\';
    }

    // Convert path to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, winPath.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return;
    std::vector<wchar_t> wpath(wlen);
    MultiByteToWideChar(CP_UTF8, 0, winPath.c_str(), -1, wpath.data(), wlen);

    // Initialize COM (S_FALSE means already initialized - that's fine)
    HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comOwned = (comHr == S_OK);

    PIDLIST_ABSOLUTE pidl = nullptr;
    HRESULT hr = SHParseDisplayName(wpath.data(), nullptr, &pidl, 0, nullptr);
    if (SUCCEEDED(hr) && pidl) {
        IShellFolder* parentFolder = nullptr;
        PCUITEMID_CHILD childPidl = nullptr;
        hr = SHBindToParent(pidl, IID_IShellFolder, (void**)&parentFolder, &childPidl);
        if (SUCCEEDED(hr) && parentFolder) {
            IContextMenu* contextMenu = nullptr;
            hr = parentFolder->GetUIObjectOf(hwnd, 1, &childPidl,
                                              IID_IContextMenu, nullptr, (void**)&contextMenu);
            if (SUCCEEDED(hr) && contextMenu) {
                HMENU hMenu = CreatePopupMenu();
                if (hMenu) {
                    hr = contextMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF,
                                                        CMF_NORMAL | CMF_EXPLORE);
                    if (SUCCEEDED(hr)) {
                        // Required for TrackPopupMenu to work from non-foreground windows
                        SetForegroundWindow(hwnd);

                        POINT pt;
                        GetCursorPos(&pt);
                        int cmd = TrackPopupMenu(hMenu,
                                                  TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                                                  pt.x, pt.y, 0, hwnd, nullptr);

                        // Required to dismiss the menu properly (KB135788)
                        PostMessage(hwnd, WM_NULL, 0, 0);

                        if (cmd > 0) {
                            CMINVOKECOMMANDINFO ci = {};
                            ci.cbSize = sizeof(ci);
                            ci.hwnd = hwnd;
                            ci.lpVerb = MAKEINTRESOURCEA(cmd - 1);
                            ci.nShow = SW_SHOWNORMAL;
                            contextMenu->InvokeCommand(&ci);
                        }
                    }
                    DestroyMenu(hMenu);
                }
                contextMenu->Release();
            }
            parentFolder->Release();
        }
        CoTaskMemFree(pidl);
    }

    if (comOwned) CoUninitialize();
}
#endif

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
                if (ImGui::MenuItem("Expand All")) {
                    CollapseExpand::instance().execute(node, ColExpAction::ExpandRecursive);
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

#ifdef _WIN32
            if (ImGui::MenuItem("Open")) {
                std::string path = node->absName();
                if (node->isDir()) {
                    ShellExecuteA(nullptr, "explore", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                } else {
                    ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }

            if (ImGui::MenuItem("Windows Context Menu")) {
                windowsContextMenuPath_ = node->absName();
            }
#endif

            if (ImGui::MenuItem("Copy Path")) {
                std::string path = node->absName();
                ImGui::SetClipboardText(path.c_str());
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
