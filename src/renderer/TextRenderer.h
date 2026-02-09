#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <array>
#include <string>

#include "MeshBuffer.h"

namespace fsvng {

class TextRenderer {
public:
    static TextRenderer& instance();

    void init();
    void shutdown();

    // Render text as a textured quad in 3D space
    void drawText3D(const std::string& text, const glm::vec3& position,
                    float scale, const glm::vec3& color = glm::vec3(0.0f));

    // Render text on a curved path (for TreeV labels)
    void drawTextCurved(const std::string& text, const glm::vec3& center,
                        float radius, float startAngle, float scale,
                        const glm::vec3& color = glm::vec3(0.0f));

    float getTextWidth(const std::string& text, float scale) const;

private:
    TextRenderer() = default;
    ~TextRenderer() = default;

    // Non-copyable
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    void buildFontAtlas();

    GLuint fontTexture_ = 0;
    MeshBuffer textMesh_;

    // Character metrics
    struct CharInfo {
        float x0 = 0.0f;  // UV left
        float y0 = 0.0f;  // UV top
        float x1 = 0.0f;  // UV right
        float y1 = 0.0f;  // UV bottom
        float width = 0.0f;
        float advance = 0.0f;
    };
    std::array<CharInfo, 128> charInfo_{};
    float charHeight_ = 0.0f;

    bool initialized_ = false;

    // Font atlas dimensions
    static constexpr int kAtlasWidth = 256;
    static constexpr int kAtlasHeight = 256;
    static constexpr int kCharWidth = 8;
    static constexpr int kCharHeight = 16;
    static constexpr int kCharsPerRow = kAtlasWidth / kCharWidth;   // 32
    static constexpr int kNumRows = kAtlasHeight / kCharHeight;     // 16
};

} // namespace fsvng
