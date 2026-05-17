#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

enum class GamePhase { RED_PHASE, CLEARANCE, BLACK_SHOOTOUT, GAME_OVER };

struct ShotResult {
    int  points       = 0;
    bool foul         = false;
    int  foulPenalty  = 0;
    bool switchPlayer = true;
    bool cueBallFoul  = false;
    bool ballInHand   = false;  // grant ball-in-hand to incoming player
    std::vector<int> respawn;   // color ball indices to put back on their spot
};

class GameLogic {
public:
    // Spot positions for balls 1-6 (yellow→black). Index 0 unused.
    static const glm::vec3 SPOTS[7];
    static constexpr int VALUE[7]       = {0, 2, 3, 4, 5, 6, 7};
    static constexpr int COLOR_ORDER[6] = {1, 2, 3, 4, 5, 6};  // clearance sequence
    static constexpr const char* COLOR_NAME[7] = {
        "", "yellow", "green", "brown", "blue", "pink", "black"
    };

    int       scores[2]     = {0, 0};
    int       currentPlayer = 0;
    int       currentBreak  = 0;   // points scored this uninterrupted turn
    GamePhase phase          = GamePhase::RED_PHASE;
    bool      expectingRed   = true;
    int       clearanceIdx   = 0;
    std::string statusMsg;

    GameLogic();

    // Called once after all balls stop and sinking animations finish.
    // firstContact : index of first ball the cue ball touched (-1 = none)
    // potted       : indices of all balls pocketed this shot
    // redsOnTable  : reds still active after the shot
    ShotResult processShot(int firstContact,
                           const std::vector<int>& potted,
                           int redsOnTable);

    void buildStatus();  // public so main can call after manual phase override

private:
    static bool isRed  (int i) { return i >= 7; }
    static bool isColor(int i) { return i >= 1 && i <= 6; }
    int  onBallMinPenalty() const;
};
