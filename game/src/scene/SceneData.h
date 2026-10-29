// Scene data read by sb2.exe. See docs/scene-loader.md for the offset convention.
#ifndef SB_SCENE_DATA_H
#define SB_SCENE_DATA_H
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
namespace sb { namespace scene {
// Fixed buffers are preserved verbatim; they need not contain a terminating NUL.
struct Text32 { char bytes[32]; std::string text() const; };
enum Tag { CAMR=0x43414d52, LITE=0x4c495445, Object2DTag=0x32444f42,
 CharacterTag=0x43484152, AnimationTag=0x414e494d, SpotTag=0x53504f54,
 ItemTag=0x4954454d, TriggerTag=0x54524947,
 Object3DTag=0x33444f42, Light3DTag=0x334c4947, SceneTag=0x5343454e };
// Reader 0x40ea40; members in disk order, oXXXX = original this-relative offset.
struct Camera {
    float o0174;
    float o0178;
    float o017c;
    uint16_t o0180;
    uint16_t o0182;
    uint16_t o0184;
    float o0188;
    uint16_t o018c;
    uint16_t o018e;
    uint16_t o0190;
    Text32 o0192;
    Text32 o01b2;
    Text32 o01d2;
    Text32 o01f2;
    float o0214;
    Text32 o0218;
    uint16_t o0238;
    uint16_t o023a;
    float o0240;
    uint16_t o023c;
    Text32 o0244;
    Text32 o0264;
    uint16_t o0284;
};
// Reader 0x423a30; members in disk order, oXXXX = original this-relative offset.
struct Light {
    uint16_t o0180;
    float o0174;
    float o0178;
    float o017c;
    float o0184;
    float o0188;
    float o018c;
    float o0190;
    float o0194;
    float o0198;
    float o019c;
    float o01a0;
};
// Reader 0x401330; members in disk order, oXXXX = original this-relative offset.
struct Object2D {
    Text32 o0174;
    float o0194;
    float o0198;
    float o019c;
    uint16_t o01a0;
    Text32 o0020;
    Text32 o0040;
    Text32 o0060;
    Text32 o0120;
    uint16_t o0014;
    uint16_t o0018;
    uint16_t o0016;
    uint16_t o001a;
    Text32 o0100;
    Text32 o0140;
    uint16_t o016c;
    uint16_t o016e;
};
// Reader 0x431bf0; members in disk order, oXXXX = original this-relative offset.
struct Character {
    Text32 o01e4;
    float o0230;
    float o0234;
    float o0238;
    uint16_t o0240;
    float o0224;
    uint16_t o027e;
    uint16_t o0280;
    float o023c;
    Text32 o0020;
    Text32 o0040;
    Text32 o0060;
    Text32 o0204;
    Text32 o0120;
    uint16_t o0014;
    uint16_t o0018;
    uint16_t o0016;
    uint16_t o001a;
    uint16_t o02a0;
    Text32 o0080;
    Text32 o00a0;
    Text32 o00c0;
    Text32 o00e0;
    Text32 o0100;
    uint16_t o0246;
    Text32 o0976;
    uint16_t o016c;
    uint16_t o016e;
    Text32 o0996;
    Text32 o09b6;
    Text32 o09d6;
    uint16_t o02a4;
};
// Reader 0x4082c0; members in disk order, oXXXX = original this-relative offset.
struct Animation {
    Text32 o0174;
    float o0194;
    float o0198;
    float o019c;
    uint16_t o01a0;
    Text32 o0020;
    Text32 o0040;
    Text32 o0060;
    Text32 o0120;
    uint16_t o0014;
    uint16_t o0018;
    uint16_t o0016;
    uint16_t o001a;
    uint16_t o01b0;
    uint16_t o01b2;
    uint16_t o01b6;
    uint16_t o01b8;
    uint16_t o01ba;
    uint16_t o01bc;
    uint16_t o01be;
    Text32 o0140;
    uint16_t o01b4;
    uint16_t o01c0;
    Text32 o0100;
    uint16_t o01ca;
    uint16_t o01d0;
    uint16_t o01d2;
    uint16_t o01d4;
};
// Reader 0x419ef0; members in disk order, oXXXX = original this-relative offset.
struct Spot {
    float o0194;
    float o0198;
    Text32 o0020;
    Text32 o0040;
    Text32 o0060;
    Text32 o0120;
    uint16_t o0014;
    uint16_t o0018;
    uint16_t o0016;
    uint16_t o001a;
    uint16_t o01b0;
    Text32 o0140;
    uint16_t o016c;
    uint16_t o016e;
};
// Reader 0x422070; members in disk order, oXXXX = original this-relative offset.
struct Item {
    Text32 o01e4;
    float o0230;
    float o0234;
    float o0238;
    uint16_t o0240;
    float o0224;
    uint16_t o0278;
    uint16_t o027a;
    float o023c;
    Text32 o0020;
    Text32 o0040;
    Text32 o0060;
    Text32 o0204;
    Text32 o0120;
    uint16_t o0014;
    uint16_t o0018;
    uint16_t o0016;
    uint16_t o001a;
    uint16_t o0242;
    uint16_t o0244;
    Text32 o0100;
    uint16_t o016c;
    uint16_t o016e;
    uint16_t o0246;
};
// Reader 0x440ca0; members in disk order, oXXXX = original this-relative offset.
struct Trigger {
    float o0230;
    float o0234;
    float o0238;
    float o023c;
    uint16_t o0278;
    Text32 o0020;
    Text32 o0040;
    Text32 o0060;
    Text32 o0120;
    uint16_t o0014;
    uint16_t o0018;
    uint16_t o0016;
    uint16_t o001a;
    uint16_t o027a;
    Text32 o0140;
    uint16_t o027c;
    Text32 o0080;
    Text32 o00a0;
    Text32 o00c0;
    Text32 o00e0;
    uint16_t o027e;
    Text32 o0280;
    uint16_t o016c;
    uint16_t o016e;
    Text32 o0100;
};
union Payload {
    Camera camera;
    Light light;
    Object2D object2D;
    Character character;
    Animation animation;
    Spot spot;
    Item item;
    Trigger trigger;
};
enum FieldKind { Word16, Word32, Float32, Text };
struct FieldInfo { unsigned originalOffset, diskSize; size_t memberOffset; FieldKind kind; };
struct Schema { uint32_t tag; const char *name; unsigned readerAddress;
    const FieldInfo *fields; size_t fieldCount; };
const Schema *schema(uint32_t tag);
struct Record {
    uint32_t tag;
    uint16_t version;
    size_t begin, end, fieldsRead;
    Payload data;
    Record();
    bool hasField(unsigned originalOffset) const;
};
struct StartPoint { float x, y, z; uint16_t heading, unknown; };
struct Grid {
    bool present;
    float halfSize, o0008, o000c, o0010, o0014;
    unsigned side;
    unsigned char cells[60][30]; // exe allocates 0x720 bytes; row stride is 30
    Grid();
};
struct Scene {
    Record camera;
    std::vector<Record> tiles; // historical API name: these are LITE lights, not tiles
    bool hasStartPoints;
    StartPoint startPoints[10];
    Grid grids[2];
    std::vector<Record> elements; // retain interleaved factory order
    size_t bytesConsumed, fileSize;
    Scene();
    size_t count(uint32_t tag) const;
};
// Transactional load: on failure output is unchanged; error includes the failing byte offset.
bool load(const std::string &path, Scene &output, std::string &error);
bool read(const unsigned char *bytes, size_t size, Scene &output, std::string &error);
} }
#endif
