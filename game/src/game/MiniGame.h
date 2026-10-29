// sb2.exe's persistent mini-game object (0x4354d0), chapter 2 portion.
#pragma once
#include "Phone.h" // chapter 4 phone keypad
namespace sbgame {
class Game;
class TextBlock;
struct SceneObject;
class MiniGame {
public:
    explicit MiniGame(Game* game);
    ~MiniGame();
    void darts(int op);
    void updateDarts(float dtMs);
    static int dartsScore(int x, int y);
    void slotsInit();
    void slotsSpin();
    void maze(int direction);
    SceneObject* pickMazeDoor(int x, int y);
    void sceneEntered();
    void update(float dtMs);
    Phone& phone() { return phone_; } // chapter 4 phone (Phone.cpp)
private:
    void pickSymbols();
    void pickClue();
    void showSymbol(int reel);
    void sound(const char* name, int pan = 0);
    void stopSound();
    void dartsText();
    Game* game_;
    short throws_, dartX_, dartY_, score_, hit_, highScore_;
    bool dartPending_, bullSound_;
    float hitMs_;
    TextBlock* dartText_[5];
    short target_[3], step_, spins_;
    bool armed_, failed_, running_[3], resultPending_;
    float elapsed_;
    Phone phone_; // chapter 4 phone (Phone.cpp)
};
}
