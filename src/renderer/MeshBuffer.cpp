#include "MeshBuffer.h"

#include <cstddef>

namespace fsvng {

MeshBuffer::~MeshBuffer() {
    destroy();
}

MeshBuffer::MeshBuffer(MeshBuffer&& other) noexcept
    : vao_(other.vao_)
    , vbo_(other.vbo_)
    , ebo_(other.ebo_)
    , vertexCount_(other.vertexCount_)
    , indexCount_(other.indexCount_)
{
    other.vao_ = 0;
    other.vbo_ = 0;
    other.ebo_ = 0;
    other.vertexCount_ = 0;
    other.indexCount_ = 0;
}

MeshBuffer& MeshBuffer::operator=(MeshBuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        ebo_ = other.ebo_;
        vertexCount_ = other.vertexCount_;
        indexCount_ = other.indexCount_;
        other.vao_ = 0;
        other.vbo_ = 0;
        other.ebo_ = 0;
        other.vertexCount_ = 0;
        other.indexCount_ = 0;
    }
    return *this;
}

void MeshBuffer::upload(const std::vector<Vertex>& vertices,
                        const std::vector<uint32_t>& indices) {
    uploadInternal(vertices, indices, GL_STATIC_DRAW);
}

void MeshBuffer::uploadDynamic(const std::vector<Vertex>& vertices,
                               const std::vector<uint32_t>& indices) {
    uploadInternal(vertices, indices, GL_DYNAMIC_DRAW);
}

void MeshBuffer::uploadInternal(const std::vector<Vertex>& vertices,
                                const std::vector<uint32_t>& indices,
                                GLenum usage) {
    // Clean up any existing buffers
    destroy();

    vertexCount_ = static_cast<int>(vertices.size());
    indexCount_ = static_cast<int>(indices.size());

    // Generate VAO
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    // Generate and upload VBO
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(),
                 usage);

    // Generate and upload EBO if indices provided
    if (!indices.empty()) {
        glGenBuffers(1, &ebo_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                     indices.data(),
                     usage);
    }

    // Set up vertex attribute pointers
    setupVAO();

    // Unbind VAO (but not EBO while VAO is bound)
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // EBO unbind must happen after VAO unbind
    if (ebo_ != 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

void MeshBuffer::update(const std::vector<Vertex>& vertices) {
    if (vbo_ == 0) {
        return;
    }

    vertexCount_ = static_cast<int>(vertices.size());

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER,
                    0,
                    static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                    vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MeshBuffer::draw(GLenum mode) const {
    if (vao_ == 0) {
        return;
    }

    glBindVertexArray(vao_);

    if (indexCount_ > 0) {
        glDrawElements(mode, indexCount_, GL_UNSIGNED_INT, nullptr);
    } else {
        glDrawArrays(mode, 0, vertexCount_);
    }

    glBindVertexArray(0);
}

void MeshBuffer::drawInstanced(int count, GLenum mode) const {
    if (vao_ == 0) {
        return;
    }

    glBindVertexArray(vao_);

    if (indexCount_ > 0) {
        glDrawElementsInstanced(mode, indexCount_, GL_UNSIGNED_INT, nullptr, count);
    } else {
        glDrawArraysInstanced(mode, 0, vertexCount_, count);
    }

    glBindVertexArray(0);
}

void MeshBuffer::destroy() {
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    vertexCount_ = 0;
    indexCount_ = 0;
}

void MeshBuffer::setupVAO() {
    // Attribute 0: position (3 floats)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, position)));

    // Attribute 1: normal (3 floats)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));

    // Attribute 2: color (3 floats)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, color)));

    // Attribute 3: texcoord (2 floats)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, texcoord)));
}

} // namespace fsvng
