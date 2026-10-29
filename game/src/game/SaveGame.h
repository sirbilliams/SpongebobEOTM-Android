/*  Save games of sb2.exe: the file format written by the progress object's writer 0x416ed0 and read by the loader
    0x417460, the slot files ("Saved\slot%d.sb2", 0x455350, 99 slots; "Saved\quick.sb2"), and the move between a
    GameState and the file.  See docs/saves.md.

    File (little endian, every string a fixed zero-padded field):
      char name[0x20]          the name typed in the Save Game screen ("Quick Save" for the quick save)
      u32  'SHOT' (0x53484f54) in the file bytes "TOHS"
      s32  thumbnail byte count, -1 = none;  then that many bytes (320x240, 16 bits per pixel)
      u32  scene id (scene+0x264: scene<<16 | level<<8 | chapter)
      f32  x, y, z; s16 facing  of the controlled character (all 0 when the scene has none)
      s16  n, then n * { char[0x20], char[0x20], s16 }          the item bar (0x421c40)
      repeated, until the end of the file, each a u32 tag and its record:
        'ITEM' char[0x20]  items given      (0x4169a0)          'CHAR' char[0x20]  characters   (0x416c70)
        'THRD' char[0x10] group, char[0x10] current line, s16, s16, u32 scene  dialogue progress (0x416d30)
        'SCRP' char[0x20]  scripts seen     (0x416610)          'SCEN' u32         flags        (0x416a60)
        'TEXT' char[0x20], char[0x20]       texture changes (0x4166d0)
        'POSI' char[0x20], u32 x4, s16      positions       (0x416880)
        'BLOK' u32 scene, s16 x4            blocked cells   (0x416ba0)
        'PUZZ' 42 bytes (+ 'HIGH' s16 when the high score is above 0)   the puzzle object
        'LAST' u32                          game+0x15c
    Port: files are written to a temporary name and renamed, so a crash never leaves half a save.             */
#pragma once
#include "GameState.h"
#include <string>
#include <vector>

class OMediaCanvas;
class OMediaGfxWorld;

namespace sbgame {

struct SaveFile {
    SaveFile();
    char name[0x20];
    bool hasThumb;                           // thumbnail byte count != -1
    std::vector<unsigned char> thumb;        // 320x240 RGB565, rows of 640 bytes
    unsigned sceneId;
    float px, py, pz; short facing;
    std::vector<GameState::BarRec> bar;
    std::vector<std::string> miet, chars, scrp;
    std::vector<unsigned> scen;
    struct Thrd { char group[0x10], cur[0x10]; short a, b; unsigned scene; };
    std::vector<Thrd> thrd;
    struct Text { char object[0x20], texture[0x20]; };
    std::vector<Text> text;
    std::vector<GameState::PosiRec> posi;
    struct Blok { unsigned scene; short a, b, c, d; };
    std::vector<Blok> blok;
    bool puzzle; unsigned char puzzleBytes[42];
    short high;
    bool hasLast; unsigned last;
};

class SaveGame {
public:
    static const int kSlots = 99;                       // 0x428950 is tried for 0..98
    static const unsigned kMagic = 0x53484f54;          // 'SHOT'
    static const int kThumbW = 320, kThumbH = 240;
    static const int kThumbBytes = 320 * 240 * 2;

    // "saved/slot%d.sb2" / "saved/quick.sb2" under `dir` (the game folder; "." by default)
    static void setDir(const std::string& dir);
    static std::string slotPath(int slot);
    static std::string quickPath();
    static bool exists(int slot);                       // 0x428950: the file opens
    static bool fileExists(const std::string& path);    // 0x40e120: opens, and is not empty
    static int firstFreeSlot();                         // the Save Game screen's first slot
    static bool deleteSlot(int slot);                   // 0x428a70: the later slots move down one

    // the file
    static bool write(const std::string& path, const SaveFile& f);
    static bool read(const std::string& path, SaveFile& f);
    // name, thumbnail and scene id only (the Save / Load Game screens, 0x4295d0)
    static bool readHeader(const std::string& path, SaveFile& f);

    // GameState <-> SaveFile
    static void capture(GameState& st, unsigned sceneId, const char* name, const std::vector<unsigned char>& thumb, SaveFile& f);
    // 0x417460: the state replaced by the file's; returns the scene to go to ("c%dl%ds%dp1", 0x455164) or "" for none
    static std::string apply(const SaveFile& f, GameState& st);

    // thumbnails: the 640x480 screen scaled to 320x240 (port: read back through a memory world)
    static bool grabThumb(OMediaGfxWorld* screen, std::vector<unsigned char>& rgb565);
    static OMediaCanvas* thumbCanvas(const std::vector<unsigned char>& rgb565);   // owned by the caller

    // port: tests only (SB_TEST_SAVEFMT=1): write, read back and compare a populated state; returns 0 when equal
    static int selfTest();
};

}  // namespace sbgame
