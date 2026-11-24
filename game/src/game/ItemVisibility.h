#pragma once
#include "../script/ScriptCondition.h"

namespace sbgame {

// Collection history (MIET in saves) outlives the HUD inventory. Only inventory
// pickups use it; scenery and script-only pickups retain their authored behavior.
template<class EvaluateCondition>
inline bool actorHidden(bool inventoryPickup, const char* name, const char* condition,
                        bool currentlyHidden, sbscript::ConditionAtoms& progress, EvaluateCondition evaluate)
{
    if (inventoryPickup && progress.hasItem(name)) return true;
    if (!*condition) return currentlyHidden;
    return !evaluate(condition);
}

} // namespace sbgame
