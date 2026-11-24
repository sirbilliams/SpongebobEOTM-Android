#pragma once
#include "../script/ScriptCondition.h"

namespace sbgame {

// Collection history (MIET in saves) outlives the HUD inventory. Only inventory
// pickups use it; scenery and script-only pickups retain their authored behavior.
inline bool actorHidden(bool inventoryPickup, const char* name, const char* condition,
                        bool currentlyHidden, sbscript::ConditionAtoms& progress)
{
    if (inventoryPickup && progress.hasItem(name)) return true;
    if (!*condition) return currentlyHidden;
    return !sbscript::evalCondition(condition, false, progress);
}

} // namespace sbgame
