#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace fsvng {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 texcoord;
};

class MeshBuffer {
public:
    MeshBuffer() = default;
    ~MeshBuffer();

    // Non-copyable
    MeshBuffer(const MeshBuffer&) = delete;
    MeshBuffer& operator=(const MeshBuffer&) = delete;

    // Movable
    MeshBuffer(MeshBuffer&& other) noexcept;
    MeshBuffer& operator=(MeshBuffer&& other) noexcept;

    // Upload vertex data (static draw hint)
    void upload(const std::vector<Vertex>& vertices,
                const std::vector<uint32_t>& indices = {});

    // Upload with dynamic hint (for frequently updated meshes)
    void uploadDynamic(const std::vector<Vertex>& vertices,
                       const std::vector<uint32_t>& indices = {});

    // Update existing buffer data (must have been uploaded first)
    void update(const std::vector<Vertex>& vertices);

    void draw(GLenum mode = GL_TRIANGLES) const;
    void drawInstanced(int count, GLenum mode = GL_TRIANGLES) const;

    bool isValid() const { return vao_ != 0; }

    void destroy();

private:
    void setupVAO();
    void uploadInternal(const std::vector<Vertex>& vertices,
                        const std::vector<uint32_t>& indices,
                        GLenum usage);

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    int vertexCount_ = 0;
    int indexCount_ = 0;
};

} // namespace fsvng
