#include "ui/FileListPanel.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>

#include "core/FsNode.h"
#include "core/Types.h"
#include "core/PlatformUtils.h"
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

// Size shown/sorted: directories report their aggregate subtree size.
static int64_t effectiveSize(const FsNode* n) {
    return n->isDir() ? n->subtree.size : n->size;
}

static std::string lowerName(const std::string& s) {
    std::string out(s.size(), '\0');
    std::transform(s.begin(), s.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

void FileListPanel::rebuildSorted() {
    s_->sorted_.clear();
    if (!s_->currentDir_) return;

    for (auto& child : s_->currentDir_->children) {
        if (child->isMetanode()) continue;
        s_->sorted_.push_back(child.get());
    }

    auto cmp = [this](FsNode* a, FsNode* b) -> bool {
        int c = 0;
        switch (s_->sortColumn_) {
            case 2: { // Size
                int64_t sa = effectiveSize(a), sb = effectiveSize(b);
                c = (sa < sb) ? -1 : (sa > sb) ? 1 : 0;
                break;
            }
            case 3: // Type
                c = (a->type < b->type) ? -1 : (a->type > b->type) ? 1 : 0;
                break;
            case 4: { // Modified
                c = (a->mtime < b->mtime) ? -1 : (a->mtime > b->mtime) ? 1 : 0;
                break;
            }
            case 1: // Name
            default:
                break;
        }
        if (c == 0) {
            // Tiebreaker (and primary for Name): case-insensitive name.
            c = lowerName(a->name).compare(lowerName(b->name));
        }
        return s_->sortAscending_ ? (c < 0) : (c > 0);
    };

    std::stable_sort(s_->sorted_.begin(), s_->sorted_.end(), cmp);
    s_->sortDirty_ = false;
}

void FileListPanel::draw() {
    ImGui::Begin("File List");

    if (!s_->currentDir_) {
        ImGui::TextDisabled("No directory selected");
        ImGui::End();
        return;
    }

    ImGui::Text("Contents of: %s", s_->currentDir_->name.c_str());
    ImGui::Separator();

    ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Sortable |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV |
        ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("FileTable", 5, tableFlags)) {
        // Column setup
        ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 40.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Pick up sort changes from the header UI.
        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
            if (sortSpecs->SpecsDirty) {
                if (sortSpecs->SpecsCount > 0) {
                    const ImGuiTableColumnSortSpecs& s = sortSpecs->Specs[0];
                    s_->sortColumn_ = s.ColumnIndex;
                    s_->sortAscending_ = (s.SortDirection != ImGuiSortDirection_Descending);
                }
                s_->sortDirty_ = true;
                sortSpecs->SpecsDirty = false;
            }
        }

        if (s_->sortDirty_) {
            rebuildSorted();
        }

        // Rows (sorted)
        for (FsNode* child : s_->sorted_) {
            ImGui::TableNextRow();

            // Icon column
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(nodeTypeIcon(child->type));

            // Name column
            ImGui::TableSetColumnIndex(1);
            std::string childPath = child->absName();
            bool isSelected = isPathSelected(childPath);
            if (ImGui::Selectable(child->name.c_str(), isSelected,
                                  ImGuiSelectableFlags_SpanAllColumns |
                                  ImGuiSelectableFlags_AllowOverlap)) {
                ImGuiIO& io = ImGui::GetIO();
                if (io.KeyCtrl) {
                    // Toggle membership.
                    if (s_->selectedPaths_.count(childPath)) {
                        s_->selectedPaths_.erase(childPath);
                    } else {
                        s_->selectedPaths_.insert(childPath);
                    }
                    s_->anchorPath_ = childPath;
                    s_->selectedNode_ = child;
                } else if (io.KeyShift && !s_->anchorPath_.empty()) {
                    // Range select from anchor to this row in sorted order.
                    int ai = -1, ci = -1;
                    for (int k = 0; k < (int)s_->sorted_.size(); ++k) {
                        if (s_->sorted_[k] == child) ci = k;
                        if (s_->sorted_[k]->absName() == s_->anchorPath_) ai = k;
                    }
                    if (ai >= 0 && ci >= 0) {
                        s_->selectedPaths_.clear();
                        int lo = std::min(ai, ci), hi = std::max(ai, ci);
                        for (int k = lo; k <= hi; ++k) {
                            s_->selectedPaths_.insert(s_->sorted_[k]->absName());
                        }
                    }
                    s_->selectedNode_ = child;
                } else {
                    // Plain click: single-select and navigate.
                    s_->selectedPaths_.clear();
                    s_->selectedPaths_.insert(childPath);
                    s_->anchorPath_ = childPath;
                    s_->selectedNode_ = child;
                    MainWindow::instance().navigateTo(child);
                }
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                // Right-click on an unselected row selects just it; on a
                // selected row keeps the existing multi-selection.
                if (!isPathSelected(childPath)) {
                    s_->selectedPaths_.clear();
                    s_->selectedPaths_.insert(childPath);
                    s_->anchorPath_ = childPath;
                }
                s_->contextMenuNode_ = child;
                s_->selectedNode_ = child;
            }

            // Drag source: carry the selected paths (newline-separated) when
            // dragging a member of a multi-selection, else just this entry.
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                std::string payload;
                if (isPathSelected(childPath) && selectionCount() > 1) {
                    for (const auto& p : s_->selectedPaths_) {
                        payload += p;
                        payload += '\n';
                    }
                    ImGui::Text("Move %d items", selectionCount());
                } else {
                    payload = childPath;
                    ImGui::Text("Move %s", child->name.c_str());
                }
                ImGui::SetDragDropPayload("FSVNG_NODE", payload.data(), payload.size());
                ImGui::EndDragDropSource();
            }
            // Drop target: dropping onto a directory row moves into it.
            if (child->isDir() && ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl =
                        ImGui::AcceptDragDropPayload("FSVNG_NODE")) {
                    std::string src(static_cast<const char*>(pl->Data),
                                    static_cast<size_t>(pl->DataSize));
                    MainWindow::instance().moveAcrossPanes(src, child->absName());
                }
                ImGui::EndDragDropTarget();
            }

            // Size column
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", formatSize(effectiveSize(child)).c_str());

            // Type column
            ImGui::TableSetColumnIndex(3);
            if (child->type >= 0 && child->type < NUM_NODE_TYPES) {
                ImGui::TextUnformatted(nodeTypeNames[child->type]);
            }

            // Modified column
            ImGui::TableSetColumnIndex(4);
            if (child->mtime != 0) {
                ImGui::TextUnformatted(PlatformUtils::formatTime(child->mtime).c_str());
            } else {
                ImGui::TextDisabled("--");
            }
        }

        ImGui::EndTable();
    }

    // Context menu popup (within this window's scope)
    if (s_->contextMenuNode_) {
        ImGui::OpenPopup("##FileListContextMenu");
        s_->contextMenuNode_pending_ = s_->contextMenuNode_;
        s_->contextMenuNode_ = nullptr;
    }
    Dialogs::instance().drawContextMenuPopup("##FileListContextMenu", s_->contextMenuNode_pending_);

    ImGui::End();
}

void FileListPanel::showEntry(FsNode* node) {
    s_->selectedNode_ = node;
}

void FileListPanel::showDirectory(FsNode* dirNode) {
    s_->currentDir_ = dirNode;
    s_->selectedNode_ = nullptr;
    s_->selectedPaths_.clear();
    s_->anchorPath_.clear();
    s_->sortDirty_ = true;
}

} // namespace fsvng
