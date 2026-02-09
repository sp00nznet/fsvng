#include <gtest/gtest.h>
#include "core/Types.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

using namespace fsvng;

TEST(CameraTest, FieldDiameter) {
    // field_diameter = 2 * distance * tan(rad(0.5 * fov))
    double fov = 60.0;
    double distance = 1000.0;
    double expected = 2.0 * distance * std::tan(fov * 0.5 * PI / 180.0);
    EXPECT_NEAR(expected, 1154.7, 1.0);
}

TEST(CameraTest, FieldDistance) {
    // field_distance = diameter * (0.5 / tan(rad(0.5 * fov)))
    double fov = 60.0;
    double diameter = 1000.0;
    double expected = diameter * (0.5 / std::tan(fov * 0.5 * PI / 180.0));
    EXPECT_NEAR(expected, 866.0, 1.0);
}

TEST(CameraTest, ViewMatrix) {
    // Basic view matrix test
    glm::vec3 eye(0, 0, 1000);
    glm::vec3 target(0, 0, 0);
    glm::vec3 up(0, 1, 0);

    glm::mat4 view = glm::lookAt(eye, target, up);

    // Transform origin should map to (0,0,-1000) in view space
    glm::vec4 origin = view * glm::vec4(0, 0, 0, 1);
    EXPECT_NEAR(origin.z, -1000.0f, 1.0f);
}

TEST(CameraTest, ProjectionMatrix) {
    float fov = glm::radians(60.0f);
    float aspect = 16.0f / 9.0f;
    float near = 1.0f;
    float far = 10000.0f;

    glm::mat4 proj = glm::perspective(fov, aspect, near, far);

    // Verify it's not identity
    EXPECT_NE(proj[0][0], 1.0f);
    // Near plane z should map to -1 in NDC
    glm::vec4 nearPoint = proj * glm::vec4(0, 0, -near, 1);
    float ndcZ = nearPoint.z / nearPoint.w;
    EXPECT_NEAR(ndcZ, -1.0f, 0.01f);
}
