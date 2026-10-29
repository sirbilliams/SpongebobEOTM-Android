/*  port: the dual-screen companion snapshot and command queue.  See Companion.h and docs/companion.md.
    SB_COMPANION_DUMP=<file> (desktop): enables it and writes every snapshot to that file.  */
#include "Companion.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <deque>

namespace sbport {

namespace {
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
bool g_enabled = false, g_envChecked = false;
std::string g_json, g_dumpFile;
std::deque<std::string> g_cmds;
double g_last = -1;

double nowSec()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void esc(std::string& out, const std::string& s)
{
    out += '"';
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '"' || ch == '\\') { out += '\\'; out += (char)ch; }
        else if (ch == '\n') out += "\\n";
        else if (ch == '\t') out += ' ';
        else if (ch < 0x20) out += ' ';
        else if (ch >= 0x80) out += '?';       // the game's text is Latin-1 (not valid UTF-8): keep the JSON clean
        else out += (char)ch;
    }
    out += '"';
}

void arr(std::string& out, const std::vector<std::string>& v)
{
    out += '[';
    for (size_t i = 0; i < v.size(); ++i) { if (i) out += ','; esc(out, v[i]); }
    out += ']';
}

void checkEnv()
{
    if (g_envChecked) return;
    g_envChecked = true;
    const char* d = getenv("SB_COMPANION_DUMP");
    if (d && *d) { g_dumpFile = d; g_enabled = true; }
}
}  // namespace

std::string CompanionJsonOf(const CompanionState& s)
{
    std::string o = "{";
    o += s.inGame ? "\"game\":1" : "\"game\":0";
    o += ",\"chapter_title\":"; esc(o, s.chapterTitle);
    o += ",\"scene_title\":"; esc(o, s.sceneTitle);
    o += s.cutscene ? ",\"cut\":1" : ",\"cut\":0";
    o += ",\"hint\":"; esc(o, s.hint);
    o += ",\"caption\":"; esc(o, s.caption);
    o += ",\"sel\":"; esc(o, s.selected);
    o += ",\"items\":"; arr(o, s.items);
    o += ",\"visited\":"; arr(o, s.visited);
    if (s.dialogue) {
        o += s.choiceMode ? ",\"dlg\":{\"choice\":1" : ",\"dlg\":{\"choice\":0";
        o += ",\"speaker\":"; esc(o, s.speaker);
        o += ",\"text\":"; esc(o, s.text);
        o += ",\"choices\":"; arr(o, s.choices);
        o += '}';
    }
    o += '}';
    return o;
}

void CompanionSetEnabled(bool on)
{
    pthread_mutex_lock(&g_lock);
    g_enabled = on;
    if (!on) { g_json.clear(); g_cmds.clear(); }
    pthread_mutex_unlock(&g_lock);
}

bool CompanionEnabled()
{
    pthread_mutex_lock(&g_lock);
    checkEnv();
    bool on = g_enabled;
    pthread_mutex_unlock(&g_lock);
    return on;
}

void CompanionPublish(const CompanionState& s)
{
    double t = nowSec();
    if (g_last >= 0 && t - g_last < 0.1) return;      // 10 a second is plenty
    g_last = t;
    std::string j = CompanionJsonOf(s);
    pthread_mutex_lock(&g_lock);
    g_json = j;
    std::string dump = g_dumpFile;
    pthread_mutex_unlock(&g_lock);
    if (!dump.empty()) {
        FILE* f = fopen(dump.c_str(), "wb");
        if (f) { fwrite(j.data(), 1, j.size(), f); fclose(f); }
    }
}

std::string CompanionJson()
{
    pthread_mutex_lock(&g_lock);
    std::string j = g_json;
    pthread_mutex_unlock(&g_lock);
    return j;
}

void CompanionPush(const std::string& cmd)
{
    pthread_mutex_lock(&g_lock);
    if (g_enabled && g_cmds.size() < 32) g_cmds.push_back(cmd);
    pthread_mutex_unlock(&g_lock);
}

bool CompanionPop(std::string* cmd)
{
    pthread_mutex_lock(&g_lock);
    bool got = !g_cmds.empty();
    if (got) { *cmd = g_cmds.front(); g_cmds.pop_front(); }
    pthread_mutex_unlock(&g_lock);
    return got;
}

}  // namespace sbport

// --------------------------------------------------------------------------- JNI (Android)

#ifdef __ANDROID__
#include <jni.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_sirbilliams_spongebob_Companion_nativeSnapshot(JNIEnv* env, jclass)
{
    return env->NewStringUTF(sbport::CompanionJson().c_str());      // ASCII only (see esc)
}

extern "C" JNIEXPORT void JNICALL
Java_com_sirbilliams_spongebob_Companion_nativeSetEnabled(JNIEnv*, jclass, jboolean on)
{
    sbport::CompanionSetEnabled(on != 0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_sirbilliams_spongebob_Companion_nativeCommand(JNIEnv* env, jclass, jstring cmd)
{
    const char* c = env->GetStringUTFChars(cmd, 0);
    if (c) { sbport::CompanionPush(c); env->ReleaseStringUTFChars(cmd, c); }
}
#endif
