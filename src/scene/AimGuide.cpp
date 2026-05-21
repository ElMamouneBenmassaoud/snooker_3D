#include "AimGuide.h"
#include <algorithm>
#include <cmath>
#include <vector>

static const float PI = 3.14159265f;

static const float DASH_LEN  = 0.040f;
static const float GAP_LEN   = 0.020f;
static const int   GHOST_SEGS = 40;

static void appendDashed(std::vector<glm::vec3>& pts,
                          glm::vec3 p0, glm::vec3 p1)
{
    glm::vec3 d   = p1 - p0;
    float     len = glm::length(d);
    if (len < 1e-4f) return;
    d /= len;
    float t = 0.0f; bool draw = true;
    while (t < len) {
        float seg = draw ? DASH_LEN : GAP_LEN;
        float end = std::min(t + seg, len);
        if (draw) { pts.push_back(p0 + d*t); pts.push_back(p0 + d*end); }
        t = end; draw = !draw;
    }
}

static void appendCircleXZ(std::vector<glm::vec3>& pts,
                             glm::vec3 centre, float radius)
{
    for (int i = 0; i < GHOST_SEGS; i++) {
        float a0 = 2.0f*PI*(float)i      /GHOST_SEGS;
        float a1 = 2.0f*PI*(float)(i+1)  /GHOST_SEGS;
        pts.push_back(centre + glm::vec3(cosf(a0)*radius, 0.0f, sinf(a0)*radius));
        pts.push_back(centre + glm::vec3(cosf(a1)*radius, 0.0f, sinf(a1)*radius));
    }
}

// Estimated stopping distance of cue ball at speed v0:
//   sliding phase  d1 = 12·v0² / (49·μk·g)
//   rolling phase  d2 = 25·v0² / (98·μr·g)
static float estimateReach(float v0)
{
    const float d1 = 12.0f * v0*v0 / (49.0f * Physics::MU_K * Physics::G);
    const float d2 = 25.0f * v0*v0 / (98.0f * Physics::MU_R * Physics::G);
    return d1 + d2;
}

// Distance from point P to line segment AB (in XZ, ignoring Y).
static float pointSegDistXZ(glm::vec3 A, glm::vec3 B, glm::vec3 P)
{
    glm::vec2 ab(B.x-A.x, B.z-A.z);
    glm::vec2 ap(P.x-A.x, P.z-A.z);
    float len2 = ab.x*ab.x + ab.y*ab.y;
    float t    = (len2 > 1e-8f) ? (ap.x*ab.x + ap.y*ab.y) / len2 : 0.0f;
    t = std::min(std::max(t, 0.0f), 1.0f);
    float cx = A.x + ab.x*t;
    float cz = A.z + ab.y*t;
    float dx = cx - P.x, dz = cz - P.z;
    return sqrtf(dx*dx + dz*dz);
}

AimGuide::AimGuide() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

AimGuide::~AimGuide() {
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
}

void AimGuide::update(const glm::vec3& cueBallPos,
                       const glm::vec3& shotDir,
                       const std::vector<BallState>& balls,
                       float power)
{
    const float R  = Physics::RADIUS;
    const float IX = Physics::IX;
    const float IZ = Physics::IZ;

    for (auto& g : groups) g.pts.clear();

    // Estimated reach when charging; infinite when idle (just aiming)
    float reach = (power > 0.001f) ? estimateReach(power) : 1e30f;

    // Ray-to-wall helper, shared by cue and target ball tracing
    auto ct = [](float wall, float orig, float dir) -> float {
        if (std::abs(dir) < 1e-6f) return 1e30f;
        float t = (wall - orig) / dir;
        return (t > 0.001f) ? t : 1e30f;
    };

    // 1.  Find first obstruction on cue ball ray (ball or cushion)
    float tMin    = 1e30f;
    int   hitBall = -1;

    for (int i = 1; i < (int)balls.size(); i++) {
        if (!balls[i].active || balls[i].sinking) continue;
        float t0, t1;
        if (Physics::raySphereIntersect(cueBallPos, shotDir,
                                        balls[i].pos, 2.0f*R, t0, t1))
            if (t0 > 0.001f && t0 < tMin) { tMin = t0; hitBall = i; }
    }

    float tCush = std::min({ ct( IX-R, cueBallPos.x, shotDir.x),
                              ct(-IX+R, cueBallPos.x, shotDir.x),
                              ct( IZ-R, cueBallPos.z, shotDir.z),
                              ct(-IZ+R, cueBallPos.z, shotDir.z) });
    if (tCush < tMin) { tMin = tCush; hitBall = -1; }

    glm::vec3 ghostPos = cueBallPos + shotDir * tMin;
    glm::vec3 pathEnd  = cueBallPos + shotDir * std::min(tMin, reach);

    // Group 0 — WHITE solid: cue ball path
    groups[0].pts.push_back(cueBallPos);
    groups[0].pts.push_back(pathEnd);

    bool willReach = (reach >= tMin);

    // Group 1 — WHITE thin: ghost ball circle at impact
    if (willReach)
        appendCircleXZ(groups[1].pts, ghostPos, R);

    if (hitBall >= 0 && willReach) {
        // Collision normal: from target-ball center toward ghost ball (cue at impact)
        glm::vec3 n         = glm::normalize(ghostPos - balls[hitBall].pos);
        glm::vec3 targetDir = -n;   // target ball travels AWAY from cue ball

        // 2.  Trace target ball to first cushion (or max 1.8 m)
        glm::vec3 tPos = balls[hitBall].pos;
        float tToCush = std::min({
            ct( IX-R, tPos.x, targetDir.x),
            ct(-IX+R, tPos.x, targetDir.x),
            ct( IZ-R, tPos.z, targetDir.z),
            ct(-IZ+R, tPos.z, targetDir.z)
        });
        tToCush = std::min(tToCush, 1.8f);
        glm::vec3 targetEnd = tPos + targetDir * tToCush;

        // 3.  Check if target ball path passes near a pocket
        int   pocketIdx  = -1;
        float pocketDist = 1e30f;
        for (int p = 0; p < 6; p++) {
            glm::vec3 pk(Physics::POCKETS[p].x, 0.0f, Physics::POCKETS[p].y);
            float d = pointSegDistXZ(tPos, targetEnd, pk);
            float threshold = Physics::POCKET_R[p] * 3.0f;   // slightly generous
            if (d < threshold && d < pocketDist) {
                pocketDist = d;
                pocketIdx  = p;
            }
        }

        // Group 2 — YELLOW: target ball direction
        //   • path aligned with a pocket → full line to pocket, pocket circle
        //   • no pocket in sight         → shorter line (~half the trace)
        if (pocketIdx >= 0) {
            // Draw right up to the pocket centre
            glm::vec3 pocketPos(Physics::POCKETS[pocketIdx].x, 0.0f,
                                Physics::POCKETS[pocketIdx].y);
            float toPocket = glm::length(pocketPos - tPos);
            toPocket = std::min(toPocket, tToCush);
            groups[2].pts.push_back(tPos);
            groups[2].pts.push_back(tPos + targetDir * toPocket);

            // Small circle around the pocket to highlight it
            appendCircleXZ(groups[2].pts, pocketPos, Physics::POCKET_R[pocketIdx] * 1.6f);
        } else {
            // No pocket — show a shorter line (≈ 40 % of cushion distance, max 0.45 m)
            float shortLen = std::min(tToCush * 0.40f, 0.45f);
            groups[2].pts.push_back(tPos);
            groups[2].pts.push_back(tPos + targetDir * shortLen);
        }

        // Group 3 — AMBER dashed: cue ball deflection after impact
        glm::vec3 deflect = shotDir - glm::dot(shotDir, n) * n;
        float     dLen    = glm::length(deflect);
        if (dLen > 0.05f) {
            deflect /= dLen;
            float cueLen = std::min(tMin * 0.55f, 0.42f);
            appendDashed(groups[3].pts, ghostPos, ghostPos + deflect * cueLen);
        }
    }

    // Group 4 — AMBER thin: power stick indicator
    if (power > 0.001f) {
        float stick = (power / 6.0f) * 0.32f;
        groups[4].pts.push_back(cueBallPos);
        groups[4].pts.push_back(cueBallPos - shotDir * stick);
    }

    for (auto& g : groups) upload(g);
}

void AimGuide::upload(LineGroup& g) {
    g.ptCount = (int)g.pts.size();
}

void AimGuide::draw(Shader& flatShader) const {
    glBindVertexArray(VAO);
    for (const auto& g : groups) {
        if (g.pts.empty()) continue;
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     (GLsizeiptr)(g.pts.size() * sizeof(glm::vec3)),
                     g.pts.data(), GL_DYNAMIC_DRAW);
        flatShader.setVec3("lineColor", g.color);
        glLineWidth(g.lineWidth);
        glDrawArrays(GL_LINES, 0, (GLsizei)g.pts.size());
    }
    glBindVertexArray(0);
    glLineWidth(1.0f);
}
