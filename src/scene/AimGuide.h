#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include "Shader.h"
#include "Physics.h"

// Multi-group aim guide rendered with flat-color GL_LINES.
//
// Groups:
//   0 — WHITE  solid  2.5px : cue ball path (cueBall → ghostPos)
//   1 — WHITE  thin   1.0px : ghost ball circle at impact point
//   2 — CYAN   dashed 1.5px : target ball direction (collision normal)
//   3 — AMBER  dashed 1.5px : cue ball deflection after impact
//   4 — AMBER  thin   1.0px : power stick indicator (behind cue ball)
struct LineGroup {
    std::vector<glm::vec3> pts;
    glm::vec3 color     = glm::vec3(1.0f);
    float     lineWidth = 1.0f;
    int       ptCount   = 0;
};

class AimGuide {
public:
    AimGuide();
    ~AimGuide();

    // Recompute all line groups.
    //   cueBallPos : world position of the cue ball
    //   shotDir    : normalised shot direction (XZ plane)
    //   balls      : all BallStates (index 0 = cue ball, skipped in intersection)
    //   power      : current charge level in m/s (0 = not charging, no power line)
    void update(const glm::vec3& cueBallPos,
                const glm::vec3& shotDir,
                const std::vector<BallState>& balls,
                float power);

    // Draw all groups using a flat (unlit) shader.
    // Caller must have bound the shader and set view + projection uniforms.
    void draw(Shader& flatShader) const;

private:
    GLuint VAO, VBO;

    // 5 groups, initialised with their fixed colours and widths
    std::array<LineGroup, 5> groups = {{
        // 0: WHITE solid 2.5px — cue ball path (cueBall → ghostPos)
        { {}, glm::vec3(1.00f, 1.00f, 1.00f), 2.5f, 0 },
        // 1: WHITE thin  1.0px — ghost ball circle at impact point
        { {}, glm::vec3(0.90f, 0.90f, 0.90f), 1.0f, 0 },
        // 2: YELLOW solid 2.5px — target ball travel direction (the KEY line)
        { {}, glm::vec3(1.00f, 0.90f, 0.10f), 2.5f, 0 },
        // 3: AMBER dashed 1.5px — cue ball deflection after impact
        { {}, glm::vec3(1.00f, 0.55f, 0.05f), 1.5f, 0 },
        // 4: AMBER thin  1.5px — power stick indicator
        { {}, glm::vec3(1.00f, 0.65f, 0.10f), 1.5f, 0 },
    }};

    // Upload one group's pts to VBO (called inside draw)
    void upload(LineGroup& g);
};
