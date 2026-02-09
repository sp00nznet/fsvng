#pragma once

#include "core/Types.h"
#include "renderer/MeshBuffer.h"

#include <glm/glm.hpp>
#include <vector>

namespace fsvng {

class FsNode;

class MapVLayout {
public:
    static MapVLayout& instance();

    void init();
    void draw(const glm::mat4& view, const glm::mat4& projection, bool highDetail);
    void drawForPicking(const glm::mat4& view, const glm::mat4& projection);

    void cameraPanFinished();

    // Constants
    static constexpr double BORDER_PROPORTION = 0.01;
    static constexpr double ROOT_ASPECT_RATIO = 1.2;
    static constexpr double DIR_HEIGHT = 384.0;
    static constexpr double LEAF_HEIGHT = 128.0;

    // Side face slant ratios by node type
    static const float sideSlantRatios[NUM_NODE_TYPES];

private:
    MapVLayout() = default;

    void initRecursive(FsNode* dnode);
    void drawRecursive(FsNode* dnode, const glm::mat4& view, const glm::mat4& proj,
                       bool geometry);
    void drawNodeMesh(FsNode* node, const glm::mat4& model);
    void drawFolder(FsNode* dnode, const glm::mat4& model);
    void drawCursor(double pos, const glm::mat4& view, const glm::mat4& proj);

    // Generate mesh for a MapV node (slanted box)
    void buildNodeMesh(FsNode* node, std::vector<Vertex>& vertices,
                       std::vector<uint32_t>& indices);

    // Generate mesh for a MapV folder outline on top of collapsed dir
    void buildFolderMesh(FsNode* dnode, std::vector<Vertex>& vertices,
                         std::vector<uint32_t>& indices);

    // Build all children geometry for a directory
    void buildDir(FsNode* dnode, std::vector<Vertex>& vertices,
                  std::vector<uint32_t>& indices);

    XYZvec cursorPrevC0_{};
    XYZvec cursorPrevC1_{};

    // Cached mesh buffers per directory
    // We use the dirty flag approach instead of display lists
};

} // namespace fsvng
