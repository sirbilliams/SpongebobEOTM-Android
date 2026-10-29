// Reconstructed field order from the readers, checked against their paired writers.
// Each version N returns after field N (the tag and version are not fields).
#include "SceneData.h"
#include <string.h>
namespace sb { namespace scene {
static const FieldInfo CameraFields[] = {
    {0x174, 4, offsetof(Camera, o0174), Float32},
    {0x178, 4, offsetof(Camera, o0178), Float32},
    {0x17c, 4, offsetof(Camera, o017c), Float32},
    {0x180, 2, offsetof(Camera, o0180), Word16},
    {0x182, 2, offsetof(Camera, o0182), Word16},
    {0x184, 2, offsetof(Camera, o0184), Word16},
    {0x188, 4, offsetof(Camera, o0188), Float32},
    {0x18c, 2, offsetof(Camera, o018c), Word16},
    {0x18e, 2, offsetof(Camera, o018e), Word16},
    {0x190, 2, offsetof(Camera, o0190), Word16},
    {0x192, 32, offsetof(Camera, o0192), Text},
    {0x1b2, 32, offsetof(Camera, o01b2), Text},
    {0x1d2, 32, offsetof(Camera, o01d2), Text},
    {0x1f2, 32, offsetof(Camera, o01f2), Text},
    {0x214, 4, offsetof(Camera, o0214), Float32},
    {0x218, 32, offsetof(Camera, o0218), Text},
    {0x238, 2, offsetof(Camera, o0238), Word16},
    {0x23a, 2, offsetof(Camera, o023a), Word16},
    {0x240, 4, offsetof(Camera, o0240), Float32},
    {0x23c, 2, offsetof(Camera, o023c), Word16},
    {0x244, 32, offsetof(Camera, o0244), Text},
    {0x264, 32, offsetof(Camera, o0264), Text},
    {0x284, 2, offsetof(Camera, o0284), Word16},
};
static const FieldInfo LightFields[] = {
    {0x180, 2, offsetof(Light, o0180), Word16},
    {0x174, 4, offsetof(Light, o0174), Float32},
    {0x178, 4, offsetof(Light, o0178), Float32},
    {0x17c, 4, offsetof(Light, o017c), Float32},
    {0x184, 4, offsetof(Light, o0184), Float32},
    {0x188, 4, offsetof(Light, o0188), Float32},
    {0x18c, 4, offsetof(Light, o018c), Float32},
    {0x190, 4, offsetof(Light, o0190), Float32},
    {0x194, 4, offsetof(Light, o0194), Float32},
    {0x198, 4, offsetof(Light, o0198), Float32},
    {0x19c, 4, offsetof(Light, o019c), Float32},
    {0x1a0, 4, offsetof(Light, o01a0), Float32},
};
static const FieldInfo Object2DFields[] = {
    {0x174, 32, offsetof(Object2D, o0174), Text},
    {0x194, 4, offsetof(Object2D, o0194), Float32},
    {0x198, 4, offsetof(Object2D, o0198), Float32},
    {0x19c, 4, offsetof(Object2D, o019c), Float32},
    {0x1a0, 2, offsetof(Object2D, o01a0), Word16},
    {0x20, 32, offsetof(Object2D, o0020), Text},
    {0x40, 32, offsetof(Object2D, o0040), Text},
    {0x60, 32, offsetof(Object2D, o0060), Text},
    {0x120, 32, offsetof(Object2D, o0120), Text},
    {0x14, 2, offsetof(Object2D, o0014), Word16},
    {0x18, 2, offsetof(Object2D, o0018), Word16},
    {0x16, 2, offsetof(Object2D, o0016), Word16},
    {0x1a, 2, offsetof(Object2D, o001a), Word16},
    {0x100, 32, offsetof(Object2D, o0100), Text},
    {0x140, 32, offsetof(Object2D, o0140), Text},
    {0x16c, 2, offsetof(Object2D, o016c), Word16},
    {0x16e, 2, offsetof(Object2D, o016e), Word16},
};
static const FieldInfo CharacterFields[] = {
    {0x1e4, 32, offsetof(Character, o01e4), Text},
    {0x230, 4, offsetof(Character, o0230), Float32},
    {0x234, 4, offsetof(Character, o0234), Float32},
    {0x238, 4, offsetof(Character, o0238), Float32},
    {0x240, 2, offsetof(Character, o0240), Word16},
    {0x224, 4, offsetof(Character, o0224), Float32},
    {0x27e, 2, offsetof(Character, o027e), Word16},
    {0x280, 2, offsetof(Character, o0280), Word16},
    {0x23c, 4, offsetof(Character, o023c), Float32},
    {0x20, 32, offsetof(Character, o0020), Text},
    {0x40, 32, offsetof(Character, o0040), Text},
    {0x60, 32, offsetof(Character, o0060), Text},
    {0x204, 32, offsetof(Character, o0204), Text},
    {0x120, 32, offsetof(Character, o0120), Text},
    {0x14, 2, offsetof(Character, o0014), Word16},
    {0x18, 2, offsetof(Character, o0018), Word16},
    {0x16, 2, offsetof(Character, o0016), Word16},
    {0x1a, 2, offsetof(Character, o001a), Word16},
    {0x2a0, 2, offsetof(Character, o02a0), Word16},
    {0x80, 32, offsetof(Character, o0080), Text},
    {0xa0, 32, offsetof(Character, o00a0), Text},
    {0xc0, 32, offsetof(Character, o00c0), Text},
    {0xe0, 32, offsetof(Character, o00e0), Text},
    {0x100, 32, offsetof(Character, o0100), Text},
    {0x246, 2, offsetof(Character, o0246), Word16},
    {0x976, 32, offsetof(Character, o0976), Text},
    {0x16c, 2, offsetof(Character, o016c), Word16},
    {0x16e, 2, offsetof(Character, o016e), Word16},
    {0x996, 32, offsetof(Character, o0996), Text},
    {0x9b6, 32, offsetof(Character, o09b6), Text},
    {0x9d6, 32, offsetof(Character, o09d6), Text},
    {0x2a4, 2, offsetof(Character, o02a4), Word16},
};
static const FieldInfo AnimationFields[] = {
    {0x174, 32, offsetof(Animation, o0174), Text},
    {0x194, 4, offsetof(Animation, o0194), Float32},
    {0x198, 4, offsetof(Animation, o0198), Float32},
    {0x19c, 4, offsetof(Animation, o019c), Float32},
    {0x1a0, 2, offsetof(Animation, o01a0), Word16},
    {0x20, 32, offsetof(Animation, o0020), Text},
    {0x40, 32, offsetof(Animation, o0040), Text},
    {0x60, 32, offsetof(Animation, o0060), Text},
    {0x120, 32, offsetof(Animation, o0120), Text},
    {0x14, 2, offsetof(Animation, o0014), Word16},
    {0x18, 2, offsetof(Animation, o0018), Word16},
    {0x16, 2, offsetof(Animation, o0016), Word16},
    {0x1a, 2, offsetof(Animation, o001a), Word16},
    {0x1b0, 2, offsetof(Animation, o01b0), Word16},
    {0x1b2, 2, offsetof(Animation, o01b2), Word16},
    {0x1b6, 2, offsetof(Animation, o01b6), Word16},
    {0x1b8, 2, offsetof(Animation, o01b8), Word16},
    {0x1ba, 2, offsetof(Animation, o01ba), Word16},
    {0x1bc, 2, offsetof(Animation, o01bc), Word16},
    {0x1be, 2, offsetof(Animation, o01be), Word16},
    {0x140, 32, offsetof(Animation, o0140), Text},
    {0x1b4, 2, offsetof(Animation, o01b4), Word16},
    {0x1c0, 2, offsetof(Animation, o01c0), Word16},
    {0x100, 32, offsetof(Animation, o0100), Text},
    {0x1ca, 2, offsetof(Animation, o01ca), Word16},
    {0x1d0, 2, offsetof(Animation, o01d0), Word16},
    {0x1d2, 2, offsetof(Animation, o01d2), Word16},
    {0x1d4, 2, offsetof(Animation, o01d4), Word16},
};
static const FieldInfo SpotFields[] = {
    {0x194, 4, offsetof(Spot, o0194), Float32},
    {0x198, 4, offsetof(Spot, o0198), Float32},
    {0x20, 32, offsetof(Spot, o0020), Text},
    {0x40, 32, offsetof(Spot, o0040), Text},
    {0x60, 32, offsetof(Spot, o0060), Text},
    {0x120, 32, offsetof(Spot, o0120), Text},
    {0x14, 2, offsetof(Spot, o0014), Word16},
    {0x18, 2, offsetof(Spot, o0018), Word16},
    {0x16, 2, offsetof(Spot, o0016), Word16},
    {0x1a, 2, offsetof(Spot, o001a), Word16},
    {0x1b0, 2, offsetof(Spot, o01b0), Word16},
    {0x140, 32, offsetof(Spot, o0140), Text},
    {0x16c, 2, offsetof(Spot, o016c), Word16},
    {0x16e, 2, offsetof(Spot, o016e), Word16},
};
static const FieldInfo ItemFields[] = {
    {0x1e4, 32, offsetof(Item, o01e4), Text},
    {0x230, 4, offsetof(Item, o0230), Float32},
    {0x234, 4, offsetof(Item, o0234), Float32},
    {0x238, 4, offsetof(Item, o0238), Float32},
    {0x240, 2, offsetof(Item, o0240), Word16},
    {0x224, 4, offsetof(Item, o0224), Float32},
    {0x278, 2, offsetof(Item, o0278), Word16},
    {0x27a, 2, offsetof(Item, o027a), Word16},
    {0x23c, 4, offsetof(Item, o023c), Float32},
    {0x20, 32, offsetof(Item, o0020), Text},
    {0x40, 32, offsetof(Item, o0040), Text},
    {0x60, 32, offsetof(Item, o0060), Text},
    {0x204, 32, offsetof(Item, o0204), Text},
    {0x120, 32, offsetof(Item, o0120), Text},
    {0x14, 2, offsetof(Item, o0014), Word16},
    {0x18, 2, offsetof(Item, o0018), Word16},
    {0x16, 2, offsetof(Item, o0016), Word16},
    {0x1a, 2, offsetof(Item, o001a), Word16},
    {0x242, 2, offsetof(Item, o0242), Word16},
    {0x244, 2, offsetof(Item, o0244), Word16},
    {0x100, 32, offsetof(Item, o0100), Text},
    {0x16c, 2, offsetof(Item, o016c), Word16},
    {0x16e, 2, offsetof(Item, o016e), Word16},
    {0x246, 2, offsetof(Item, o0246), Word16},
};
static const FieldInfo TriggerFields[] = {
    {0x230, 4, offsetof(Trigger, o0230), Float32},
    {0x234, 4, offsetof(Trigger, o0234), Float32},
    {0x238, 4, offsetof(Trigger, o0238), Float32},
    {0x23c, 4, offsetof(Trigger, o023c), Float32},
    {0x278, 2, offsetof(Trigger, o0278), Word16},
    {0x20, 32, offsetof(Trigger, o0020), Text},
    {0x40, 32, offsetof(Trigger, o0040), Text},
    {0x60, 32, offsetof(Trigger, o0060), Text},
    {0x120, 32, offsetof(Trigger, o0120), Text},
    {0x14, 2, offsetof(Trigger, o0014), Word16},
    {0x18, 2, offsetof(Trigger, o0018), Word16},
    {0x16, 2, offsetof(Trigger, o0016), Word16},
    {0x1a, 2, offsetof(Trigger, o001a), Word16},
    {0x27a, 2, offsetof(Trigger, o027a), Word16},
    {0x140, 32, offsetof(Trigger, o0140), Text},
    {0x27c, 2, offsetof(Trigger, o027c), Word16},
    {0x80, 32, offsetof(Trigger, o0080), Text},
    {0xa0, 32, offsetof(Trigger, o00a0), Text},
    {0xc0, 32, offsetof(Trigger, o00c0), Text},
    {0xe0, 32, offsetof(Trigger, o00e0), Text},
    {0x27e, 2, offsetof(Trigger, o027e), Word16},
    {0x280, 32, offsetof(Trigger, o0280), Text},
    {0x16c, 2, offsetof(Trigger, o016c), Word16},
    {0x16e, 2, offsetof(Trigger, o016e), Word16},
    {0x100, 32, offsetof(Trigger, o0100), Text},
};
static const Schema schemas[] = {
    {Object3DTag, "Object3D", 0x440c80, 0, 0},
    {Light3DTag, "Light3D", 0x440c80, 0, 0},
    {SceneTag, "Scene", 0x440c80, 0, 0},
    {0x43414d52, "Camera", 0x40ea40, CameraFields, sizeof(CameraFields)/sizeof(FieldInfo)},
    {0x4c495445, "Light", 0x423a30, LightFields, sizeof(LightFields)/sizeof(FieldInfo)},
    {0x32444f42, "Object2D", 0x401330, Object2DFields, sizeof(Object2DFields)/sizeof(FieldInfo)},
    {0x43484152, "Character", 0x431bf0, CharacterFields, sizeof(CharacterFields)/sizeof(FieldInfo)},
    {0x414e494d, "Animation", 0x4082c0, AnimationFields, sizeof(AnimationFields)/sizeof(FieldInfo)},
    {0x53504f54, "Spot", 0x419ef0, SpotFields, sizeof(SpotFields)/sizeof(FieldInfo)},
    {0x4954454d, "Item", 0x422070, ItemFields, sizeof(ItemFields)/sizeof(FieldInfo)},
    {0x54524947, "Trigger", 0x440ca0, TriggerFields, sizeof(TriggerFields)/sizeof(FieldInfo)},
};
const Schema *schema(uint32_t tag) {
    for (size_t i=0; i<sizeof(schemas)/sizeof(Schema); ++i)
        if (schemas[i].tag==tag) return &schemas[i];
    return 0;
}
std::string Text32::text() const {
    size_t n=0; while (n<32 && bytes[n]) ++n;
    return std::string(bytes,n);
}
Record::Record() : tag(0), version(0), begin(0), end(0), fieldsRead(0) {
    memset(&data,0,sizeof(data));
}
bool Record::hasField(unsigned offset) const {
    const Schema *s=schema(tag);
    if (s) for (size_t i=0;i<fieldsRead;++i)
        if (s->fields[i].originalOffset==offset) return true;
    return false;
}
Grid::Grid() : present(false), halfSize(0), o0008(0), o000c(0), o0010(0), o0014(0), side(0) {
    memset(cells,0,sizeof(cells));
}
Scene::Scene() : hasStartPoints(false), bytesConsumed(0), fileSize(0) {
    memset(startPoints,0,sizeof(startPoints));
}
size_t Scene::count(uint32_t tag) const {
    size_t n=0; for (size_t i=0;i<elements.size();++i) if (elements[i].tag==tag) ++n;
    return n;
}
} }
