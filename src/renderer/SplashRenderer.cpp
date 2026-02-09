#include "SplashRenderer.h"
#include "Renderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace fsvng {

// ---------------------------------------------------------------------------
// Simple 3D block-letter definitions for "FSVNG"
// Each letter is defined as a set of box segments (quads).
// Coordinates are in a local grid where each unit cell is 1.0.
// Letters are roughly 5 units wide and 7 units tall.
// ---------------------------------------------------------------------------

struct BoxSegment {
    float x, y, w, h; // position and size in the letter's local 2D grid
};

// Letter definitions as horizontal/vertical bar segments
// clang-format off
static const BoxSegment kLetterF[] = {
    {0, 6, 5, 1}, // top bar
    {0, 3, 4, 1}, // middle bar
    {0, 0, 1, 7}, // vertical bar
};
static const BoxSegment kLetterS[] = {
    {0, 6, 5, 1}, // top bar
    {0, 5, 1, 1}, // upper-left
    {0, 3, 5, 1}, // middle bar
    {4, 1, 1, 1}, // lower-right
    {0, 0, 5, 1}, // bottom bar
};
static const BoxSegment kLetterV[] = {
    {0, 3, 1, 4}, // left vertical (upper)
    {1, 1, 1, 2}, // left diagonal part
    {2, 0, 1, 1}, // bottom center
    {3, 1, 1, 2}, // right diagonal part
    {4, 3, 1, 4}, // right vertical (upper)
};
static const BoxSegment kLetterN[] = {
    {0, 0, 1, 7}, // left vertical
    {1, 5, 1, 1}, // upper diagonal
    {2, 3, 1, 2}, // mid diagonal
    {3, 1, 1, 2}, // lower diagonal
    {4, 0, 1, 7}, // right vertical
};
static const BoxSegment kLetterG[] = {
    {0, 0, 5, 1}, // bottom bar
    {0, 6, 5, 1}, // top bar
    {0, 0, 1, 7}, // left vertical
    {4, 0, 1, 4}, // right vertical (lower)
    {2, 3, 3, 1}, // middle-right bar
};
// clang-format on

struct LetterDef {
    const BoxSegment* segments;
    int count;
    glm::vec3 color;
};

static const LetterDef kLetters[] = {
    { kLetterF, 3, glm::vec3(0.2f, 0.6f, 1.0f) },  // F - blue
    { kLetterS, 5, glm::vec3(0.2f, 0.8f, 0.3f) },  // S - green
    { kLetterV, 5, glm::vec3(1.0f, 0.8f, 0.1f) },  // V - yellow
    { kLetterN, 5, glm::vec3(1.0f, 0.4f, 0.1f) },  // N - orange
    { kLetterG, 5, glm::vec3(0.8f, 0.2f, 0.2f) },  // G - red
};
static constexpr int kLetterCount = 5;

// Helper to add a 3D box (6 faces, 24 vertices, 36 indices)
static void addBox(std::vector<Vertex>& verts, std::vector<uint32_t>& idxs,
                   float x, float y, float z,
                   float w, float h, float d,
                   const glm::vec3& color) {
    uint32_t base = static_cast<uint32_t>(verts.size());

    // 8 corners
    glm::vec3 corners[8] = {
        {x,     y,     z    }, // 0: front-bottom-left
        {x + w, y,     z    }, // 1: front-bottom-right
        {x + w, y + h, z    }, // 2: front-top-right
        {x,     y + h, z    }, // 3: front-top-left
        {x,     y,     z + d}, // 4: back-bottom-left
        {x + w, y,     z + d}, // 5: back-bottom-right
        {x + w, y + h, z + d}, // 6: back-top-right
        {x,     y + h, z + d}, // 7: back-top-left
    };

    // Face normals
    struct Face {
        int v[4];
        glm::vec3 normal;
    };

    Face faces[6] = {
        {{0, 1, 2, 3}, { 0,  0, -1}}, // front
        {{5, 4, 7, 6}, { 0,  0,  1}}, // back
        {{4, 0, 3, 7}, {-1,  0,  0}}, // left
        {{1, 5, 6, 2}, { 1,  0,  0}}, // right
        {{3, 2, 6, 7}, { 0,  1,  0}}, // top
        {{4, 5, 1, 0}, { 0, -1,  0}}, // bottom
    };

    for (int f = 0; f < 6; ++f) {
        uint32_t fBase = static_cast<uint32_t>(verts.size());
        for (int v = 0; v < 4; ++v) {
            Vertex vert;
            vert.position = corners[faces[f].v[v]];
            vert.normal = faces[f].normal;
            vert.color = color;
            vert.texcoord = glm::vec2(0.0f);
            verts.push_back(vert);
        }
        // Two triangles per face
        idxs.push_back(fBase + 0);
        idxs.push_back(fBase + 1);
        idxs.push_back(fBase + 2);
        idxs.push_back(fBase + 0);
        idxs.push_back(fBase + 2);
        idxs.push_back(fBase + 3);
    }
}

SplashRenderer& SplashRenderer::instance() {
    static SplashRenderer inst;
    return inst;
}

void SplashRenderer::init() {
    if (initialized_) {
        return;
    }

    buildLogoMesh();
    animTime_ = 0.0f;
    complete_ = false;
    initialized_ = true;

    std::cout << "SplashRenderer: Initialized" << std::endl;
}

void SplashRenderer::shutdown() {
    if (!initialized_) {
        return;
    }

    logoMesh_.destroy();
    initialized_ = false;

    std::cout << "SplashRenderer: Shut down" << std::endl;
}

void SplashRenderer::buildLogoMesh() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float letterSpacing = 7.0f;  // space between letter origins
    float depth = 2.0f;          // extrusion depth
    float blockScale = 10.0f;    // scale factor for each unit

    // Center the entire word
    float totalWidth = kLetterCount * letterSpacing - (letterSpacing - 5.0f);
    float offsetX = -totalWidth * blockScale * 0.5f;
    float offsetY = -3.5f * blockScale; // center vertically (letters are 7 units tall)

    for (int li = 0; li < kLetterCount; ++li) {
        const LetterDef& ld = kLetters[li];
        float baseX = offsetX + static_cast<float>(li) * letterSpacing * blockScale;

        for (int si = 0; si < ld.count; ++si) {
            const BoxSegment& seg = ld.segments[si];
            float bx = baseX + seg.x * blockScale;
            float by = offsetY + seg.y * blockScale;
            float bz = -depth * blockScale * 0.5f;
            float bw = seg.w * blockScale;
            float bh = seg.h * blockScale;
            float bd = depth * blockScale;

            addBox(vertices, indices, bx, by, bz, bw, bh, bd, ld.color);
        }
    }

    logoMesh_.upload(vertices, indices);
}

void SplashRenderer::update(float dt) {
    if (complete_) {
        return;
    }

    animTime_ += dt;
    if (animTime_ >= kAnimDuration) {
        animTime_ = kAnimDuration;
        complete_ = true;
    }
}

void SplashRenderer::draw(const glm::mat4& viewProj) {
    if (!initialized_) {
        return;
    }

    // Compute animation parameter [0, 1]
    float t = std::min(animTime_ / kAnimDuration, 1.0f);

    // Ease-out animation
    float ease = 1.0f - (1.0f - t) * (1.0f - t);

    // Build model matrix with animation:
    // - Start scaled down and rotated, end at identity
    float scale = 0.3f + 0.7f * ease;
    float rotation = (1.0f - ease) * 360.0f; // spin during animation

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scale));

    // Use the node shader for lit rendering
    ShaderProgram& shader = Renderer::instance().getNodeShader();
    shader.use();
    shader.setMat4("uModel", model);
    shader.setFloat("uHighlight", 0.0f);

    logoMesh_.draw(GL_TRIANGLES);

    shader.unuse();
}

} // namespace fsvng
