#include <cstring>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <signal.h>
#include "crashHandler.h"
#include <TFE_System/system.h>


// On Linux, /proc/self/status contains a "TracerPid:" entry
// with the PID of the controlling process, or 0 if run directly.
// Use this to determine whether we are running under
// gdb/valgrind/...
static bool are_we_being_debugged(void)
{
	char buf[4096], *c;
	int fd, rd;

	fd = open("/proc/self/status", O_RDONLY);
	if (!fd)
		return false;	// assume no.
	rd = read(fd, buf, 4096);
	close(fd);
	if (rd <= 0)
		return false;

	buf[rd] = 0;
	c = strstr(buf, "TracerPid:");
	if (!c)
		return false;	// can't say.

	c += 10;
	while (*c && isspace(*c))
		c++;

	if (!*c)
		return false;	// can't say for sure.

	return (isdigit(*c) && (*c != '0'));	// PID != 0 => being traced.
}

static void tfe_sigaction(int signo, siginfo_t *siginfo, void *uctx)
{
	int entries, i;
	void *buf[512];
	char **ents;

	// gently quit
	if ((signo == SIGINT) || (signo == SIGTERM)) {
		TFE_System::postQuitMessage();
		return;
	}

	TFE_System::logWrite(LOG_ERROR, "CrashHandler", "Received Signal %d errno %d code %d", signo, siginfo->si_addr, siginfo->si_errno, siginfo->si_code);

	switch (signo) {
	case SIGILL:
	case SIGFPE:
	case SIGSEGV:
	case SIGBUS:
		TFE_System::logWrite(LOG_ERROR, "CrashHandler", "faulting address %p", siginfo->si_addr);
	}

	// backtrace() can also segfault; purposefully ignore SEGV before calling it.
	signal(SIGSEGV, SIG_IGN);
	entries = backtrace(buf, 512);
	if (entries) {
		ents = backtrace_symbols(buf, entries);
		TFE_System::logWrite(LOG_ERROR, "CrashHandler", "Backtrace %d:", entries);
		for (i = 0; i < entries; i++) {
			TFE_System::logWrite(LOG_ERROR, "CrashHandler", "%03d %s", i, ents[i]);
		}
	} else {
		TFE_System::logWrite(LOG_ERROR, "CrashHandler", "no backtrace possible");
	}

	// for certain signals, the default handler will create
	// a coredump if enabled by administrator.
	signal(signo, SIG_DFL);
	kill(getpid(), signo);
}

static void setsig(int signo)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = tfe_sigaction;
	if (sigaction(signo, &sa, NULL)) {
		fprintf(stderr, "Crash Handler: setsig(%d) failed with %d (%s)\n", signo, errno, strerror(errno));
	}
}

namespace TFE_CrashHandler
{
	void setProcessExceptionHandlers()
	{
		// skip signal hooking when running under debuggers.
		if (are_we_being_debugged())
			return;

		setsig(SIGINT);		// ctrl-c
		setsig(SIGTERM);	// kill -15 received
		setsig(SIGFPE);		// FPU exception
		setsig(SIGILL);		// unknown opcode
		setsig(SIGBUS);		// alignment issues
		setsig(SIGSEGV);	// pointer gone awry
		setsig(SIGABRT);	// abort() was invoked
	}

	void setThreadExceptionHandlers()
	{
	}
}  // namespace TFE_CrashHandler
