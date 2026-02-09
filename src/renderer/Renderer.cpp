#include "Renderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace fsvng {

Renderer& Renderer::instance() {
    static Renderer inst;
    return inst;
}

void Renderer::init() {
    if (initialized_) {
        return;
    }

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Set clear color to dark gray
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);

    // Enable MSAA if available
    glEnable(GL_MULTISAMPLE);

    // Enable back-face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Load shader programs
    loadShaders();

    initialized_ = true;

    std::cout << "Renderer: Initialized" << std::endl;
}

void Renderer::shutdown() {
    if (!initialized_) {
        return;
    }

    // Shader programs are cleaned up by their destructors
    nodeShader_ = ShaderProgram();
    pickingShader_ = ShaderProgram();
    textShader_ = ShaderProgram();
    cursorShader_ = ShaderProgram();

    initialized_ = false;

    std::cout << "Renderer: Shut down" << std::endl;
}

void Renderer::renderFrame(GLuint targetFBO, int width, int height) {
    // Bind the target framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);

    // Set viewport
    glViewport(0, 0, width, height);

    // Clear color and depth buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Compute aspect ratio
    float aspect = (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;

    // Set up projection matrix (perspective)
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100000.0f);

    // Set up view matrix (default identity -- will be overridden by Camera system)
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 500.0f, 1000.0f),  // Camera position (default)
        glm::vec3(0.0f, 0.0f, 0.0f),        // Look-at target
        glm::vec3(0.0f, 1.0f, 0.0f)         // Up vector
    );

    // Set up model matrix (identity for world-space geometry)
    glm::mat4 model = glm::mat4(1.0f);

    // Camera/eye position for specular calculations
    glm::vec3 viewPos(0.0f, 500.0f, 1000.0f);

    // Activate the node shader and set common uniforms
    nodeShader_.use();
    nodeShader_.setMat4("uModel", model);
    nodeShader_.setMat4("uView", view);
    nodeShader_.setMat4("uProjection", projection);
    nodeShader_.setVec3("uLightPos", lightPos_);
    nodeShader_.setVec3("uAmbient", ambientColor_);
    nodeShader_.setVec3("uDiffuse", diffuseColor_);
    nodeShader_.setVec3("uViewPos", viewPos);
    nodeShader_.setFloat("uHighlight", 0.0f);

    // Scene geometry will be drawn by the GeometryManager (to be integrated).
    // For now, this sets up the render state for external draw calls.

    nodeShader_.unuse();
}

void Renderer::loadShaders() {
    // Shader file paths (relative to working directory)
    const std::string shaderDir = "shaders/";

    if (!nodeShader_.loadFromFiles(shaderDir + "node.vert", shaderDir + "node.frag")) {
        std::cerr << "Renderer: Failed to load node shader" << std::endl;
    }

    if (!pickingShader_.loadFromFiles(shaderDir + "picking.vert", shaderDir + "picking.frag")) {
        std::cerr << "Renderer: Failed to load picking shader" << std::endl;
    }

    if (!textShader_.loadFromFiles(shaderDir + "text.vert", shaderDir + "text.frag")) {
        std::cerr << "Renderer: Failed to load text shader" << std::endl;
    }

    if (!cursorShader_.loadFromFiles(shaderDir + "cursor.vert", shaderDir + "cursor.frag")) {
        std::cerr << "Renderer: Failed to load cursor shader" << std::endl;
    }
}

void Renderer::setLightPosition(const glm::vec3& pos) {
    lightPos_ = pos;
}

} // namespace fsvng
