#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "Shader.h"
#include "Camera.h"
#include "Table.h"
#include "Texture.h"
#include "TextureGen.h"
#include "Ball.h"
#include "RedBalls.h"
#include "Physics.h"
#include "AimGuide.h"
#include "Cubemap.h"
#include "GameLogic.h"
#include <stb_easy_font.h>
#include "../../3rdParty/miniaudio.h"

const int WIDTH  = 1280;
const int HEIGHT = 720;

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

// Orbital camera (right-click drag or Tab to switch to free mode)
Camera* gCamera    = nullptr;
bool    gRightDown = false;   // right mouse button held → orbit
double  gLastX = 0.0, gLastY = 0.0;

// Aim direction (left-click drag horizontal movement)
float     gShotAngle = (float)M_PI;                          // radians
glm::vec3 gShotDir   = glm::vec3(-1.0f, 0.0f, 0.0f);
bool      gAimDragging = false;                              // left drag outside cue zone
static const float AIM_SENSITIVITY = 0.004f;                // rad/pixel

// Camera modes: shot cam (behind cue ball) or orbital
bool  gShotCamMode   = true;
bool  gWasTab        = false;
float gShotCamHeight = 0.16f;   // fixed height above table
float gShotCamDist   = 0.55f;   // distance behind cue ball (scroll = avant/arrière)

// Post-shot overview camera
bool  gBallsMoving = false;  // true from shot fire until allStopped + 2-s delay
float gStopTimer   = 0.0f;   // seconds elapsed since all balls stopped

// Game logic trigger — set true when shot is fired, reset after processShot()
bool  gWaitingForShotResult = false;

// ---------------------------------------------------------------------------
// 2D power gauge — left strip, drag upward = more power.
// ---------------------------------------------------------------------------
const float GAUGE_X    =   42.0f;   // center X of bar
const float GAUGE_W    =   14.0f;   // half-width
const float GAUGE_Y_TOP=   80.0f;
const float GAUGE_Y_BOT=  640.0f;
const float GAUGE_DRAG_X = 90.0f;   // x < this → power drag zone

// ---------------------------------------------------------------------------
// Spin selector — circle bottom-right.
// ---------------------------------------------------------------------------
const float SPIN_CX = (float)WIDTH  - 80.0f;   // 1200
const float SPIN_CY = (float)HEIGHT - 95.0f;   // 625
const float SPIN_R  = 52.0f;

bool  gCueDragging  = false;
float gCueDragBaseY = 0.0f;
float gCuePower     = 0.0f;
bool  gShooting     = false;

// Spin (effect) values: u=left/right, v=top/back  (each in [-1,1])
float gSpinU        = 0.0f;
float gSpinV        = 0.0f;
bool  gSpinDragging = false;

// Foul notification overlay
std::string gFoulLine1;
std::string gFoulLine2;
float       gFoulTimer = 0.0f;

// Player names and frame (match) scores
std::string gPlayerName[2]  = {"", ""};
int         gFrameScore[2]  = {0, 0};

// Startup name panel (shown before first game)
bool        gSetupScreen = true;
int         gSetupFocus  = 0;          // 0 or 1: which field is active
std::string gSetupBuf[2] = {"", ""};

// Menu state
bool        gMenuOpen        = false;
int         gMenuEditPlayer  = -1;    // -1=none, 0 or 1 = editing that player's name
std::string gMenuEditBuf;

// Camera button (bottom-right, left of spin selector)
const float CAM_BTN_CX = 1105.0f;
const float CAM_BTN_CY = 673.0f;
const float CAM_BTN_R  = 22.0f;

// Menu button (top-left corner)
const float MNU_BTN_X0 = 8.0f;
const float MNU_BTN_Y0 = 8.0f;
const float MNU_BTN_X1 = 80.0f;
const float MNU_BTN_Y1 = 36.0f;

// Pending game reset (set by surrender/restart, executed in main loop)
bool gPendingReset     = false;
int  gResetWinner      = -1;   // -1 = tie/manual restart, 0 or 1 = winner index

// Winner announcement overlay (shown before reset)
std::string gWinnerMsg;
std::string gWinnerSub;   // subtitle line
float       gWinnerTimer  = 0.0f;   // seconds remaining to show overlay

// Audio
float      gMusicVolume    = 0.4f;    // 0.0 = mute, 1.0 = full
bool       gSliderDragging = false;
ma_sound*  gBgMusic        = nullptr; // set after init

// Ball-in-hand: player places cue ball inside the D after a pocket
bool      gBallInHand = false;
glm::mat4 gLastView   = glm::mat4(1.0f);   // view from previous frame (for unproject)
glm::mat4 gLastProj   = glm::mat4(1.0f);

// D geometry (must match Table.cpp constants)
static constexpr float BAULK_X  = 0.873f;   // IX - 2*IX*0.207
static constexpr float D_RADIUS = 0.242f;

// ---------------------------------------------------------------------------
// GLFW callbacks
// ---------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); }
static void commitMenuEdit();  // forward declaration

// Audio engine pointer (set in main after init)
static ma_engine* gAudioEngine = nullptr;

static void playSound(const char* filename) {
    if (!gAudioEngine) return;
    std::string path = std::string(PATH_TO_ASSETS) + "/sounds/" + filename;
    ma_engine_play_sound(gAudioEngine, path.c_str(), nullptr);
}

static void playBreak(int total) {
    if (total < 1 || total > 147) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d.wav", total);
    playSound(buf);
}

// Menu button hit-test helpers (defined before callbacks, used in mouse_button_callback)
struct Rect { float x0, y0, x1, y1; };
static bool hitRect(const Rect& r, float x, float y) {
    return x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1;
}

// Menu layout constants (matching draw code below)
static constexpr float MNU_W  = 460.0f;
static constexpr float MNU_H  = 526.0f;
static constexpr float MNU_X0 = (float)WIDTH  * 0.5f - MNU_W * 0.5f;
static constexpr float MNU_Y0 = (float)HEIGHT * 0.5f - MNU_H * 0.5f;
// Button rows inside menu (y offsets from MNU_Y0)
static Rect mnuBtnField(int player) {  // name input fields
    return {MNU_X0+100, MNU_Y0 + 58.f + player*50.f,
            MNU_X0+MNU_W-20, MNU_Y0 + 80.f + player*50.f};
}
static Rect mnuBtnSurrender(int player) {
    return {MNU_X0+20, MNU_Y0 + 170.f + player*52.f,
            MNU_X0+MNU_W-20, MNU_Y0 + 208.f + player*52.f};
}
static Rect mnuBtnRestart() {
    return {MNU_X0+20, MNU_Y0+288.f, MNU_X0+MNU_W-20, MNU_Y0+326.f};
}

void mouse_button_callback(GLFWwindow*, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        float mx = (float)gLastX, my = (float)gLastY;

        // --- Setup screen clicks ---
        if (gSetupScreen) {
            // Setup panel constants (must match draw code)
            const float SW = 420.0f, SH = 260.0f;
            const float SX0 = (float)WIDTH*0.5f - SW*0.5f;
            const float SY0 = (float)HEIGHT*0.5f - SH*0.5f;
            // Fields
            for (int p = 0; p < 2; ++p) {
                Rect fr{SX0+130, SY0+60.f+p*62.f, SX0+SW-20, SY0+84.f+p*62.f};
                if (hitRect(fr, mx, my)) { gSetupFocus = p; return; }
            }
            // Start button
            Rect startBtn{SX0+20, SY0+SH-54.f, SX0+SW-20, SY0+SH-20.f};
            if (hitRect(startBtn, mx, my)) { gSetupScreen = false; return; }
            return;
        }

        // --- Menu open: route all clicks to menu ---
        if (gMenuOpen) {
            // Name input fields
            for (int p = 0; p < 2; ++p) {
                if (hitRect(mnuBtnField(p), mx, my)) {
                    commitMenuEdit();          // save any in-progress edit first
                    gMenuEditPlayer = p;
                    gMenuEditBuf    = gPlayerName[p];
                    return;
                }
            }
            // Surrender buttons
            for (int p = 0; p < 2; ++p) {
                if (hitRect(mnuBtnSurrender(p), mx, my)) {
                    gMenuOpen      = false;
                    gMenuEditPlayer= -1;
                    int winner     = 1 - p;
                    gResetWinner   = winner;
                    gPendingReset  = true;
                    // Announce winner
                    gWinnerMsg   = gPlayerName[winner];
                    gWinnerSub   = "wins the frame!";
                    gWinnerTimer = 3.5f;
                    return;
                }
            }
            // Restart button
            if (hitRect(mnuBtnRestart(), mx, my)) {
                gMenuOpen      = false;
                gMenuEditPlayer= -1;
                gResetWinner   = -1;
                gPendingReset  = true;
                return;
            }
            // Volume slider
            {
                float sy  = MNU_Y0 + 336.f;
                float tx0 = MNU_X0 + 90.f;
                float tx1 = MNU_X0 + MNU_W - 50.f;
                Rect  track{tx0, sy, tx1, sy + 18.f};
                Rect  muteBtn{tx1 + 8.f, sy, tx1 + 42.f, sy + 18.f};
                if (hitRect(muteBtn, mx, my)) {
                    gMusicVolume = (gMusicVolume < 0.01f) ? 0.4f : 0.0f;
                    return;
                }
                if (hitRect(track, mx, my)) {
                    gMusicVolume = std::max(0.0f, std::min(1.0f, (mx - tx0) / (tx1 - tx0)));
                    gSliderDragging = true;
                    return;
                }
            }
            // Click outside menu → close
            Rect panel{MNU_X0, MNU_Y0, MNU_X0+MNU_W, MNU_Y0+MNU_H};
            if (!hitRect(panel, mx, my)) {
                if (gMenuEditPlayer >= 0) {
                    gPlayerName[gMenuEditPlayer] = gMenuEditBuf.empty()
                        ? (gMenuEditPlayer==0 ? "Player 1" : "Player 2")
                        : gMenuEditBuf;
                    gMenuEditPlayer = -1;
                } else {
                    gMenuOpen = false;
                }
            }
            return;
        }

        // --- Menu button (top-left) ---
        if (hitRect({MNU_BTN_X0, MNU_BTN_Y0, MNU_BTN_X1, MNU_BTN_Y1}, mx, my)) {
            gMenuOpen = true;
            return;
        }

        // --- Camera button (bottom-right) ---
        float cdx = mx - CAM_BTN_CX, cdy = my - CAM_BTN_CY;
        if (cdx*cdx + cdy*cdy <= CAM_BTN_R*CAM_BTN_R) {
            gShotCamMode = !gShotCamMode;
            gBallsMoving = false;
            gStopTimer   = 0.0f;
            return;
        }

        // --- Ball-in-hand: any left click confirms placement ---
        if (gBallInHand) {
            gBallInHand = false;
            return;
        }
        // Priority 1: spin selector circle
        float sdx = mx - SPIN_CX, sdy = my - SPIN_CY;
        if (sdx*sdx + sdy*sdy <= SPIN_R*SPIN_R) {
            gSpinDragging = true;
        }
        // Priority 2: power gauge left strip
        else if (mx < GAUGE_DRAG_X) {
            gCueDragging  = true;
            gCueDragBaseY = my;
            gCuePower     = 0.0f;
        }
        // Priority 3: aim rotation (rest of screen)
        else {
            gAimDragging = true;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        if (gCueDragging) {
            gShooting    = (gCuePower > 0.2f);
            gCueDragging = false;
            if (!gShooting) gCuePower = 0.0f;
        }
        gSpinDragging   = false;
        gAimDragging    = false;
        gSliderDragging = false;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
        gRightDown = (action == GLFW_PRESS);
}

// Confirm the current menu name field and deactivate it
static void commitMenuEdit() {
    if (gMenuEditPlayer < 0) return;
    const char* def[2] = {"Player 1", "Player 2"};
    gPlayerName[gMenuEditPlayer] = gMenuEditBuf.empty() ? def[gMenuEditPlayer] : gMenuEditBuf;
    gMenuEditPlayer = -1;
}

// Character input — routes to whichever text field is active
void char_callback(GLFWwindow*, unsigned int cp) {
    if (cp >= 128) return;
    if (gSetupScreen) {
        if (gSetupBuf[gSetupFocus].size() < 20)
            gSetupBuf[gSetupFocus] += (char)cp;
        return;
    }
    if (gMenuEditPlayer >= 0 && gMenuEditBuf.size() < 20)
        gMenuEditBuf += (char)cp;
}

void key_callback(GLFWwindow*, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    // --- Setup screen input ---
    if (gSetupScreen) {
        if (key == GLFW_KEY_BACKSPACE && !gSetupBuf[gSetupFocus].empty())
            gSetupBuf[gSetupFocus].pop_back();
        if (key == GLFW_KEY_TAB)
            gSetupFocus = 1 - gSetupFocus;
        if (key == GLFW_KEY_ENTER) {
            if (gSetupFocus == 0) { gSetupFocus = 1; }
            else {
                // Start game — names set in main loop when gSetupScreen goes false
                gSetupScreen = false;
            }
        }
        return;
    }

    // --- In-game keys ---
    if (key == GLFW_KEY_ESCAPE) {
        if (gMenuEditPlayer >= 0) commitMenuEdit();
        else                      gMenuOpen = !gMenuOpen;
    }
    if (key == GLFW_KEY_ENTER && gMenuEditPlayer >= 0) commitMenuEdit();
    if (key == GLFW_KEY_BACKSPACE && gMenuEditPlayer >= 0 && !gMenuEditBuf.empty())
        gMenuEditBuf.pop_back();

    // Space: shoot with current power (keyboard-only play, no mouse needed)
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS && !gMenuOpen && !gSetupScreen)
        gShooting = true;
}

void cursor_pos_callback(GLFWwindow*, double x, double y) {
    float dx = (float)(x - gLastX);
    float dy = (float)(y - gLastY);

    // Right-click drag: orbit camera
    if (gRightDown && gCamera)
        gCamera->processMouseDrag(dx, -dy);

    // Left-click drag (not cue zone): rotate aim direction
    if (gAimDragging)
        gShotAngle += dx * AIM_SENSITIVITY;

    // Cue power drag (left strip)
    if (gCueDragging) {
        float pullback = gCueDragBaseY - (float)y;
        gCuePower = std::min(std::max(pullback / 350.0f * 6.0f, 0.0f), 6.0f);
    }

    // Spin drag: map mouse to unit circle, Y inverted (up = topspin = +v)
    if (gSpinDragging) {
        float sdx = (float)x - SPIN_CX;
        float sdy = (float)y - SPIN_CY;
        gSpinU = sdx / SPIN_R;
        gSpinV = -sdy / SPIN_R;
        float len = sqrtf(gSpinU*gSpinU + gSpinV*gSpinV);
        if (len > 1.0f) { gSpinU /= len; gSpinV /= len; }
    }

    // Volume slider drag
    if (gSliderDragging && gMenuOpen) {
        float tx0 = MNU_X0 + 90.f;
        float tx1 = MNU_X0 + MNU_W - 50.f;
        gMusicVolume = std::max(0.0f, std::min(1.0f, ((float)x - tx0) / (tx1 - tx0)));
    }

    gLastX = x; gLastY = y;
}

void scroll_callback(GLFWwindow*, double, double dy) {
    if (gShotCamMode && !gBallsMoving) {
        // Scroll up (dy>0) = avancer/descendre, scroll down = reculer/monter
        float delta = (float)dy * 0.05f;
        gShotCamDist  -= delta;
        gShotCamHeight -= delta * 0.45f;   // monte quand on recule, descend quand on avance
        gShotCamDist   = std::min(std::max(gShotCamDist,   0.35f), 1.50f);
        gShotCamHeight = std::min(std::max(gShotCamHeight, 0.12f), 1.20f);
    } else if (gCamera) {
        gCamera->processScroll((float)dy);
    }
}

void processInput(GLFWwindow*) {
    // ESC is handled in key_callback (opens/closes menu)
}

// ---------------------------------------------------------------------------
// 2D renderer — lazy-init VAO/VBO, orthographic pixel coordinates.
//   mode = GL_LINES or GL_TRIANGLES
// ---------------------------------------------------------------------------
static void draw2D(const std::vector<glm::vec3>& pts, GLenum mode) {
    if (pts.empty()) return;
    static GLuint vao = 0, vbo = 0;
    if (!vao) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(pts.size() * sizeof(glm::vec3)),
                 pts.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(vao);
    glDrawArrays(mode, 0, (GLsizei)pts.size());
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Filled rectangle (orthographic 2D coords).
static void drawRect(float x0, float y0, float x1, float y1) {
    draw2D({{x0,y0,0},{x1,y0,0},{x1,y1,0},
            {x0,y0,0},{x1,y1,0},{x0,y1,0}}, GL_TRIANGLES);
}

// ASCII text using stb_easy_font (quads → triangles). Returns rendered width.
static float drawText(float x, float y, const std::string& txt, float scale = 1.0f) {
    static char buf[65536];
    struct V { float x, y, z; unsigned char c[4]; };
    int nq = stb_easy_font_print(0, 0, (char*)txt.c_str(), nullptr, buf, sizeof(buf));
    auto* v = (V*)buf;
    std::vector<glm::vec3> pts;
    pts.reserve(nq * 6);
    for (int q = 0; q < nq; q++) {
        V* qv = v + q * 4;
        for (int i : {0,1,2}) pts.push_back({x + qv[i].x*scale, y + qv[i].y*scale, 0.f});
        for (int i : {0,2,3}) pts.push_back({x + qv[i].x*scale, y + qv[i].y*scale, 0.f});
    }
    draw2D(pts, GL_TRIANGLES);
    return (float)stb_easy_font_width((char*)txt.c_str()) * scale;
}

// ---------------------------------------------------------------------------
// Snooker respawn rule: place ball on its own spot if free, otherwise on the
// highest-value free spot, otherwise as close as possible to its own spot
// (toward the black end, then toward the yellow end) without overlapping any ball.
//
// balls    : all 22 BallState objects (to test occupancy)
// ballIdx  : 1-6 (colored ball to respawn)
// Returns the world position to place the ball.
static glm::vec3 findRespawnPos(const std::vector<BallState>& balls, int ballIdx) {
    const float MIN_DIST = 2.0f * Physics::RADIUS + 0.001f;  // min centre-to-centre

    // Returns true if 'pos' is clear of every active ball (except the one being respawned)
    auto isFree = [&](const glm::vec3& pos) {
        for (int i = 0; i < (int)balls.size(); ++i) {
            if (i == ballIdx) continue;
            if (!balls[i].active) continue;
            glm::vec3 d = balls[i].pos - pos;
            if (d.x*d.x + d.z*d.z < MIN_DIST*MIN_DIST) return false;
        }
        return true;
    };

    // 1. Try own spot
    if (isFree(GameLogic::SPOTS[ballIdx]))
        return GameLogic::SPOTS[ballIdx];

    // 2. Try spots in descending value order (black=6 down to yellow=1), skip own
    static constexpr int DESCENDING[6] = {6, 5, 4, 3, 2, 1};
    for (int s : DESCENDING) {
        if (s == ballIdx) continue;
        glm::vec3 p = GameLogic::SPOTS[s];
        p.y = Physics::RADIUS;
        if (isFree(p)) return p;
    }

    // 3. All spots occupied: walk from own spot toward black end (+X), then yellow end (-X)
    glm::vec3 base = GameLogic::SPOTS[ballIdx];
    const float STEP = MIN_DIST;
    const float X_MAX =  Physics::IX - Physics::RADIUS;
    const float X_MIN = -Physics::IX + Physics::RADIUS;

    // Walk toward black (+X direction first, then -X)
    for (int sign : {1, -1}) {
        for (float dist = STEP; dist < 3.0f; dist += STEP) {
            glm::vec3 candidate = base;
            candidate.x += sign * dist;
            candidate.x = std::max(X_MIN, std::min(X_MAX, candidate.x));
            if (isFree(candidate)) return candidate;
        }
    }

    // Fallback: return own spot (should never happen in a real game)
    return base;
}

// ---------------------------------------------------------------------------
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Snooker 3D", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    glEnable(GL_DEPTH_TEST);

    std::string feltPath = std::string(PATH_TO_ASSETS) + "/textures/felt.jpg";
    std::string woodPath = std::string(PATH_TO_ASSETS) + "/textures/wood.jpg";

    Shader   shader(PATH_TO_SHADERS"/basic.vert",       PATH_TO_SHADERS"/basic.frag");
    Shader   instancedShader(PATH_TO_SHADERS"/instanced.vert", PATH_TO_SHADERS"/basic.frag");
    Shader   flatShader(PATH_TO_SHADERS"/flat.vert",    PATH_TO_SHADERS"/flat.frag");
    Shader   skyboxShader(PATH_TO_SHADERS"/skybox.vert", PATH_TO_SHADERS"/skybox.frag");
    Table    table;
    AimGuide aimGuide;
    Texture  feltTex(feltPath);
    Texture  woodTex(woodPath);
    Camera   camera(glm::vec3(0.0f), 5.5f, 0.0f, 35.0f);
    gCamera = &camera;

    // --- Skybox ---
    std::string skyDir = std::string(PATH_TO_ASSETS) + "/skybox/";
    Cubemap skybox({
        skyDir + "right.png",
        skyDir + "left.png",
        skyDir + "top.png",
        skyDir + "bottom.png",
        skyDir + "front.png",
        skyDir + "back.png"
    });
    // Keep cubemap permanently on unit 1 (shared by skybox + envMap reflection).
    // Unit 0 is reserved for 2D textures (felt, wood).
    skybox.bind(1);
    skyboxShader.use();
    skyboxShader.setInt("skybox", 1);

    // --- Ball setup ---
    const float B  = Ball::RADIUS;
    const float BY = Ball::RADIUS;
    const float BX = 0.873f;
    const float DX = B * 1.7321f;

    std::vector<Ball> balls;
    const float DR = 0.242f;
    balls.emplace_back(glm::vec3( BX + 0.05f, BY, -DR * 0.5f), glm::vec3(1.00f, 1.00f, 1.00f));
    balls.emplace_back(glm::vec3( BX,         BY, -DR        ), glm::vec3(0.95f, 0.85f, 0.05f));
    balls.emplace_back(glm::vec3( BX,         BY,  DR        ), glm::vec3(0.10f, 0.65f, 0.15f));
    balls.emplace_back(glm::vec3( BX,         BY,  0.000f    ), glm::vec3(0.55f, 0.27f, 0.07f));
    balls.emplace_back(glm::vec3( 0.000f,     BY,  0.000f    ), glm::vec3(0.05f, 0.20f, 0.78f));
    balls.emplace_back(glm::vec3(-0.896f,     BY,  0.000f    ), glm::vec3(0.95f, 0.50f, 0.65f));
    balls.emplace_back(glm::vec3(-1.220f,     BY,  0.000f    ), glm::vec3(0.05f, 0.05f, 0.05f));

    float rx = -0.896f - 2.0f * B;
    RedBalls redBalls(std::array<glm::vec3, RedBalls::COUNT>{{
        { rx,        BY,  0.000f },
        { rx - DX,   BY,  B     }, { rx - DX,   BY, -B     },
        { rx - 2*DX, BY,  2*B   }, { rx - 2*DX, BY,  0.000f}, { rx - 2*DX, BY, -2*B   },
        { rx - 3*DX, BY,  3*B   }, { rx - 3*DX, BY,  B     }, { rx - 3*DX, BY, -B     }, { rx - 3*DX, BY, -3*B   },
        { rx - 4*DX, BY,  4*B   }, { rx - 4*DX, BY,  2*B   }, { rx - 4*DX, BY,  0.000f}, { rx - 4*DX, BY, -2*B   }, { rx - 4*DX, BY, -4*B   },
    }});

    std::vector<glm::vec3> initPos;
    for (const auto& b : balls)              initPos.push_back(b.pos);
    for (const auto& p : redBalls.positions) initPos.push_back(p);
    Physics   physics(initPos);
    GameLogic gameLogic;
    gBallInHand = true;  // opening break: place cue ball anywhere in the D

    // Reset game to initial state; winner (-1=none) gets +1 frame if >= 0.
    auto resetGame = [&](int winner) {
        if (winner >= 0) gFrameScore[winner]++;
        physics   = Physics(initPos);
        gameLogic = GameLogic();
        // Reset render-side ball orientations
        for (int i = 0; i < 7; ++i) {
            balls[i].pos    = initPos[i];
            balls[i].orient = glm::quat(1,0,0,0);
        }
        for (int i = 0; i < RedBalls::COUNT; ++i) {
            redBalls.positions[i] = initPos[7 + i];
            redBalls.orients[i]   = glm::quat(1,0,0,0);
        }
        gBallsMoving          = false;
        gBallInHand           = true;   // opening break: place cue ball in the D
        gWaitingForShotResult = false;
        gFoulTimer            = 0.0f;
        gCuePower             = 0.0f;
        gSpinU                = 0.0f;
        gSpinV                = 0.0f;
        gShotCamMode          = true;
        gPendingReset         = false;
        gResetWinner          = -1;
    };

    glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                            (float)WIDTH / HEIGHT, 0.01f, 100.0f);
    glm::mat4 ortho2D    = glm::ortho(0.0f, (float)WIDTH, (float)HEIGHT, 0.0f, -1.0f, 1.0f);

    auto setLights = [](Shader& s) {
        s.use();
        s.setVec3("lightPos[0]", -1.1f, 4.0f, 0.0f);
        s.setVec3("lightPos[1]",  0.0f, 4.0f, 0.0f);
        s.setVec3("lightPos[2]",  1.1f, 4.0f, 0.0f);
    };
    setLights(shader);
    setLights(instancedShader);

    // Bind cubemap to unit 1 for environment reflection in basic.frag
    shader.use();
    shader.setInt("textureSampler", 0);
    shader.setInt("envMap", 1);
    shader.setFloat("reflectStrength", 0.0f);  // default: no reflection
    instancedShader.use();
    instancedShader.setInt("envMap", 1);

    // Initial window title showing game state
    glfwSetWindowTitle(window, "Snooker | P1:0  P2:0 | P1: pot a red");

    // --- Background music (miniaudio) ---
    ma_engine audioEngine;
    ma_sound  bgMusic;
    bool      audioOk = false;
    if (ma_engine_init(NULL, &audioEngine) == MA_SUCCESS) {
        gAudioEngine = &audioEngine;
        std::string musicPath = std::string(PATH_TO_ASSETS) + "/music.wav";
        if (ma_sound_init_from_file(&audioEngine, musicPath.c_str(),
                                    MA_SOUND_FLAG_STREAM, NULL, NULL, &bgMusic) == MA_SUCCESS) {
            ma_sound_set_looping(&bgMusic, MA_TRUE);
            ma_sound_set_volume(&bgMusic, gMusicVolume);
            ma_sound_start(&bgMusic);
            gBgMusic = &bgMusic;
            audioOk = true;
        }
    }

    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dt  = now - lastTime;
        lastTime  = now;

        processInput(window);

        // --- Winner overlay countdown ---
        if (gWinnerTimer > 0.0f) {
            gWinnerTimer -= dt;
            if (gWinnerTimer <= 0.0f && gPendingReset) {
                resetGame(gResetWinner);
                gWinnerMsg.clear();
                gWinnerSub.clear();
            }
        }

        // --- Apply music volume ---
        if (gBgMusic) ma_sound_set_volume(gBgMusic, gMusicVolume);

        // --- Pending reset with no overlay (manual restart) ---
        if (gPendingReset && gWinnerTimer <= 0.0f) { resetGame(gResetWinner); }

        // --- Auto-announce + reset on GAME_OVER ---
        static bool gGameOverAnnounced = false;
        if (gameLogic.phase == GamePhase::GAME_OVER) {
            if (!gGameOverAnnounced) {
                gGameOverAnnounced = true;
                // In a shootout the scores may be equal — winner is currentPlayer
                // (they either potted the black or their opponent fouled).
                // For a normal match end, use the score comparison.
                int w = (gameLogic.scores[0] != gameLogic.scores[1])
                      ? (gameLogic.scores[0] > gameLogic.scores[1] ? 0 : 1)
                      : gameLogic.currentPlayer;
                gWinnerMsg  = gPlayerName[w];
                gWinnerSub  = "wins the match!";
                gWinnerTimer  = 4.0f;
                gResetWinner  = w;
                gPendingReset = true;
            }
        } else {
            gGameOverAnnounced = false;
        }

        // --- Apply names when setup screen is dismissed ---
        if (!gSetupScreen && (gPlayerName[0].empty() || gPlayerName[1].empty())) {
            const char* def[2] = {"Player 1", "Player 2"};
            for (int p = 0; p < 2; ++p)
                gPlayerName[p] = gSetupBuf[p].empty() ? def[p] : gSetupBuf[p];
        }

        // Pre-compute allStopped (used both inside and outside the gameplay block)
        bool allStopped = physics.balls[0].active;
        if (allStopped)
            for (const auto& b : physics.balls)
                if ((b.active || b.sinking) && glm::length(b.vel) > Physics::V_STOP * 2.0f)
                    { allStopped = false; break; }

        if (!gMenuOpen && !gSetupScreen) {

        // --- Tab: toggle camera mode (also cancels post-shot overview) ---
        bool tabNow = (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS);
        if (tabNow && !gWasTab) {
            gShotCamMode  = !gShotCamMode;
            gBallsMoving  = false;   // manual override exits overview
            gStopTimer    = 0.0f;
        }
        gWasTab = tabNow;

        // --- Keyboard controls ---
        const float ROT_SPEED   = glm::radians(45.0f);
        const float PWR_SPEED   = 3.0f;   // power units/s  (max=6 → full charge in 2s)
        const float SPIN_SPEED  = 1.2f;   // spin units/s

        // Aim: left/right arrows
        if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_4)  == GLFW_PRESS)
            gShotAngle -= ROT_SPEED * dt;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_6)  == GLFW_PRESS)
            gShotAngle += ROT_SPEED * dt;
        gShotDir = glm::normalize(glm::vec3(cosf(gShotAngle), 0.0f, sinf(gShotAngle)));

        // Power: up/down arrows
        if (allStopped && physics.balls[0].active && !gBallInHand &&
            gameLogic.phase != GamePhase::GAME_OVER) {
            if (glfwGetKey(window, GLFW_KEY_UP)   == GLFW_PRESS)
                gCuePower = std::min(gCuePower + PWR_SPEED * dt, 6.0f);
            if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS)
                gCuePower = std::max(gCuePower - PWR_SPEED * dt, 0.0f);

            // Spin: Q/E = side (U),  W/S = top/back (V)
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
                gSpinU = std::max(gSpinU - SPIN_SPEED * dt, -1.0f);
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
                gSpinU = std::min(gSpinU + SPIN_SPEED * dt,  1.0f);
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                gSpinV = std::min(gSpinV + SPIN_SPEED * dt,  1.0f);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                gSpinV = std::max(gSpinV - SPIN_SPEED * dt, -1.0f);
            // Clamp spin to unit circle
            float slen = sqrtf(gSpinU*gSpinU + gSpinV*gSpinV);
            if (slen > 1.0f) { gSpinU /= slen; gSpinV /= slen; }
        }

        if (allStopped && physics.balls[0].active && !gBallInHand)
            aimGuide.update(physics.balls[0].pos, gShotDir, physics.balls, gCuePower);

        // Check sinking state (needed for shot gating + game logic trigger)
        bool anySinking = false;
        for (const auto& b : physics.balls) if (b.sinking) { anySinking = true; break; }

        if (gShooting) {
            if (allStopped && !anySinking && physics.balls[0].active
                && gameLogic.phase != GamePhase::GAME_OVER && !gBallInHand) {
                physics.resetShotTracking();
                Physics::applyCueStrike(physics.balls[0], gShotDir,
                                        std::max(gCuePower, 0.3f), gSpinU, gSpinV);
                gBallsMoving          = true;
                gStopTimer            = 0.0f;
                gWaitingForShotResult = true;
                gSpinU = 0.0f;
                gSpinV = 0.0f;
            }
            gShooting = false;
            gCuePower = 0.0f;
        }

        // During ball-in-hand the cue ball must not participate in physics
        // (otherwise it pushes the colored balls when dragged over them).
        if (gBallInHand) physics.balls[0].active = false;
        physics.update(dt);
        if (gBallInHand) physics.balls[0].active = true;
        if (gFoulTimer > 0.0f) gFoulTimer -= dt;

        // --- Cue ball pocketed → enter ball-in-hand mode (player places ball in D) ---
        // Wait until ALL other balls have stopped and finished sinking first.
        {
            auto& cb = physics.balls[0];
            if (!cb.active && !cb.sinking
                    && gameLogic.phase != GamePhase::BLACK_SHOOTOUT
                    && gameLogic.phase != GamePhase::GAME_OVER
                    && !gBallInHand) {
                // Check other balls stopped
                bool othersStopped = true;
                for (int i = 1; i < (int)physics.balls.size(); ++i) {
                    const auto& b = physics.balls[i];
                    if (b.sinking) { othersStopped = false; break; }
                    if (b.active && glm::length(b.vel) > Physics::V_STOP * 2.0f)
                        { othersStopped = false; break; }
                }
                if (othersStopped) {
                    cb.pos    = glm::vec3(BAULK_X, Physics::RADIUS, 0.0f);
                    cb.prevPos = cb.pos;
                    cb.vel    = glm::vec3(0.0f);
                    cb.spin   = glm::vec3(0.0f);
                    cb.orient = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                    cb.active = true;
                    cb.sinkT  = 0.0f;
                    balls[0].orient = cb.orient;
                    gBallInHand = true;
                }
            }
        }

        // --- Ball-in-hand: project cursor onto table plane, constrain to D semi-circle ---
        if (gBallInHand) {
            float xNDC = (2.0f * (float)gLastX / WIDTH)  - 1.0f;
            float yNDC = 1.0f - (2.0f * (float)gLastY / HEIGHT);
            glm::mat4 invPV = glm::inverse(gLastProj * gLastView);
            glm::vec4 nw = invPV * glm::vec4(xNDC, yNDC, -1.0f, 1.0f);  nw /= nw.w;
            glm::vec4 fw = invPV * glm::vec4(xNDC, yNDC,  1.0f, 1.0f);  fw /= fw.w;
            glm::vec3 rayDir = glm::normalize(glm::vec3(fw) - glm::vec3(nw));
            if (std::abs(rayDir.y) > 1e-4f) {
                float t = (Physics::RADIUS - nw.y) / rayDir.y;
                if (t > 0.0f) {
                    glm::vec3 wp = glm::vec3(nw) + t * rayDir;
                    // Constrain: x >= BAULK_X, dist from (BAULK_X,0) <= D_RADIUS
                    float bx = wp.x - BAULK_X;
                    if (bx < 0.0f) bx = 0.0f;
                    float dist = sqrtf(bx * bx + wp.z * wp.z);
                    if (dist > D_RADIUS) { float s = D_RADIUS / dist; bx *= s; wp.z *= s; }
                    glm::vec3 desired = glm::vec3(BAULK_X + bx, Physics::RADIUS, wp.z);

                    // Iteratively push white ball clear of any overlapping ball.
                    // Multiple passes handle chain-cases (push from A causes overlap with B).
                    const float MIN_GAP = 2.0f * Physics::RADIUS + 0.001f;
                    for (int iter = 0; iter < 8; ++iter) {
                        bool anyOverlap = false;
                        for (int bi = 1; bi < (int)physics.balls.size(); ++bi) {
                            if (!physics.balls[bi].active) continue;
                            float dx = desired.x - physics.balls[bi].pos.x;
                            float dz = desired.z - physics.balls[bi].pos.z;
                            float d2 = dx*dx + dz*dz;
                            if (d2 < MIN_GAP * MIN_GAP) {
                                anyOverlap = true;
                                glm::vec3 pushDir;
                                if (d2 < 1e-8f) {
                                    // Exact same position — push toward centre of D
                                    float cx = desired.x - BAULK_X;
                                    float cz = desired.z;
                                    float cl = sqrtf(cx*cx + cz*cz);
                                    pushDir = (cl > 1e-6f)
                                        ? glm::vec3(-cx/cl, 0.0f, -cz/cl)
                                        : glm::vec3(0.0f, 0.0f, -1.0f);
                                } else {
                                    float d = sqrtf(d2);
                                    pushDir = glm::vec3(dx/d, 0.0f, dz/d);
                                }
                                desired = physics.balls[bi].pos + pushDir * MIN_GAP;
                                desired.y = Physics::RADIUS;
                                // Re-clamp to D
                                float nbx = desired.x - BAULK_X;
                                if (nbx < 0.0f) nbx = 0.0f;
                                float nd = sqrtf(nbx*nbx + desired.z*desired.z);
                                if (nd > D_RADIUS) { float s = D_RADIUS/nd; nbx *= s; desired.z *= s; }
                                desired.x = BAULK_X + nbx;
                            }
                        }
                        if (!anyOverlap) break;
                    }

                    auto& cb   = physics.balls[0];
                    cb.pos     = desired;
                    cb.prevPos = cb.pos;
                }
            }
        }

        // --- Sync positions + orientations ---
        for (int i = 0; i < 7; i++) {
            balls[i].pos    = physics.balls[i].pos;
            balls[i].orient = physics.balls[i].orient;
        }
        // Instanced red balls: move inactive ones below table so they're clipped
        for (int i = 0; i < RedBalls::COUNT; i++) {
            const auto& bs = physics.balls[7 + i];
            if (bs.active || bs.sinking) {
                redBalls.positions[i] = bs.pos;
                redBalls.orients[i]   = bs.orient;
            } else {
                redBalls.positions[i] = glm::vec3(0.0f, -10.0f, 0.0f);
                redBalls.orients[i]   = bs.orient;
            }
        }
        redBalls.uploadInstanceData();

        // Re-evaluate stopped/sinking state AFTER physics ran this frame.
        // This prevents processShot from firing on the same frame as the shot.
        bool allStoppedNow = physics.balls[0].active;
        if (allStoppedNow)
            for (const auto& b : physics.balls)
                if ((b.active || b.sinking) && glm::length(b.vel) > Physics::V_STOP * 2.0f)
                    { allStoppedNow = false; break; }
        bool anySinkingNow = false;
        for (const auto& b : physics.balls) if (b.sinking) { anySinkingNow = true; break; }

        // --- Process shot result once all balls have stopped and finished sinking ---
        if (gWaitingForShotResult && allStoppedNow && !anySinkingNow) {
            int redsOnTable = 0;
            for (int i = 7; i < (int)physics.balls.size(); i++)
                if (physics.balls[i].active) redsOnTable++;

            ShotResult res = gameLogic.processShot(
                physics.shotFirstContact,
                physics.shotPottedBalls,
                redsOnTable
            );

            // Respawn colored balls: own spot if free, else highest free spot, else nearest gap
            for (int idx : res.respawn) {
                auto& bs    = physics.balls[idx];
                bs.pos      = findRespawnPos(physics.balls, idx);
                bs.prevPos  = bs.pos;
                bs.vel      = glm::vec3(0.0f);
                bs.spin     = glm::vec3(0.0f);
                bs.orient   = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                bs.active   = true;
                bs.sinking  = false;
                bs.sinkT    = 0.0f;
                balls[idx].orient = bs.orient;
            }

            if (res.foul) {
                gFoulLine1 = "FOUL!";
                gFoulLine2 = "+" + std::to_string(res.foulPenalty)
                           + " for " + gPlayerName[gameLogic.currentPlayer];
                gFoulTimer = 2.5f;
                playSound("foul.wav");
            } else if (res.points > 0) {
                playBreak(gameLogic.currentBreak);
                if ((rand() % 3) == 0)
                    playSound("applause.wav");
            } else if (!res.foul && res.switchPlayer) {
                // Miss: random ~1 time out of 3
                if ((rand() % 3) == 0)
                    playSound("miss.wav");
            }

            // Ball-in-hand granted by game logic (shootout miss/foul, or tie respot)
            if (res.ballInHand) {
                auto& cb   = physics.balls[0];
                cb.pos     = glm::vec3(BAULK_X, Physics::RADIUS, 0.0f);
                cb.prevPos = cb.pos;
                cb.vel     = glm::vec3(0.0f);
                cb.spin    = glm::vec3(0.0f);
                cb.orient  = glm::quat(1,0,0,0);
                cb.active  = true;
                cb.sinkT   = 0.0f;
                balls[0].orient = cb.orient;
                gBallInHand = true;
            }

            char title[256];
            snprintf(title, sizeof(title), "Snooker | P1:%d  P2:%d | %s",
                     gameLogic.scores[0], gameLogic.scores[1],
                     gameLogic.statusMsg.c_str());
            glfwSetWindowTitle(window, title);

            gWaitingForShotResult = false;
        }

        // --- Post-shot timer: auto-return to shot cam 2 s after all balls stop ---
        if (gBallsMoving) {
            if (!allStoppedNow) {
                gStopTimer = 0.0f;
            } else {
                gStopTimer += dt;
                if (gStopTimer >= 2.0f) {
                    gBallsMoving = false;
                    gStopTimer   = 0.0f;
                    gShotCamMode = true;
                }
            }
        }

        } // end if (!gMenuOpen)

        // --- View matrix ---
        glm::mat4 view;
        glm::vec3 viewPos;
        if (gBallInHand) {
            // Top-down view. Camera raised to y=3.0 so the full table fits in the
            // viewport area below the HUD bar without clipping any pocket.
            viewPos = glm::vec3(0.0f, 3.0f, 0.0f);
            view    = glm::lookAt(viewPos,
                                  glm::vec3(0.0f, 0.0f, 0.0f),
                                  glm::vec3(0.0f, 0.0f, -1.0f));
        } else if (gBallsMoving) {
            // Post-shot: same top-down view.
            viewPos = glm::vec3(0.0f, 3.0f, 0.0f);
            view    = glm::lookAt(viewPos,
                                  glm::vec3(0.0f, 0.0f, 0.0f),
                                  glm::vec3(0.0f, 0.0f, -1.0f));
        } else if (gShotCamMode && physics.balls[0].active) {
            // Shot camera: behind cue ball, distance controlled by scroll
            glm::vec3 cBP = physics.balls[0].pos;
            viewPos = cBP - gShotDir * gShotCamDist + glm::vec3(0.0f, gShotCamHeight, 0.0f);
            // Look-at scales with distance: close = just past cue ball, far = further ahead
            float lookAhead = gShotCamDist * 0.8f;
            view    = glm::lookAt(viewPos, cBP + gShotDir * lookAhead,
                                  glm::vec3(0.0f, 1.0f, 0.0f));
        } else {
            // Top-down view of the whole table
            viewPos = glm::vec3(0.0f, 3.0f, 0.0f);
            view    = glm::lookAt(viewPos,
                                  glm::vec3(0.0f, 0.0f, 0.0f),
                                  glm::vec3(0.0f, 0.0f, -1.0f));
        }

        // Store for next frame's ball-in-hand unproject
        gLastView = view;
        gLastProj = projection;

        // --- 3D render ---
        // Use wide panoramic projection during post-shot overview
        const glm::mat4& activeProj = projection;

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Skybox (draw first, depth test off so it never fails the z=1 vs clear=1 comparison) ---
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        skyboxShader.use();
        glm::mat4 skyView = glm::mat4(glm::mat3(view));
        skyboxShader.setMat4("view",       skyView);
        skyboxShader.setMat4("projection", activeProj);
        skybox.draw();   // cubemap already on unit 1
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);

        shader.use();
        shader.setMat4("projection", activeProj);
        shader.setMat4("view",       view);
        shader.setVec3("viewPos",    viewPos);
        shader.setMat4("model",      glm::mat4(1.0f));
        table.draw(shader, feltTex, woodTex);

        // Draw coloured balls with subtle environment reflection (cubemap on unit 1)
        shader.setFloat("reflectStrength", 0.12f);
        for (int i = 0; i < 7; i++) {
            const auto& bs = physics.balls[i];
            if (bs.active || bs.sinking)
                balls[i].draw(shader);
        }
        shader.setFloat("reflectStrength", 0.0f);

        instancedShader.use();
        instancedShader.setMat4("projection", activeProj);
        instancedShader.setMat4("view",       view);
        instancedShader.setVec3("viewPos",    viewPos);
        instancedShader.setFloat("reflectStrength", 0.12f);
        redBalls.draw(instancedShader);
        instancedShader.setFloat("reflectStrength", 0.0f);

        // 3D aim guide (only when cue ball is at rest, not during overview)
        if (!gBallsMoving && allStopped && physics.balls[0].active) {
            flatShader.use();
            flatShader.setMat4("projection", activeProj);
            flatShader.setMat4("view",       view);
            aimGuide.draw(flatShader);
        }

        // --- 2D HUD overlay ---
        glDisable(GL_DEPTH_TEST);
        flatShader.use();
        flatShader.setMat4("projection", ortho2D);
        flatShader.setMat4("view", glm::mat4(1.0f));

        // ---- MENU button (top-left corner) ----
        {
            float mx = (float)gLastX, my = (float)gLastY;
            bool hov = hitRect({MNU_BTN_X0, MNU_BTN_Y0, MNU_BTN_X1, MNU_BTN_Y1}, mx, my);
            // Background
            flatShader.setVec3("lineColor", hov ? 0.22f : 0.10f, hov ? 0.22f : 0.10f, hov ? 0.26f : 0.12f);
            drawRect(MNU_BTN_X0, MNU_BTN_Y0, MNU_BTN_X1, MNU_BTN_Y1);
            // Border
            float bd = hov ? 0.65f : 0.38f;
            flatShader.setVec3("lineColor", bd, bd, bd);
            glLineWidth(1.2f);
            draw2D({{MNU_BTN_X0,MNU_BTN_Y0,0},{MNU_BTN_X1,MNU_BTN_Y0,0},
                    {MNU_BTN_X1,MNU_BTN_Y0,0},{MNU_BTN_X1,MNU_BTN_Y1,0},
                    {MNU_BTN_X1,MNU_BTN_Y1,0},{MNU_BTN_X0,MNU_BTN_Y1,0},
                    {MNU_BTN_X0,MNU_BTN_Y1,0},{MNU_BTN_X0,MNU_BTN_Y0,0}}, GL_LINES);
            glLineWidth(1.0f);
            // Label
            float tc = hov ? 1.0f : 0.75f;
            flatShader.setVec3("lineColor", tc, tc, tc);
            const char* lbl = "MENU";
            float lw = (float)stb_easy_font_width((char*)lbl) * 1.6f;
            drawText((MNU_BTN_X0+MNU_BTN_X1)*0.5f - lw*0.5f,
                     (MNU_BTN_Y0+MNU_BTN_Y1)*0.5f - 6.f, lbl, 1.6f);
        }

        // Power gauge and spin selector only shown while aiming (not during top-down view)
        if (!gBallsMoving && !gBallInHand)
        {
            const float GX0 = GAUGE_X - GAUGE_W;
            const float GX1 = GAUGE_X + GAUGE_W;
            float t     = gCuePower / 6.0f;
            float fillY = GAUGE_Y_BOT - t * (GAUGE_Y_BOT - GAUGE_Y_TOP);

            // --- Power gauge ---
            // Background
            flatShader.setVec3("lineColor", 0.10f, 0.10f, 0.10f);
            draw2D({ {GX0,GAUGE_Y_TOP,0},{GX1,GAUGE_Y_TOP,0},{GX1,GAUGE_Y_BOT,0},
                     {GX0,GAUGE_Y_TOP,0},{GX1,GAUGE_Y_BOT,0},{GX0,GAUGE_Y_BOT,0} },
                   GL_TRIANGLES);

            // Colored fill: green (t=0) → yellow → red (t=1)
            if (t > 0.001f) {
                float r = std::min(2.0f * t,        1.0f);
                float g = std::min(2.0f * (1.0f-t), 1.0f);
                flatShader.setVec3("lineColor", r, g, 0.0f);
                draw2D({ {GX0,fillY,0},{GX1,fillY,0},{GX1,GAUGE_Y_BOT,0},
                         {GX0,fillY,0},{GX1,GAUGE_Y_BOT,0},{GX0,GAUGE_Y_BOT,0} },
                       GL_TRIANGLES);
            }

            // White border
            glLineWidth(1.5f);
            flatShader.setVec3("lineColor", 0.85f, 0.85f, 0.85f);
            draw2D({ {GX0,GAUGE_Y_TOP,0},{GX1,GAUGE_Y_TOP,0},
                     {GX1,GAUGE_Y_TOP,0},{GX1,GAUGE_Y_BOT,0},
                     {GX1,GAUGE_Y_BOT,0},{GX0,GAUGE_Y_BOT,0},
                     {GX0,GAUGE_Y_BOT,0},{GX0,GAUGE_Y_TOP,0} }, GL_LINES);

            // Tick marks at 25 / 50 / 75 %
            flatShader.setVec3("lineColor", 0.45f, 0.45f, 0.45f);
            for (float pct : {0.25f, 0.50f, 0.75f}) {
                float ty = GAUGE_Y_BOT - pct*(GAUGE_Y_BOT - GAUGE_Y_TOP);
                draw2D({ {GX0,ty,0},{GX0+6,ty,0}, {GX1-6,ty,0},{GX1,ty,0} }, GL_LINES);
            }

            // --- Spin selector ---
            // Outer circle
            const int SEGS = 48;
            std::vector<glm::vec3> circlePts;
            circlePts.reserve(SEGS*2);
            for (int i = 0; i < SEGS; i++) {
                float a0 = 2.0f*3.14159f*(float)i     /SEGS;
                float a1 = 2.0f*3.14159f*(float)(i+1) /SEGS;
                circlePts.push_back({SPIN_CX + cosf(a0)*SPIN_R, SPIN_CY + sinf(a0)*SPIN_R, 0});
                circlePts.push_back({SPIN_CX + cosf(a1)*SPIN_R, SPIN_CY + sinf(a1)*SPIN_R, 0});
            }
            // Dark fill inside circle
            std::vector<glm::vec3> fillPts;
            fillPts.reserve(SEGS*3);
            for (int i = 0; i < SEGS; i++) {
                float a0 = 2.0f*3.14159f*(float)i     /SEGS;
                float a1 = 2.0f*3.14159f*(float)(i+1) /SEGS;
                fillPts.push_back({SPIN_CX, SPIN_CY, 0});
                fillPts.push_back({SPIN_CX + cosf(a0)*SPIN_R, SPIN_CY + sinf(a0)*SPIN_R, 0});
                fillPts.push_back({SPIN_CX + cosf(a1)*SPIN_R, SPIN_CY + sinf(a1)*SPIN_R, 0});
            }
            flatShader.setVec3("lineColor", 0.08f, 0.08f, 0.08f);
            draw2D(fillPts, GL_TRIANGLES);

            flatShader.setVec3("lineColor", 0.85f, 0.85f, 0.85f);
            glLineWidth(1.5f);
            draw2D(circlePts, GL_LINES);

            // Crosshair
            flatShader.setVec3("lineColor", 0.35f, 0.35f, 0.35f);
            glLineWidth(1.0f);
            draw2D({ {SPIN_CX-SPIN_R,SPIN_CY,0},{SPIN_CX+SPIN_R,SPIN_CY,0},
                     {SPIN_CX,SPIN_CY-SPIN_R,0},{SPIN_CX,SPIN_CY+SPIN_R,0} }, GL_LINES);

            // Hit-point dot
            float dotX = SPIN_CX + gSpinU * SPIN_R;
            float dotY = SPIN_CY - gSpinV * SPIN_R;
            const float dotR = 7.0f;
            const int   DSEGS = 14;
            std::vector<glm::vec3> dotPts;
            dotPts.reserve(DSEGS*3);
            for (int i = 0; i < DSEGS; i++) {
                float a0 = 2.0f*3.14159f*(float)i     /DSEGS;
                float a1 = 2.0f*3.14159f*(float)(i+1) /DSEGS;
                dotPts.push_back({dotX, dotY, 0});
                dotPts.push_back({dotX + cosf(a0)*dotR, dotY + sinf(a0)*dotR, 0});
                dotPts.push_back({dotX + cosf(a1)*dotR, dotY + sinf(a1)*dotR, 0});
            }
            // Amber when spin active, white when centered
            float spinLen = sqrtf(gSpinU*gSpinU + gSpinV*gSpinV);
            if (spinLen > 0.05f)
                flatShader.setVec3("lineColor", 1.0f, 0.70f, 0.10f);
            else
                flatShader.setVec3("lineColor", 0.90f, 0.90f, 0.90f);
            draw2D(dotPts, GL_TRIANGLES);
        } // end gauge + spin

        // ---- HUD: single centered top bar ----
        {
            const float BAR_W  = 740.0f;
            const float BAR_H  = 64.0f;
            const float BAR_X0 = (float)WIDTH  * 0.5f - BAR_W * 0.5f;
            const float BAR_X1 = BAR_X0 + BAR_W;
            const float BAR_Y0 = 6.0f;
            const float BAR_Y1 = BAR_Y0 + BAR_H;
            const float DIV1   = BAR_X0 + 200.0f;
            const float DIV2   = BAR_X1 - 200.0f;

            int  cp      = gameLogic.currentPlayer;
            bool topDown = gBallsMoving || gBallInHand;

            float bgA   = topDown ? 0.04f : 0.14f;
            float bordA = topDown ? 0.20f : 0.50f;

            // Background
            flatShader.setVec3("lineColor", bgA, bgA, bgA * 1.4f);
            drawRect(BAR_X0, BAR_Y0, BAR_X1, BAR_Y1);

            // Gold accent bar on active player's side
            if (!topDown) {
                flatShader.setVec3("lineColor", 0.90f, 0.68f, 0.08f);
                if (cp == 0) drawRect(BAR_X0, BAR_Y1-3.f, DIV1, BAR_Y1);
                else         drawRect(DIV2,   BAR_Y1-3.f, BAR_X1, BAR_Y1);
            }

            glLineWidth(1.0f);
            flatShader.setVec3("lineColor", bordA, bordA, bordA);
            draw2D({{BAR_X0,BAR_Y0,0},{BAR_X1,BAR_Y0,0},{BAR_X1,BAR_Y0,0},{BAR_X1,BAR_Y1,0},
                    {BAR_X1,BAR_Y1,0},{BAR_X0,BAR_Y1,0},{BAR_X0,BAR_Y1,0},{BAR_X0,BAR_Y0,0}}, GL_LINES);
            draw2D({{DIV1,BAR_Y0+5.f,0},{DIV1,BAR_Y1-5.f,0}}, GL_LINES);
            draw2D({{DIV2,BAR_Y0+5.f,0},{DIV2,BAR_Y1-5.f,0}}, GL_LINES);

            // Helper: draw name truncated to fit in maxW pixels
            auto drawName = [&](float x, float y, const std::string& name, float scale, float maxW) {
                // Truncate with "..." if too wide
                std::string s = name;
                while (!s.empty() && (float)stb_easy_font_width((char*)s.c_str()) * scale > maxW)
                    s = s.substr(0, s.size()-1);
                if (s != name) s += "..";
                drawText(x, y, s, scale);
            };

            // --- Player 1 section ---
            bool p1active = (cp == 0);
            float p1b = topDown ? 0.35f : (p1active ? 1.0f : 0.38f);
            if (p1active && !topDown) flatShader.setVec3("lineColor", 1.0f, 0.80f, 0.15f);
            else                      flatShader.setVec3("lineColor", p1b, p1b, p1b);
            drawName(BAR_X0+10.f, BAR_Y0+6.f, gPlayerName[0], 1.4f, DIV1 - BAR_X0 - 55.f);

            // Frame score (small, top-right of section)
            std::string fr1 = std::to_string(gFrameScore[0]) + " fr";
            float fr1A = topDown ? 0.22f : (p1active ? 0.70f : 0.30f);
            flatShader.setVec3("lineColor", fr1A, fr1A*0.85f, fr1A*0.2f);
            float fr1w = (float)stb_easy_font_width((char*)fr1.c_str()) * 1.15f;
            drawText(DIV1 - fr1w - 6.f, BAR_Y0+6.f, fr1, 1.15f);

            // Game score (large)
            if (p1active && !topDown) flatShader.setVec3("lineColor", 1.0f, 0.80f, 0.15f);
            else                      flatShader.setVec3("lineColor", p1b, p1b, p1b);
            std::string sc1 = std::to_string(gameLogic.scores[0]);
            float s1w = (float)stb_easy_font_width((char*)sc1.c_str()) * 3.0f;
            drawText(DIV1 - s1w - 10.f, BAR_Y0+28.f, sc1, 3.0f);

            // --- Player 2 section ---
            bool p2active = (cp == 1);
            float p2b = topDown ? 0.35f : (p2active ? 1.0f : 0.38f);
            if (p2active && !topDown) flatShader.setVec3("lineColor", 1.0f, 0.80f, 0.15f);
            else                      flatShader.setVec3("lineColor", p2b, p2b, p2b);
            // Name right-aligned
            float n2len = (float)stb_easy_font_width((char*)gPlayerName[1].c_str()) * 1.4f;
            float n2maxW = BAR_X1 - DIV2 - 55.f;
            {
                std::string s = gPlayerName[1];
                while (!s.empty() && (float)stb_easy_font_width((char*)s.c_str())*1.4f > n2maxW)
                    s = s.substr(0, s.size()-1);
                if (s != gPlayerName[1]) s += "..";
                float sw = (float)stb_easy_font_width((char*)s.c_str()) * 1.4f;
                drawText(BAR_X1 - sw - 10.f, BAR_Y0+6.f, s, 1.4f);
                (void)n2len;
            }

            std::string fr2 = "fr " + std::to_string(gFrameScore[1]);
            float fr2A = topDown ? 0.22f : (p2active ? 0.70f : 0.30f);
            flatShader.setVec3("lineColor", fr2A, fr2A*0.85f, fr2A*0.2f);
            drawText(DIV2 + 6.f, BAR_Y0+6.f, fr2, 1.15f);

            if (p2active && !topDown) flatShader.setVec3("lineColor", 1.0f, 0.80f, 0.15f);
            else                      flatShader.setVec3("lineColor", p2b, p2b, p2b);
            std::string sc2 = std::to_string(gameLogic.scores[1]);
            drawText(DIV2 + 10.f, BAR_Y0+28.f, sc2, 3.0f);

            // --- Center section: action text + break ---
            {
                float midX = (DIV1 + DIV2) * 0.5f;
                float ct = topDown ? 0.42f : 0.95f;
                flatShader.setVec3("lineColor", ct, ct*0.92f, ct*0.75f);
                float tw = (float)stb_easy_font_width((char*)gameLogic.statusMsg.c_str()) * 1.4f;
                drawText(midX - tw*0.5f, BAR_Y0+6.f, gameLogic.statusMsg, 1.4f);

                if (gameLogic.currentBreak > 0) {
                    std::string brkStr = "Break: " + std::to_string(gameLogic.currentBreak);
                    float bt = topDown ? 0.28f : 0.75f;
                    flatShader.setVec3("lineColor", 0.40f*bt, 0.90f*bt, 0.55f*bt);
                    float bw = (float)stb_easy_font_width((char*)brkStr.c_str()) * 1.3f;
                    drawText(midX - bw*0.5f, BAR_Y0+36.f, brkStr, 1.3f);
                }
            }
        }

        // ---- Camera toggle button (bottom-right, to the left of spin selector) ----
        if (!gBallsMoving && !gBallInHand) {
            bool camHover = false;
            {
                float dx = (float)gLastX - CAM_BTN_CX, dy = (float)gLastY - CAM_BTN_CY;
                camHover = (dx*dx + dy*dy <= (CAM_BTN_R+4)*(CAM_BTN_R+4));
            }
            float camBg = camHover ? 0.22f : 0.10f;
            float camBd = camHover ? 0.75f : 0.45f;

            // Button background (filled circle)
            const int CSEGS = 24;
            std::vector<glm::vec3> camFill;
            camFill.reserve(CSEGS*3);
            for (int i = 0; i < CSEGS; i++) {
                float a0 = 2.f*3.14159f*(float)i/CSEGS;
                float a1 = 2.f*3.14159f*(float)(i+1)/CSEGS;
                camFill.push_back({CAM_BTN_CX, CAM_BTN_CY, 0});
                camFill.push_back({CAM_BTN_CX+cosf(a0)*CAM_BTN_R, CAM_BTN_CY+sinf(a0)*CAM_BTN_R, 0});
                camFill.push_back({CAM_BTN_CX+cosf(a1)*CAM_BTN_R, CAM_BTN_CY+sinf(a1)*CAM_BTN_R, 0});
            }
            flatShader.setVec3("lineColor", camBg, camBg, camBg);
            draw2D(camFill, GL_TRIANGLES);

            // Border circle
            std::vector<glm::vec3> camBord; camBord.reserve(CSEGS*2);
            for (int i = 0; i < CSEGS; i++) {
                float a0 = 2.f*3.14159f*(float)i/CSEGS;
                float a1 = 2.f*3.14159f*(float)(i+1)/CSEGS;
                camBord.push_back({CAM_BTN_CX+cosf(a0)*CAM_BTN_R, CAM_BTN_CY+sinf(a0)*CAM_BTN_R, 0});
                camBord.push_back({CAM_BTN_CX+cosf(a1)*CAM_BTN_R, CAM_BTN_CY+sinf(a1)*CAM_BTN_R, 0});
            }
            flatShader.setVec3("lineColor", camBd, camBd, camBd);
            glLineWidth(1.5f);
            draw2D(camBord, GL_LINES);

            // Camera icon: body rectangle + lens circle + viewfinder bump
            float icX = CAM_BTN_CX, icY = CAM_BTN_CY + 2.f;
            // Body
            flatShader.setVec3("lineColor", camBd, camBd, camBd);
            draw2D({{icX-9,icY-5,0},{icX+9,icY-5,0},{icX+9,icY+6,0},
                    {icX-9,icY-5,0},{icX+9,icY+6,0},{icX-9,icY+6,0}}, GL_TRIANGLES);
            // Viewfinder bump (top center)
            draw2D({{icX-3,icY-8,0},{icX+3,icY-8,0},{icX+3,icY-5,0},
                    {icX-3,icY-8,0},{icX+3,icY-5,0},{icX-3,icY-5,0}}, GL_TRIANGLES);
            // Lens (dark circle inside body)
            const int LSEGS = 12;
            std::vector<glm::vec3> lens; lens.reserve(LSEGS*3);
            for (int i = 0; i < LSEGS; i++) {
                float a0 = 2.f*3.14159f*(float)i/LSEGS;
                float a1 = 2.f*3.14159f*(float)(i+1)/LSEGS;
                lens.push_back({icX, icY+1.f, 0});
                lens.push_back({icX+cosf(a0)*4.5f, icY+1.f+sinf(a0)*4.5f, 0});
                lens.push_back({icX+cosf(a1)*4.5f, icY+1.f+sinf(a1)*4.5f, 0});
            }
            flatShader.setVec3("lineColor", camBg*0.5f, camBg*0.5f, camBg*0.5f + 0.12f);
            draw2D(lens, GL_TRIANGLES);
            glLineWidth(1.0f);

            // Mode indicator text below button
            const char* modeStr = gShotCamMode ? "Shot" : "Top";
            float modeW = (float)stb_easy_font_width((char*)modeStr) * 1.1f;
            flatShader.setVec3("lineColor", camBd*0.7f, camBd*0.7f, camBd*0.7f);
            drawText(CAM_BTN_CX - modeW*0.5f, CAM_BTN_CY + CAM_BTN_R + 4.f, modeStr, 1.1f);
        }

        // ---- Ball-in-hand indicator ----
        if (gBallInHand) {
            const char* msg = "Place the white ball in the D  (click to confirm)";
            flatShader.setVec3("lineColor", 0.08f, 0.08f, 0.10f);
            float mw = (float)stb_easy_font_width((char*)msg) * 1.5f;
            float mx = (float)WIDTH * 0.5f - mw * 0.5f;
            drawRect(mx - 10.f, (float)HEIGHT - 46.f, mx + mw + 10.f, (float)HEIGHT - 10.f);
            flatShader.setVec3("lineColor", 0.95f, 0.85f, 0.30f);
            drawText(mx, (float)HEIGHT - 36.f, msg, 1.5f);
        }

        // ---- Winner announcement overlay ----
        if (gWinnerTimer > 0.0f && !gWinnerMsg.empty()) {
            // Dim background
            flatShader.setVec3("lineColor", 0.0f, 0.0f, 0.02f);
            drawRect(0, 0, (float)WIDTH, (float)HEIGHT);

            const float OW = 620.0f, OH = 180.0f;
            const float OX0 = (float)WIDTH*0.5f - OW*0.5f;
            const float OY0 = (float)HEIGHT*0.5f - OH*0.5f;
            const float OX1 = OX0+OW, OY1 = OY0+OH;

            // Panel
            flatShader.setVec3("lineColor", 0.06f, 0.06f, 0.08f);
            drawRect(OX0, OY0, OX1, OY1);

            // Gold border
            flatShader.setVec3("lineColor", 0.85f, 0.65f, 0.08f);
            glLineWidth(2.5f);
            draw2D({{OX0,OY0,0},{OX1,OY0,0},{OX1,OY0,0},{OX1,OY1,0},
                    {OX1,OY1,0},{OX0,OY1,0},{OX0,OY1,0},{OX0,OY0,0}}, GL_LINES);
            glLineWidth(1.0f);

            // Player name — big, gold, centered
            float nameScale = 4.5f;
            float nw = (float)stb_easy_font_width((char*)gWinnerMsg.c_str()) * nameScale;
            // Scale down if name is too wide
            while (nw > OW - 40.f && nameScale > 1.5f) {
                nameScale -= 0.3f;
                nw = (float)stb_easy_font_width((char*)gWinnerMsg.c_str()) * nameScale;
            }
            flatShader.setVec3("lineColor", 1.0f, 0.85f, 0.20f);
            drawText(OX0+OW*0.5f - nw*0.5f, OY0+24.f, gWinnerMsg, nameScale);

            // Subtitle line
            float subScale = 2.0f;
            float sw2 = (float)stb_easy_font_width((char*)gWinnerSub.c_str()) * subScale;
            flatShader.setVec3("lineColor", 0.90f, 0.90f, 0.85f);
            drawText(OX0+OW*0.5f - sw2*0.5f, OY0+OH - 52.f, gWinnerSub, subScale);

            // Countdown bar at bottom
            float prog = gWinnerTimer / (gWinnerSub.find("frame") != std::string::npos ? 3.5f : 4.0f);
            float barW = (OW - 30.f) * prog;
            flatShader.setVec3("lineColor", 0.20f, 0.20f, 0.22f);
            drawRect(OX0+15.f, OY1-12.f, OX1-15.f, OY1-5.f);
            flatShader.setVec3("lineColor", 0.70f, 0.55f, 0.08f);
            drawRect(OX0+15.f, OY1-12.f, OX0+15.f+barW, OY1-5.f);
        }

        // ---- Startup name panel ----
        if (gSetupScreen) {
            const float SW = 420.0f, SH = 260.0f;
            const float SX0 = (float)WIDTH*0.5f - SW*0.5f;
            const float SY0 = (float)HEIGHT*0.5f - SH*0.5f;
            const float SX1 = SX0 + SW, SY1 = SY0 + SH;

            // Dim background
            flatShader.setVec3("lineColor", 0.0f, 0.0f, 0.0f);
            drawRect(0, 0, (float)WIDTH, (float)HEIGHT);

            // Panel
            flatShader.setVec3("lineColor", 0.08f, 0.08f, 0.11f);
            drawRect(SX0, SY0, SX1, SY1);
            flatShader.setVec3("lineColor", 0.50f, 0.50f, 0.55f);
            glLineWidth(1.5f);
            draw2D({{SX0,SY0,0},{SX1,SY0,0},{SX1,SY0,0},{SX1,SY1,0},
                    {SX1,SY1,0},{SX0,SY1,0},{SX0,SY1,0},{SX0,SY0,0}}, GL_LINES);
            glLineWidth(1.0f);

            // Title
            flatShader.setVec3("lineColor", 0.90f, 0.80f, 0.20f);
            const char* ttl = "SNOOKER 3D";
            float ttlW = (float)stb_easy_font_width((char*)ttl) * 2.4f;
            drawText(SX0 + SW*0.5f - ttlW*0.5f, SY0+14.f, ttl, 2.4f);
            flatShader.setVec3("lineColor", 0.28f, 0.28f, 0.30f);
            draw2D({{SX0+10,SY0+44.f,0},{SX1-10,SY0+44.f,0}}, GL_LINES);

            // Name fields
            const char* labels[2] = {"Player 1 :", "Player 2 :"};
            for (int p = 0; p < 2; ++p) {
                float fy = SY0 + 52.f + p*62.f;
                // Label
                flatShader.setVec3("lineColor", 0.55f, 0.55f, 0.60f);
                drawText(SX0+14.f, fy+4.f, labels[p], 1.35f);
                // Input box
                Rect fr{SX0+130, fy, SX1-20, fy+28.f};
                bool focused = (gSetupFocus == p);
                flatShader.setVec3("lineColor", focused?0.16f:0.10f, focused?0.16f:0.10f, focused?0.20f:0.12f);
                drawRect(fr.x0, fr.y0, fr.x1, fr.y1);
                float brd = focused ? 0.75f : 0.35f;
                flatShader.setVec3("lineColor", focused?0.45f:brd, focused?0.65f:brd, focused?0.90f:brd);
                glLineWidth(focused ? 1.8f : 1.0f);
                draw2D({{fr.x0,fr.y0,0},{fr.x1,fr.y0,0},{fr.x1,fr.y0,0},{fr.x1,fr.y1,0},
                        {fr.x1,fr.y1,0},{fr.x0,fr.y1,0},{fr.x0,fr.y1,0},{fr.x0,fr.y0,0}}, GL_LINES);
                glLineWidth(1.0f);
                // Text + cursor
                flatShader.setVec3("lineColor", 0.95f, 0.95f, 0.95f);
                std::string disp = gSetupBuf[p] + (focused ? "_" : "");
                drawText(fr.x0+6.f, fr.y0+5.f, disp, 1.4f);
            }

            // Start button
            Rect startBtn{SX0+20, SY1-54.f, SX1-20, SY1-20.f};
            float mx2 = (float)gLastX, my2 = (float)gLastY;
            bool hovStart = hitRect(startBtn, mx2, my2);
            flatShader.setVec3("lineColor", hovStart?0.12f:0.07f, hovStart?0.22f:0.14f, hovStart?0.12f:0.07f);
            drawRect(startBtn.x0, startBtn.y0, startBtn.x1, startBtn.y1);
            float sbrd = hovStart ? 0.60f : 0.38f;
            flatShader.setVec3("lineColor", 0.15f, sbrd, 0.20f);
            glLineWidth(1.5f);
            draw2D({{startBtn.x0,startBtn.y0,0},{startBtn.x1,startBtn.y0,0},
                    {startBtn.x1,startBtn.y0,0},{startBtn.x1,startBtn.y1,0},
                    {startBtn.x1,startBtn.y1,0},{startBtn.x0,startBtn.y1,0},
                    {startBtn.x0,startBtn.y1,0},{startBtn.x0,startBtn.y0,0}}, GL_LINES);
            glLineWidth(1.0f);
            flatShader.setVec3("lineColor", 0.85f, 0.97f, 0.85f);
            const char* slbl = "Start Game";
            float slw = (float)stb_easy_font_width((char*)slbl) * 1.7f;
            drawText((startBtn.x0+startBtn.x1)*0.5f - slw*0.5f, startBtn.y0+7.f, slbl, 1.7f);

            // Hint
            flatShader.setVec3("lineColor", 0.32f, 0.32f, 0.35f);
            const char* hint = "Tab to switch field  |  Enter to confirm";
            float hw = (float)stb_easy_font_width((char*)hint) * 1.1f;
            drawText(SX0+SW*0.5f-hw*0.5f, SY1-12.f, hint, 1.1f);
        }

        // ---- FOUL overlay ----
        if (gFoulTimer > 0.0f) {
            const float FX0 = 400.f, FX1 = 880.f;
            const float FY0 = 268.f, FY1 = 390.f;
            // Dark background
            flatShader.setVec3("lineColor", 0.15f, 0.03f, 0.03f);
            drawRect(FX0, FY0, FX1, FY1);
            // Red border
            glLineWidth(2.5f);
            flatShader.setVec3("lineColor", 0.90f, 0.12f, 0.12f);
            draw2D({{FX0,FY0,0},{FX1,FY0,0},{FX1,FY0,0},{FX1,FY1,0},
                    {FX1,FY1,0},{FX0,FY1,0},{FX0,FY1,0},{FX0,FY0,0}}, GL_LINES);
            glLineWidth(1.5f);
            // "FOUL!" large centered
            flatShader.setVec3("lineColor", 1.0f, 0.18f, 0.12f);
            float fw = (float)stb_easy_font_width((char*)"FOUL!") * 5.0f;
            drawText((FX0 + FX1) * 0.5f - fw * 0.5f, FY0 + 18.f, "FOUL!", 5.0f);
            // Penalty line
            flatShader.setVec3("lineColor", 0.95f, 0.95f, 0.85f);
            float pw = (float)stb_easy_font_width((char*)gFoulLine2.c_str()) * 2.2f;
            drawText((FX0 + FX1) * 0.5f - pw * 0.5f, FY0 + 88.f, gFoulLine2, 2.2f);
        }

        // ---- Menu overlay (Escape to toggle) ----
        if (gMenuOpen) {
            // Full-screen dark tint
            flatShader.setVec3("lineColor", 0.0f, 0.0f, 0.0f);
            drawRect(0, 0, (float)WIDTH, (float)HEIGHT);

            const float MX0 = MNU_X0, MY0 = MNU_Y0;
            const float MX1 = MX0 + MNU_W, MY1 = MY0 + MNU_H;

            // Panel background
            flatShader.setVec3("lineColor", 0.08f, 0.08f, 0.10f);
            drawRect(MX0, MY0, MX1, MY1);
            flatShader.setVec3("lineColor", 0.40f, 0.40f, 0.45f);
            glLineWidth(1.5f);
            draw2D({{MX0,MY0,0},{MX1,MY0,0},{MX1,MY0,0},{MX1,MY1,0},
                    {MX1,MY1,0},{MX0,MY1,0},{MX0,MY1,0},{MX0,MY0,0}}, GL_LINES);
            glLineWidth(1.0f);

            // Title
            flatShader.setVec3("lineColor", 0.90f, 0.80f, 0.20f);
            const char* title = "MENU";
            float tw = (float)stb_easy_font_width((char*)title) * 2.2f;
            drawText(MX0 + MNU_W*0.5f - tw*0.5f, MY0+14.f, title, 2.2f);
            flatShader.setVec3("lineColor", 0.30f, 0.30f, 0.32f);
            draw2D({{MX0+10,MY0+38.f,0},{MX1-10,MY0+38.f,0}}, GL_LINES);

            // Name fields
            for (int p = 0; p < 2; ++p) {
                float fy = MY0 + 44.f + p * 50.f;
                // Label
                flatShader.setVec3("lineColor", 0.55f, 0.55f, 0.60f);
                std::string lbl = "P" + std::to_string(p+1) + " name:";
                drawText(MX0+14.f, fy+4.f, lbl, 1.3f);
                // Input box
                Rect fr = mnuBtnField(p);
                bool editing = (gMenuEditPlayer == p);
                flatShader.setVec3("lineColor", editing ? 0.18f : 0.12f,
                                               editing ? 0.18f : 0.12f,
                                               editing ? 0.22f : 0.14f);
                drawRect(fr.x0, fr.y0, fr.x1, fr.y1);
                float brdC = editing ? 0.70f : 0.35f;
                flatShader.setVec3("lineColor", brdC, brdC, brdC);
                draw2D({{fr.x0,fr.y0,0},{fr.x1,fr.y0,0},{fr.x1,fr.y0,0},{fr.x1,fr.y1,0},
                        {fr.x1,fr.y1,0},{fr.x0,fr.y1,0},{fr.x0,fr.y1,0},{fr.x0,fr.y0,0}}, GL_LINES);
                // Text in field
                flatShader.setVec3("lineColor", 0.95f, 0.95f, 0.95f);
                const std::string& txt = editing ? gMenuEditBuf : gPlayerName[p];
                std::string display = txt + (editing ? "_" : "");
                drawText(fr.x0+6.f, fr.y0+4.f, display, 1.35f);
            }

            // Surrender buttons
            const char* surrenderLabel[2] = {"Surrender (P1 loses frame)", "Surrender (P2 loses frame)"};
            for (int p = 0; p < 2; ++p) {
                Rect br = mnuBtnSurrender(p);
                float mx = (float)gLastX, my = (float)gLastY;
                bool hov = hitRect(br, mx, my);
                flatShader.setVec3("lineColor", hov ? 0.28f : 0.16f, 0.04f, 0.04f);
                drawRect(br.x0, br.y0, br.x1, br.y1);
                flatShader.setVec3("lineColor", hov ? 0.95f : 0.70f, 0.15f, 0.10f);
                glLineWidth(1.3f);
                draw2D({{br.x0,br.y0,0},{br.x1,br.y0,0},{br.x1,br.y0,0},{br.x1,br.y1,0},
                        {br.x1,br.y1,0},{br.x0,br.y1,0},{br.x0,br.y1,0},{br.x0,br.y0,0}}, GL_LINES);
                glLineWidth(1.0f);
                flatShader.setVec3("lineColor", 0.95f, 0.90f, 0.85f);
                // Build label with actual player name
                std::string slbl = std::string(p==0 ? gPlayerName[0] : gPlayerName[1]) + " surrenders";
                float slw = (float)stb_easy_font_width((char*)slbl.c_str()) * 1.3f;
                drawText((br.x0+br.x1)*0.5f - slw*0.5f, br.y0+7.f, slbl, 1.3f);
            }

            // Restart button
            {
                Rect br = mnuBtnRestart();
                float mx = (float)gLastX, my = (float)gLastY;
                bool hov = hitRect(br, mx, my);
                flatShader.setVec3("lineColor", hov ? 0.14f : 0.08f, hov ? 0.22f : 0.14f, hov ? 0.14f : 0.08f);
                drawRect(br.x0, br.y0, br.x1, br.y1);
                float brdC2 = hov ? 0.55f : 0.35f;
                flatShader.setVec3("lineColor", 0.15f, brdC2, 0.20f);
                glLineWidth(1.3f);
                draw2D({{br.x0,br.y0,0},{br.x1,br.y0,0},{br.x1,br.y0,0},{br.x1,br.y1,0},
                        {br.x1,br.y1,0},{br.x0,br.y1,0},{br.x0,br.y1,0},{br.x0,br.y0,0}}, GL_LINES);
                glLineWidth(1.0f);
                flatShader.setVec3("lineColor", 0.85f, 0.95f, 0.85f);
                const char* rlbl = "Restart game (no frame awarded)";
                float rlw = (float)stb_easy_font_width((char*)rlbl) * 1.3f;
                drawText((br.x0+br.x1)*0.5f - rlw*0.5f, br.y0+7.f, rlbl, 1.3f);
            }

            // Volume slider
            {
                float sy    = MNU_Y0 + 336.f;
                float tx0   = MX0 + 90.f;
                float tx1   = MX1 - 50.f;
                float tw2   = tx1 - tx0;
                float hx    = tx0 + gMusicVolume * tw2;

                flatShader.setVec3("lineColor", 0.55f, 0.55f, 0.60f);
                drawText(MX0 + 16.f, sy + 3.f, "Music:", 1.3f);

                // Track background
                flatShader.setVec3("lineColor", 0.15f, 0.15f, 0.18f);
                drawRect(tx0, sy + 4.f, tx1, sy + 14.f);
                // Filled part
                flatShader.setVec3("lineColor", 0.20f, 0.55f, 0.85f);
                drawRect(tx0, sy + 4.f, hx, sy + 14.f);
                // Track border
                flatShader.setVec3("lineColor", 0.40f, 0.40f, 0.45f);
                draw2D({{tx0, sy+4.f,0},{tx1, sy+4.f,0},{tx1, sy+4.f,0},{tx1, sy+14.f,0},
                         {tx1,sy+14.f,0},{tx0,sy+14.f,0},{tx0,sy+14.f,0},{tx0, sy+4.f,0}}, GL_LINES);
                // Handle
                flatShader.setVec3("lineColor", 0.90f, 0.90f, 0.95f);
                drawRect(hx - 5.f, sy, hx + 5.f, sy + 18.f);

                // Mute button
                Rect muteBtn{tx1 + 8.f, sy, tx1 + 42.f, sy + 18.f};
                bool mutHov = hitRect(muteBtn, (float)gLastX, (float)gLastY);
                bool muted  = (gMusicVolume < 0.01f);
                flatShader.setVec3("lineColor", muted ? 0.55f : (mutHov ? 0.30f : 0.15f),
                                               muted ? 0.10f : (mutHov ? 0.10f : 0.10f),
                                               muted ? 0.10f : 0.10f);
                drawRect(muteBtn.x0, muteBtn.y0, muteBtn.x1, muteBtn.y1);
                flatShader.setVec3("lineColor", 0.90f, 0.90f, 0.90f);
                drawText(muteBtn.x0 + 4.f, muteBtn.y0 + 3.f, muted ? "Unmute" : "Mute", 1.1f);
            }

            // Help section
            float hy = MNU_Y0 + 360.f;
            flatShader.setVec3("lineColor", 0.30f, 0.30f, 0.32f);
            draw2D({{MX0+10, hy, 0},{MX1-10, hy, 0}}, GL_LINES);
            hy += 6.f;
            flatShader.setVec3("lineColor", 0.55f, 0.75f, 0.55f);
            const char* helpTitle = "CONTROLS";
            float htw = (float)stb_easy_font_width((char*)helpTitle) * 1.4f;
            drawText(MX0 + MNU_W*0.5f - htw*0.5f, hy, helpTitle, 1.4f);
            hy += 18.f;

            struct HelpLine { const char* key; const char* desc; };
            HelpLine helpLines[] = {
                { "Aim:",    "Left/Right arrows  or  mouse left-click drag" },
                { "Power:",  "Up/Down arrows  or  mouse pull-back (left-click)" },
                { "Shoot:",  "Space  or  release left-click" },
                { "Spin U:", "Q (left) / E (right)  -- side effect" },
                { "Spin V:", "W (top) / S (back)    -- gauge ball icon bottom-right" },
                { "View:",   "Tab  or  camera button  (Shot / Top)" },
                { "Menu:",   "ESC  or  MENU button top-left" },
            };
            for (auto& hl : helpLines) {
                flatShader.setVec3("lineColor", 0.70f, 0.70f, 0.30f);
                drawText(MX0 + 16.f, hy, hl.key, 1.2f);
                flatShader.setVec3("lineColor", 0.75f, 0.75f, 0.78f);
                drawText(MX0 + 90.f, hy, hl.desc, 1.2f);
                hy += 14.f;
            }

            // Hint at bottom
            flatShader.setVec3("lineColor", 0.35f, 0.35f, 0.38f);
            const char* hint = "ESC to close  |  Click field to edit name  |  Enter to confirm";
            float hw = (float)stb_easy_font_width((char*)hint) * 1.1f;
            drawText(MX0 + MNU_W*0.5f - hw*0.5f, MY1-18.f, hint, 1.1f);
        }

        glLineWidth(1.0f);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    if (audioOk) {
        ma_sound_uninit(&bgMusic);
        ma_engine_uninit(&audioEngine);
    }
    glfwTerminate();
    return 0;
}
