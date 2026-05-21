#include "Physics.h"
#include <cmath>
#include <algorithm>
#include <glm/gtc/quaternion.hpp>

// Pocket centres at the inner cushion boundary corners/midpoints.
// Using IX/IZ so they stay in sync with the cushion wall positions.
//   4 corners : (±IX, ±IZ)   r = 0.062
//   2 middle  : (  0, ±IZ)   r = 0.068  (slightly larger opening)
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

void Physics::resetShotTracking() {
    shotFirstContact = -1;
    shotPottedBalls.clear();
}

Physics::Physics(const std::vector<glm::vec3>& initialPositions) {
    balls.resize(initialPositions.size());
    for (int i = 0; i < (int)initialPositions.size(); i++)
        balls[i].pos = initialPositions[i];
}

void Physics::update(float frameTime) {
    accumulator += frameTime;
    while (accumulator >= DT) {
        step();
        accumulator -= DT;
    }
}

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
    for (int i = 0; i < (int)balls.size(); i++) {
        if (!balls[i].active || balls[i].sinking) continue;
        checkPocket(balls[i], i);
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
            bool hit = resolveBallBall(balls[i], balls[j]);
            // Track first ball touched by the cue ball (index 0)
            if (hit && shotFirstContact == -1 && (i == 0 || j == 0))
                shotFirstContact = (i == 0) ? j : i;
        }
    }
}

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
void Physics::applyFriction(BallState& b) {
    static const glm::vec3 R_C(0.0f, -RADIUS, 0.0f);          // contact point
    static const float     I_INV = 5.0f / (2.0f * RADIUS * RADIUS); // 1/(I/m)

    // Velocity at contact point
    glm::vec3 v_contact = b.vel + glm::cross(b.spin, R_C);
    glm::vec3 v_slip    = glm::vec3(v_contact.x, 0.0f, v_contact.z);
    float     slip      = glm::length(v_slip);

    if (slip > 0.001f) {
        // Sliding regime: clamp friction so the coupled (vel+spin) slip never overshoots zero.
        // The coupled system reduces slip by (7/2)*f*DT per step, so the maximum
        // force that brings slip exactly to zero without reversing is slip/(3.5*DT).
        float f_mag = std::min(MU_K * G, slip / (3.5f * DT));
        glm::vec3 f  = -glm::normalize(v_slip) * f_mag;
        b.vel  += f * DT;
        b.spin += glm::cross(R_C, f) * I_INV * DT;
    } else {
        // Rolling regime
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

// Apply a cue strike to the cue ball.
//
// u, v ∈ [-1, 1] (spin selector, constrained to unit circle):
//   v > 0 → coulé (topspin)   v < 0 → rétro (backspin)
//   u > 0 → effet droit       u < 0 → effet gauche
//
// Back/top spin model:
//   ω_roll = cross(up, shotDir) * power/R  — rolling spin for this shot direction
//   cueBall.spin = ω_roll * v * SPIN_SCALE
//
//   SPIN_SCALE = 2.5:
//     • |v| < 1.0   → ball rolls forward after sliding (decelerates more with spin)
//     • |v| = 1.0   → stun (ball stops dead — NO reversal without a collision)
//     After a ball-ball collision the cue ball vel drops to ~2.5 % of v0
//     while spin is unchanged → spin >> stun threshold → strong rétro. ✓
//
//   Derivation of stun threshold:
//     Linear decel:  a_v  = μk·g
//     Angular decel: a_ω  = 5·μk·g / (2R)    (torque from table friction)
//     Stun when both reach zero simultaneously:
//       v0 / a_v = ω0 / a_ω   →   ω0_stun = 2.5 · v0/R
//     With SPIN_SCALE = 2.5: ω_max = 2.5·v0/R = ω0_stun  → stun at |v|=1
//
// Side spin:
//   cueBall.spin.y = -u * power/R * SIDE_SCALE
//   Affects cushion rebounds (running / check side). No table-felt interaction
//   because ω_y × r_c = 0 at the contact point r_c = (0, -R, 0).
void Physics::applyCueStrike(BallState& cueBall,
                              glm::vec3 shotDir, float power,
                              float u, float v) {
    shotDir = glm::normalize(shotDir);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    // Linear velocity (main impulse)
    cueBall.vel  = shotDir * power;
    cueBall.spin = glm::vec3(0.0f);

    // Back / top spin (v axis): ω_roll is the angular velocity for pure rolling in the shot direction.
    // Rolling constraint in applyFriction:  spin.z = -vel.x/R, spin.x = vel.z/R
    //   →  ω_roll = cross(up, shotDir) * power/R
    const float SPIN_SCALE = 1.8f;
    glm::vec3 omega_roll = glm::cross(up, shotDir) * (power / RADIUS);
    cueBall.spin += omega_roll * (v * SPIN_SCALE);

    // Side spin (u axis): rotates ball around vertical Y-axis, manifests at cushion rebounds.
    // u > 0 → effet droit (ωy > 0): ball deflects RIGHT on far/near cushions
    // u < 0 → effet gauche (ωy < 0): ball deflects LEFT on far/near cushions
    const float SIDE_SCALE = 0.7f;
    cueBall.spin.y = u * (power / RADIUS) * SIDE_SCALE;
}

// Sphere-plane: cushion collisions with side-spin deflection (RTR Ch22).
//
// Normal reflection: v_normal *= -E_BC.
//
// Side-spin (ω_y) deflection — spin-only contribution, no translational
// component, to avoid amplifying the ball's tangential speed:
//
//   Contact r_c, spin surface velocity = ω × r_c, tangential component:
//     Left  (r_c = -R·x̂): contact_z = +ω_y·R  → kick_z = -ω_y·R·MU_CUSH
//     Right (r_c = +R·x̂): contact_z = -ω_y·R  → kick_z = +ω_y·R·MU_CUSH
//     Far   (r_c = -R·ẑ): contact_x = -ω_y·R  → kick_x = +ω_y·R·MU_CUSH  ← deflects RIGHT for ω_y>0
//     Near  (r_c = +R·ẑ): contact_x = +ω_y·R  → kick_x = -ω_y·R·MU_CUSH
//
//   MU_CUSH = 0.08: small coefficient — spin is a gentle redirection, not
//   a speed amplifier.  ω_y decays by SPIN_RETAIN each contact.
void Physics::resolveCushions(BallState& b) {
    const float CORNER_GAP  = POCKET_R[0];
    const float MIDDLE_GAP  = POCKET_R[1] * 1.1f;
    const float MU_CUSH     = 0.06f;   // spin→velocity transfer at cushion
    const float SPIN_RETAIN = 0.85f;   // ω_y fraction kept after contact

    // Spin kick helpers (spin-only, no translational component)
    float spinR = b.spin.y * RADIUS;   // pre-compute ω_y · R

    // Left cushion (x = -IX)
    if (b.pos.x < -IX + RADIUS) {
        bool gap = (std::abs(b.pos.z - (-IZ)) < CORNER_GAP ||
                    std::abs(b.pos.z -   IZ)  < CORNER_GAP);
        if (!gap) {
            b.pos.x = -IX + RADIUS;
            if (b.vel.x < 0.0f) {
                b.vel.x  = -b.vel.x * E_BC;
                b.vel.z  -= spinR * MU_CUSH;   // contact_z = +ω_y·R → friction −z
                b.spin.y *= SPIN_RETAIN;
                spinR     = b.spin.y * RADIUS;
            }
        }
    }
    // Right cushion (x = +IX)
    if (b.pos.x > IX - RADIUS) {
        bool gap = (std::abs(b.pos.z - (-IZ)) < CORNER_GAP ||
                    std::abs(b.pos.z -   IZ)  < CORNER_GAP);
        if (!gap) {
            b.pos.x = IX - RADIUS;
            if (b.vel.x > 0.0f) {
                b.vel.x  = -b.vel.x * E_BC;
                b.vel.z  += spinR * MU_CUSH;   // contact_z = -ω_y·R → friction +z
                b.spin.y *= SPIN_RETAIN;
                spinR     = b.spin.y * RADIUS;
            }
        }
    }
    // Far cushion (z = -IZ)
    if (b.pos.z < -IZ + RADIUS) {
        bool gap = (std::abs(b.pos.x - (-IX)) < CORNER_GAP ||
                    std::abs(b.pos.x        ) < MIDDLE_GAP ||
                    std::abs(b.pos.x -   IX)  < CORNER_GAP);
        if (!gap) {
            b.pos.z = -IZ + RADIUS;
            if (b.vel.z < 0.0f) {
                b.vel.z  = -b.vel.z * E_BC;
                b.vel.x  += spinR * MU_CUSH;   // contact_x = -ω_y·R → friction +x (right for ω_y>0)
                b.spin.y *= SPIN_RETAIN;
                spinR     = b.spin.y * RADIUS;
            }
        }
    }
    // Near cushion (z = +IZ)
    if (b.pos.z > IZ - RADIUS) {
        bool gap = (std::abs(b.pos.x - (-IX)) < CORNER_GAP ||
                    std::abs(b.pos.x        ) < MIDDLE_GAP ||
                    std::abs(b.pos.x -   IX)  < CORNER_GAP);
        if (!gap) {
            b.pos.z = IZ - RADIUS;
            if (b.vel.z > 0.0f) {
                b.vel.z  = -b.vel.z * E_BC;
                b.vel.x  -= spinR * MU_CUSH;   // contact_x = +ω_y·R → friction -x
                b.spin.y *= SPIN_RETAIN;
                spinR     = b.spin.y * RADIUS;
            }
        }
    }
    (void)spinR;  // silence unused-variable warning if no cushion was hit
}

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
bool Physics::resolveBallBall(BallState& a, BallState& b) {
    const float minD = 2.0f * RADIUS;

    glm::vec3 d0 = a.prevPos - b.prevPos;          // relative pos at step start
    float     c0 = glm::dot(d0, d0) - minD * minD; // > 0 → separate, ≤ 0 → overlap

    if (c0 <= 0.0f) {
        // Case 1: already overlapping → standard resolution
        glm::vec3 diff  = a.pos - b.pos;
        float     dist2 = glm::dot(diff, diff);
        if (dist2 < 1e-8f) return false;
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
        return true;
    }

    // Case 2: CCD swept test
    glm::vec3 dv = (a.pos - a.prevPos) - (b.pos - b.prevPos);
    float A  = glm::dot(dv, dv);
    if (A < 1e-12f) return false;
    float B    = 2.0f * glm::dot(d0, dv);
    float disc = B * B - 4.0f * A * c0;
    if (disc < 0.0f) return false;

    float t = (-B - std::sqrt(disc)) / (2.0f * A);
    if (t < 0.0f || t > 1.0f) return false;

    // Position of both balls at contact time t
    glm::vec3 aT = a.prevPos + (a.pos - a.prevPos) * t;
    glm::vec3 bT = b.prevPos + (b.pos - b.prevPos) * t;
    glm::vec3 n  = aT - bT;
    float     d  = glm::length(n);
    if (d < 1e-8f) return false;
    n /= d;

    // Impulse using post-friction velocities
    float vRel = glm::dot(a.vel - b.vel, n);
    if (vRel >= 0.0f) return false;

    float j = -(1.0f + E_BB) * vRel * 0.5f;
    a.vel += j * n;
    b.vel -= j * n;

    float rem = (1.0f - t) * DT;
    a.pos = aT + a.vel * rem;
    b.pos = bT + b.vel * rem;
    return true;
}

// Pocket detection with swept check:
//   1. Direct distance test at current position.
//   2. Closest-point-on-segment test (prevPos → pos) to catch fast balls that
//      tunnel through the capture radius in a single step.
//   3. Safety boundary: ball escaped the table → sink unconditionally.
void Physics::checkPocket(BallState& b, int idx) {
    auto sink = [&]() {
        b.sinking = true; b.sinkT = 0.0f;
        b.vel = glm::vec3(0.0f); b.spin = glm::vec3(0.0f);
        shotPottedBalls.push_back(idx);
    };

    for (int i = 0; i < 6; i++) {
        const float pr  = POCKET_R[i];
        const float px  = POCKETS[i].x;
        const float pz  = POCKETS[i].y;

        // Middle pockets (indices 1 and 4) are recessed notches in the long cushion.
        // A ball rolling along the cushion face (z ≈ ±(IZ-RADIUS)) must not be captured —
        // only capture once the ball has actually entered the notch past the cushion face.
        if (i == 1 && b.pos.z > -IZ + RADIUS * 0.5f) continue;  // far middle
        if (i == 4 && b.pos.z <  IZ - RADIUS * 0.5f) continue;  // near middle

        // 1. Direct check
        float dx = b.pos.x - px, dz = b.pos.z - pz;
        if (dx*dx + dz*dz < pr*pr) { sink(); return; }

        // 2. Swept check (prevPos → pos segment vs pocket circle in XZ)
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

    // 3. Safety: ball completely escaped table geometry
    if (std::abs(b.pos.x) > IX + 0.12f || std::abs(b.pos.z) > IZ + 0.12f)
        sink();
}

// Ray-sphere intersection — PBR book 4th ed., Ch6.1
//   Numerically robust half-b form to avoid catastrophic cancellation.
//   Solves: a*t² + 2*hb*t + c = 0
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
