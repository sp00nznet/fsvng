#include "ui/DirTreePanel.h"

#include <imgui.h>

#include "core/FsNode.h"
#include "core/FsTree.h"
#include "ui/FileListPanel.h"
#include "ui/MainWindow.h"
#include "ui/Dialogs.h"

namespace fsvng {

DirTreePanel& DirTreePanel::instance() {
    static DirTreePanel s;
    return s;
}

void DirTreePanel::draw() {
    ImGui::Begin("Directory Tree");

    FsNode* root = FsTree::instance().rootDir();
    if (root) {
        drawNode(root);
    } else {
        ImGui::TextDisabled("No filesystem loaded");
    }

    // Context menu popup (within this window's scope)
    if (s_->contextMenuNode_) {
        ImGui::OpenPopup("##DirTreeContextMenu");
        s_->contextMenuNode_pending_ = s_->contextMenuNode_;
        s_->contextMenuNode_ = nullptr;
    }
    Dialogs::instance().drawContextMenuPopup("##DirTreeContextMenu", s_->contextMenuNode_pending_);

    ImGui::End();
}

void DirTreePanel::drawNode(FsNode* node) {
    if (!node) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    // Leaf nodes (non-directories or empty dirs) get the Leaf flag
    bool hasChildren = false;
    if (node->isDir()) {
        for (auto& child : node->children) {
            if (child->isDir()) {
                hasChildren = true;
                break;
            }
        }
    }

    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // Highlight the selected node
    if (node == s_->selectedNode_) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Sync expanded state
    if (s_->expandedNodes_.count(node)) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }

    bool nodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(node->id)),
                                       flags, "%s", node->name.c_str());

    // Handle click selection
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        s_->selectedNode_ = node;
        MainWindow::instance().navigateTo(node);
    }

    // Right-click context menu
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        s_->contextMenuNode_ = node;
        s_->selectedNode_ = node;
    }

    // Track expansion state
    if (nodeOpen && hasChildren) {
        s_->expandedNodes_.insert(node);
        for (auto& child : node->children) {
            if (child->isDir()) {
                drawNode(child.get());
            }
        }
        ImGui::TreePop();
    } else if (!nodeOpen) {
        s_->expandedNodes_.erase(node);
    }
}

bool DirTreePanel::isEntryExpanded(FsNode* node) const {
    return s_->expandedNodes_.count(node) > 0;
}

void DirTreePanel::setEntryExpanded(FsNode* node, bool expanded) {
    if (expanded) {
        s_->expandedNodes_.insert(node);
    } else {
        s_->expandedNodes_.erase(node);
    }
}

void DirTreePanel::selectNode(FsNode* node) {
    s_->selectedNode_ = node;
}

void DirTreePanel::clearExpanded() {
    s_->expandedNodes_.clear();
    s_->selectedNode_ = nullptr;
}

std::vector<std::string> DirTreePanel::expandedPaths() const {
    std::vector<std::string> paths;
    paths.reserve(s_->expandedNodes_.size());
    for (FsNode* n : s_->expandedNodes_) {
        if (n) paths.push_back(n->absName());
    }
    return paths;
}

void DirTreePanel::restoreExpanded(const std::vector<std::string>& paths) {
    s_->expandedNodes_.clear();
    for (const std::string& p : paths) {
        if (FsNode* n = FsTree::instance().nodeByPath(p)) {
            s_->expandedNodes_.insert(n);
        }
    }
}

} // namespace fsvng
