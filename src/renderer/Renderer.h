#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "ShaderProgram.h"

namespace fsvng {

class Renderer {
public:
    static Renderer& instance();

    void init();
    void shutdown();

    // Render a frame to the given FBO (0 for default framebuffer)
    void renderFrame(GLuint targetFBO, int width, int height);

    // Get shaders
    ShaderProgram& getNodeShader() { return nodeShader_; }
    ShaderProgram& getPickingShader() { return pickingShader_; }
    ShaderProgram& getTextShader() { return textShader_; }
    ShaderProgram& getCursorShader() { return cursorShader_; }

    // Lighting
    void setLightPosition(const glm::vec3& pos);

private:
    Renderer() = default;
    ~Renderer() = default;

    // Non-copyable
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void loadShaders();

    ShaderProgram nodeShader_;
    ShaderProgram pickingShader_;
    ShaderProgram textShader_;
    ShaderProgram cursorShader_;

    glm::vec3 lightPos_{0.0f, 10000.0f, 10000.0f};
    glm::vec3 ambientColor_{0.2f, 0.2f, 0.2f};
    glm::vec3 diffuseColor_{0.5f, 0.5f, 0.5f};

    bool initialized_ = false;
};

} // namespace fsvng
