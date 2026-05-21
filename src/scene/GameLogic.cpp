#include "GameLogic.h"
#include "Physics.h"
#include <algorithm>
#include <cstdio>

// Spot positions for colored balls — must match initial positions in main.cpp.
// Y = Physics::RADIUS so the ball sits on the table surface.
const glm::vec3 GameLogic::SPOTS[7] = {
    { 0.0f,            Physics::RADIUS,  0.0f   },  // 0 = white, unused
    { 0.873f,          Physics::RADIUS, -0.242f },  // 1 = yellow
    { 0.873f,          Physics::RADIUS,  0.242f },  // 2 = green
    { 0.873f,          Physics::RADIUS,  0.0f   },  // 3 = brown
    { 0.0f,            Physics::RADIUS,  0.0f   },  // 4 = blue
    {-0.896f,          Physics::RADIUS,  0.0f   },  // 5 = pink
    {-1.220f,          Physics::RADIUS,  0.0f   },  // 6 = black
};

GameLogic::GameLogic() {
    buildStatus();
}

// Minimum penalty = max(4, value of ON ball).
int GameLogic::onBallMinPenalty() const {
    if (phase == GamePhase::RED_PHASE)
        return 4;
    if (phase == GamePhase::CLEARANCE)
        return std::max(4, VALUE[COLOR_ORDER[clearanceIdx]]);
    if (phase == GamePhase::BLACK_SHOOTOUT)
        return 7;
    return 7;
}

void GameLogic::buildStatus() {
    char buf[128];
    const char* pname = currentPlayer == 0 ? "P1" : "P2";

    if (phase == GamePhase::GAME_OVER) {
        snprintf(buf, sizeof(buf), "GAME OVER  P1:%d  P2:%d  (%s wins!)",
                 scores[0], scores[1],
                 scores[0] > scores[1] ? "P1" : "P2");
    } else if (phase == GamePhase::BLACK_SHOOTOUT) {
        snprintf(buf, sizeof(buf), "%s: pot the black to win!", pname);
    } else if (phase == GamePhase::RED_PHASE) {
        if (expectingRed)
            snprintf(buf, sizeof(buf), "%s: pot a red", pname);
        else
            snprintf(buf, sizeof(buf), "%s: pot a colour", pname);
    } else {
        snprintf(buf, sizeof(buf), "%s: pot the %s (%dpts)",
                 pname, COLOR_NAME[COLOR_ORDER[clearanceIdx]],
                 VALUE[COLOR_ORDER[clearanceIdx]]);
    }
    statusMsg = buf;
}

ShotResult GameLogic::processShot(int firstContact,
                                   const std::vector<int>& potted,
                                   int redsOnTable) {
    ShotResult res;

    bool cuePotted = std::find(potted.begin(), potted.end(), 0) != potted.end();
    res.cueBallFoul = cuePotted;

    // Foul detection
    bool foul = false;
    if (firstContact == -1 || cuePotted) {
        foul = true;
    } else if (phase == GamePhase::RED_PHASE) {
        if (expectingRed  && !isRed  (firstContact)) foul = true;
        if (!expectingRed && isRed   (firstContact)) foul = true;
    } else if (phase == GamePhase::CLEARANCE) {
        int onBall = COLOR_ORDER[clearanceIdx];
        if (firstContact != onBall) foul = true;
        // Potting any ball that isn't "on" is also a foul
        if (!foul) {
            for (int i : potted)
                if (isColor(i) && i != onBall) { foul = true; break; }
        }
    } else if (phase == GamePhase::BLACK_SHOOTOUT) {
        if (firstContact != 6) foul = true;  // must hit the black
    }

    // BLACK_SHOOTOUT foul: opponent wins immediately
    if (foul && phase == GamePhase::BLACK_SHOOTOUT) {
        res.foul        = true;
        res.foulPenalty = 7;
        res.switchPlayer = true;
        currentPlayer   = 1 - currentPlayer;   // winner is the other player
        currentBreak    = 0;
        phase           = GamePhase::GAME_OVER;
        buildStatus();
        return res;
    }

    if (foul) {
        res.foul = true;
        // Penalty = max(4, value of "on" ball, value of ball first contacted, value of any pocketed ball)
        int penalty = onBallMinPenalty();
        if (firstContact >= 1 && firstContact <= 6)
            penalty = std::max(penalty, VALUE[firstContact]);
        for (int i : potted)
            if (i >= 1 && i <= 6) penalty = std::max(penalty, VALUE[i]);
        res.foulPenalty  = penalty;
        res.switchPlayer = true;
        // Colors pocketed during a foul go back on their spots
        for (int i : potted)
            if (isColor(i)) res.respawn.push_back(i);
        scores[1 - currentPlayer] += res.foulPenalty;
        currentPlayer = 1 - currentPlayer;
        currentBreak  = 0;
        if (phase == GamePhase::RED_PHASE)
            expectingRed = true;

        char buf[80];
        const char* pname = currentPlayer == 0 ? "P1" : "P2";
        snprintf(buf, sizeof(buf), "FOUL +%d -> %s plays", res.foulPenalty, pname);
        statusMsg = buf;
        return res;
    }

    // Valid shot
    if (phase == GamePhase::RED_PHASE) {
        if (expectingRed) {
            int redsIn = 0;
            for (int i : potted) {
                if (isRed(i))   { redsIn++; }
                if (isColor(i)) { res.respawn.push_back(i); }  // accidental color → back
            }
            if (redsIn > 0) {
                res.points = redsIn;
                scores[currentPlayer] += redsIn;
                currentBreak += redsIn;
                expectingRed    = false;
                res.switchPlayer = false;
            } else {
                res.switchPlayer = true;
                currentBreak     = 0;
                currentPlayer    = 1 - currentPlayer;
            }
        } else {
            // Expecting a color
            int colorIn = -1;
            for (int i : potted)
                if (isColor(i) && colorIn == -1) colorIn = i;

            if (colorIn != -1) {
                res.points = VALUE[colorIn];
                scores[currentPlayer] += VALUE[colorIn];
                currentBreak += VALUE[colorIn];
                res.respawn.push_back(colorIn);  // color goes back on spot
                res.switchPlayer = false;

                if (redsOnTable > 0) {
                    expectingRed = true;
                } else {
                    // All reds gone → transition to clearance
                    phase        = GamePhase::CLEARANCE;
                    clearanceIdx = 0;
                }
            } else {
                res.switchPlayer = true;
                currentBreak     = 0;
                expectingRed     = true;
                currentPlayer    = 1 - currentPlayer;
            }
        }
    } else if (phase == GamePhase::CLEARANCE) {
        int onBall    = COLOR_ORDER[clearanceIdx];
        bool onPotted = std::find(potted.begin(), potted.end(), onBall) != potted.end();

        if (onPotted) {
            res.points = VALUE[onBall];
            scores[currentPlayer] += VALUE[onBall];
            currentBreak += VALUE[onBall];
            clearanceIdx++;
            res.switchPlayer = false;
            if (clearanceIdx >= 6) {
                if (scores[0] == scores[1]) {
                    // Tie: respot black, ball-in-hand, shootout
                    phase = GamePhase::BLACK_SHOOTOUT;
                    res.respawn.push_back(6);   // put black back on its spot
                    res.ballInHand = true;       // current player plays from D
                } else {
                    phase = GamePhase::GAME_OVER;
                }
            }
        } else {
            res.switchPlayer = true;
            currentBreak     = 0;
            currentPlayer    = 1 - currentPlayer;
        }
    } else if (phase == GamePhase::BLACK_SHOOTOUT) {
        bool blackPotted = std::find(potted.begin(), potted.end(), 6) != potted.end();

        if (blackPotted) {
            phase = GamePhase::GAME_OVER;
            res.switchPlayer = false;
        } else {
            // Miss: switch player, leave balls exactly where they are (no respot, no ball-in-hand)
            res.switchPlayer = true;
            currentBreak     = 0;
            currentPlayer    = 1 - currentPlayer;
        }
    }

    buildStatus();
    return res;
}
