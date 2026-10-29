#include "World3D.h"
#include "Game.h"
#include "SceneScriptHost.h"
#include "OMedia3DRasterizerDesc.h"
#include "OMediaDisplay.h"
#include "OMediaViewPort.h"
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

namespace sbgame {

static const float kWalkSpeed = 220.0f;      // port: units per second (the exe walks by the animation's root motion)
static const float kRunSpeed = 380.0f;
static const double kPi = 3.14159265358979;

Actor3D::Actor3D()
    : obj(0), model(0), elem(0), mode(Idle), x(0), y(0), z(0), heading(0), walking(false), tx(0), tz(0), speed(0), rotating(false),
      rotTarget(0), rotMs(0), rotLeft(0), idling(false), idleLeft(0), waited(false), scheduled(false)
{
}

static std::string lower(std::string s)
{
    for (size_t i = 0; i < s.size(); ++i) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

// The rasterizer is made once per process (sb2.exe 0x40ceb0 InitRasterizer: the first accelerated one of the screen device).
OMedia3DAbstractRasterizer* World3D::rasterizer()
{
    static OMedia3DAbstractRasterizer* ras = 0;
    if (ras) return ras;
    OMediaScreenDevice* dev = OMediaScreenDevice::getdefaultdevice();
    std::vector<OMedia3DRasterizerDesc*>* rl = dev ? dev->get_rasterizers() : 0;
    for (size_t i = 0; rl && i < rl->size() && !ras; i++)
        if (((*rl)[i]->attributes & omcrda_Accelerated)) ras = (*rl)[i]->build(NULL, 0);
    if (!ras) ras = OMedia3DRasterizerManager::get_default()->build_rasterizer(NULL, NULL, 0);
    if (ras) { ras->set_anti_aliasing(3); ras->set_texture_filtering(3); }
    return ras;
}

World3D::World3D(Game* game, GameScene* scene)
    : game_(game), scene_(scene), w3_(0), vp_(0), vpe_(0), wake_(false)
{
    dir_ = game->root();
    OMedia3DAbstractRasterizer* ras = rasterizer();
    if (!ras) { game_->log("3D: no rasterizer\n"); return; }
    omd_REGISTERCLASS(OMedia3DMaterial);
    if (!OMedia3DGrannyModel::init(dir_)) { game_->log("3D: granny init failed\n"); return; }
    const sb::scene::Scene& sc = scene->data();
    const sb::scene::Camera& cam = sc.camera.data.camera;
    // 0x4033f0: the scene's 3D world; no background, so the 2D art shows through
    w3_ = new OMedia3DWorld;
    w3_->set_framerate_mode(omfrc_Max);
    w3_->setbackground(NULL);
    w3_->focal = cam.o0188;
    w3_->far_z = 4500.0f;
    w3_->near_z = 10.0f;
    w3_->ambient.setred8bits((unsigned char)cam.o018c);
    w3_->ambient.setgreen8bits((unsigned char)cam.o018e);
    w3_->ambient.setblue8bits((unsigned char)cam.o0190);
    w3_->rasterizer = ras;
    vp_ = new OMediaViewPort(w3_);
    OMediaRect b3(0, 0, 640, 480);
    vp_->setbounds(&b3);
    vp_->place(cam.o0174, cam.o0178, cam.o017c);
    vp_->set_angle((short)((cam.o0180 * 0x4000) / 360), (short)((cam.o0182 * 0x4000) / 360), (short)((cam.o0184 * 0x4000) / 360));
    vpe_ = new OMediaVPElement;
    vpe_->set_viewport(vp_);
    vpe_->link(scene->world());
    vpe_->place(0, 0, 100);        // port: between the 2D background (z 0) and the foreground objects
    // the LITE records (the type-0 ones are switched off)
    for (size_t i = 0; i < sc.tiles.size(); i++) {
        const sb::scene::Light& l = sc.tiles[i].data.light;
        if (!l.o0180) continue;
        OMedia3DLight* light = new OMedia3DLight;
        light->link(w3_);
        light->kind = 2;
        light->range = l.o0184;
        light->falloff = l.o0188;
        light->diffuse[0] = l.o018c; light->diffuse[1] = l.o0190; light->diffuse[2] = l.o0194;
        light->place(l.o0174, l.o0178, l.o017c);
        lights_.push_back(light);
    }
    // the characters and items
    for (size_t i = 0; i < scene->objects().size(); i++) {
        SceneObject* o = scene->objects()[i];
        if (o->kind != SceneObject::Char && o->kind != SceneObject::Item) continue;
        Actor3D* a = new Actor3D;
        a->obj = o;
        a->name = lower(o->art);
        a->x = o->px; a->y = o->py; a->z = o->pz;
        const sb::scene::Record* rec = 0;
        for (size_t k = 0, n = 0; k < sc.elements.size(); ++k) {
            const sb::scene::Record& r = sc.elements[k];
            if (r.tag == sb::scene::Object2DTag || r.tag == sb::scene::AnimationTag || r.tag == sb::scene::SpotTag ||
                r.tag == sb::scene::ItemTag || r.tag == sb::scene::CharacterTag || r.tag == sb::scene::TriggerTag) {
                if (n++ == i) { rec = &r; break; }
            }
        }
        a->heading = (rec && rec->tag == sb::scene::CharacterTag) ? (rec->data.character.o0240 & 0x3fff) : 0;
        a->model = new OMedia3DGrannyModel;
        std::string sub = o->kind == SceneObject::Char ? "/models/people/" : "/models/items/";
        if (!a->model->load(dir_ + sub + a->name + "_model")) {
            game_->log("3D: %s: %s\n", a->name.c_str(), a->model->get_error().c_str());
            delete a->model; delete a;
            continue;
        }
        a->elem = new OMedia3DShapeElement;
        a->elem->setshape(a->model->get_shape());
        a->elem->link(w3_);
        place(a);
        actors_.push_back(a);
        if (o->kind == SceneObject::Char) setAnim(a, "idle1", 9999, 1.0f, Actor3D::Idle);
        game_->log("3D: %s at %g %g %g heading %d\n", a->name.c_str(), a->x, a->y, a->z, a->heading);
    }
}

World3D::~World3D()
{
    for (size_t i = 0; i < actors_.size(); i++) delete actors_[i]->elem;
    for (size_t i = 0; i < actors_.size(); i++) { delete actors_[i]->model; delete actors_[i]; }
    for (size_t i = 0; i < lights_.size(); i++) delete lights_[i];
    delete vpe_;
    delete vp_;
    delete w3_;
}

void World3D::place(Actor3D* a)
{
    a->obj->px = a->x; a->obj->py = a->y; a->obj->pz = a->z;
    a->elem->place(a->x, a->y, a->z);
    a->elem->set_angle(0, (short)(a->heading & 0x3fff), 0);
}

Actor3D* World3D::actorFor(SceneObject* o)
{
    for (size_t i = 0; i < actors_.size(); i++) if (actors_[i]->obj == o) return actors_[i];
    return 0;
}

std::string World3D::animPath(Actor3D* a, const char* name)
{
    return dir_ + (a->obj->kind == SceneObject::Char ? "/models/people/" : "/models/items/") + a->name + "_" + lower(name) + ".grn";
}

static bool fileCI(const std::string& path)        // Windows file names: any letter case
{
    struct stat st;
    if (stat(path.c_str(), &st) == 0) return true;
    size_t s = path.rfind('/');
    std::string dir = path.substr(0, s), base = path.substr(s + 1);
    DIR* d = opendir(dir.c_str());
    bool found = false;
    struct dirent* e;
    while (d && (e = readdir(d)) != NULL) if (strcasecmp(e->d_name, base.c_str()) == 0) { found = true; break; }
    if (d) closedir(d);
    return found;
}

bool World3D::exists(Actor3D* a, const char* name) { return fileCI(animPath(a, name)); }

void World3D::setAnim(Actor3D* a, const char* name, int loops, float speed, Actor3D::Mode m)
{
    a->mode = m;
    a->anim = name;
    a->model->play(animPath(a, name), speed, loops, 0.15f);
}

void World3D::finished(Actor3D* a)
{
    if (a->waited) { a->waited = false; wake_ = true; }
}

bool World3D::playAnim(SceneObject* o, const char* name, bool wait, double speed)
{
    Actor3D* a = actorFor(o);
    if (!a || !exists(a, name)) return false;
    setAnim(a, name, 1, (float)(speed > 0 ? speed : 1.0), Actor3D::OneShot);
    if (wait) a->waited = true;
    return wait;
}

void World3D::loopAnim(SceneObject* o, const char* name)
{
    Actor3D* a = actorFor(o);
    if (a && exists(a, name)) setAnim(a, name, 9999, 1.0f, Actor3D::Loop);
}

void World3D::oneShotAnim(SceneObject* o, const char* name)
{
    Actor3D* a = actorFor(o);
    if (a && exists(a, name)) setAnim(a, name, 1, 1.0f, Actor3D::OneShot);
}

void World3D::talk(SceneObject* o, bool on)
{
    Actor3D* a = actorFor(o);
    if (!a) return;
    if (on) { if (a->mode != Actor3D::Talk && exists(a, "talk")) setAnim(a, "talk", 9999, 1.0f, Actor3D::Talk); }
    else if (a->mode == Actor3D::Talk) setAnim(a, "idle1", 9999, 1.0f, Actor3D::Idle);
}

void World3D::talkAll(bool on)
{
    for (size_t i = 0; i < actors_.size(); i++) if (actors_[i]->mode == Actor3D::Talk) talk(actors_[i]->obj, on);
}

// Facing of a walk: heading 0 looks along -z (towards the camera), a turn to the right (+x) is positive.
static int headingOf(float dx, float dz)
{
    double a = atan2((double)dx, (double)-dz);          // 0 at -z
    int h = (int)floor(a * 16384.0 / (2.0 * kPi) + 0.5);
    return h & 0x3fff;
}

bool World3D::walk(SceneObject* o, float x, float z, bool run)
{
    Actor3D* a = actorFor(o);
    if (!a) return false;
    float dx = x - a->x, dz = z - a->z;
    if (dx * dx + dz * dz < 1.0f) return false;
    a->walking = true; a->tx = x; a->tz = z; a->speed = run ? kRunSpeed : kWalkSpeed;
    a->heading = headingOf(dx, dz);
    a->rotating = false;
    if (exists(a, run ? "run" : "walk")) setAnim(a, run ? "run" : "walk", 9999, 1.0f, Actor3D::Loop);
    else if (exists(a, "walk")) setAnim(a, "walk", 9999, 1.0f, Actor3D::Loop);
    a->waited = true;
    return true;
}

void World3D::stopWalk(SceneObject* o)
{
    Actor3D* a = actorFor(o);
    if (!a || !a->walking) return;
    a->walking = false; a->waited = false;
    setAnim(a, "idle1", 9999, 1.0f, Actor3D::Idle);
}

void World3D::moveTo(SceneObject* o, float x, float z)
{
    Actor3D* a = actorFor(o);
    if (!a) return;
    a->x = x; a->z = z; a->walking = false;
    place(a);
}

bool World3D::rotate(SceneObject* o, int angle, int ms)
{
    Actor3D* a = actorFor(o);
    if (!a) return false;
    angle &= 0x3fff;
    if (a->heading == angle) return false;
    a->rotating = true; a->rotTarget = angle; a->rotMs = a->rotLeft = (float)(ms > 0 ? ms : 1);
    a->waited = true;
    return true;
}

void World3D::setHidden(SceneObject* o, bool hide)
{
    Actor3D* a = actorFor(o);
    if (!a) return;
    if (hide) { if (a->elem->get_hide_count() == 0) a->elem->hide(true); }
    else while (a->elem->get_hide_count() > 0) a->elem->hide(false);
}

void World3D::refreshVisibility()
{
    for (size_t i = 0; i < actors_.size(); i++) {
        SceneObject* o = actors_[i]->obj;
        if (o->visibleCond.empty()) continue;
        setHidden(o, !game_->cond(o->visibleCond.c_str()));
    }
}

void World3D::enter(int startPoint)
{
    if (startPoint < 0 || startPoint > 9) startPoint = 0;
    const sb::scene::StartPoint& sp = scene_->data().startPoints[startPoint];
    if (sp.x == 0 && sp.y == 0 && sp.z == 0) return;       // the CHAR record's own position (c1l0s1: all start points are zero)
    for (size_t i = 0; i < actors_.size(); i++) {
        Actor3D* a = actors_[i];
        if (a->obj->kind != SceneObject::Char || !a->obj->userCtl) continue;
        a->x = sp.x; a->y = sp.y; a->z = sp.z;
        a->heading = sp.heading & 0x3fff;
        place(a);
        game_->log("3D: %s placed at start point %d: %g %g %g heading %d\n", a->name.c_str(), startPoint + 1, a->x, a->y, a->z, a->heading);
        break;
    }
}

void World3D::update(float dtMs)
{
    if (!w3_) return;
    float dt = dtMs * 0.001f;
    OMedia3DGrannyModel::advance_time(dt);
    for (size_t i = 0; i < actors_.size(); i++) {
        Actor3D* a = actors_[i];
        bool moved = false;
        if (a->walking) {
            float dx = a->tx - a->x, dz = a->tz - a->z, d = sqrtf(dx * dx + dz * dz), step = a->speed * dt;
            if (d <= step) {
                a->x = a->tx; a->z = a->tz; a->walking = false;
                setAnim(a, "idle1", 9999, 1.0f, Actor3D::Idle);
                finished(a);
            } else { a->x += dx / d * step; a->z += dz / d * step; }
            moved = true;
        }
        if (a->rotating) {
            a->rotLeft -= dtMs;
            int diff = ((a->rotTarget - a->heading) & 0x3fff);
            if (diff > 0x2000) diff -= 0x4000;
            if (a->rotLeft <= 0) { a->heading = a->rotTarget; a->rotating = false; finished(a); }
            else a->heading = (a->heading + (int)((float)diff * (dtMs / (a->rotLeft + dtMs)))) & 0x3fff;
            moved = true;
        }
        if (moved) place(a);
        if (a->mode == Actor3D::OneShot && a->model->animation_expired()) {
            setAnim(a, "idle1", 9999, 1.0f, Actor3D::Idle);
            finished(a);
        }
        a->model->update_pose();
    }
    w3_->update();
    if (wake_) { wake_ = false; game_->host()->actorDone(); }
}

bool World3D::screenBounds(SceneObject* o, int& x1, int& y1, int& x2, int& y2)
{
    Actor3D* a = actorFor(o);
    OMedia3DVPModel* m = vp_ ? dynamic_cast<OMedia3DVPModel*>(vp_->get_vpmodel()) : 0;
    float l, t, r, b, z;
    if (!a || !m || a->elem->get_hide_count() != 0 || !m->get_element_bounds(a->elem, l, t, r, b, z)) return false;
    x1 = (int)l; y1 = (int)t; x2 = (int)r; y2 = (int)b;
    return true;
}

// scene vt+0xc4 (0x43a310), the passes over the 3D objects: the nearest clickable character / item under the pointer
SceneObject* World3D::pick(int x, int y)
{
    OMedia3DVPModel* m = vp_ ? dynamic_cast<OMedia3DVPModel*>(vp_->get_vpmodel()) : 0;
    if (!m) return 0;
    SceneObject* best = 0;
    float bz = 0;
    for (size_t i = 0; i < actors_.size(); i++) {
        Actor3D* a = actors_[i];
        if (!a->obj->clickable || a->elem->get_hide_count() != 0) continue;
        float l, t, r, b, z;
        if (!m->get_element_bounds(a->elem, l, t, r, b, z)) continue;
        if (x < l || x > r || y < t || y > b) continue;
        if (!best || z < bz) { best = a->obj; bz = z; }
    }
    return best;
}

}  // namespace sbgame
