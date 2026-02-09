#include "FsNode.h"

#include <algorithm>

namespace fsvng {

// Build absolute path by traversing parent pointers.
// Matches the logic of the original node_absname():
//   - Collect all ancestors including metanode
//   - Build path from root to this node
std::string FsNode::absName() const {
    // Collect names from this node up to the root (including metanode).
    std::vector<const FsNode*> ancestors;
    const FsNode* cur = this;
    while (cur != nullptr) {
        ancestors.push_back(cur);
        cur = cur->parent;
    }

    // Reverse so root (metanode) is first.
    std::reverse(ancestors.begin(), ancestors.end());

    // Build the path.
    std::string path;
    for (size_t i = 0; i < ancestors.size(); ++i) {
        const std::string& n = ancestors[i]->name;
        if (i == 0) {
            // Metanode or root: use name directly.
            path = n;
        } else {
            // Ensure separator between components.
            if (!path.empty() && path.back() != '/' && path.back() != '\\') {
                path += '/';
            }
            path += n;
        }
    }

    return path;
}

FsNode* FsNode::addChild(std::unique_ptr<FsNode> child) {
    child->parent = this;
    FsNode* raw = child.get();
    children.push_back(std::move(child));
    return raw;
}

} // namespace fsvng
