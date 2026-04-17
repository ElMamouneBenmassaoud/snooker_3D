#include "Physics.h"
#include <cmath>
#include <algorithm>
#include <glm/gtc/quaternion.hpp>

// ---------------------------------------------------------------------------
// Pocket centres at the inner cushion boundary corners/midpoints.
// Using IX/IZ so they stay in sync with the cushion wall positions.
//   4 corners : (±IX, ±IZ)   r = 0.062
//   2 middle  : (  0, ±IZ)   r = 0.068  (slightly larger opening)
// ---------------------------------------------------------------------------
const std::array<glm::vec2, 6> Physics::POCKETS = {{
    { -Physics::IX, -Physics::IZ },   // far-left  corner
    {  0.00f,       -Physics::IZ },   // far       middle
    {  Physics::IX, -Physics::IZ },   // far-right corner
    { -Physics::IX,  Physics::IZ },   // near-left  corner
    {  0.00f,        Physics::IZ },   // near       middle
    {  Physics::IX,  Physics::IZ },   // near-right corner
}};

const std::array<float, 6> Physics::POCKET_R = {{
    0.062f, 0.068f, 0.062f,
    0.062f, 0.068f, 0.062f,
}};

// ---------------------------------------------------------------------------
Physics::Physics(const std::vector<glm::vec3>& initialPositions) {
    balls.resize(initialPositions.size());
    for (int i = 0; i < (int)initialPositions.size(); i++)
        balls[i].pos = initialPositions[i];
}

// ---------------------------------------------------------------------------
void Physics::update(float frameTime) {
    accumulator += frameTime;
    while (accumulator >= DT) {
        step();
        accumulator -= DT;
    }
}

// ---------------------------------------------------------------------------
void Physics::step() {
    // 0. Pocket-sink animation (runs independently of physics)
    for (auto& b : balls) {
        if (!b.sinking) continue;
        b.pos.y -= SINK_SPEED * DT;
        b.sinkT  += DT;
        if (b.sinkT >= SINK_DURATION) {
            b.active  = false;
            b.sinking = false;
        }
    }

    // 1. Save previous positions, integrate, apply friction and orientation
    for (auto& b : balls) {
        if (!b.active || b.sinking) continue;
        b.prevPos = b.pos;                   // used by swept pocket detection below
        b.pos += b.vel * DT;
        applyFriction(b);
        float omega = glm::length(b.spin);
        if (omega > 0.001f)
            b.orient = glm::normalize(glm::angleAxis(omega * DT, b.spin / omega) * b.orient);
    }

    // 2. Pocket detection — BEFORE cushion resolution.
    //    Uses swept check (prevPos → pos) to catch fast balls that would
    //    otherwise tunnel through the pocket capture zone in one step.
    for (auto& b : balls) {
        if (!b.active || b.sinking) continue;
        checkPocket(b);
    }

    // 3. Sphere-plane: cushion collisions (RTR Ch22 — sphere/plane intersection)
    //    Pocket openings are skipped inside resolveCushions.
    for (auto& b : balls) {
        if (!b.active || b.sinking) continue;
        resolveCushions(b);
    }

    // 4. Sphere-sphere: ball/ball collisions (RTR Ch22 — sphere/sphere intersection)
    for (int i = 0; i < (int)balls.size(); i++) {
        if (!balls[i].active || balls[i].sinking) continue;
        for (int j = i + 1; j < (int)balls.size(); j++) {
            if (!balls[j].active || balls[j].sinking) continue;
            resolveBallBall(balls[i], balls[j]);
        }
    }
}

// ---------------------------------------------------------------------------
// Two-regime friction model with spin (angular velocity ω).
//
// Contact point directly below centre: r_c = (0, -R, 0)
// Velocity at contact = v_cm + ω × r_c
// Slip velocity (XZ only) = v_contact projected onto the table plane
//
// Sliding regime (|v_slip| > ε):
//   friction force  f = -μ_k·g · normalize(v_slip)
//   linear accel:   Δv   = f · dt
//   angular accel:  Δω   = (r_c × f) / I · dt,   I/m = (2/5)·R²
//
// Rolling regime (|v_slip| ≤ ε):
//   enforce rolling constraint: ω.z = -v.x/R, ω.x = v.z/R
//   apply rolling friction to slow v_cm down
// ---------------------------------------------------------------------------
void Physics::applyFriction(BallState& b) {
    static const glm::vec3 R_C(0.0f, -RADIUS, 0.0f);          // contact point
    static const float     I_INV = 5.0f / (2.0f * RADIUS * RADIUS); // 1/(I/m)

    // Velocity at contact point
    glm::vec3 v_contact = b.vel + glm::cross(b.spin, R_C);
    glm::vec3 v_slip    = glm::vec3(v_contact.x, 0.0f, v_contact.z);
    float     slip      = glm::length(v_slip);

    if (slip > 0.001f) {
        // --- Sliding regime ---
        // Clamp friction so the coupled (vel+spin) slip never overshoots zero.
        // The coupled system reduces slip by (7/2)*f*DT per step, so the maximum
        // force that brings slip exactly to zero without reversing is slip/(3.5*DT).
        float f_mag = std::min(MU_K * G, slip / (3.5f * DT));
        glm::vec3 f  = -glm::normalize(v_slip) * f_mag;
        b.vel  += f * DT;
        b.spin += glm::cross(R_C, f) * I_INV * DT;
    } else {
        // --- Rolling regime ---
        float speed = glm::length(b.vel);
        if (speed < V_STOP) {
            b.vel  = glm::vec3(0.0f);
            b.spin = glm::vec3(0.0f);
            return;
        }
        // Enforce rolling constraint
        b.spin.z = -b.vel.x / RADIUS;
        b.spin.x =  b.vel.z / RADIUS;

        float decel = MU_R * G * DT;
        if (decel >= speed)
            b.vel = glm::vec3(0.0f);
        else
            b.vel -= glm::normalize(b.vel) * decel;
    }
}

// ---------------------------------------------------------------------------
// Apply a cue strike to the cue ball with spin effect.
//
// The hit point (u, v) on the cue ball face (each ∈ [-0.5, 0.5]):
//   u > 0 → massé droit   u < 0 → massé gauche
//   v > 0 → coulé         v < 0 → rétro
//
// hitOffset = u·right·R + v·up·R   (offset from ball centre to cue tip contact)
// spin = cross(hitOffset, shotDir·power) / (I/m),  I/m = (2/5)·R²
// ---------------------------------------------------------------------------
void Physics::applyCueStrike(BallState& cueBall,
                              glm::vec3 shotDir, float power,
                              float u, float v) {
    shotDir = glm::normalize(shotDir);

    // Local frame of the shot
    glm::vec3 up    = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(shotDir, up));

    // Hit offset on cue ball face
    glm::vec3 hitOffset = (u * right + v * up) * RADIUS;

    // Linear velocity
    cueBall.vel = shotDir * power;

    // Angular velocity:  ω = (hitOffset × F) / I,  F = shotDir·power (impulse/mass)
    float I_inv = 5.0f / (2.0f * RADIUS * RADIUS);
    cueBall.spin = glm::cross(hitOffset, shotDir * power) * I_inv;
}

// ---------------------------------------------------------------------------
// Sphere-plane intersection (RTR Ch22):
//   signed distance from ball centre to each wall plane = ball coord - wall offset
//   collision when |dist| < RADIUS  AND  ball moving toward the wall
//   response: reflect normal component scaled by restitution E_BC
// ---------------------------------------------------------------------------
void Physics::resolveCushions(BallState& b) {
    // Gap = distance along the wall within which the cushion is open for a pocket.
    // Kept tight: the swept pocket detection in checkPocket handles tunnelling.
    const float CORNER_GAP = POCKET_R[0];          // ~0.062 m each side of pocket
    const float MIDDLE_GAP = POCKET_R[1] * 1.1f;  // ~0.075 m each side

    // Left cushion  (x = -IX)
    if (b.pos.x < -IX + RADIUS) {
        bool gap = (std::abs(b.pos.z - (-IZ)) < CORNER_GAP ||
                    std::abs(b.pos.z -   IZ)  < CORNER_GAP);
        if (!gap) { b.pos.x = -IX + RADIUS; if (b.vel.x < 0.0f) b.vel.x = -b.vel.x * E_BC; }
    }
    // Right cushion (x = +IX)
    if (b.pos.x > IX - RADIUS) {
        bool gap = (std::abs(b.pos.z - (-IZ)) < CORNER_GAP ||
                    std::abs(b.pos.z -   IZ)  < CORNER_GAP);
        if (!gap) { b.pos.x = IX - RADIUS; if (b.vel.x > 0.0f) b.vel.x = -b.vel.x * E_BC; }
    }
    // Far cushion   (z = -IZ)
    if (b.pos.z < -IZ + RADIUS) {
        bool gap = (std::abs(b.pos.x - (-IX)) < CORNER_GAP ||
                    std::abs(b.pos.x        ) < MIDDLE_GAP ||
                    std::abs(b.pos.x -   IX)  < CORNER_GAP);
        if (!gap) { b.pos.z = -IZ + RADIUS; if (b.vel.z < 0.0f) b.vel.z = -b.vel.z * E_BC; }
    }
    // Near cushion  (z = +IZ)
    if (b.pos.z > IZ - RADIUS) {
        bool gap = (std::abs(b.pos.x - (-IX)) < CORNER_GAP ||
                    std::abs(b.pos.x        ) < MIDDLE_GAP ||
                    std::abs(b.pos.x -   IX)  < CORNER_GAP);
        if (!gap) { b.pos.z = IZ - RADIUS; if (b.vel.z > 0.0f) b.vel.z = -b.vel.z * E_BC; }
    }
}

// ---------------------------------------------------------------------------
// Sphere-sphere collision with Continuous Collision Detection (CCD).
//
// At max power (6 m/s) a ball moves 6/120 = 0.05 m per step, larger than
// the ball diameter (0.044 m).  A discrete end-of-step test misses the
// collision entirely (tunnelling).  CCD tests the swept path
// prevPos → pos using the quadratic:
//
//   |d0 + t·dv|² = (2R)²
//   where d0 = a.prevPos - b.prevPos,  dv = ΔposA - ΔposB
//
// Case 1 — balls already overlapping at step start (c0 ≤ 0):
//   standard impulse + push-apart at current positions.
// Case 2 — balls separate at step start, CCD finds contact at t ∈ [0,1]:
//   apply impulse at contact point, advance both balls the remaining
//   (1-t)·DT with the post-impulse velocities.
// Case 3 — no intersection this step: skip.
// ---------------------------------------------------------------------------
void Physics::resolveBallBall(BallState& a, BallState& b) {
    const float minD = 2.0f * RADIUS;

    glm::vec3 d0 = a.prevPos - b.prevPos;          // relative pos at step start
    float     c0 = glm::dot(d0, d0) - minD * minD; // > 0 → separate, ≤ 0 → overlap

    if (c0 <= 0.0f) {
        // --- Case 1: already overlapping → standard resolution ---
        glm::vec3 diff  = a.pos - b.pos;
        float     dist2 = glm::dot(diff, diff);
        if (dist2 < 1e-8f) return;
        float     dist  = std::sqrt(dist2);
        glm::vec3 n     = diff / dist;
        float vRel = glm::dot(a.vel - b.vel, n);
        if (vRel < 0.0f) {
            float j = -(1.0f + E_BB) * vRel * 0.5f;
            a.vel += j * n;
            b.vel -= j * n;
        }
        float ov = minD - dist;
        if (ov > 0.0f) { a.pos += n * (ov * 0.5f); b.pos -= n * (ov * 0.5f); }
        return;
    }

    // --- Case 2: CCD swept test ---
    glm::vec3 dv = (a.pos - a.prevPos) - (b.pos - b.prevPos); // relative displacement
    float A  = glm::dot(dv, dv);
    if (A < 1e-12f) return;                        // no relative motion
    float B    = 2.0f * glm::dot(d0, dv);
    float disc = B * B - 4.0f * A * c0;
    if (disc < 0.0f) return;                       // trajectories miss each other

    float t = (-B - std::sqrt(disc)) / (2.0f * A); // earliest contact in [0,1]
    if (t < 0.0f || t > 1.0f) return;

    // Position of both balls at contact time t
    glm::vec3 aT = a.prevPos + (a.pos - a.prevPos) * t;
    glm::vec3 bT = b.prevPos + (b.pos - b.prevPos) * t;
    glm::vec3 n  = aT - bT;
    float     d  = glm::length(n);
    if (d < 1e-8f) return;
    n /= d;

    // Impulse using post-friction velocities
    float vRel = glm::dot(a.vel - b.vel, n);
    if (vRel >= 0.0f) return;                      // already separating

    float j = -(1.0f + E_BB) * vRel * 0.5f;
    a.vel += j * n;
    b.vel -= j * n;

    // Reposition: place at contact, then advance remaining step time
    float rem = (1.0f - t) * DT;
    a.pos = aT + a.vel * rem;
    b.pos = bT + b.vel * rem;
}

// ---------------------------------------------------------------------------
// Pocket detection with swept check:
//   1. Direct distance test at current position.
//   2. Closest-point-on-segment test (prevPos → pos) to catch fast balls that
//      tunnel through the capture radius in a single step.
//   3. Safety boundary: ball escaped the table → sink unconditionally.
// ---------------------------------------------------------------------------
void Physics::checkPocket(BallState& b) {
    auto sink = [&]() {
        b.sinking = true; b.sinkT = 0.0f;
        b.vel = glm::vec3(0.0f); b.spin = glm::vec3(0.0f);
    };

    for (int i = 0; i < 6; i++) {
        const float pr  = POCKET_R[i];
        const float px  = POCKETS[i].x;
        const float pz  = POCKETS[i].y;

        // --- 1. Direct check ---
        float dx = b.pos.x - px, dz = b.pos.z - pz;
        if (dx*dx + dz*dz < pr*pr) { sink(); return; }

        // --- 2. Swept check (prevPos → pos segment vs pocket circle in XZ) ---
        glm::vec3 seg  = b.pos - b.prevPos;
        float     len2 = glm::dot(seg, seg);
        if (len2 > 1e-8f) {
            glm::vec3 pk(px, 0.0f, pz);
            float t = glm::dot(pk - b.prevPos, seg) / len2;
            t = std::max(0.0f, std::min(1.0f, t));
            glm::vec3 c = b.prevPos + seg * t;
            float cdx = c.x - px, cdz = c.z - pz;
            if (cdx*cdx + cdz*cdz < pr*pr) { sink(); return; }
        }
    }

    // --- 3. Safety: ball completely escaped table geometry ---
    if (std::abs(b.pos.x) > IX + 0.12f || std::abs(b.pos.z) > IZ + 0.12f)
        sink();
}

// ---------------------------------------------------------------------------
// Ray-sphere intersection — PBR book 4th ed., Ch6.1
//   Numerically robust half-b form to avoid catastrophic cancellation.
//   Solves: a*t² + 2*hb*t + c = 0
// ---------------------------------------------------------------------------
bool Physics::raySphereIntersect(glm::vec3 origin, glm::vec3 dir,
                                  glm::vec3 center, float radius,
                                  float& tMin, float& tMax) {
    glm::vec3 oc  = origin - center;
    float     a   = glm::dot(dir, dir);
    float     hb  = glm::dot(oc, dir);         // half b
    float     c   = glm::dot(oc, oc) - radius * radius;
    float     disc = hb * hb - a * c;

    if (disc < 0.0f) return false;

    float sq = std::sqrt(disc);
    tMin = (-hb - sq) / a;
    tMax = (-hb + sq) / a;
    return true;
}
