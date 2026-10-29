#include "Phone.h"
#include "Game.h"
#include "DialogueUI.h"
#include <stdio.h>
#include <algorithm>

namespace sbgame {
Phone::Phone(Game* g) : game_(g), index_(-1), text_(), pending_(), block_(0)
{
    for (int i = 0; i < 7; ++i) digit_[i] = 0;
    if (game_->cond("Ssphone1")) index_ = 0;     // 0x4354d0: the coin was already paid (script seen)
}
Phone::~Phone() { delete block_; }

void Phone::reset() // 0x436610
{
    index_ = 0; text_.clear();
}
void Phone::key(int n) // 0x436640
{
    if (index_ == -1) return;
    if (n == 10) { index_ = 0; text_.clear(); game_->log("mini phone cleared\n"); return; }
    if (index_ < 0 || index_ >= 7) return;       // port: the exe would write past the 7 digit slots
    digit_[index_] = (short)n;
    char b[16]; sprintf(b, "%d", n);
    if (index_ == 3) text_ += "-";
    text_ += b;
    ++index_;
    game_->log("mini phone digit %d display '%s'\n", n, text_.c_str());
    if (index_ != 7) return;
    static const short number[7] = { 5, 5, 5, 4, 4, 4, 4 };
    bool ok = true;
    for (int i = 0; i < 7; ++i) if (digit_[i] != number[i]) ok = false;
    // the right number rings once (sphone3, until it has been seen); anything else is busy
    setPending(ok && !game_->cond("Ssphone3") ? "sphone3" : "sphone2");
}
void Phone::setPending(const char* script) { pending_ = script; game_->log("mini phone dialled -> %s\n", script); }

void Phone::update(float) // 0x4355a0
{
    GameScene* s = game_->scene();
    // port: the exe stores the script name in game+0x1a6 and I found no reader; start it once no script runs
    if (!pending_.empty() && !game_->runner()) {
        std::string p = pending_; pending_.clear();
        game_->switchTo(p.c_str());
    }
    bool want = s && s->name() == "c4l2s2" && index_ != -1 && !text_.empty();
    if (!want) { if (block_) { delete block_; block_ = 0; shown_.clear(); } return; }
    if (block_ && shown_ == text_) return;
    if (!block_) block_ = new TextBlock;
    int w = (int)game_->font()->get_text_length(text_);
    block_->show(game_, (640 - w) / 2, 200, 10100, false, text_, 640);   // centred at y=200
    shown_ = text_;
}
}
