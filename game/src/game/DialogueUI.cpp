#include "DialogueUI.h"
#include "Game.h"
#include "SceneScriptHost.h"
#include "OMediaEventManager.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

namespace sbgame {

using sbscript::DialogueGroup;
using sbscript::DialogueLine;

// ------------------------------------------------------------------------------------------- TextBlock
void TextBlock::clear()
{
    for (size_t i = 0; i < rows_.size(); ++i) delete rows_[i];
    rows_.clear();
    delete box_;
    box_ = 0;
}

int TextBlock::show(Game* g, int x, int y, int z, bool highlighted, const std::string& text, int maxWidth)
{
    clear();
    OMediaTextFont* font = g->font();
    // greedy word wrap
    std::vector<std::string> rows;
    std::string row, word;
    for (size_t i = 0; i <= text.size(); ++i) {
        char c = i < text.size() ? text[i] : ' ';
        if (c == ' ') {
            if (word.empty()) continue;
            std::string t = row.empty() ? word : row + " " + word;
            if (!row.empty() && font->get_text_length(t) > maxWidth) { rows.push_back(row); row = word; }
            else row = t;
            word.clear();
        } else word += c;
    }
    if (!row.empty()) rows.push_back(row);
    if (rows.empty()) rows.push_back("");
    int fh = font->get_font_height();
    int width = 0;
    for (size_t i = 0; i < rows.size(); ++i) width = std::max(width, (int)font->get_text_length(rows[i]));
    int h = (int)rows.size() * fh;
    if (highlighted) {
        box_ = new OMediaBoxElement;
        box_->set_size((short)(width + 8), (short)(h + 4));
        box_->set_filled(true);
        box_->set_color(OMediaRGBColor(0x1800, 0x2800, 0x7000));
        box_->link(g->world());
        box_->place((float)(x - 4), (float)(y - 2), (float)(z - 1));
    }
    for (size_t i = 0; i < rows.size(); ++i) {
        OMediaCCaption* c = new OMediaCCaption;
        c->set_font(font);
        c->set_string(rows[i]);
        c->link(g->world());
        c->place((float)x, (float)(y + (int)i * fh), (float)z);
        rows_.push_back(c);
    }
    bottom_ = y + h;
    return h;
}

// ------------------------------------------------------------------------------------------ DialogueUI
DialogueUI::DialogueUI(Game* g)
    : game_(g), sceneId_(0), active_(false), choice_(false), exiting_(false), zOpen_(false), branch_(0), cur_(-1), voice_(0),
      timerMs_(0), dirty_(false)
{
}

DialogueUI::~DialogueUI()
{
    stopVoice();
    for (size_t i = 0; i < blocks_.size(); ++i) delete blocks_[i];
}

bool DialogueUI::load(unsigned sceneId)
{
    sceneId_ = sceneId;
    std::string err;
    groups_.clear();
    if (!table_.load(*game_->host(), sceneId, &game_->state(), &err)) return false;
    groups_ = table_.groups();
    return true;
}

bool DialogueUI::isAnim(const std::string& a)
{
    return a.size() >= 4 && toupper((unsigned char)a[0]) == 'A' && toupper((unsigned char)a[1]) == 'N' &&
           toupper((unsigned char)a[2]) == 'I' && toupper((unsigned char)a[3]) == 'M';
}

bool DialogueUI::eligible(const DialogueGroup& g, bool flagZ)
{
    if (g.stateB != 0) return false;
    if (g.key.size() < 2 || g.lines.empty()) return false;
    if (toupper((unsigned char)g.key[0]) != toupper((unsigned char)branch_)) return false;
    if (toupper((unsigned char)g.key[1]) == 'Z') return flagZ;
    const std::string& action = g.lines[0].action;
    if (isAnim(action)) return true;
    return game_->cond(action.c_str());
}

int DialogueUI::eligibleCount()
{
    int n = 0;
    for (size_t i = 0; i < groups_.size(); ++i) if (eligible(groups_[i], false)) ++n;
    return n;
}

DialogueGroup* DialogueUI::groupByKey(const std::string& key)
{
    for (size_t i = 0; i < groups_.size(); ++i) if (groups_[i].key == key) return &groups_[i];
    return 0;
}

// 0x411920
bool DialogueUI::speak(const char* key)
{
    if (groups_.empty()) return false;
    size_t len = strlen(key);
    const char* k = len == 6 ? key + 3 : key;      // 0x411820: a six letter name carries a scene prefix
    branch_ = k[0];
    int cnt = 0, first = -1;
    for (size_t i = 0; i < groups_.size(); ++i) {
        DialogueGroup& g = groups_[i];
        if (g.stateB != 0 && g.current >= 0) g.stateB = 0;       // a finished node that still has a line reopens
        if (eligible(g, false)) { ++cnt; if (first < 0) first = (int)i; }
    }
    cur_ = -1;
    zOpen_ = false;
    if (first < 0) return false;
    if (cnt == 1 && toupper((unsigned char)groups_[first].key[1]) == 'X') return false;   // only "leave": nothing to say
    game_->log("dialogue %s: %d choice(s)\n", key, cnt);
    active_ = true;
    choice_ = true;
    exiting_ = false;
    cur_ = first;
    dirty_ = true;
    DialogueGroup& g0 = groups_[first];
    // a node with a single line that is the only one of the branch speaks at once
    if (cnt == 1) { confirm(); return true; }
    // a "Z" node of the branch is the opening statement, spoken before the list is offered
    for (size_t i = 0; i < groups_.size(); ++i) {
        const DialogueGroup& g = groups_[i];
        if (g.key.size() > 1 && toupper((unsigned char)g.key[0]) == toupper((unsigned char)branch_) &&
            toupper((unsigned char)g.key[1]) == 'Z' && g.stateB == 0) {
            cur_ = (int)i;
            zOpen_ = true;              // +0x2c: speak it once, then back to the list
            confirm();
            return true;
        }
    }
    (void)g0;
    return true;
}

// 0x412bb0
void DialogueUI::confirm()
{
    if (!choice_ || cur_ < 0) return;
    choice_ = false;
    DialogueGroup& g = groups_[cur_];
    if (g.current < 0) g.current = 0;
    if (g.key.size() > 1 && toupper((unsigned char)g.key[1]) == 'X') { exiting_ = true; zOpen_ = true; }   // +0x2d, +0x2c
    playLine();
}

bool DialogueUI::startVoice(const std::string& key)
{
    stopVoice();
    OMediaDataBase* bank = game_->sceneSoundDb();
    if (!bank) return false;
    unsigned id = sceneId_;
    char ch[4] = { key[0], key.size() > 1 ? key[1] : ' ', key.size() > 2 ? key[2] : ' ', 0 };
    // 0x412d00 tries the three key letters in seven upper / lower case combinations (not all upper case)
    static const int combos[7][3] = { {0,0,0},{1,0,1},{1,1,0},{0,1,1},{0,1,0},{1,0,0},{0,0,1} };
    for (int c = 0; c < 7; ++c) {
        char n[16];
        sprintf(n, "%d%d%d%c%c%c", (int)(id & 0xf), (int)((id >> 8) & 0xf), (int)((id >> 16) & 0xf),
                combos[c][0] ? toupper((unsigned char)ch[0]) : tolower((unsigned char)ch[0]),
                combos[c][1] ? toupper((unsigned char)ch[1]) : tolower((unsigned char)ch[1]),
                combos[c][2] ? toupper((unsigned char)ch[2]) : tolower((unsigned char)ch[2]));
        if (!bank->object_exists(OMediaSound::db_type, std::string(n))) continue;      // the bank's vt+0x18
        OMediaSound* s = (OMediaSound*)bank->get_object(OMediaSound::db_type, std::string(n));
        if (s) { voice_ = s; game_->log("voice %s (%.2f s)\n", n, s->get_sound_secs()); return true; }
    }
    return false;
}

void DialogueUI::stopVoice()
{
    if (voice_) game_->stopChannel(4);
    voice_ = 0;
}

// 0x412d00
void DialogueUI::playLine()
{
    DialogueGroup& g = groups_[cur_];
    if (g.current < 0 || g.current >= (int)g.lines.size()) { stop(); return; }
    const DialogueLine& l = g.lines[g.current];
    curText_ = l.text;
    curSpeaker_ = l.speaker;
    timerMs_ = 0;
    if (startVoice(l.key)) {
        game_->playOnChannel(4, voice_, 0);
    } else {
        float secs = (float)curText_.size() / 14.0f;       // the line's length / 14 s, at least 1 s
        if (secs < 1.0f) secs = 1.0f;
        timerMs_ = secs * 1000.0f;
    }
    game_->log("line %s [%s] %s\n", l.key.c_str(), l.speaker.c_str(), l.text.c_str());
    // the speaker plays its "talk" animation: scene vt+0xe4(name) -> char vt+0xc0("talk")
    SceneObject* a = game_->scene() ? game_->scene()->find(l.speaker.c_str(), 1 << SceneObject::Char) : 0;
    game_->host()->speakerStarted(a, l.speaker.c_str());
    dirty_ = true;
}

void DialogueUI::lineDone()
{
    stopVoice();
    advance();
}

// 0x413c40
void DialogueUI::advance()
{
    if (!active_ || choice_ || cur_ < 0) return;
    DialogueGroup& g = groups_[cur_];
    if (g.current < 0) { stop(); return; }
    const DialogueLine& l = g.lines[g.current];
    bool last = g.current + 1 >= (int)g.lines.size();
    // port: group.lines is the vector the exe chains through +0x158 / +0x15c
    if (last) {
        // run the node's action: a command line, not an "ANIM" marker and not the "0" placeholder
        bool finished = false;
        if (!isAnim(l.action) && l.action.size() > 3) {
            std::string a = l.action;
            game_->switchTo(a.c_str());
        }
        g.stateA = 1;
        int n2 = l.flag2;
        if (n2 == 1 || zOpen_) { g.current = 0; zOpen_ = false; }           // restarts from the first line
        else if (n2 == 2) {                                                 // back to the last "n1" line
            do { g.current--; } while (g.current > 0 && g.lines[g.current].flag1 == 0);
            if (g.current < 0) g.current = 0;
        } else finished = true;
        if (!zOpen_) g.stateB = 1;
        bool wasExit = exiting_;
        if (!exiting_ && !zOpen_) storeProgress(g);
        if (finished) g.current = -1;
        if (l.flag3 == 0 && !wasExit) {
            choice_ = true;                                                 // back to the choice list
            exiting_ = false;
            dirty_ = true;
            // 0x4117a0: exactly one choice left: take it
            int cnt = 0, only = -1;
            for (size_t i = 0; i < groups_.size(); ++i) if (eligible(groups_[i], false)) { ++cnt; only = (int)i; }
            if (cnt == 1) { cur_ = only; confirm(); }
            else if (cnt == 0) stop();
            else if (!eligible(groups_[cur_], false)) selectNext(1);
        } else {
            stop();
        }
    } else {
        g.current++;
        if (g.lines[g.current].flag1 != 0) {                                // a decision point: offer the list
            choice_ = true;
            storeProgress(g);
            dirty_ = true;
            return;
        }
        playLine();
    }
}

void DialogueUI::storeProgress(DialogueGroup& g)
{
    std::string ck = g.current >= 0 && g.current < (int)g.lines.size() ? g.lines[g.current].key : std::string();
    game_->state().storeDialogue(sceneId_, g.key, ck, g.stateA, g.stateB);
}

// 0x411ba0
void DialogueUI::stop()
{
    bool wasActive = active_;
    stopVoice();
    active_ = false; choice_ = false; exiting_ = false; zOpen_ = false; cur_ = -1;
    curText_.clear(); curSpeaker_.clear();
    dirty_ = true;
    render();
    if (wasActive) {
        game_->log("dialogue ended\n");
        game_->host()->talkFinished();
    }
}

void DialogueUI::selectNext(int step)
{
    if (!choice_ || groups_.empty()) return;
    int n = (int)groups_.size();
    for (int tries = 0; tries < n; ++tries) {
        cur_ = (cur_ + step + n) % n;
        if (eligible(groups_[cur_], false)) break;
    }
    dirty_ = true;
}

void DialogueUI::update(float dt)       // 0x412c00
{
    if (active_ && !choice_) {
        if (voice_) {
            if (!game_->channelBusy(4)) lineDone();                 // voice finished: scene notify (0x439890) advances
        } else if (timerMs_ > 0) {
            timerMs_ -= dt;                                          // the talk animation ends; the line waits for a click
            if (timerMs_ < 0) timerMs_ = 0;
        }
    }
    if (dirty_) render();
}

void DialogueUI::render()               // 0x4128d0
{
    dirty_ = false;
    for (size_t i = 0; i < blocks_.size(); ++i) delete blocks_[i];
    blocks_.clear();
    if (!active_) return;
    if (!choice_) {
        // a single line at the bottom (y = 450)
        if (curText_.empty()) return;
        TextBlock* b = new TextBlock;
        b->show(game_, 5, 450, 10100, false, curText_, 630);
        blocks_.push_back(b);
        return;
    }
    int y = 450;
    for (size_t i = 0; i < groups_.size(); ++i) {
        if (!eligible(groups_[i], false)) continue;
        const std::string& t = groups_[i].lines[groups_[i].current >= 0 ? groups_[i].current : 0].text;
        TextBlock* b = new TextBlock;
        int h = b->show(game_, 5, y, 10100, (int)i == cur_, t, 630);
        blocks_.push_back(b);
        y -= h + 12;
    }
}

bool DialogueUI::onMove(int x, int y)   // 0x412af0: true if the pointer is on a choice
{
    if (!choice_) return false;
    int yy = 450;
    for (size_t i = 0; i < groups_.size(); ++i) {
        if (!eligible(groups_[i], false)) continue;
        const std::string& t = groups_[i].lines[groups_[i].current >= 0 ? groups_[i].current : 0].text;
        int w = game_->font()->get_text_length(t);
        int rows = (w + 629) / 630; if (rows < 1) rows = 1;
        int h = rows * game_->font()->get_font_height();
        if (x > 1 && x < std::min(w, 630) + 9 && y >= yy - 2 && y < yy + h + 2) {
            if (cur_ != (int)i) { cur_ = (int)i; dirty_ = true; }
            return true;
        }
        yy -= h + 12;
    }
    return false;
}

void DialogueUI::onClick(int x, int y)  // 0x4129e0
{
    if (!active_) return;
    sbscript::ScriptRunner* r = game_->runner();
    if (!choice_) {
        if (!r || r->waitingTalk()) { stopVoice(); advance(); }        // click skips the line
        return;
    }
    if (onMove(x, y) && cur_ >= 0 && eligible(groups_[cur_], false)) confirm();
}

void DialogueUI::onKey(int key)         // 0x413e90
{
    if (!active_) return;
    if (key == omck_Escape) {
        if (choice_) stop();
        else { stopVoice(); advance(); }
    } else if (key == omck_Enter || key == ' ') {
        if (choice_) confirm(); else { stopVoice(); advance(); }
    } else if (key == omck_Up || key == omck_PageUp) selectNext(1);
    else if (key == omck_Down || key == omck_PageDown) selectNext(-1);
}

}  // namespace sbgame
