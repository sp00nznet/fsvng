#include "ui/DirTreePanel.h"

#include <imgui.h>

#include "core/FsNode.h"
#include "core/FsTree.h"
#include "ui/FileListPanel.h"
#include "ui/MainWindow.h"

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
    if (node == selectedNode_) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Sync expanded state
    if (expandedNodes_.count(node)) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }

    bool nodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(node->id)),
                                       flags, "%s", node->name.c_str());

    // Handle click selection
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        selectedNode_ = node;
        MainWindow::instance().navigateTo(node);
    }

    // Track expansion state
    if (nodeOpen && hasChildren) {
        expandedNodes_.insert(node);
        for (auto& child : node->children) {
            if (child->isDir()) {
                drawNode(child.get());
            }
        }
        ImGui::TreePop();
    } else if (!nodeOpen) {
        expandedNodes_.erase(node);
    }
}

bool DirTreePanel::isEntryExpanded(FsNode* node) const {
    return expandedNodes_.count(node) > 0;
}

void DirTreePanel::setEntryExpanded(FsNode* node, bool expanded) {
    if (expanded) {
        expandedNodes_.insert(node);
    } else {
        expandedNodes_.erase(node);
    }
}

void DirTreePanel::selectNode(FsNode* node) {
    selectedNode_ = node;
}

void DirTreePanel::clearExpanded() {
    expandedNodes_.clear();
    selectedNode_ = nullptr;
}

} // namespace fsvng
