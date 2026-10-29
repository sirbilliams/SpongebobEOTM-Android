#include "World3D.h"
#include "Game.h"
#include "SceneScriptHost.h"
#include "OMedia3DRasterizerDesc.h"
#include "OMediaDisplay.h"
#include "OMediaViewPort.h"
#include <dirent.h>
#include <algorithm>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

namespace sbgame {

static const float kWalkSpeed = 150.0f;      // 0x42f1a0: 0.15 world units per elapsed millisecond
static const float kRunSpeed = 300.0f;

Actor3D::Actor3D()
    : obj(0), model(0), elem(0), mode(Idle), x(0), y(0), z(0), heading(0), walking(false), tx(0), tz(0), speed(0), rotating(false),
      rotTarget(0), rotTolerance(0), idling(false), idleLeft(0), waited(false), scheduled(false)
{
    waypoint = 0; interaction = 0; radius = 50; control = true; useGrid = false;
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
    game->state().hasPlayer = false;
    // Match sb2 startup (also StartupApp): zero disables OMT database eviction.
    // The direct scene-test entry bypasses StartupApp; Granny material cookies retain these objects.
    OMediaDataBase::set_cache_size(0);
    dir_ = game->root();
    grids_[0] = scene->data().grids[0]; grids_[1] = scene->data().grids[1];
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
        if (rec && rec->tag == sb::scene::CharacterTag) { a->radius = rec->data.character.o023c; a->useGrid = rec->data.character.o0280 != 0; }
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

// OMedia3DVector::angles, then sb2 0x434650: integer degrees, subtract 90.
static int headingOf(float dx, float dz)
{
    OMedia3DVector direction(dx,0,dz);
    short pitch, yaw; direction.angles(pitch,yaw);
    int d = ((int)yaw * 360 >> 14) - 90;
    if (d < 0) d += 360;
    return (d * 16384 / 360) & 0x3fff;
}

const sb::scene::Grid* World3D::grid(float y) const
{
    for (int i = 0; i < 2; ++i) {
        const sb::scene::Grid& g = grids_[i];
        if (g.present && g.o0014 == y) return &g;
    }
    return 0;
}

// 0x4183d0: a boundary belongs to the cell on its lower side.
static int gridIndex(float v, float origin, float size, float half)
{
    if (size <= 0 || !isfinite(v)) return -1;
    float edge = origin - half * size;
    for (int i = 0; i < 30; ++i) {
        if (v <= edge) return i - 1;
        edge += size;
    }
    return v <= edge ? 29 : -1;
}

// 0x418460 accepts fractional cell coordinates; scripts pass them unchanged (zero based).
static void gridPoint(const sb::scene::Grid& g, float gx, float gz, float& x, float& z)
{
    x = g.o000c + (gx - g.halfSize) * g.o0008;
    z = g.o0010 + (gz - g.halfSize) * g.o0008;
}

void World3D::blockCell(int x, int z, bool blocked)
{
    if (x < 0 || z < 0 || x >= 30 || z >= 30) return;
    float y = 0;
    for (size_t i = 0; i < actors_.size(); ++i) if (actors_[i]->obj->userCtl) { y=actors_[i]->y; break; }
    for (int i=0; i<2; ++i) if (grids_[i].present && grids_[i].o0014 == y) {
        grids_[i].cells[x][z] = blocked ? 1 : 0; break;
    }
}

bool World3D::cell(const sb::scene::Grid& g, int x, int z) const
{
    return x >= 0 && z >= 0 && x < (int)g.side && z < (int)g.side &&
        !g.cells[x][z] && !game_->state().isBlocked(scene_->sceneId(), x, z);
}

bool World3D::gridWalk(Actor3D* a, float x, float z, int gx, int gz, bool run)
{
    const sb::scene::Grid* g = grid(a->y);
    std::vector< std::pair<float,float> > route;
    if (g && a->useGrid && (!game_->runner() || game_->runner()->terrain())) {
        if (gx < 0 || gz < 0) {
            gx = gridIndex(x, g->o000c, g->o0008, g->halfSize);
            gz = gridIndex(z, g->o0010, g->o0008, g->halfSize);
        }
        int sx = gridIndex(a->x, g->o000c, g->o0008, g->halfSize);
        int sz = gridIndex(a->z, g->o0010, g->o0008, g->halfSize);
        if (!cell(*g, gx, gz) || sx < 0 || sz < 0 || sx >= 30 || sz >= 30) return false;
        // 0x42af50: eight neighbours, cardinal cost 64, diagonal 90.
        // port: bounded A* storage replaces the exe's pointer-linked open list.
        const int dx[8] = {0,1,0,-1,1,1,-1,-1};
        const int dz[8] = {-1,0,1,0,-1,1,1,-1};
        int cost[900], parent[900], order[900]; bool closed[900];
        int serial = 1;
        for (int i = 0; i < 900; ++i) { cost[i] = INT_MAX; parent[i] = -1; closed[i] = false; order[i] = INT_MAX; }
        // 0x439e20/0x4181e0: other visible characters occupy their centre and four +/-20 corners.
        bool occupied[900]; memset(occupied,0,sizeof occupied);
        const float ox[5] = {0,20,20,-20,-20}, oz[5] = {0,20,-20,20,-20};
        for (size_t i = 0; i < actors_.size(); ++i) {
            Actor3D* other = actors_[i];
            if (other == a || other->obj->kind != SceneObject::Char || other->y != a->y || other->elem->get_hide_count()) continue;
            for (int k=0;k<5;++k) {
                int cx=gridIndex(other->x+ox[k],g->o000c,g->o0008,g->halfSize);
                int cz=gridIndex(other->z+oz[k],g->o0010,g->o0008,g->halfSize);
                if(cx>=0 && cx<30 && cz>=0 && cz<30) occupied[cx*30+cz]=true;
            }
        }
        int source = sx * 30 + sz, target = gx * 30 + gz;
        cost[source] = 0; order[source] = 0;
        for (int n = 0; n < 900; ++n) {
            int best = -1;
            for (int i = 0; i < 900; ++i)
                if (!closed[i] && cost[i] != INT_MAX) {
                    int hx = i/30-gx, hz = i%30-gz;
                    int score = cost[i] + (int)(sqrt((double)(hx*hx+hz*hz))*64);
                    int bx = best < 0 ? 0 : best/30-gx, bz = best < 0 ? 0 : best%30-gz;
                    int prev = best < 0 ? INT_MAX : cost[best] + (int)(sqrt((double)(bx*bx+bz*bz))*64);
                    if (best < 0 || score < prev || (score == prev && order[i] < order[best])) best = i;
                }
            if (best < 0 || best == target) break;
            closed[best] = true;
            for (int d = 0; d < 8; ++d) {
                int nx = best / 30 + dx[d], nz = best % 30 + dz[d];
                if (!cell(*g, nx, nz) || occupied[nx*30+nz]) continue;
                int next = nx * 30 + nz, c = cost[best] + (d < 4 ? 64 : 90);
                if (c < cost[next]) { cost[next] = c; parent[next] = best; if (order[next] == INT_MAX) order[next] = serial++; }
            }
        }
        if (cost[target] == INT_MAX) return false;
        for (int i = target; i != source; i = parent[i]) {
            float px, pz; gridPoint(*g, i / 30 + 0.5f, i % 30 + 0.5f, px, pz);
            route.push_back(std::make_pair(px, pz));
        }
        std::reverse(route.begin(), route.end());
        // The exe walks through cell centres after route search.
        if (route.empty()) return false;
    } else {
        if (gx >= 0 && gz >= 0) { if (!g) return false; gridPoint(*g,(float)gx,(float)gz,x,z); }
        if ((x-a->x)*(x-a->x)+(z-a->z)*(z-a->z) < 1) return false;
        route.push_back(std::make_pair(x,z));
    }
    a->route = route; a->waypoint = 0; a->interaction = 0;
    a->walking = true; a->tx = route[0].first; a->tz = route[0].second;
    a->speed = run ? kRunSpeed : kWalkSpeed;
    a->rotating = false;
    int targetHeading = headingOf(a->tx - a->x, a->tz - a->z);
    int diff = ((targetHeading - a->heading + 8192) & 16383) - 8192;
    if (abs(diff) >= 500 && exists(a, "rotate")) {
        a->rotating = true; a->rotTarget = targetHeading; a->rotTolerance = 500;
        setAnim(a, "rotate", 9999, 1, Actor3D::Loop);
    } else {
        a->heading = targetHeading;
        setAnim(a, run && exists(a,"run") ? "run" : "walk", 9999, 1, Actor3D::Loop);
    }
    a->waited = true;
    game_->log("walk %s: %u waypoints, destination %g,%g\n", a->name.c_str(), (unsigned)route.size(), a->route.back().first, a->route.back().second);
    return true;
}

bool World3D::walk(SceneObject* o, float x, float z, bool run)
{
    Actor3D* a = actorFor(o);
    return a && gridWalk(a, x, z, -1, -1, run);
}

bool World3D::walkGrid(SceneObject* o, int gx, int gz)
{
    Actor3D* a = actorFor(o);
    return a && gridWalk(a, 0, 0, gx, gz, false);
}

void World3D::moveGrid(SceneObject* o, int gx, int gz)
{
    Actor3D* a = actorFor(o);
    const sb::scene::Grid* g = a ? grid(a->y) : 0;
    if (!g || gx < 0 || gz < 0 || gx >= (int)g->side || gz >= (int)g->side) return;
    float x,z; gridPoint(*g, (float)gx, (float)gz, x,z); moveTo(o,x,z);
}

void World3D::setRunning(SceneObject* o)
{
    Actor3D* a = actorFor(o);
    if (!a || !a->walking) return;
    a->speed = kRunSpeed;
    if (!a->rotating && exists(a,"run")) setAnim(a,"run",9999,1,Actor3D::Loop);
}

bool World3D::clickFloor(int x, int y)
{
    Actor3D* a = 0;
    for (size_t i = 0; i < actors_.size(); ++i)
        if (actors_[i]->obj->userCtl && actors_[i]->control) { a = actors_[i]; break; }
    if (!a) return false;
    const sb::scene::Grid* g = grid(a->y);
    if (!g) return false;
    const sb::scene::Camera& c = scene_->data().camera.data.camera;
    if (c.o0188 <= 0) return false;
    OMedia3DVector ray((float)x - 320, 240 - (float)y, c.o0188);
    // Inverse of the renderer's world -> view transform; input already mapped to logical pixels.
    ray.rotate((short)(c.o0180*16384/360), (short)(c.o0182*16384/360), (short)(c.o0184*16384/360));
    if (fabs(ray.y) < 0.00001) return false;
    float t = (g->o0014 - c.o0178) / ray.y;
    if (t <= 0) return false;
    bool result = walk(a->obj, c.o0174 + t*ray.x, c.o017c + t*ray.z, false);
    if (result) a->waited = false;
    return result;
}

bool World3D::approach(SceneObject* target)
{
    if (target->gridX == -1 && target->gridY == -1) return false;
    for (size_t i = 0; i < actors_.size(); ++i) {
        Actor3D* a = actors_[i];
        if (!a->obj->userCtl || !a->control || a->obj == target) continue;
        bool started = false;
        if (target->gridX > 0 && target->gridY > 0)
            started = walkGrid(a->obj, target->gridX-1, target->gridY-1);
        else if (target->kind == SceneObject::Char || target->kind == SceneObject::Item)
            started = walk(a->obj, target->px, target->pz, false);
        if (started) { a->interaction = target; a->waited = false; return true; }
    }
    return false;
}

void World3D::finishInteraction(Actor3D* a)
{
    SceneObject* target = a->interaction;
    if (!target) return;
    if (target->kind == SceneObject::Char || target->kind == SceneObject::Item) {
        int heading = headingOf(target->px-a->x,target->pz-a->z);
        if (rotate(a->obj,heading,500)) { a->interaction = target; a->waited = false; return; }
    }
    a->interaction = 0;
    scene_->pickEvent(target,true);
}

void World3D::stopWalk(SceneObject* o)
{
    Actor3D* a = actorFor(o);
    if (!a) return;
    bool active = a->walking || a->rotating;
    a->rotating = false;
    a->walking = false; a->waited = false; a->interaction = 0; a->route.clear();
    if (!active) return;
    setAnim(a, "idle1", 9999, 1.0f, Actor3D::Idle);
}

void World3D::moveTo(SceneObject* o, float x, float z)
{
    Actor3D* a = actorFor(o);
    if (!a) return;
    a->x = x; a->z = z;
    place(a);
}

bool World3D::rotate(SceneObject* o, int angle, int tolerance)
{
    Actor3D* a = actorFor(o);
    if (!a) return false;
    angle &= 0x3fff;
    int diff = ((angle - a->heading + 8192) & 16383) - 8192;
    if (!diff || abs(diff) < tolerance) return false;
    stopWalk(o);
    setAnim(a,"rotate",9999,1,Actor3D::Loop);
    a->rotating = true; a->rotTarget = angle; a->rotTolerance = (float)(tolerance > 0 ? tolerance : 1);
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
    GameState& st = game_->state();
    if (st.resumePos) {                                      // progress+0x64: a loaded save puts the player where he was
        st.resumePos = false;
        for (size_t i = 0; i < actors_.size(); i++) {
            Actor3D* a = actors_[i];
            if (a->obj->kind != SceneObject::Char || !a->obj->userCtl) continue;
            a->x = st.playerX; a->y = st.playerY; a->z = st.playerZ;
            a->heading = st.playerFacing & 0x3fff;
            place(a);
            game_->log("3D: %s restored from save: %g %g %g heading %d\n", a->name.c_str(), a->x, a->y, a->z, a->heading);
            return;
        }
    }
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
    for (size_t i = 0; i < actors_.size(); i++) {            // the save writer (0x416ed0) reads the controlled character's position
        Actor3D* a = actors_[i];
        if (a->obj->kind != SceneObject::Char || !a->obj->userCtl) continue;
        GameState& st = game_->state();
        st.hasPlayer = true; st.playerX = a->x; st.playerY = a->y; st.playerZ = a->z; st.playerFacing = (short)a->heading;
        break;
    }
    for (size_t i = 0; i < actors_.size(); i++) {
        Actor3D* a = actors_[i];
        bool moved = false;
        if (a->walking && !a->rotating) {
            float dx = a->tx - a->x, dz = a->tz - a->z, d = sqrtf(dx * dx + dz * dz), step = a->speed * dt;
            float tolerance = a->radius * 0.5f; if (tolerance < 25) tolerance = 25;
            if (d <= step || d <= tolerance) {
                if (d <= step) { a->x = a->tx; a->z = a->tz; }
                if (++a->waypoint < a->route.size()) {
                    a->tx = a->route[a->waypoint].first; a->tz = a->route[a->waypoint].second;
                    a->heading = headingOf(a->tx-a->x, a->tz-a->z);
                } else {
                    a->walking = false;
                    setAnim(a, "idle1", 9999, 1.0f, Actor3D::Idle);
                    game_->log("walk stopped %s at %g,%g heading %d: idle1\n", a->name.c_str(),a->x,a->z,a->heading);
                    finished(a);
                    finishInteraction(a);
                }
            } else { a->x += dx / d * step; a->z += dz / d * step; }
            moved = true;
        }
        if (a->rotating) {

            int diff = ((a->rotTarget - a->heading) & 0x3fff);
            if (diff > 0x2000) diff -= 0x4000;
            // 0x42ee80: eight increments of elapsed-ms angle units (sixteen when running).
            int step = (int)(dtMs * (a->speed == kRunSpeed && a->walking ? 16 : 8));
            if (abs(diff) <= step || abs(diff) < a->rotTolerance) {
                a->heading = a->rotTarget; a->rotating = false;
                if (a->walking) setAnim(a, a->speed == kRunSpeed && exists(a,"run") ? "run" : "walk",9999,1,Actor3D::Loop);
                else { setAnim(a,"idle1",9999,1,Actor3D::Idle); finished(a); finishInteraction(a); }
            } else a->heading = (a->heading + (diff < 0 ? -step : step)) & 0x3fff;
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
