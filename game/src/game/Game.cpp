#include "Game.h"
#include "DialogueUI.h"
#include "SceneScriptHost.h"
#include "World3D.h"
#include "Hud.h"        // port: hud
#include "Cursor.h"     // port: cursor
#include "../port/CompanionFeed.h"
#include "OMediaFilePath.h"
#include "OMediaSoundManager.h"
#include "OMediaEventManager.h"
#include "OMediaBackground.h"
#include "OMediaRGBColor.h"
#include "OMediaCanvas.h"
#include "OMediaError.h"
#include <exception>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

namespace sbgame {

// ---------------------------------------------------------------------------------------- OmtFile
void OmtFile::reset()
{
    delete db; delete stream; delete path;
    db = 0; stream = 0; path = 0;
}

OmtFile::~OmtFile() { reset(); }

static bool fileExists(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

bool OmtFile::open(const std::string& file)
{
    if (!fileExists(file)) return false;
    try {
        path = new OMediaFilePath(file);
        stream = new OMediaFileStream;
        stream->setpath(path);
        stream->open(omcfp_Read);
        db = new OMediaDataBase(stream);
    } catch (OMediaError&) {
        return false;
    }
    return true;
}

// ------------------------------------------------------------------------------------------- Game
Game::Game()
    : blending(true), optText(1), optShadows(1), movieHook(0), movieHookData(0), suspended(false), inputConsumed(false), letterboxIn(false), logFile(stdout), world_(0), vp_(0), retail_(0), font_(0), w3_(0), scene_(0), pending_(0),
      pendingId_(0), pendingStart_(0), runner_(0), host_(0), hoverCap_(0), commentCap_(0), commentMs_(0), mx_(320), my_(240),
      inputAttached_(false), commentBlock_(0), commentVoice_(false), hud_(0), cursor_(0)
{
}

Game::~Game()
{
    if (inputAttached_ && OMediaEventManager::get_event_manager()) {
        OMediaEventManager::get_event_manager()->get_mouse_broadcaster()->removelistener(this);
        OMediaEventManager::get_event_manager()->get_key_broadcaster()->removelistener(this);
    }
    delete runner_;
    runner_ = 0;
    delete commentBlock_;
    delete hoverCap_;
    delete commentCap_;
    delete cursor_;     // port: cursor
    delete hud_;        // port: hud
    delete w3_;
    w3_ = 0;
    delete scene_;
    scene_ = 0;
    delete host_;
    delete vp_;
    delete world_;
    delete font_;
    delete retail_;
}

void Game::log(const char* fmt, ...)
{
    if (!logFile) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(logFile, fmt, ap);
    va_end(ap);
}

bool Game::init(const std::string& root, OMediaCanvas* canvas)
{
    root_ = root;
    omd_REGISTERCLASS(OMediaCanvas);
    omd_REGISTERCLASS(OMediaSound);
    if (!sb2_.open(root + "/sb2.omt")) { log("cannot open sb2.omt\n"); return false; }
    if (!fontDb_.open(root + "/Font.omt")) { log("cannot open Font.omt\n"); return false; }
    try { retail_ = new RetailData(root + "/sb2.exe"); } catch (std::exception&) { log("cannot read sb2.exe\n"); return false; }
    font_ = new GameFont(fontDb_.db, *retail_);          // lane C's font class (0x414820..0x414fe0)
    world_ = new OMedia2DWorld;
    world_->set_sort_mode(omc2dsort_Z);             // 0x442010 (the exe's world class): [world+0x88] = 3, dirty; elements are drawn by z
    world_->set_framerate_mode(omfrc_Max);        // port: the headless test draws at every update
    OMediaRGBColor black(0, 0, 0);
    ((OMediaColorBackground*)world_->getbackground())->setcolor(&black);
    vp_ = new OMediaViewPort(world_);
    OMediaRect b(0, 0, canvas->getwidth(), canvas->getheight());
    vp_->setbounds(&b);
    vp_->setcanvas(canvas);
    OMediaSoundManager* sm = OMediaSoundManager::getdefaultmanager();
    if (!dynamic_cast<OMediaDXSoundManager*>(sm)) {
        OMediaSoundManager::setdefaultmanager(new OMediaDXSoundManager);
        sm = OMediaSoundManager::getdefaultmanager();
    }
    sm->set_nchannels(0xf);
    host_ = new SceneScriptHost(this);
    state_.chapter = 1;
    itemsBank_.open(root + "/sounds/items.omt");
    hud_ = new Hud(this);         // port: hud (the exe's HUD object 0x457334)
    cursor_ = new Cursor(this);   // port: cursor
    return true;
}

void Game::attachInput()
{
    OMediaEventManager* em = OMediaEventManager::get_event_manager();
    if (!em || inputAttached_) return;
    em->get_mouse_broadcaster()->addlistener(this);
    em->get_key_broadcaster()->addlistener(this);
    inputAttached_ = true;
}

void Game::listen_to_message(omt_Message msg, void* param)
{
    if (suspended) return;      // port (saves): the menu is up
    if (msg == omsg_MouseClick) {
        OMediaEvent* ev = (OMediaEvent*)param;
        mouseDown(ev->x, ev->y);
    } else if (msg == omsg_MouseClick_Up) {                      // port: hud (0x420b20 runs on the release)
        OMediaEvent* ev = (OMediaEvent*)param;
        if (hud_) hud_->onMouseUp(ev->x, ev->y);
    } else if (msg == omsg_KeyDown) {
        OMediaEvent* ev = (OMediaEvent*)param;
        if (hud_ && (ev->key == 27 || ev->character == 27) && hud_->onEscape()) return;   // port: hud
        if (scene_ && scene_->dialogue()) scene_->dialogue()->onKey(ev->key ? ev->key : (int)ev->character);
    }
}

void Game::mouseDown(int x, int y)
{
    mx_ = x; my_ = y;
    if (!scene_) return;
    if (scene_->dialogue() && scene_->dialogue()->busy()) { scene_->dialogue()->onClick(x, y); return; }
    if (hud_ && hud_->onMouseDown(x, y)) return;                // port: hud (0x420d50)
    if (runner_) runner_->onClick((short)x, (short)y);        // 0x43dea0
    scene_->click(x, y);
}

void Game::mouseMove(int x, int y)
{
    mx_ = x; my_ = y;
    if (hud_) hud_->onMove(x, y);                               // port: hud
    if (scene_ && scene_->dialogue() && scene_->dialogue()->busy()) scene_->dialogue()->onMove(x, y);
}

// ------------------------------------------------------------------------- scene requests (0x40ab60)
static int atoi2(const char* p)   // 0x442f06: atoi of the one character at p
{
    char b[2] = { p[0], 0 };
    return atoi(b);
}

void Game::requestScene(const char* name)
{
    if (pendingId_ != 0) return;                      // 0x40ab60: ignored while a switch is pending
    log("Switch to scene %s\n", name);
    if (strlen(name) > 5) {
        int c = atoi2(name + 1), l = atoi2(name + 3), s = atoi2(name + 5);
        pendingId_ = (unsigned)((s * 0x100 + l) * 0x100 + c);
        if (toupper((unsigned char)name[6]) == 'P') {
            int p = atoi(name + 7);
            pendingStart_ = p > 0 ? p - 1 : p;
        } else pendingStart_ = 0;
    }
}

void Game::exitScene(int dir)       // 0x40ac20 (the "M..." exit names would start a movie: switchTo handles both)
{
    if (!scene_ || pendingId_) return;
    std::string e = scene_->exit(dir);
    if (e.empty()) return;
    switchTo(e.c_str());
}

void Game::loadSkies(int chapter)       // chapter loader 0x40b090: sceneart\c%dskies.omt into game+0x64, sky cache cleared
{
    if (chapter == state_.chapter && skies_.db) return;
    skies_.reset();
    for (int i = 0; i < 10; i++) skyCache_[i] = 0;
    char b[96];
    sprintf(b, "%s/sceneart/c%dskies.omt", root_.c_str(), chapter);
    skies_.open(b);
}

OMediaCanvas* Game::skyCanvas(int index)
{
    if (index < 0 || index > 9 || !skies_.db) return 0;
    if (!skyCache_[index]) {
        char n[16];
        sprintf(n, "sky0%d", index);
        skyCache_[index] = (OMediaCanvas*)skies_.db->get_object(OMediaCanvas::db_type, n);
    }
    return skyCache_[index];
}

void Game::loadSceneBanks(unsigned id)
{
    char b[96];
    sceneBank_.reset();
    sprintf(b, "%s/sounds/c%dl%ds%d.omt", root_.c_str(), (int)(id & 0xf), (int)((id >> 8) & 0xf), (int)((id >> 16) & 0xf));
    sceneBank_.open(b);                                     // game vt+0x64 -> game+0x5c
    if ((int)(id & 0xf) != state_.chapter || !sharedBank_.db) {
        sharedBank_.reset();
        sprintf(b, "%s/sounds/c%d.omt", root_.c_str(), (int)(id & 0xf));
        sharedBank_.open(b);                                // game+0x68
        commentBank_.reset();
        sprintf(b, "%s/sounds/comments%d.omt", root_.c_str(), (int)(id & 0xf));
        commentBank_.open(b);
    }
}

void Game::doSwitch()       // DoSwitch 0x40b720 (the part that matters without a 3D world)
{
    if (!pendingId_) return;
    if (scene_ && scene_->barState() != 0) return;
    unsigned id = pendingId_;
    int start = pendingStart_;
    pendingId_ = 0;
    delete runner_; runner_ = 0;                // the runner belongs to the scene (scene+0x1c7c)
    host_->resetForScene();
    delete w3_;
    w3_ = 0;
    delete scene_;
    scene_ = 0;
    int chapter = (int)(id & 0xf);
    loadSkies(chapter);
    GameScene* s = new GameScene(this);
    state_.sceneId = id;
    if (!s->load(id)) { log("Couldn't find scene in DoSwitch ()\n"); delete s; return; }
    scene_ = s;
    w3_ = new World3D(this, s);                 // 0x4033f0: the scene's 3D world and characters
    if (!w3_->ok() || getenv("SB_NO3D")) { delete w3_; w3_ = 0; }
    state_.chapter = chapter;
    loadSceneBanks(id);
    std::string err;
    comments_.load(*host_, chapter, (int)((id >> 8) & 0xf), &err);
    help_.load(*host_, chapter, &err);
    host_->sceneLoaded();
    scene_->enter(start);
}

// ------------------------------------------------------------------------ the event classifier
void Game::switchTo(const char* name)       // game vt+0xb0, 0x40a230
{
    if (!name || !*name) return;
    std::string n = name;
    log("switch '%s'\n", name);
    if (n == "unlock") host_->uiUnlock();
    else if (n == "skip") host_->gameSkip();
    else if (toupper((unsigned char)n[0]) == 'H') {
        showComment(name);                       // comment caption; the actor plays "talk" (3D)
        host_->commentTalk();
    } else if (n.size() >= 5 && toupper((unsigned char)n[0]) == 'C' && toupper((unsigned char)n[2]) == 'L' &&
               toupper((unsigned char)n[4]) == 'S') {
        requestScene(name);
    } else if (toupper((unsigned char)n[0]) == 'M') {
        if (n == "movies") { movieLog.push_back("stop"); }
        else {
            // the first word is the movie, the second what to go to afterwards (game+0x186)
            size_t sp = n.find(' ');
            std::string mv = sp == std::string::npos ? n : n.substr(0, sp);
            movieLog.push_back(mv);
            log("movie %s\n", mv.c_str());
            chained = sp == std::string::npos ? std::string() : n.substr(sp + 1);
            // port: the Bink player is the startup's; without one the movie "ends" at once
            if (!movieHook) {
                if (!chained.empty()) { std::string c = chained; chained.clear(); switchTo(c.c_str()); }
            } else movieHook(mv.c_str(), movieHookData);
        }
    } else if (toupper((unsigned char)n[0]) == 'S') {
        if (runner_) return;                    // the scene already runs a script: refused (no 0x24e)
        runScript(name);
    } else {
        // scene vt+0xb4 (0x4397c0): a dialogue key (8 chars: the scene prefix is cut)
        bool ok = false;
        if (scene_ && scene_->dialogue()) {
            const char* key = n.size() == 8 ? name + 3 : name;
            ok = scene_->dialogue()->speak(key);
        }
        if (ok) host_->dialogueStarted();
        else switchTo("H000F");
    }
    inputConsumed = true;                       // game+0x24e
}

void Game::runScript(const char* name)
{
    runner_ = new sbscript::ScriptRunner(host_);
    host_->scriptRunnerCreated();
    bool ok = runner_->load(name, 0);           // 0x43dae0, runs to the first wait
    if (!ok) log("script %s: no file\n", name);
    if (!ok || runner_->finished()) {
        delete runner_; runner_ = 0;
    } else if (!runner_->keepControl()) {
        host_->controlLostToScript(name);       // the actor loses control (char+0x32a = 0)
    }
}

void Game::forceRedraw() { if (vp_) { OMediaRect r(0, 0, 640, 480); vp_->setbounds(&r); } }

void Game::wake()       // scene vt+0xe0 (0x43aa80)
{
    if (!runner_) return;
    sbscript::ScriptRunner* r = runner_;
    r->runUntilWait();
    if (runner_ == r && r->finished()) {
        delete r;
        runner_ = 0;
    }
}

// ------------------------------------------------------------------------------------- comments
void Game::showComment(const char* id)      // game+0xc4 vt+0x14
{
    const sbscript::CommentEntry* e = 0;     // 0x410a60 compares case-insensitively ('h126b' finds H126B)
    for (size_t i = 0; i < comments_.entries().size() && !e; ++i)
        if (strcasecmp(comments_.entries()[i].id.c_str(), id) == 0) e = &comments_.entries()[i];
    delete commentBlock_;
    commentBlock_ = 0;
    commentMs_ = 0;
    if (!e) { log("comment '%s' not found\n", id); return; }
    log("comment %s: %s\n", id, e->text.c_str());
    commentBlock_ = new TextBlock;
    commentBlock_->show(this, 5, 440, 10100, false, e->text, 630);
    OMediaSound* s = 0;
    s = soundCI(commentBank_.db, sbscript::CommentTable::voiceName(id));
    if (s && playOnChannel(8, s, 0)) { commentVoice_ = true; commentMs_ = 100; }
    else { commentVoice_ = false; commentMs_ = 1500.0f + 50.0f * (float)e->text.size(); }
}

void Game::updateComment(float dt)
{
    if (commentMs_ <= 0) return;
    bool done;
    if (commentVoice_) done = !channelBusy(8);
    else { commentMs_ -= dt; done = commentMs_ <= 0; }
    if (!done) return;
    commentMs_ = 0;
    delete commentBlock_;
    commentBlock_ = 0;
    if (runner_ && runner_->waitingComment()) wake();       // 0x439890
}

// --------------------------------------------------------------------------------------- sounds
// Wave by name in a bank: exact, then lower case, then upper case (the bank holds e.g. "121aab")
OMediaSound* Game::soundCI(OMediaDataBase* db, const std::string& name)
{
    if (!db) return 0;
    std::string v[3] = { name, name, name };
    for (size_t i = 0; i < name.size(); ++i) { v[1][i] = (char)tolower((unsigned char)name[i]); v[2][i] = (char)toupper((unsigned char)name[i]); }
    for (int k = 0; k < 3; ++k)
        if (db->object_exists(OMediaSound::db_type, v[k])) return (OMediaSound*)db->get_object(OMediaSound::db_type, v[k]);
    return 0;
}

OMediaSound* Game::findSound(const char* name, bool* shared)
{
    OMediaSound* s = 0;
    if (shared) *shared = false;
    s = soundCI(sceneBank_.db, name);
    if (s) return s;
    if (shared) *shared = true;
    s = soundCI(sharedBank_.db, name);
    if (!s) s = soundCI(itemsBank_.db, name);
    return s;
}

bool Game::playOnChannel(int ch, OMediaSound* s, int pan)
{
    OMediaSoundManager* m = OMediaSoundManager::getdefaultmanager();
    if (!s || ch >= m->get_nchannels()) return false;
    m->get_channel(ch)->set_pan(pan < 0 ? -10000 : pan > 0 ? 10000 : 0);
    m->get_channel(ch)->stop();
    bool ok = m->play(ch, s, false, false);
    log("      sound: channel %d play '%s' %.2fs -> %d busy=%d\n", ch, "", s->get_sound_secs(), ok ? 1 : 0, channelBusy(ch) ? 1 : 0);
    return ok;
}

// 0x4256b0: the Options screen plays "ibt1" (Sound) / "H000L" (Voice) from the open banks, if the game has them
void Game::previewSound(bool voice)
{
    bool shared = false;
    OMediaSound* s = voice ? soundCI(commentBank_.db, "H000L") : findSound("ibt1", &shared);
    if (s) playOnChannel(voice ? 4 : 1, s, 0);
}

bool Game::channelBusy(int ch)
{
    OMediaSoundManager* m = OMediaSoundManager::getdefaultmanager();
    if (ch >= m->get_nchannels()) return false;
    return m->get_channel(ch)->get_playing_sound() != NULL;
}

void Game::stopChannel(int ch)
{
    OMediaSoundManager* m = OMediaSoundManager::getdefaultmanager();
    if (ch < m->get_nchannels()) m->get_channel(ch)->stop();
}

// ------------------------------------------------------------------------------------------ update
void Game::update(float dt)
{
    OMediaSoundManager* m = OMediaSoundManager::getdefaultmanager();
    for (int i = 0; i < m->get_nchannels(); ++i) m->get_channel(i)->spend_time();
    doSwitch();
    if (!scene_) { if (cursor_) cursor_->update(mx_, my_); return; }   // port: cursor
    scene_->update(dt);
    if (scene_->dialogue()) scene_->dialogue()->update(dt);
    host_->update(dt);
    if (w3_) w3_->update(dt);
    if (runner_) {
        if (runner_->idleElapsed(dt)) wake();                 // idle without an actor (0x43d650)
        else if (runner_ && runner_->soundDone()) wake();     // sound wait (0x43d6e0)
    }
    updateComment(dt);
    if (hud_) hud_->update(dt);        // port: hud
    sbport::companionTick(this, dt);   // port: companion
    // the hover description follows the cursor (game cursor object +0x88, vt+0x30)
    std::string h;
    if (!(scene_->dialogue() && scene_->dialogue()->busy()) && !runner_) {
        SceneObject* o = scene_->hover(mx_, my_);
        if (o) h = o->description;
    }
    if (hud_) { std::string hc; if (hud_->caption(&hc)) h = hc; }   // port: hud (0x4201c0: the HUD's caption wins)
    if (h != hoverText_) {
        hoverText_ = h;
        delete hoverCap_;
        hoverCap_ = 0;
        if (!h.empty()) {
            hoverCap_ = new OMediaCCaption;
            hoverCap_->set_font(font_);
            hoverCap_->set_string(h);
            hoverCap_->link(world_);
        }
    }
    if (hoverCap_) {
        long w, hh;
        hoverCap_->getsize(w, hh);
        int x = mx_ + 14, y = my_ + 16;
        if (x + w > 636) x = 636 - (int)w;
        if (y + hh > 476) y = my_ - 8 - (int)hh;
        hoverCap_->place((float)x, (float)y, 10200.0f);
    }
    if (cursor_) cursor_->update(mx_, my_);   // port: cursor
}

}  // namespace sbgame
