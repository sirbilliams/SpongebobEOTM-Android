#include "GameState.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

namespace sbgame {

GameState::GameState() : hasPlayer(false), playerX(0), playerY(0), playerZ(0), playerFacing(0), resumePos(false), lastScene(0),
                         debug(false), miniFlag(false), sceneId(0), chapter(1), hasPuzzle_(false), high_(0)
{
    memset(puzzle_, 0, sizeof puzzle_);
}

// 0x4163b0: every list emptied (and 0x41e160 the item bar); a save is loaded into a clean object.
void GameState::clearProgress()
{
    seen_.clear(); flags_.clear(); items_.clear(); blocked_.clear(); dialogue_.clear(); textures_.clear();
    scen_.clear(); chars_.clear(); posi_.clear(); bar_.clear();
    hasPuzzle_ = false; high_ = 0; memset(puzzle_, 0, sizeof puzzle_);
    hasPlayer = false; playerX = playerY = playerZ = 0; playerFacing = 0; resumePos = false; lastScene = 0;
}

// 0x416ad0: the id of a flag term is its characters 1, 3 and 5 read as decimal digits (atoi of "%c"): (c5*256 + c3)*256 + c1.
unsigned GameState::flagId(const char* term)
{
    char c[3] = { 0, 0, 0 };
    if (!term || strlen(term) < 6) return 0xffffffffu;
    int v[3];
    for (int i = 0; i < 3; ++i) { c[0] = term[1 + 2 * i]; v[i] = atoi(c); }
    return (unsigned)(((v[2] * 0x100 + v[1]) * 0x100) + v[0]);
}

std::string GameState::low(const std::string& s)
{
    std::string r = s;
    for (size_t i = 0; i < r.size(); ++i) r[i] = (char)tolower((unsigned char)r[i]);
    return r;
}

// The terms of the condition language carry their prefix letter (S..., I...), and the script / item names the
// scripts use start with the same letter ('sload1', 'ikc1'), so a term is looked up under its own text.
bool GameState::scriptSeen(const char* term) { return seen_.count(low(term)) != 0; }
bool GameState::hasItem(const char* term)
{
    std::string t = low(term);
    return std::find(items_.begin(), items_.end(), t) != items_.end();
}
bool GameState::flagSet(const char* term)
{
    if (flags_.count(low(term)) != 0) return true;
    unsigned id = flagId(term);
    return id != 0xffffffffu && scen_.count(id) != 0;       // 0x416b70
}

static std::string dkey(unsigned scene, const std::string& g)
{
    char b[16];
    sprintf(b, "%08x:", scene);
    std::string k = g;
    for (size_t i = 0; i < k.size(); ++i) k[i] = (char)tolower((unsigned char)k[i]);
    return std::string(b) + k;
}

bool GameState::dialogueStarted(unsigned sceneId_, const char* groupKey)
{
    std::map<std::string, DEntry>::const_iterator it = dialogue_.find(dkey(sceneId_, groupKey));
    return it != dialogue_.end() && it->second.a != 0;
}

bool GameState::lookup(unsigned scene, const std::string& groupKey, std::string* currentKey, short* a, short* b)
{
    std::map<std::string, DEntry>::const_iterator it = dialogue_.find(dkey(scene, groupKey));
    if (it == dialogue_.end()) return false;
    if (currentKey) *currentKey = it->second.key;
    if (a) *a = it->second.a;
    if (b) *b = it->second.b;
    return true;
}

void GameState::storeDialogue(unsigned scene, const std::string& groupKey, const std::string& currentKey, short a, short b)
{
    DEntry e;
    e.key = currentKey; e.a = a; e.b = b; e.group = groupKey;
    dialogue_[dkey(scene, groupKey)] = e;
}

void GameState::markScriptSeen(const char* name) { seen_.insert(low(name)); }
void GameState::undoScript(const char* name) { seen_.erase(low(name)); flags_.erase(low(name)); }

void GameState::giveItem(const char* name)
{
    std::string n = low(name);
    if (std::find(items_.begin(), items_.end(), n) == items_.end()) items_.push_back(n);
}
void GameState::removeItem(const char* name)
{
    std::string n = low(name);
    items_.erase(std::remove(items_.begin(), items_.end(), n), items_.end());
}
void GameState::setFlag(const char* name, bool on)
{
    if (on) flags_.insert(low(name)); else flags_.erase(low(name));
}

static unsigned long long bkey(unsigned scene, int x, int y)
{
    return ((unsigned long long)scene << 32) | ((unsigned)(x & 0xffff) << 16) | (unsigned)(y & 0xffff);
}
void GameState::setBlocked(unsigned scene, int x, int y, bool bl)
{
    if (bl) blocked_.insert(bkey(scene, x, y)); else blocked_.erase(bkey(scene, x, y));
}
bool GameState::isBlocked(unsigned scene, int x, int y) const { return blocked_.count(bkey(scene, x, y)) != 0; }

}  // namespace sbgame

namespace sbgame {
void GameState::addChar(const char* name)       // 0x416c70: once per name
{
    for (size_t i = 0; i < chars_.size(); ++i) if (chars_[i] == name) return;
    chars_.push_back(name);
}
}
