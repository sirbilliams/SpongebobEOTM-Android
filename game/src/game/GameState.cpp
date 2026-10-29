#include "GameState.h"
#include <ctype.h>
#include <stdio.h>
#include <algorithm>

namespace sbgame {

GameState::GameState() : debug(false), miniFlag(false), sceneId(0), chapter(1) {}

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
bool GameState::flagSet(const char* term) { return flags_.count(low(term)) != 0; }

static std::string dkey(unsigned scene, const std::string& g)
{
    char b[16];
    sprintf(b, "%08x:", scene);
    return std::string(b) + g;
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
    e.key = currentKey; e.a = a; e.b = b;
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
