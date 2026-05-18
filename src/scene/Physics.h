#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <array>

// One ball's physics state
struct BallState {
    glm::vec3 pos;
    glm::vec3 prevPos = glm::vec3(0.0f);  // position before last integration step (swept pocket detection)
    glm::vec3 vel     = glm::vec3(0.0f);
    glm::vec3 spin    = glm::vec3(0.0f);  // angular velocity ω (rad/s)
    glm::quat orient  = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    bool      active  = true;
    bool      sinking = false;   // true while the ball is animating into the pocket
    float     sinkT   = 0.0f;   // seconds elapsed since pocket capture
};

// Physics engine for snooker balls.
//
// Intersection tests based on:
//   Real-Time Rendering 4th ed., Ch22 (Akenine-Möller et al.)
//     - Sphere-sphere  → ball/ball collisions
//     - Sphere-plane   → ball/cushion bounces
//   PBR book 4th ed., Ch6.1 (Pharr, Jakob, Humphreys)
//     - Ray-sphere     → aiming guide (raySphereIntersect)
class Physics {
public:
    // Physical constants (snooker, SI units — table is in metres)
    static constexpr float RADIUS  = 0.022f;   // ball radius (m)
    static constexpr float MU_K    = 0.20f;    // kinetic/sliding friction (ball on felt)
    static constexpr float MU_R    = 0.02f;    // rolling friction
    static constexpr float E_BB    = 0.95f;    // ball-ball coefficient of restitution
    static constexpr float E_BC    = 0.80f;    // ball-cushion coefficient of restitution
    static constexpr float G       = 9.81f;    // gravity (m/s²)
    static constexpr float DT      = 1.0f / 120.0f;  // fixed physics timestep (s)
    static constexpr float V_STOP  = 0.010f;  // velocity threshold below which ball stops

    // Pocket animation
    static constexpr float SINK_SPEED    = 0.22f;   // m/s downward drop speed
    static constexpr float SINK_DURATION = 0.38f;   // seconds until ball disappears

    // Inner playing-area half-extents (must match Table.cpp constants)
    static constexpr float IX = 1.49f;
    static constexpr float IZ = 0.75f;

    // All 22 balls: index 0-6 = coloured, 7-21 = reds
    std::vector<BallState> balls;

    explicit Physics(const std::vector<glm::vec3>& initialPositions);

    // Advance simulation by frameTime seconds (handles fixed-timestep accumulation)
    void update(float frameTime);

    // Apply a cue strike to the cue ball with spin effect.
    // shotDir : normalised direction of the shot (XZ plane)
    // power   : initial speed (m/s)
    // u, v    : hit point on spin selector (each in [-1,1], constrained to unit circle):
    //             u > 0 → effet droit,   u < 0 → effet gauche
    //             v > 0 → coulé,         v < 0 → rétro
    // Back/top: spin = cross(up, shotDir) * power/R * v * 4  (stun at |v|≈0.62)
    // Side:     spin.y = -u * power/R * 2.5  (affects cushion rebounds)
    static void applyCueStrike(BallState& cueBall,
                               glm::vec3 shotDir, float power,
                               float u, float v);

    // Ray-sphere intersection (PBR book Ch6.1, numerically robust half-b form).
    // Returns true if ray hits sphere; tMin/tMax are the two hit distances.
    static bool raySphereIntersect(glm::vec3 origin, glm::vec3 dir,
                                   glm::vec3 center, float radius,
                                   float& tMin, float& tMax);

    // 6 pocket centres [x,z] and their capture radii (public for aim guide)
    static const std::array<glm::vec2, 6> POCKETS;
    static const std::array<float,    6> POCKET_R;

    float accumulator = 0.0f;

    // Shot tracking — reset by resetShotTracking() before each cue strike.
    int              shotFirstContact = -1;  // index of first ball touched by cue ball
    std::vector<int> shotPottedBalls;        // indices of all balls pocketed this shot
    void resetShotTracking();

private:
    void step();

    static void applyFriction(BallState& b);
    static void resolveCushions(BallState& b);
    static bool resolveBallBall(BallState& a, BallState& b);
    void checkPocket(BallState& b, int idx);
};
