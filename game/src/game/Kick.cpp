#include "Kick.h"
#include "World3D.h"
#include "Game.h"
#include <math.h>
#include <strings.h>

namespace sbgame {
bool Kick::start(World3D& w, Actor3D* a) // 0x435160
{
    if (!a) return false;
    Actor3D* player = 0;
    for (size_t i=0; i<w.actors_.size(); ++i)
        if (w.actors_[i]->obj->userCtl) { player=w.actors_[i]; break; }
    if (!player) return false;
    float dx=a->x-player->x, dz=a->z-player->z;
    if (dx==0 && dz==0) return false; // port: coincident centres have no kick direction.
    // OMT.dll 0x10032cd0 matches the shared table-based magnitude implementation.
    OMedia3DVector direction(dx,0,dz);
    float d=direction.quick_magnitude();
    a->tx=a->x+dx/d*400.0f; a->tz=a->z+dz/d*400.0f;
    a->kicking=true; a->walking=false; a->rotating=false; a->route.clear();
    a->waited=true;
    w.game_->log("kick %s from %g,%g player %g,%g target %g,%g\n",a->name.c_str(),a->x,a->z,player->x,player->z,a->tx,a->tz);
    return true;
}

bool Kick::goal(World3D& w, Actor3D* a) // 0x4313e0 contact list + 0x431870 accept-name match
{
    if (!a) return false;
    const std::vector<sb::scene::Record>& records=w.scene_->data().elements;
    for (size_t i=0; i<records.size(); ++i) {
        if (records[i].tag!=sb::scene::TriggerTag) continue;
        const sb::scene::Trigger& t=records[i].data.trigger;
        if (t.o0234!=a->y || strcasecmp(t.o0080.text().c_str(),a->name.c_str())!=0) continue;
        float dx=a->x-t.o0230, dz=a->z-t.o0238, r=a->radius+t.o023c;
        if (dx*dx+dz*dz<r*r) return true;
    }
    return false;
}

bool Kick::clear(World3D& w, Actor3D* a, float x, float z) // 0x431940 / 0x418170
{
    if (!w.scene_->door().empty() || (w.game_->runner() && !w.game_->runner()->terrain())) return true;
    const sb::scene::Grid* g=w.grid(a->y);
    if (!g) return false;
    // 0x4183d0 gives a boundary to the lower cell, unlike floor().
    int ix=(int)ceilf((x-g->o000c)/g->o0008+g->halfSize)-1;
    int iz=(int)ceilf((z-g->o0010)/g->o0008+g->halfSize)-1;
    return w.cell(*g,ix,iz);
}

void Kick::update(World3D& w, Actor3D* a, float dtMs) // kick branch of 0x42f1a0
{
    if (!a->kicking) return;
    float dx=a->tx-a->x, dz=a->tz-a->z, d=sqrtf(dx*dx+dz*dz);
    // 0x434650: arrival radius = max(25, character radius * 0.5).
    float tolerance=a->radius*0.5f; if (tolerance<25) tolerance=25;
    if (d>tolerance) {
        dx/=d; dz/=d;
        float step=dtMs*0.3f;
        if (step>d) step=d; // port: prevent a long frame overshooting the destination.
        if (clear(w,a,a->x+dx*step,a->z+dz*step)) {
            a->x+=dx*step; a->z+=dz*step;
        } else if (goal(w,a)) { a->tx=a->x; a->tz=a->z; }
        else {
            a->tx=a->x-d*dx; a->tz=a->z-d*dz;
            a->x-=dx*step; a->z-=dz*step;
            w.game_->log("kick bounce at %g,%g\n",a->x,a->z);
        }
        w.place(a);
        dx=a->tx-a->x; dz=a->tz-a->z; d=sqrtf(dx*dx+dz*dz);
    }
    if (d<=tolerance) {
        a->kicking=false;
        w.setAnim(a,"idle1",9999,1,Actor3D::Idle);
        w.game_->log("kick stopped %g,%g goal=%d missed=%d\n",a->x,a->z,goal(w,a),missed(w,a));
        w.finished(a); // dispatcher sets last command to walk; wake only after movement.
    }
}

bool Kick::missed(World3D& w, Actor3D* a) // 0x431a90: projected centre outside 640 x 480
{
    if (!a) return false;
    const sb::scene::Camera& c=w.scene_->data().camera.data.camera;
    OMedia3DVector p(a->x-c.o0174,a->y-c.o0178,a->z-c.o017c);
    p.inv_rotate((short)(-c.o0180*16384/360),(short)(-c.o0182*16384/360),(short)(-c.o0184*16384/360));
    if (p.z==0) return true; // port: projection at the camera plane.
    int x=(int)(320+p.x*c.o0188/p.z), y=(int)(240-p.y*c.o0188/p.z);
    return x<0 || x>639 || y<0 || y>479;
}
}
