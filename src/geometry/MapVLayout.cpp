#include "geometry/MapVLayout.h"
#include "geometry/GeometryManager.h"
#include "core/FsNode.h"
#include "core/FsTree.h"
#include "ui/DirTreePanel.h"
#include "renderer/Renderer.h"
#include "renderer/ShaderProgram.h"
#include "animation/Morph.h"
#include "animation/Animation.h"
#include "ui/ThemeManager.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <vector>
#include <memory>

namespace fsvng {

// Side face slant ratios by node type
const float MapVLayout::sideSlantRatios[NUM_NODE_TYPES] = {
    0.0f,   // Metanode (not used)
    0.032f, // Directory
    0.064f, // Regular file
    0.333f, // Symlink
    0.0f,   // FIFO
    0.0f,   // Socket
    0.25f,  // Character device
    0.25f,  // Block device
    0.0f    // Unknown
};

MapVLayout& MapVLayout::instance() {
    static MapVLayout inst;
    return inst;
}

// ============================================================================
// Layout algorithm: THE TREEMAP
// Ported from mapv_init_recursive in geometry.c
// ============================================================================

void MapVLayout::initRecursive(FsNode* dnode) {
    // Internal block/row structures for treemap algorithm
    struct MapVBlock {
        FsNode* node;
        double area;
    };
    struct MapVRow {
        size_t firstBlockIndex;
        double area;
    };

    assert(dnode->isDir());

    MorphEngine::instance().morphBreak(&dnode->deployment);
    if (DirTreePanel::instance().isEntryExpanded(dnode))
        dnode->deployment = 1.0;
    else
        dnode->deployment = 0.0;
    GeometryManager::instance().queueRebuild(dnode);

    // If this directory has no children, there is nothing further to do
    if (dnode->children.empty())
        return;

    // Obtain dimensions of top face of directory
    XYvec dirDims;
    dirDims.x = dnode->mapvWidth();
    dirDims.y = dnode->mapvDepth();
    double k = sideSlantRatios[NODE_DIRECTORY];
    dirDims.x -= 2.0 * std::min(dnode->mapvGeom.height, k * dirDims.x);
    dirDims.y -= 2.0 * std::min(dnode->mapvGeom.height, k * dirDims.y);

    // Approximate/nominal node border width
    double a = BORDER_PROPORTION * std::sqrt(dirDims.x * dirDims.y);
    double b = std::min(dirDims.x, dirDims.y) / 3.0;
    double nominalBorder = std::min(a, b);

    // Trim half a border width off the perimeter
    dirDims.x -= nominalBorder;
    dirDims.y -= nominalBorder;
    double dirArea = dirDims.x * dirDims.y;

    // First pass: create blocks, find total area, create block list
    std::vector<MapVBlock> blockList;
    double totalBlockArea = 0.0;

    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        int64_t size = std::max(int64_t(4096), node->size);
        if (node->isDir())
            size += node->subtree.size;
        k = std::sqrt(static_cast<double>(size)) + nominalBorder;
        double area = k * k; // SQR(k)
        totalBlockArea += area;

        MapVBlock block;
        block.node = node;
        block.area = area;
        blockList.push_back(block);
    }

    // Scale factor: blocks total area > directory area, scale down
    double scaleFactor = dirArea / totalBlockArea;

    // Second pass: scale blocks and generate first-draft rows
    std::vector<MapVRow> rowList;
    MapVRow* currentRow = nullptr;

    for (size_t i = 0; i < blockList.size(); ++i) {
        MapVBlock& block = blockList[i];
        block.area *= scaleFactor;

        if (currentRow == nullptr) {
            // Begin new row
            MapVRow newRow;
            newRow.firstBlockIndex = i;
            newRow.area = 0.0;
            rowList.push_back(newRow);
            currentRow = &rowList.back();
        }

        // Add block to row
        currentRow->area += block.area;

        // Dimensions of block (blockDims.y == depth of row)
        XYvec blockDims;
        blockDims.y = currentRow->area / dirDims.x;
        blockDims.x = block.area / blockDims.y;

        // Check aspect ratio of block
        if ((blockDims.x / blockDims.y) < 1.0) {
            // Next block will go into next row
            currentRow = nullptr;
        }
    }

    // Third pass - optimize layout (placeholder as in original)
    // Note to self: write layout optimization routine sometime

    // Fourth pass - output final arrangement
    // Start at right/rear corner, laying out rows of successively smaller blocks
    XYvec startPos;
    startPos.x = dnode->mapvCenterX() + 0.5 * dirDims.x;
    startPos.y = dnode->mapvCenterY() + 0.5 * dirDims.y;

    XYvec pos;
    pos.y = startPos.y;

    size_t blockIdx = 0;
    for (size_t rowIdx = 0; rowIdx < rowList.size(); ++rowIdx) {
        MapVRow& row = rowList[rowIdx];
        XYvec blockDims;
        blockDims.y = row.area / dirDims.x;
        pos.x = startPos.x;

        // Note first block of next row
        size_t nextFirstBlockIndex = (rowIdx + 1 < rowList.size())
            ? rowList[rowIdx + 1].firstBlockIndex
            : blockList.size();

        // Output one row
        while (blockIdx < blockList.size()) {
            if (blockIdx == nextFirstBlockIndex)
                break; // finished with row

            MapVBlock& block = blockList[blockIdx];
            blockDims.x = block.area / blockDims.y;

            int64_t size = std::max(int64_t(256), block.node->size);
            if (block.node->isDir())
                size += block.node->subtree.size;
            double area = scaleFactor * static_cast<double>(size);

            // Calculate exact width of block's border region
            k = blockDims.x + blockDims.y;
            // area == scaled area of node, block.area == scaled area of node+border
            double border = 0.25 * (k - std::sqrt(k * k - 4.0 * (block.area - area)));

            // Assign geometry (pos is right/rear corner of block)
            block.node->mapvGeom.c0.x = pos.x - blockDims.x + border;
            block.node->mapvGeom.c0.y = pos.y - blockDims.y + border;
            block.node->mapvGeom.c1.x = pos.x - border;
            block.node->mapvGeom.c1.y = pos.y - border;

            if (block.node->isDir()) {
                block.node->mapvGeom.height = DIR_HEIGHT;
                // Recurse into directory
                initRecursive(block.node);
            } else {
                block.node->mapvGeom.height = LEAF_HEIGHT;
            }

            pos.x -= blockDims.x;
            ++blockIdx;
        }

        pos.y -= blockDims.y;
    }
}

// ============================================================================
// Top-level init (port of mapv_init)
// ============================================================================

void MapVLayout::init() {
    FsTree& tree = FsTree::instance();
    FsNode* metanode = tree.root();
    if (!metanode) return;

    FsNode* rootDir = tree.rootDir();
    if (!rootDir) return;

    // Determine dimensions of bottommost (root) node
    XYvec rootDims;
    rootDims.y = std::sqrt(static_cast<double>(metanode->subtree.size) / ROOT_ASPECT_RATIO);
    rootDims.x = ROOT_ASPECT_RATIO * rootDims.y;

    // Set up base geometry for metanode
    metanode->mapvGeom.height = 0.0;

    // Set up root directory geometry
    rootDir->mapvGeom.c0.x = -0.5 * rootDims.x;
    rootDir->mapvGeom.c0.y = -0.5 * rootDims.y;
    rootDir->mapvGeom.c1.x = 0.5 * rootDims.x;
    rootDir->mapvGeom.c1.y = 0.5 * rootDims.y;
    rootDir->mapvGeom.height = DIR_HEIGHT;

    initRecursive(rootDir);

    // Initial cursor state
    double k = 4.0; // default scale for initial cursor
    cursorPrevC0_.x = k * rootDir->mapvGeom.c0.x;
    cursorPrevC0_.y = k * rootDir->mapvGeom.c0.y;
    cursorPrevC0_.z = -0.25 * k * rootDir->mapvDepth();
    cursorPrevC1_.x = k * rootDir->mapvGeom.c1.x;
    cursorPrevC1_.y = k * rootDir->mapvGeom.c1.y;
    cursorPrevC1_.z = 0.25 * k * rootDir->mapvDepth();
}

// ============================================================================
// Camera pan finished hook (port of mapv_camera_pan_finished)
// ============================================================================

void MapVLayout::cameraPanFinished() {
    // In the original, this saves cursor position from globals.current_node
    // For now we leave a stub - the current_node concept moves to a higher layer
}

// ============================================================================
// Build vertex data for a single MapV node (slanted box)
// Port of mapv_gldraw_node
// ============================================================================

void MapVLayout::buildNodeMesh(FsNode* node,
                               std::vector<Vertex>& vertices,
                               std::vector<uint32_t>& indices) {
    // Dimensions of node
    double dimsX = node->mapvWidth();
    double dimsY = node->mapvDepth();
    double dimsZ = node->mapvGeom.height;

    // Calculate normals for slanted sides
    double k = sideSlantRatios[node->type];
    double offsetX = std::min(dimsZ, k * dimsX);
    double offsetY = std::min(dimsZ, k * dimsY);
    double la = std::sqrt(offsetX * offsetX + dimsZ * dimsZ);
    double lb = std::sqrt(offsetY * offsetY + dimsZ * dimsZ);

    // Avoid division by zero
    if (la < EPSILON) la = 1.0;
    if (lb < EPSILON) lb = 1.0;

    float normalX = static_cast<float>(dimsZ / la);
    float normalY = static_cast<float>(dimsZ / lb);
    float normalZnx = static_cast<float>(offsetX / la);
    float normalZny = static_cast<float>(offsetY / lb);

    // Node color
    glm::vec3 col(0.7f, 0.7f, 0.7f);
    if (node->color) {
        col = glm::vec3(node->color->r, node->color->g, node->color->b);
    }

    auto& gp = node->mapvGeom;
    float x0 = static_cast<float>(gp.c0.x);
    float y0 = static_cast<float>(gp.c0.y);
    float x1 = static_cast<float>(gp.c1.x);
    float y1 = static_cast<float>(gp.c1.y);
    float h = static_cast<float>(gp.height);
    float ox = static_cast<float>(offsetX);
    float oy = static_cast<float>(offsetY);

    uint32_t base = static_cast<uint32_t>(vertices.size());

    // Side faces as a quad strip (10 vertices forming 4 quads + wrap)
    // We convert the quad strip into individual triangles

    // Rear face (y1 side)
    // Bottom-left-rear, Top-left-rear, Bottom-right-rear, Top-right-rear
    glm::vec3 nRear(0.0f, normalY, normalZny);
    vertices.push_back({glm::vec3(x0, y1, 0.0f), nRear, col, glm::vec2(0.0f)});         // 0
    vertices.push_back({glm::vec3(x0 + ox, y1 - oy, h), nRear, col, glm::vec2(0.0f)});  // 1

    // Right face (x1 side)
    glm::vec3 nRight(normalX, 0.0f, normalZnx);
    vertices.push_back({glm::vec3(x1, y1, 0.0f), nRight, col, glm::vec2(0.0f)});        // 2
    vertices.push_back({glm::vec3(x1 - ox, y1 - oy, h), nRight, col, glm::vec2(0.0f)}); // 3

    // Front face (y0 side)
    glm::vec3 nFront(0.0f, -normalY, normalZny);
    vertices.push_back({glm::vec3(x1, y0, 0.0f), nFront, col, glm::vec2(0.0f)});        // 4
    vertices.push_back({glm::vec3(x1 - ox, y0 + oy, h), nFront, col, glm::vec2(0.0f)}); // 5

    // Left face (x0 side)
    glm::vec3 nLeft(-normalX, 0.0f, normalZnx);
    vertices.push_back({glm::vec3(x0, y0, 0.0f), nLeft, col, glm::vec2(0.0f)});         // 6
    vertices.push_back({glm::vec3(x0 + ox, y0 + oy, h), nLeft, col, glm::vec2(0.0f)});  // 7

    // Closing vertices (same as rear, for quad strip wrap)
    vertices.push_back({glm::vec3(x0, y1, 0.0f), nLeft, col, glm::vec2(0.0f)});         // 8
    vertices.push_back({glm::vec3(x0 + ox, y1 - oy, h), nLeft, col, glm::vec2(0.0f)});  // 9

    // Convert quad strip to triangles: each consecutive pair of vertices forms a quad
    // Quad 0 (rear): 0,1,2,3 -> triangles (0,1,2), (2,1,3)
    indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
    indices.push_back(base + 2); indices.push_back(base + 1); indices.push_back(base + 3);
    // Quad 1 (right): 2,3,4,5
    indices.push_back(base + 2); indices.push_back(base + 3); indices.push_back(base + 4);
    indices.push_back(base + 4); indices.push_back(base + 3); indices.push_back(base + 5);
    // Quad 2 (front): 4,5,6,7
    indices.push_back(base + 4); indices.push_back(base + 5); indices.push_back(base + 6);
    indices.push_back(base + 6); indices.push_back(base + 5); indices.push_back(base + 7);
    // Quad 3 (left): 6,7,8,9
    indices.push_back(base + 6); indices.push_back(base + 7); indices.push_back(base + 8);
    indices.push_back(base + 8); indices.push_back(base + 7); indices.push_back(base + 9);

    // Top face
    glm::vec3 nTop(0.0f, 0.0f, 1.0f);
    uint32_t topBase = static_cast<uint32_t>(vertices.size());
    vertices.push_back({glm::vec3(x0 + ox, y0 + oy, h), nTop, col, glm::vec2(0.0f)});   // 0
    vertices.push_back({glm::vec3(x1 - ox, y0 + oy, h), nTop, col, glm::vec2(0.0f)});   // 1
    vertices.push_back({glm::vec3(x1 - ox, y1 - oy, h), nTop, col, glm::vec2(0.0f)});   // 2
    vertices.push_back({glm::vec3(x0 + ox, y1 - oy, h), nTop, col, glm::vec2(0.0f)});   // 3

    // Two triangles for the top quad
    indices.push_back(topBase + 0); indices.push_back(topBase + 1); indices.push_back(topBase + 2);
    indices.push_back(topBase + 0); indices.push_back(topBase + 2); indices.push_back(topBase + 3);
}

// ============================================================================
// Build folder outline mesh (port of mapv_gldraw_folder)
// ============================================================================

void MapVLayout::buildFolderMesh(FsNode* dnode,
                                 std::vector<Vertex>& vertices,
                                 std::vector<uint32_t>& indices) {
    assert(dnode->isDir());

    // Obtain corners/dimensions of top face
    double dimsX = dnode->mapvWidth();
    double dimsY = dnode->mapvDepth();
    double k = sideSlantRatios[NODE_DIRECTORY];
    double offsetX = std::min(dnode->mapvGeom.height, k * dimsX);
    double offsetY = std::min(dnode->mapvGeom.height, k * dimsY);
    double c0x = dnode->mapvGeom.c0.x + offsetX;
    double c0y = dnode->mapvGeom.c0.y + offsetY;
    double c1x = dnode->mapvGeom.c1.x - offsetX;
    double c1y = dnode->mapvGeom.c1.y - offsetY;
    dimsX -= 2.0 * offsetX;
    dimsY -= 2.0 * offsetY;

    // Folder geometry
    double border = 0.0625 * std::min(dimsX, dimsY);
    double fc0x = c0x + border;
    double fc0y = c0y + border;
    double fc1x = c1x - border;
    double fc1y = c1y - border;
    // Coordinates of the concave vertex
    double ftabx = fc1x - (MAGIC_NUMBER - 1.0) * (fc1x - fc0x);
    double ftaby = fc1y - border;

    float h = static_cast<float>(dnode->mapvGeom.height);

    glm::vec3 col(0.7f, 0.7f, 0.7f);
    if (dnode->color) {
        col = glm::vec3(dnode->color->r, dnode->color->g, dnode->color->b);
    }

    glm::vec3 nUp(0.0f, 0.0f, 1.0f);

    // Build folder as a line strip approximated by thin quads (for GL_TRIANGLES)
    // Points of the folder outline (8 points, closed)
    struct Pt { float x, y; };
    Pt pts[] = {
        { float(fc0x), float(fc0y) },
        { float(fc0x), float(ftaby) },
        { float(fc0x + border), float(fc1y) },
        { float(ftabx - border), float(fc1y) },
        { float(ftabx), float(ftaby) },
        { float(fc1x), float(ftaby) },
        { float(fc1x), float(fc0y) },
        { float(fc0x), float(fc0y) }  // close
    };

    // Draw as a thin line strip using degenerate quads
    float lineWidth = static_cast<float>(border * 0.25);
    uint32_t base = static_cast<uint32_t>(vertices.size());

    for (int i = 0; i < 7; ++i) {
        float dx = pts[i + 1].x - pts[i].x;
        float dy = pts[i + 1].y - pts[i].y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6f) continue;

        // Perpendicular direction for line width
        float nx = -dy / len * lineWidth * 0.5f;
        float ny = dx / len * lineWidth * 0.5f;

        uint32_t vbase = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(pts[i].x + nx, pts[i].y + ny, h + 0.1f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(pts[i].x - nx, pts[i].y - ny, h + 0.1f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(pts[i+1].x + nx, pts[i+1].y + ny, h + 0.1f), nUp, col, glm::vec2(0.0f)});
        vertices.push_back({glm::vec3(pts[i+1].x - nx, pts[i+1].y - ny, h + 0.1f), nUp, col, glm::vec2(0.0f)});

        indices.push_back(vbase + 0); indices.push_back(vbase + 1); indices.push_back(vbase + 2);
        indices.push_back(vbase + 2); indices.push_back(vbase + 1); indices.push_back(vbase + 3);
    }
}

// ============================================================================
// Build directory children geometry (port of mapv_build_dir)
// ============================================================================

void MapVLayout::buildDir(FsNode* dnode,
                          std::vector<Vertex>& vertices,
                          std::vector<uint32_t>& indices) {
    assert(dnode->isDir() || dnode->isMetanode());

    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        buildNodeMesh(node, vertices, indices);
    }
}

// ============================================================================
// Draw (port of mapv_draw_recursive + mapv_draw)
// ============================================================================

void MapVLayout::drawRecursive(FsNode* dnode, const glm::mat4& view,
                               const glm::mat4& proj, bool geometry) {
    assert(dnode->isDir() || dnode->isMetanode());

    GeometryManager& gm = GeometryManager::instance();
    MatrixStack& ms = gm.modelStack();

    ms.push();
    ms.translate(0.0f, 0.0f, static_cast<float>(dnode->mapvGeom.height));

    bool dirCollapsed = dnode->isCollapsed();
    bool dirExpanded = dnode->isExpanded();

    if (!dirCollapsed && !dirExpanded) {
        // Grow/shrink children heightwise during deployment
        ms.scale(1.0f, 1.0f, static_cast<float>(dnode->deployment));
    }

    if (geometry) {
        // Draw directory face or geometry of children
        float nodeGlow = ThemeManager::instance().currentTheme().baseEmissive + dnode->glowIntensity;

        // Rebuild mesh each frame (to be optimized with mesh caching)
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        if (dirCollapsed) {
            buildFolderMesh(dnode, vertices, indices);
        } else {
            buildDir(dnode, vertices, indices);
        }

        if (!vertices.empty()) {
            ShaderProgram& shader = Renderer::instance().getNodeShader();
            shader.use();
            shader.setMat4("uModel", ms.top());
            shader.setMat4("uView", view);
            shader.setMat4("uProjection", proj);
            shader.setFloat("uGlowIntensity", nodeGlow);

            MeshBuffer mesh;
            mesh.upload(vertices, indices);
            mesh.draw(GL_TRIANGLES);
        }

        dnode->aDlistStale = false;
    }

    // Update geometry status
    dnode->geomExpanded = !dirCollapsed;

    if (!dirCollapsed) {
        // Recurse into subdirectories
        for (auto& childPtr : dnode->children) {
            FsNode* node = childPtr.get();
            if (!node->isDir())
                break;
            drawRecursive(node, view, proj, geometry);
        }
    }

    ms.pop();
}

void MapVLayout::draw(const glm::mat4& view, const glm::mat4& projection, bool highDetail) {
    FsTree& tree = FsTree::instance();
    FsNode* root = tree.root();
    if (!root) return;

    GeometryManager& gm = GeometryManager::instance();
    gm.modelStack().loadIdentity();

    // Draw geometry
    drawRecursive(root, view, projection, true);

    if (highDetail) {
        // High-detail: outlines, labels, cursor would go here
        // Labels and cursor drawing deferred to TextRenderer integration
    }
}

void MapVLayout::drawForPicking(const glm::mat4& view, const glm::mat4& projection) {
    FsTree& tree = FsTree::instance();
    FsNode* root = tree.root();
    if (!root) return;

    GeometryManager& gm = GeometryManager::instance();
    gm.modelStack().loadIdentity();

    // For picking, we draw each node with a unique color encoding its ID
    // This is a simplified picking pass - full implementation would encode
    // node IDs into the color channel of a picking framebuffer
    drawRecursive(root, view, projection, true);
}

void MapVLayout::drawNodeMesh(FsNode* node, const glm::mat4& model) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    buildNodeMesh(node, vertices, indices);

    if (!vertices.empty()) {
        ShaderProgram& shader = Renderer::instance().getNodeShader();
        shader.setMat4("uModel", model);

        MeshBuffer mesh;
        mesh.upload(vertices, indices);
        mesh.draw(GL_TRIANGLES);
    }
}

void MapVLayout::drawFolder(FsNode* dnode, const glm::mat4& model) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    buildFolderMesh(dnode, vertices, indices);

    if (!vertices.empty()) {
        ShaderProgram& shader = Renderer::instance().getNodeShader();
        shader.setMat4("uModel", model);

        MeshBuffer mesh;
        mesh.upload(vertices, indices);
        mesh.draw(GL_TRIANGLES);
    }
}

void MapVLayout::drawCursor(double pos, const glm::mat4& view, const glm::mat4& proj) {
    // Cursor drawing will be integrated with the cursor shader
    // Port of mapv_draw_cursor - interpolates between previous and current position
    (void)pos;
    (void)view;
    (void)proj;
}

} // namespace fsvng
