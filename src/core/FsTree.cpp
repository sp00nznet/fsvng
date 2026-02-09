#include "FsTree.h"

#include <algorithm>
#include <cstring>

namespace fsvng {

FsTree& FsTree::instance() {
    static FsTree inst;
    return inst;
}

FsNode* FsTree::root() const {
    return root_.get();
}

FsNode* FsTree::rootDir() const {
    if (root_ && !root_->children.empty()) {
        return root_->children[0].get();
    }
    return nullptr;
}

void FsTree::setRoot(std::unique_ptr<FsNode> root) {
    // Clear old lookup tables (they reference the old tree)
    nodeTable_.clear();
    pathTable_.clear();
    root_ = std::move(root);
}

void FsTree::clear() {
    root_.reset();
    nodeTable_.clear();
    pathTable_.clear();
    nextId_ = 0;
}

FsNode* FsTree::nodeById(unsigned int id) const {
    if (id < nodeTable_.size()) {
        return nodeTable_[id];
    }
    return nullptr;
}

FsNode* FsTree::nodeByPath(const std::string& absname) const {
    auto it = pathTable_.find(absname);
    if (it != pathTable_.end()) {
        return it->second;
    }
    return nullptr;
}

void FsTree::buildNodeTable() {
    nodeTable_.clear();
    pathTable_.clear();

    if (!root_) {
        return;
    }

    // First pass: find the max ID so we can size the table correctly.
    // The scanner assigns IDs incrementally, so we just need the max.
    unsigned int maxId = 0;
    struct IdFinder {
        unsigned int& maxId;
        void visit(FsNode* node) {
            if (!node) return;
            if (node->id > maxId) maxId = node->id;
            for (auto& child : node->children) {
                visit(child.get());
            }
        }
    };
    IdFinder finder{maxId};
    finder.visit(root_.get());
    nextId_ = maxId + 1;

    // Resize table to hold all IDs.
    nodeTable_.resize(nextId_, nullptr);

    // Populate via recursive traversal.
    struct Populator {
        std::vector<FsNode*>& table;
        std::unordered_map<std::string, FsNode*>& paths;

        void visit(FsNode* node) {
            if (!node) return;
            if (node->id < table.size()) {
                table[node->id] = node;
            }
            if (!node->isMetanode()) {
                paths[node->absName()] = node;
            }
            for (auto& child : node->children) {
                visit(child.get());
            }
        }
    };

    Populator pop{nodeTable_, pathTable_};
    pop.visit(root_.get());
}

void FsTree::setupTree() {
    if (root_) {
        setupRecursive(root_.get());
        buildNodeTable();
    }
}

void FsTree::setupRecursive(FsNode* node) {
    if (!node) return;

    // Initialize subtree counts for directories.
    if (node->isDir() || node->isMetanode()) {
        node->subtree.size = 0;
        std::memset(node->subtree.counts, 0, sizeof(node->subtree.counts));

        // Recurse into children first.
        for (auto& child : node->children) {
            setupRecursive(child.get());
        }

        // After children are set up, accumulate subtree info.
        for (auto& child : node->children) {
            FsNode* c = child.get();

            // Add this child's own contribution.
            node->subtree.counts[c->type]++;
            node->subtree.size += c->size;

            // If child is a directory, also add its subtree.
            if (c->isDir()) {
                node->subtree.size += c->subtree.size;
                for (int i = 0; i < NUM_NODE_TYPES; ++i) {
                    node->subtree.counts[i] += c->subtree.counts[i];
                }
            }
        }

        // Sort children: directories first, then by size descending, then alphabetically.
        sortChildren(node);
    }
}

void FsTree::sortChildren(FsNode* node) {
    if (!node) return;

    std::sort(node->children.begin(), node->children.end(),
        [](const std::unique_ptr<FsNode>& a, const std::unique_ptr<FsNode>& b) -> bool {
            // Directories come before non-directories.
            bool aIsDir = a->isDir();
            bool bIsDir = b->isDir();
            if (aIsDir != bIsDir) {
                return aIsDir;  // dirs first
            }

            // Within same category, larger size first.
            int64_t aSize = aIsDir ? a->subtree.size : a->size;
            int64_t bSize = bIsDir ? b->subtree.size : b->size;
            if (aSize != bSize) {
                return aSize > bSize;
            }

            // Tie-break: alphabetical by name (case-insensitive comparison
            // for consistency with original).
            // Use a simple locale-independent comparison.
            const std::string& an = a->name;
            const std::string& bn = b->name;
            size_t len = std::min(an.size(), bn.size());
            for (size_t i = 0; i < len; ++i) {
                char ac = static_cast<char>(std::tolower(static_cast<unsigned char>(an[i])));
                char bc = static_cast<char>(std::tolower(static_cast<unsigned char>(bn[i])));
                if (ac != bc) return ac < bc;
            }
            return an.size() < bn.size();
        });
}

} // namespace fsvng
