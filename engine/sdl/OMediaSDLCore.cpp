/*  OMT SDL platform layer - core services (file paths, file streams, memory, timer, clipboard)
    Port of OS/MSWindows equivalents to POSIX + SDL2. LGPL, same as OMT. */

#include "OMediaSysDefs.h"
#include "OMediaRetarget.h"
#include "OMediaFilePath.h"
#include "OMediaFileStream.h"
#include "OMediaMoveableMem.h"
#include "OMediaMemTools.h"
#include "OMediaTimeCounter.h"
#include "OMediaClipboard.h"
#include "OMediaApplication.h"
#include "OMediaError.h"
#include "OMediaSDLRetarget.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>

// ---------------------------------------------------------------------------
// Path helpers: game data uses Windows-style, case-insensitive paths
// (".\\omt\\Icons.omt"). Normalise separators and resolve each component
// case-insensitively against the real filesystem.

string omf_SDLResolvePath(const string &in)
{
	string p = in;
	for (size_t i = 0; i < p.size(); i++) if (p[i] == '\\') p[i] = '/';
	if (p.size() > 1 && p[1] == ':') p = p.substr(2);          // strip drive letter

	string out = (p.size() && p[0] == '/') ? "/" : "";
	size_t i = (p.size() && p[0] == '/') ? 1 : 0;
	while (i <= p.size())
	{
		size_t j = p.find('/', i);
		if (j == string::npos) j = p.size();
		string comp = p.substr(i, j - i);
		i = j + 1;
		if (comp.empty()) { if (j >= p.size()) break; continue; }
		if (comp == ".") { if (out.empty()) out = "./"; continue; }

		string dir = out.empty() ? "." : out;
		string cand = out + comp;
		struct stat st;
		if (stat(cand.c_str(), &st) != 0)
		{
			DIR *d = opendir(dir.c_str());
			if (d)
			{
				struct dirent *e;
				while ((e = readdir(d)) != NULL)
					if (strcasecmp(e->d_name, comp.c_str()) == 0) { cand = out + e->d_name; break; }
				closedir(d);
			}
		}
		out = cand;
		if (j < p.size()) out += "/";
		if (j >= p.size()) break;
	}
	return out;
}

// ---------------------------------------------------------------------------
// OMediaFilePath

OMediaSDLRtgFilePath::OMediaSDLRtgFilePath() : OMediaRetarget(omcrtg_FilePath) { file = NULL; }
OMediaSDLRtgFilePath::~OMediaSDLRtgFilePath() {}

#define RTG_PATH(r) (((OMediaSDLRtgFilePath*)(r))->path)

OMediaFilePath::OMediaFilePath(void)
{
	retarget = new OMediaSDLRtgFilePath;
	app_type = file_type = 0;
}

OMediaFilePath::OMediaFilePath(string name, const OMediaFilePath *base_path, omt_AppType appt, omt_FileType filet)
{
	retarget = new OMediaSDLRtgFilePath;
	setpath(name, base_path, appt, filet);
}

OMediaFilePath::OMediaFilePath(string name, omt_AppType appt, omt_FileType filet)
{
	retarget = new OMediaSDLRtgFilePath;
	setpath(name, appt, filet);
}

OMediaFilePath::~OMediaFilePath(void) { delete retarget; }

void OMediaFilePath::setpath(string filename, omt_AppType appt, omt_FileType filet)
{
	app_type = appt; file_type = filet;
	RTG_PATH(retarget) = filename;
}

void OMediaFilePath::setpath(string filename, const OMediaFilePath *base_path, omt_AppType appt, omt_FileType filet)
{
	app_type = appt; file_type = filet;
	string bpath = RTG_PATH(base_path->getretarget());
	int p = (int)bpath.size() - 1;
	while (p >= 0)
	{
		if (bpath[p] == '/' || bpath[p] == '\\' || bpath[p] == ':') { bpath.resize(p + 1); break; }
		p--;
	}
	RTG_PATH(retarget) = (p > 0) ? bpath : string("");
	RTG_PATH(retarget).append(filename);
}

void OMediaFilePath::get_filename(string &str)
{
	string bpath = RTG_PATH(retarget);
	int p = (int)bpath.size() - 1;
	while (p >= 0)
	{
		if (bpath[p] == '/' || bpath[p] == '\\' || bpath[p] == ':')
		{
			p++;
			if (p >= (int)bpath.size()) { str = ""; return; }
			break;
		}
		p--;
	}
	str = (p > 0) ? string(bpath.c_str() + p) : bpath;
}

void OMediaFilePath::set_default_preference_path(string filename, omt_AppType appt, omt_FileType filet)
{
	app_type = appt; file_type = filet;
	char *pref = SDL_GetPrefPath("OMT", "prefs");
	RTG_PATH(retarget) = pref ? pref : "./";
	if (pref) SDL_free(pref);
	RTG_PATH(retarget) += filename;
}

void OMediaFilePath::setpath(const OMediaFilePath *newpath)
{
	RTG_PATH(retarget) = RTG_PATH(newpath->getretarget());
	app_type = newpath->getapptype();
	file_type = newpath->getfiletype();
}

bool OMediaFilePath::ask_user(OMediaWindow *, OMediaAskUserFilters &, bool, string, string, string)
{
	return false;   // no native file dialog on this platform
}

// ---------------------------------------------------------------------------
// OMediaFileStream

#define RTG_FILE(fp) (((OMediaSDLRtgFilePath*)(fp).getretarget()))

OMediaFileStream::OMediaFileStream(void) { isopenflag = false; readonlyflag = false; position = 0; }
OMediaFileStream::~OMediaFileStream(void) { close(); }

unsigned int OMediaFileStream::do_getsize(void)
{
	if (!isopenflag) return 0;
	FILE *f = (FILE*)RTG_FILE(filepath)->file;
	long cur = ftell(f); fseek(f, 0, SEEK_END); long siz = ftell(f); fseek(f, cur, SEEK_SET);
	return (unsigned int)siz;
}

void OMediaFileStream::do_setsize(unsigned int newsize)
{
	if (!isopenflag) return;
	FILE *f = (FILE*)RTG_FILE(filepath)->file;
	fflush(f);
	if (ftruncate(fileno(f), newsize) != 0) omd_OSEXCEPTION(errno);
	do_setposition(newsize, omcfr_Start);
}

void OMediaFileStream::open(omt_FilePermission perm, bool truncate, bool createifnofile)
{
	close();
	OMediaSDLRtgFilePath *rp = RTG_FILE(filepath);
	string real = omf_SDLResolvePath(rp->path);
	struct stat st; bool exists = stat(real.c_str(), &st) == 0;
	const char *mode;
	if (perm == omcfp_Read)
	{
		if (!exists) omd_OSEXCEPTION(ENOENT);
		mode = "rb";
	}
	else if (truncate) mode = (perm == omcfp_Write) ? "wb" : "w+b";
	else if (exists) mode = "r+b";
	else
	{
		if (!createifnofile) omd_OSEXCEPTION(ENOENT);
		mode = "w+b";
	}
	FILE *f = fopen(real.c_str(), mode);
	if (!f) omd_OSEXCEPTION(errno);
	rp->file = f;
	isopenflag = true;
	readonlyflag = perm == omcfp_Read;					// port: see ~OMediaFormattedStream
	position = 0;
	set_buffer_size(2 * 1024);
}

void OMediaFileStream::close(void)
{
	if (!isopenflag) return;
	flush_buffer();
	fclose((FILE*)RTG_FILE(filepath)->file);
	RTG_FILE(filepath)->file = NULL;
	isopenflag = false;
	set_buffer_size(0);
}

bool OMediaFileStream::fileexists(void)
{
	if (isopenflag) return true;
	struct stat st;
	return stat(omf_SDLResolvePath(RTG_FILE(filepath)->path).c_str(), &st) == 0;
}

void OMediaFileStream::do_read(void *buffer, unsigned int nbytes)
{
	if (!isopenflag) return;
	size_t got = fread(buffer, 1, nbytes, (FILE*)RTG_FILE(filepath)->file);
	if (got < nbytes) memset((char*)buffer + got, 0, nbytes - got);
	position += nbytes;
}

void OMediaFileStream::do_write(void *buffer, unsigned int nbytes)
{
	if (!isopenflag) return;
	if (fwrite(buffer, 1, nbytes, (FILE*)RTG_FILE(filepath)->file) != nbytes) omd_OSEXCEPTION(errno);
	position += nbytes;
}

void OMediaFileStream::kill(void)
{
	close();
	if (remove(omf_SDLResolvePath(RTG_FILE(filepath)->path).c_str()) != 0) omd_OSEXCEPTION(errno);
}

void OMediaFileStream::do_setposition(int offset, short omtrelative)
{
	if (!isopenflag) return;
	int whence = SEEK_SET;
	if (omtrelative == omcfr_End) whence = SEEK_END;
	else if (omtrelative == omcfr_Current) whence = SEEK_CUR;
	FILE *f = (FILE*)RTG_FILE(filepath)->file;
	if (fseek(f, offset, whence) != 0) omd_OSEXCEPTION(errno);
	position = (int)ftell(f);
}

int OMediaFileStream::do_getposition(void) { return position; }
void OMediaFileStream::setpath(const OMediaFilePath *p) { filepath.setpath(p); }
bool OMediaFileStream::getpath(OMediaFilePath *p) const { p->setpath(&filepath); return true; }

// ---------------------------------------------------------------------------
// OMediaMoveableMem (plain heap block; "moveable" handles are a classic Mac/Win concept)

class OMediaSDLRtgMoveMem : public OMediaRetarget
{
	public:
	OMediaSDLRtgMoveMem() : OMediaRetarget(omcrtg_MoveableMem) { block = NULL; }
	void *block;
};

OMediaMoveableMem::OMediaMoveableMem(int blocksize)
{
	lock_count = 0;
	init_retarget();
	((OMediaSDLRtgMoveMem*)retarget)->block = malloc(blocksize ? blocksize : 1);
	if (!((OMediaSDLRtgMoveMem*)retarget)->block) omd_EXCEPTION(omcerr_OutOfMemory);
	real_size = blocksize;
}

OMediaMoveableMem::~OMediaMoveableMem(void)
{
	free(((OMediaSDLRtgMoveMem*)retarget)->block);
	delete retarget;
}

int OMediaMoveableMem::getsize(void) { return real_size; }

void OMediaMoveableMem::setsize(int newsize)
{
	void *b = realloc(((OMediaSDLRtgMoveMem*)retarget)->block, newsize ? newsize : 1);
	if (!b) omd_EXCEPTION(omcerr_OutOfMemory);
	((OMediaSDLRtgMoveMem*)retarget)->block = b;
	real_size = newsize;
}

void *OMediaMoveableMem::lock(void) { lock_count++; return ((OMediaSDLRtgMoveMem*)retarget)->block; }
void OMediaMoveableMem::unlock(void) { lock_count--; }
void OMediaMoveableMem::init_retarget(void) { retarget = new OMediaSDLRtgMoveMem; }

void OMediaMemTools::copy(const void *src, void *des, int nbytes) { memmove(des, src, nbytes); }

// ---------------------------------------------------------------------------
// OMediaTimeCounter

static bool millisecs_paused = false;
static int millisecs_delta = 0;
static unsigned int paused_value = 0;
static omt_MillisecFuncTrap millisectrap = NULL;
static omt_MilliSecsFlags millisecflags = 0;

void OMediaTimeCounter::set_flags_millisecs(omt_MilliSecsFlags flags) { millisecflags = flags; }
omt_MilliSecsFlags OMediaTimeCounter::get_flags_millisecs(void) { return millisecflags; }
void OMediaTimeCounter::add_millisecs(int delta_msecs) { millisecs_delta -= delta_msecs; }
void OMediaTimeCounter::set_millisecs_trap(omt_MillisecFuncTrap func) { millisectrap = func; }
omt_MillisecFuncTrap OMediaTimeCounter::get_millisecs_trap(void) { return millisectrap; }
unsigned int OMediaTimeCounter::lowlevel_get_millisecs(void) { return (unsigned int)SDL_GetTicks(); }

unsigned int OMediaTimeCounter::get_millisecs(void)
{
	if (millisecflags & ommsf_AlwaysUseLowCounter) return lowlevel_get_millisecs();
	if (millisectrap) return millisectrap();
	if (millisecs_paused) return paused_value;
	return lowlevel_get_millisecs() - millisecs_delta;
}

void OMediaTimeCounter::pause_millisecs(bool pause)
{
	if (pause && !millisecs_paused) { paused_value = get_millisecs(); millisecs_paused = true; }
	else if (!pause && millisecs_paused) { millisecs_paused = false; millisecs_delta += get_millisecs() - paused_value; }
}

bool OMediaTimeCounter::is_paused_millisecs(void) { return millisecs_paused; }

// ---------------------------------------------------------------------------
// OMediaClipboard - text only via SDL, other clip types unsupported

void OMediaClipboard::open(void) {}
void OMediaClipboard::close(void) {}
void OMediaClipboard::clear(void) { SDL_SetClipboardText(""); }
bool OMediaClipboard::clip_exist(omt_ClipType) { return SDL_HasClipboardText() == SDL_TRUE; }
void OMediaClipboard::add_clip(omt_ClipType, OMediaMoveableMem &) {}
bool OMediaClipboard::get_clip(omt_ClipType, OMediaMoveableMem &) { return false; }

// ---------------------------------------------------------------------------
// OMediaApplication

void OMediaApplication::init_retarget(omt_InitAppHints) {}
