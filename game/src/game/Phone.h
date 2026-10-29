// Chapter 4 phone-booth keypad (c4l2s2): the phone part of sb2.exe's persistent mini-game object
// (ctor 0x4354d0 fields +0x18..+0x28, key handler 0x436640, reset 0x436610, text 0x4355a0).
#pragma once
#include <string>
namespace sbgame {
class Game;
class TextBlock;
class Phone {
public:
    explicit Phone(Game* game);
    ~Phone();
    void reset();             // 0x436610 ("phone 0"): ready for the first digit, empty display
    void key(int n);          // 0x436640 ("phone 1..9", 10 = hang up / clear, 11)
    void update(float dtMs);  // 0x4355a0 (display text in c4l2s2) + starting the number's script
    const std::string& display() const { return text_; }
    int index() const { return index_; }
private:
    void setPending(const char* script);
    Game* game_;
    short index_;             // +0x26: digits dialled so far, -1 until the coin is paid
    short digit_[7];          // +0x18..+0x24
    std::string text_;        // +0x28: "555-4444" as typed
    std::string pending_;     // 0x1a6 in the game object: the script the number selects
    TextBlock* block_;
    std::string shown_;
};
}
