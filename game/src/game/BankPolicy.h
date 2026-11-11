#pragma once
// DoSwitch 0x40b720: the chapter sound banks (sounds\c<n>.omt, comments<n>.omt) reload when the chapter changes
// (compared with the chapter of the scene we leave, not the one we enter) or when no shared bank is open yet.
inline bool sharedBankNeedsReload(int prevChapter, int newChapter, bool sharedOpen)
{
    return newChapter != prevChapter || !sharedOpen;
}
