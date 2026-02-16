#include "ui/FileListPanel.h"

#include <imgui.h>

#include "core/FsNode.h"
#include "core/Types.h"
#include "ui/MainWindow.h"
#include "ui/Dialogs.h"

namespace fsvng {

FileListPanel& FileListPanel::instance() {
    static FileListPanel s;
    return s;
}

static const char* nodeTypeIcon(NodeType type) {
    switch (type) {
        case NODE_DIRECTORY: return "[DIR]";
        case NODE_REGFILE:   return "[FIL]";
        case NODE_SYMLINK:   return "[LNK]";
        case NODE_FIFO:      return "[PIP]";
        case NODE_SOCKET:    return "[SOC]";
        case NODE_CHARDEV:   return "[CHR]";
        case NODE_BLOCKDEV:  return "[BLK]";
        default:             return "[???]";
    }
}

static std::string formatSize(int64_t bytes) {
    if (bytes < 0) return "---";

    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }

    char buf[64];
    if (unitIndex == 0) {
        snprintf(buf, sizeof(buf), "%lld B", static_cast<long long>(bytes));
    } else {
        snprintf(buf, sizeof(buf), "%.1f %s", size, units[unitIndex]);
    }
    return buf;
}

void FileListPanel::draw() {
    ImGui::Begin("File List");

    if (!currentDir_) {
        ImGui::TextDisabled("No directory selected");
        ImGui::End();
        return;
    }

    ImGui::Text("Contents of: %s", currentDir_->name.c_str());
    ImGui::Separator();

    ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Sortable |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV |
        ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("FileTable", 4, tableFlags)) {
        // Column setup
        ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 40.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Sort specification
        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
            if (sortSpecs->SpecsDirty) {
                // Sorting would be applied here if we had a separate sorted list.
                // For now, we just mark clean.
                sortSpecs->SpecsDirty = false;
            }
        }

        // Rows
        for (auto& child : currentDir_->children) {
            if (child->isMetanode()) continue;

            ImGui::TableNextRow();

            // Icon column
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(nodeTypeIcon(child->type));

            // Name column
            ImGui::TableSetColumnIndex(1);
            bool isSelected = (child.get() == selectedNode_);
            if (ImGui::Selectable(child->name.c_str(), isSelected,
                                  ImGuiSelectableFlags_SpanAllColumns |
                                  ImGuiSelectableFlags_AllowOverlap)) {
                selectedNode_ = child.get();
                MainWindow::instance().navigateTo(child.get());
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                contextMenuNode_ = child.get();
                selectedNode_ = child.get();
            }

            // Size column
            ImGui::TableSetColumnIndex(2);
            if (child->isDir()) {
                ImGui::Text("%s", formatSize(child->subtree.size).c_str());
            } else {
                ImGui::Text("%s", formatSize(child->size).c_str());
            }

            // Type column
            ImGui::TableSetColumnIndex(3);
            if (child->type >= 0 && child->type < NUM_NODE_TYPES) {
                ImGui::TextUnformatted(nodeTypeNames[child->type]);
            }
        }

        ImGui::EndTable();
    }

    // Context menu popup (within this window's scope)
    if (contextMenuNode_) {
        ImGui::OpenPopup("##FileListContextMenu");
        contextMenuNode_pending_ = contextMenuNode_;
        contextMenuNode_ = nullptr;
    }
    Dialogs::instance().drawContextMenuPopup("##FileListContextMenu", contextMenuNode_pending_);

    ImGui::End();
}

void FileListPanel::showEntry(FsNode* node) {
    selectedNode_ = node;
}

void FileListPanel::showDirectory(FsNode* dirNode) {
    currentDir_ = dirNode;
    selectedNode_ = nullptr;
}

} // namespace fsvng
