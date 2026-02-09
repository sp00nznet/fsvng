#include "NodePicker.h"
#include "Renderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace fsvng {

NodePicker& NodePicker::instance() {
    static NodePicker inst;
    return inst;
}

void NodePicker::init(int width, int height) {
    createFBO(width, height);
    std::cout << "NodePicker: Initialized (" << width << "x" << height << ")" << std::endl;
}

void NodePicker::resize(int width, int height) {
    if (width == width_ && height == height_) {
        return;
    }
    destroyFBO();
    createFBO(width, height);
}

void NodePicker::shutdown() {
    destroyFBO();
    std::cout << "NodePicker: Shut down" << std::endl;
}

void NodePicker::createFBO(int width, int height) {
    width_ = width;
    height_ = height;

    // Create framebuffer
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    // Create color texture attachment (RGB8 for ID encoding)
    glGenTextures(1, &colorTex_);
    glBindTexture(GL_TEXTURE_2D, colorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);

    // Create depth renderbuffer
    glGenRenderbuffers(1, &depthRbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo_);

    // Check framebuffer completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "NodePicker: Framebuffer incomplete, status: 0x"
                  << std::hex << status << std::dec << std::endl;
    }

    // Unbind
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void NodePicker::destroyFBO() {
    if (colorTex_ != 0) {
        glDeleteTextures(1, &colorTex_);
        colorTex_ = 0;
    }
    if (depthRbo_ != 0) {
        glDeleteRenderbuffers(1, &depthRbo_);
        depthRbo_ = 0;
    }
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    width_ = 0;
    height_ = 0;
}

void NodePicker::renderPick(int width, int height) {
    // Resize FBO if needed
    resize(width, height);

    // Bind picking FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width, height);

    // Clear with black (ID 0 = no node)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Disable blending for picking (need exact colors)
    glDisable(GL_BLEND);

    // Activate picking shader
    ShaderProgram& shader = Renderer::instance().getPickingShader();
    shader.use();

    // Set up view/projection matrices (same as main render pass).
    // These should match the main camera. For now, set up defaults
    // that match Renderer::renderFrame defaults.
    float aspect = (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100000.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 500.0f, 1000.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    shader.setMat4("uView", view);
    shader.setMat4("uProjection", projection);
    shader.setMat4("uModel", glm::mat4(1.0f));

    // Scene geometry will be drawn by the GeometryManager with picking colors.
    // Each node sets uPickColor = encodeId(nodeId) before drawing.

    shader.unuse();

    // Re-enable blending
    glEnable(GL_BLEND);

    // Restore clear color
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);

    // Unbind FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int NodePicker::pick(int x, int y) {
    if (fbo_ == 0) {
        return 0;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);

    // Flip y coordinate (OpenGL origin is bottom-left)
    int glY = height_ - y - 1;

    unsigned char pixel[3] = {0, 0, 0};
    glReadPixels(x, glY, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    return static_cast<int>(decodeId(pixel[0], pixel[1], pixel[2]));
}

glm::vec3 NodePicker::encodeId(unsigned int id) {
    float r = static_cast<float>(id & 0xFF) / 255.0f;
    float g = static_cast<float>((id >> 8) & 0xFF) / 255.0f;
    float b = static_cast<float>((id >> 16) & 0xFF) / 255.0f;
    return glm::vec3(r, g, b);
}

unsigned int NodePicker::decodeId(unsigned char r, unsigned char g, unsigned char b) {
    return static_cast<unsigned int>(r)
         | (static_cast<unsigned int>(g) << 8)
         | (static_cast<unsigned int>(b) << 16);
}

} // namespace fsvng
