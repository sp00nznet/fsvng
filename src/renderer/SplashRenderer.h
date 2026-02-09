#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "MeshBuffer.h"

namespace fsvng {

class SplashRenderer {
public:
    static SplashRenderer& instance();

    void init();
    void shutdown();
    void draw(const glm::mat4& viewProj);
    void update(float dt);

    bool isComplete() const { return complete_; }

private:
    SplashRenderer() = default;
    ~SplashRenderer() = default;

    // Non-copyable
    SplashRenderer(const SplashRenderer&) = delete;
    SplashRenderer& operator=(const SplashRenderer&) = delete;

    void buildLogoMesh();

    MeshBuffer logoMesh_;
    float animTime_ = 0.0f;
    bool complete_ = false;
    bool initialized_ = false;

    // Animation duration in seconds
    static constexpr float kAnimDuration = 3.0f;
};

} // namespace fsvng
