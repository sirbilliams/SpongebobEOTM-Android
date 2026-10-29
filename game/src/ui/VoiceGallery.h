/*  port: the voice gallery behind the 'Voices' page of View Movies (docs/reveal-and-gallery.md).  Lists the dialogue voice clips of the game
    (dialogue/*.sb2 lines that have a wave in sounds/cXlYsZ.omt), which of them the player has heard (GameState: voices of this run + the saved
    dialogue progress), grouped by chapter and speaker, and plays one on the voice channel (4) like DialogueUI does.  */
#pragma once
#include "../game/GameState.h"
#include "../game/Game.h"
#include <string>
#include <vector>

namespace sbgame {

struct VoiceClip {
    unsigned scene;                 // packed scene id (sbscript::sceneIdPack)
    std::string key;                // line key "AAH"
    std::string speaker;            // display name
    std::string text;
    bool heard;
};

class VoiceGallery {
public:
    VoiceGallery();
    ~VoiceGallery();
    // Scan the install (cwd) and mark the heard clips from `st` (may be null).  SB_TEST_VOICES_HEARD seeds heard clips (tests only).
    void build(const GameState* st);
    const std::vector<VoiceClip>& clips() const { return clips_; }
    std::vector<std::string> speakers(int chapter) const;                       // display names, in first-appearance order
    std::vector<int> select(int chapter, const std::string& speaker) const;     // indices into clips()
    void count(int chapter, const std::string& speaker, int* heard, int* total) const;   // speaker "" = the whole chapter
    bool play(int index);                                                       // true if a wave started
    void stop();
    bool playing() const;
    static int chapterOf(unsigned scene) { return (int)(scene & 0xf); }
private:
    std::vector<VoiceClip> clips_;
    OmtFile bank_;
    unsigned bankScene_;
};

}  // namespace sbgame
