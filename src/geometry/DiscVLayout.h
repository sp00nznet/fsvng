#pragma once

#include "core/Types.h"
#include "renderer/MeshBuffer.h"

#include <glm/glm.hpp>
#include <vector>

namespace fsvng {

class FsNode;

class DiscVLayout {
public:
    static DiscVLayout& instance();

    void init();
    void draw(const glm::mat4& view, const glm::mat4& projection, bool highDetail);
    void drawForPicking(const glm::mat4& view, const glm::mat4& projection);

    static constexpr double CURVE_GRANULARITY = 15.0;
    static constexpr double LEAF_RANGE_ARC_WIDTH = 315.0;
    static constexpr double LEAF_STEM_PROPORTION = 0.5;

private:
    DiscVLayout() = default;

    void initRecursive(FsNode* dnode, double stemTheta);
    void drawRecursive(FsNode* dnode, const glm::mat4& view, const glm::mat4& proj,
                       bool geometry);
    void buildNodeDisc(FsNode* node, double dirDeployment,
                       std::vector<Vertex>& vertices,
                       std::vector<uint32_t>& indices);
    void buildFolderOutline(FsNode* dnode,
                            std::vector<Vertex>& vertices,
                            std::vector<uint32_t>& indices);
    void buildDir(FsNode* dnode,
                  std::vector<Vertex>& vertices,
                  std::vector<uint32_t>& indices);
};

} // namespace fsvng
