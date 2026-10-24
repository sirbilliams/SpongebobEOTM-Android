/*  Crash backtrace into omt-log.txt.  port: no counterpart in sb2.exe (MSVC's SEH / Dr. Watson did this).

    The log file is the one OMediaSDLMain.cpp opens (<external files dir>/omt-log.txt on Android; on the
    desktop OMT_LOG if set, else omt-log.txt in the data folder). It is appended to with write(2) from the
    signal handler. The backtrace uses the C++ unwinder (_Unwind_Backtrace), which works across the whole
    process: libspongebob.so, libSDL2.so, libc ...: each frame is printed as
        #NN pc <offset in the module> <module> (<nearest symbol>+<offset>)
    so "addr2line -e libspongebob.so <offset>" resolves it.                                          */

#include "CrashLog.h"
#include <SDL.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unwind.h>

static char			g_log_path[1024];
static stack_t		g_altstack;
static struct sigaction g_old[32];
static const int	g_signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
static const int	g_num_signals = 5;
static volatile sig_atomic_t g_in_handler = 0;

struct Unwinder
{
	int fd;
	int count;
};

static void out(int fd, const char *s)
{
	size_t n = strlen(s);
	while (n)
	{
		ssize_t w = write(fd, s, n);
		if (w <= 0) break;
		s += w; n -= (size_t)w;
	}
}

static _Unwind_Reason_Code unwind_step(struct _Unwind_Context *ctx, void *arg)
{
	Unwinder *u = (Unwinder*)arg;
	uintptr_t pc = (uintptr_t)_Unwind_GetIP(ctx);
	if (!pc) return _URC_END_OF_STACK;
	if (u->count >= 64) return _URC_END_OF_STACK;
	char line[640];
	Dl_info info;
	if (dladdr((void*)pc, &info) && info.dli_fname)
	{
		const char *mod = strrchr(info.dli_fname, '/');
		mod = mod ? mod + 1 : info.dli_fname;
		unsigned long off = (unsigned long)(pc - (uintptr_t)info.dli_fbase);
		if (info.dli_sname)
			snprintf(line, sizeof line, "  #%02d pc %016lx  %s (%s+%lu)\n", u->count, off, mod, info.dli_sname,
					 (unsigned long)(pc - (uintptr_t)info.dli_saddr));
		else
			snprintf(line, sizeof line, "  #%02d pc %016lx  %s\n", u->count, off, mod);
	}
	else snprintf(line, sizeof line, "  #%02d pc %016lx  (unknown)\n", u->count, (unsigned long)pc);
	out(u->fd, line);
	u->count++;
	return _URC_NO_REASON;
}

static void handler(int sig, siginfo_t *info, void *ctx)
{
	if (!g_in_handler)
	{
		g_in_handler = 1;
		int fd = g_log_path[0] ? open(g_log_path, O_WRONLY | O_APPEND | O_CREAT, 0644) : -1;
		char head[256];
		snprintf(head, sizeof head, "\n*** CRASH: signal %d (%s) fault address %p\n", sig, strsignal(sig), info ? info->si_addr : NULL);
		for (int pass = 0; pass < 2; pass++)
		{
			int target = pass == 0 ? fd : 2;
			if (target < 0) continue;
			out(target, head);
			Unwinder u = { target, 0 };
			_Unwind_Backtrace(unwind_step, &u);
			out(target, "*** end of backtrace\n");
		}
		if (fd >= 0) { fsync(fd); close(fd); }
	}
	// hand over to whoever was installed before (debuggerd on Android) and re-raise
	int idx = -1;
	for (int i = 0; i < g_num_signals; i++) if (g_signals[i] == sig) idx = i;
	if (idx >= 0 && sig < 32) sigaction(sig, &g_old[sig], NULL); else signal(sig, SIG_DFL);
	if (sig == SIGABRT || sig == SIGFPE || sig == SIGILL) raise(sig);	// faults re-execute and trap again
	(void)ctx;
}

void port_install_crash_log(void)
{
	const char *log = getenv("OMT_LOG");
#ifdef __ANDROID__
	const char *ext = SDL_AndroidGetExternalStoragePath();
	if (ext && !log) { snprintf(g_log_path, sizeof g_log_path, "%s/omt-log.txt", ext); log = g_log_path; }
#endif
	if (log && log != g_log_path) snprintf(g_log_path, sizeof g_log_path, "%s", log);
	if (!g_log_path[0]) snprintf(g_log_path, sizeof g_log_path, "omt-log.txt");	// (the data folder is the cwd)
	if (g_log_path[0] != '/')									// absolute now: the cwd can change
	{
		char cwd[800];
		if (getcwd(cwd, sizeof cwd)) { char tmp[1024]; snprintf(tmp, sizeof tmp, "%s/%s", cwd, g_log_path); snprintf(g_log_path, sizeof g_log_path, "%s", tmp); }
	}
	// a stack of its own, so a stack overflow can be reported too
	g_altstack.ss_size = 64 * 1024;
	g_altstack.ss_sp = malloc(g_altstack.ss_size);
	g_altstack.ss_flags = 0;
	if (g_altstack.ss_sp) sigaltstack(&g_altstack, NULL);
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_sigaction = handler;
	sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
	sigemptyset(&sa.sa_mask);
	for (int i = 0; i < g_num_signals; i++) sigaction(g_signals[i], &sa, &g_old[g_signals[i]]);
}
