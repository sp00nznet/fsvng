#include "ui/Dialogs.h"

#include <imgui.h>
#include <cstring>

#include "core/FsNode.h"
#include "core/Types.h"
#include "ui/MainWindow.h"
#include "app/Config.h"

namespace fsvng {

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

void Dialogs::showContextMenu(FsNode* node) {
    contextMenuNode_ = node;
    showContextMenu_ = true;
}

void Dialogs::draw() {
    drawChangeRoot();
    drawSetDefaultPath();
    drawColorConfig();
    drawAbout();
    drawContextMenu();
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
        ImGui::Text("MapV, TreeV, and DiscV modes.");
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

void Dialogs::drawContextMenu() {
    if (!showContextMenu_) return;

    ImGui::OpenPopup("##NodeContextMenu");
    showContextMenu_ = false;  // Only open once; popup stays open on its own

    if (ImGui::BeginPopup("##NodeContextMenu")) {
        if (contextMenuNode_) {
            ImGui::TextDisabled("%s", contextMenuNode_->name.c_str());
            ImGui::Separator();

            if (contextMenuNode_->isDir()) {
                if (ImGui::MenuItem("Expand")) {
                    // TODO: Expand directory in visualization
                }
                if (ImGui::MenuItem("Collapse")) {
                    // TODO: Collapse directory in visualization
                }
                if (ImGui::MenuItem("Set as Root")) {
                    // TODO: Re-root visualization at this directory
                }
                ImGui::Separator();
            }

            if (ImGui::MenuItem("Properties")) {
                // TODO: Show properties dialog for this node
            }

            if (ImGui::MenuItem("Look At")) {
                // TODO: Navigate camera to this node
            }
        }

        ImGui::EndPopup();
    }
}

} // namespace fsvng
