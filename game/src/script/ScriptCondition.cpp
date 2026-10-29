#include "ScriptCondition.h"
#include "Sb2File.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <string>

namespace sbscript {

static int digitOf(char c)   // sprintf("%c") then atoi of the one-character string
{
    char b[2] = { c, 0 };
    return atoi(b);
}

bool evalConditionTerm(const char* term, bool touching, ConditionAtoms& atoms)
{
    if (strcmp(term, "0") == 0) return true;
    if (strlen(term) == 0) return true;
    if (strcmp(term, " ") == 0) return true;

    bool bl = true;                       // result when the atom matches
    std::string t;
    if (term[0] == '!') { t = term + 1; bl = false; }
    else t = term;
    // port: the exe copies into a 20-byte buffer without a bound check

    const char* c = t.c_str();
    if (strcmp(c, "touching") == 0) {
        if (!touching && !atoms.debugAll()) return !bl;
        return bl;
    }
    if (strcmp(c, "debug") == 0) return !bl;
    if (strcmp(c, "zero") == 0) return !bl;

    int up = toupper((unsigned char)c[0]);
    size_t len = t.size();
    if (up == 'S') return atoms.scriptSeen(c) ? bl : !bl;
    if (up == 'I') return atoms.hasItem(c) ? bl : !bl;
    if (up == 'C' && len > 3) return atoms.flagSet(c) ? bl : !bl;

    if (len == 6) {
        unsigned id = (unsigned)((digitOf(c[2]) * 0x100 + digitOf(c[1])) * 0x100 + digitOf(c[0]));
        char key[4] = { c[3], c[4], c[5], 0 };
        return atoms.dialogueStarted(id, key) ? bl : !bl;
    }
    if (len == 3) return atoms.dialogueStarted(atoms.currentSceneId(), c) ? bl : !bl;
    if (strcmp(c, "xxxx") == 0 && atoms.miniGameFlag()) return bl;
    return !bl;
}

bool evalCondition(const char* expr, bool touching, ConditionAtoms& atoms)
{
    if (atoms.debugAll()) return true;
    bool orMode = strstr(expr, " or ") != 0;
    SbTok tok(expr, " ");
    std::string t;
    if (!tok.next(t)) return !orMode;
    bool r = evalConditionTerm(t.c_str(), touching, atoms);
    if (!r) { if (!orMode) return false; }
    else { if (orMode) return true; }
    for (;;) {
        do {
            if (!tok.next(t)) return !orMode;
        } while (t == "or" || t == "and");
        r = evalConditionTerm(t.c_str(), touching, atoms);
        if (!r) { if (!orMode) return false; }
        else { if (orMode) return true; }
    }
}

}  // namespace sbscript
