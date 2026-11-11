// The 2D depth of scene objects and of the 3D layers, shared by GameScene::place and World3D::place so a test can check the order.
// 0x401330 (2DOB) / 0x4017f0 (ANIM) / 0x4096b0: z = -pz, or 10000 with Post Render.  0x42e560 (a CHAR / ITEM's place): its own
// VPElement (0x4033f0) is placed at z = -(its world z).  The 2D world draws by ascending z (key = ftol(z)).
#pragma once
namespace sbgame {
inline float objectDepth(bool postRender, float pz) { return postRender ? 10000.0f : -pz; }
inline float actorLayerDepth(float worldZ) { return -worldZ; }
// true when the 2D object is drawn over the character.  A tie is decided by the world's list order (0x10002950 sorts with MSVC 6 std::sort;
// stable up to 16 elements): the later linked element is drawn on top, and the scene links its elements in object-list order (0x42a310).
// This helper assumes the object is linked after the character, as in the tests.
inline bool objectCoversActor(bool postRender, float pz, float actorWorldZ)
{
    return (long)objectDepth(postRender, pz) > (long)actorLayerDepth(actorWorldZ);
}
}
