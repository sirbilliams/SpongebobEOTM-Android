/*  Crash backtrace into omt-log.txt (shared from the launcher's Test page).  See CrashLog.cpp.   */
#pragma once

// port: installs handlers for SIGSEGV / SIGBUS / SIGILL / SIGFPE / SIGABRT that append the signal
// and a backtrace (library + offset, for addr2line against the build's unstripped .so) to
// <data folder>/omt-log.txt, then let the previous handler (Android's debuggerd) run.
void port_install_crash_log(void);
