// Beachball character methods, sb2.exe 0x435160 / 0x431870 / 0x431a90.
#pragma once
namespace sbgame {
class World3D;
struct Actor3D;
class Kick {
public:
    static bool start(World3D& w, Actor3D* a);
    static void update(World3D& w, Actor3D* a, float dtMs);
    static bool goal(World3D& w, Actor3D* a);
    static bool missed(World3D& w, Actor3D* a);
private:
    static bool clear(World3D& w, Actor3D* a, float x, float z);
};
}
