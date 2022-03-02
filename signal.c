#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <sys/wait.h>

#include "defs.h"

static sigset_t sigset_mask;

int child_cnt(int state)
{
    static int cnt = 0;

    switch (state) {
        case 0:
            return cnt;
        case -1:
            if (cnt == 0)
                return 0;
            return --cnt;
        case 1:
            return ++cnt;
    }

    return cnt;
}

void signal_action(int sig, void (*handler)(int))
{
    struct sigaction sact;

    memset(&sact, 0, sizeof(sact));
    sact.sa_handler = handler;
    sact.sa_flags = SA_RESTART; // | SA_NOCLDWAIT | SA_NOCLDSTOP;
    sigaction(sig, &sact, NULL);
}

void signal_action_flags(int sig, void (*handler)(int), int flags)
{
    struct sigaction sact;

    memset(&sact, 0, sizeof(sact));
    sact.sa_handler = handler;
    sact.sa_flags = flags;
    sigaction(sig, &sact, NULL);
}


void signal_block(int sig)
{
    sigaddset(&sigset_mask, sig);
    sigprocmask(SIG_BLOCK, &sigset_mask, NULL);
}

void signal_unblock(int sig)
{
    static sigset_t tmp_mask;

    sigemptyset(&tmp_mask);
    sigaddset(&tmp_mask, sig);
    sigprocmask(SIG_UNBLOCK, &tmp_mask, NULL);
    sigdelset(&sigset_mask, sig);
}

void signal_hdl__child(int sig)
{
    int pid;
    int status;

    while (3) {
        pid = waitpid(-1, &status, WNOHANG);
        if (pid == -1) {
            if (errno == EINTR)
                continue;
            break;
        } else if (pid == 0) {
            break;
        } else {
            child_cnt(-1);
            continue;
        }
    }
}

char *signal_text(int sig)
{
    char *cptr;
    static char sigunknown[15];

    sig &= 0x7F;
    switch (sig) {
        case SIGHUP:
            cptr = "SIGHUP";
            break;
        case SIGINT:
            cptr = "SIGINT";
            break;
        case SIGQUIT:
            cptr = "SIGQUIT";
            break;
        case SIGILL:
            cptr = "SIGILL";
            break;
        case SIGTRAP:
            cptr = "SIGTRAP";
            break;
        case SIGIOT:
            cptr = "SIGIOT";
            break;
        case SIGFPE:
            cptr = "SIGFPE";
            break;
        case SIGKILL:
            cptr = "SIGKILL";
            break;
        case SIGBUS:
            cptr = "SIGBUS";
            break;
        case SIGSEGV:
            cptr = "SIGSEGV";
            break;
        case SIGPIPE:
            cptr = "SIGPIPE";
            break;
        case SIGALRM:
            cptr = "SIGALRM";
            break;
        case SIGTERM:
            cptr = "SIGTERM";
            break;
        case SIGUSR1:
            cptr = "SIGUSR1";
            break;
        case SIGUSR2:
            cptr = "SIGUSR2";
            break;
        case SIGCHLD:
            cptr = "SIGCHLD";
            break;
        case SIGWINCH:
            cptr = "SIGWINCH";
            break;
        default:
            snprintf(sigunknown, sizeof(sigunknown), "SIGNAL %u", sig);
            return sigunknown;
    }
    return cptr;
}

void signal_hdl__pipe(int sig)
{
    syslog(LOG_NOTICE, "%s caught", signal_text(sig));
    exit(0);
}


void signal_hdl__exit(int sig)
{
    syslog(LOG_NOTICE, "%s caught", signal_text(sig));
    exit(0);
}
