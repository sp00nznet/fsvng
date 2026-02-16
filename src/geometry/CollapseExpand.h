#pragma once

#include "core/Types.h"
#include "animation/Morph.h"

namespace fsvng {

class FsNode;

enum class ColExpAction {
    CollapseRecursive,
    Expand,
    ExpandAny,
    ExpandRecursive
};

class CollapseExpand {
public:
    static CollapseExpand& instance();

    void execute(FsNode* dnode, ColExpAction action);

    // Duration of a single collapse/expansion (in seconds)
    static constexpr double MAPV_TIME = 0.375;
    static constexpr double TREEV_TIME = 0.5;

private:
    CollapseExpand() = default;

    int collapsedDepth(FsNode* dnode) const;
    int maxExpandedDepth(FsNode* dnode) const;
    void executeRecursive(FsNode* dnode, ColExpAction action,
                          int depth, int maxDepth, double colexpTime);

    static void progressCallback(Morph* morph);

    // State tracking across recursive calls
    bool scrollbarsColexpAdjust_ = false;
};

} // namespace fsvng
