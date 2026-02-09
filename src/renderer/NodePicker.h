#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

namespace fsvng {

class NodePicker {
public:
    static NodePicker& instance();

    void init(int width, int height);
    void resize(int width, int height);
    void shutdown();

    // Render the picking pass
    void renderPick(int width, int height);

    // Read back the node ID at pixel (x, y)
    int pick(int x, int y);

    GLuint getFBO() const { return fbo_; }

    // Encode/decode node ID as color
    static glm::vec3 encodeId(unsigned int id);
    static unsigned int decodeId(unsigned char r, unsigned char g, unsigned char b);

private:
    NodePicker() = default;
    ~NodePicker() = default;

    // Non-copyable
    NodePicker(const NodePicker&) = delete;
    NodePicker& operator=(const NodePicker&) = delete;

    void createFBO(int width, int height);
    void destroyFBO();

    GLuint fbo_ = 0;
    GLuint colorTex_ = 0;
    GLuint depthRbo_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace fsvng
