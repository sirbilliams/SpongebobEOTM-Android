// sb2.exe's persistent mini-game object (0x4354d0), chapter 2 portion.
#pragma once
namespace sbgame {
class Game;
struct SceneObject;
class MiniGame {
public:
    explicit MiniGame(Game* game);
    void slotsInit();
    void slotsSpin();
    void maze(int direction);
    SceneObject* pickMazeDoor(int x, int y);
    void sceneEntered();
    void update(float dtMs);
    bool flag() const { return armed_ && !failed_; }   // lane f: the condition term "xxxx" (shorts +0x14 != 0, +0x12 == 0)
private:
    void pickSymbols();
    void pickClue();
    void showSymbol(int reel);
    void sound(const char* name, int pan = 0);
    void stopSound();
    Game* game_;
    short target_[3], step_, spins_;
    bool armed_, failed_, running_[3], resultPending_;
    float elapsed_;
};
}
