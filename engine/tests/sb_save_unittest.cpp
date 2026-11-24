// Save compatibility and collected-item visibility, without proprietary game data.
#include "../../game/src/game/SaveGame.h"
#include "../../game/src/game/ItemVisibility.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

int main()
{
    using namespace sbgame;
    char dir[] = "/tmp/sb-save-test-XXXXXX";
    if (!mkdtemp(dir)) return 1;
    SaveGame::setDir(dir);
    int failures = SaveGame::selfTest() != 0;
    GameState old;
    old.giveItem("ikp4"); // v1.0 already writes collection history separately from the bar
    SaveFile file;
    SaveGame::capture(old, 0x10201, "Collected", std::vector<unsigned char>(), file);
    std::string path = std::string(dir) + "/collected.sb2";
    SaveFile loaded;
    bool ok = SaveGame::write(path, file) && SaveGame::read(path, loaded);
    if (ok) {
        GameState restored;
        SaveGame::apply(loaded, restored);
        ok = restored.bar().empty() && actorHidden(true, "IKP4", "", false, restored,
             [&restored](const char* c) { return sbscript::evalCondition(c, false, restored); });
    }
    if (!ok) ++failures;
    std::remove(path.c_str());
    rmdir((std::string(dir) + "/saved").c_str());
    rmdir(dir);
    printf("sb_save_unittest: round trip and collected item with empty inventory: %d failed\n", failures);
    return failures ? 1 : 0;
}
