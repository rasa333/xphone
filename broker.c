#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <syslog.h>
#include <fcntl.h>


#include "defs.h"
#include "history.h"

HISTORY *Hhead = NULL;
HISTORY *Htail = NULL;

#define TRUE   1
#define FALSE  0


struct alias {
    char *nummer;
    char *name;
    struct alias *next;
    struct alias *prev;
};
typedef struct alias ALIAS;

ALIAS *Ahead = NULL;
ALIAS *Atail = NULL;
struct stat st;
time_t mtime;
char log_file[1024];
char alias_file[1024];


void usage(char *bind_to_host, int port, char *lfile, char *afile)
{
    printf("Usage: xphone_broker [options]\n");
    printf("    -h          this help\n");
    printf("    -b host     bind to host [%s]\n", bind_to_host == NULL ? "NULL" : bind_to_host);
    printf("    -p port     xphone_broker port [%d]\n", port);
    printf("    -l file     log filename [%s]\n", lfile);
    printf("    -a file     alias filename [%s]\n", afile);
}


int broker(char *bindtohost, int port, void (*func)(int fd), int blocking_flag, int max_childs)
{
  struct sockaddr_in mysocket;
  struct sockaddr_in child;
  struct hostent *hp;
  int addrlen, connfd, sockd;
  int len = sizeof(mysocket);
  int one = 1;
  int val = 1;
  
  // init server-process-socket
  bzero((char *)&mysocket, sizeof(mysocket));
  
  mysocket.sin_port        = htons(port);
  if (bindtohost == NULL) {
    mysocket.sin_addr.s_addr = htonl(INADDR_ANY);
  } else {
    hp = gethostbyname(bindtohost);              
    if (hp == NULL) {
      syslog(LOG_ERR, "gethostbyname: '%s' %s", bindtohost, hstrerror(h_errno));
      exit(1);
    }
    mysocket.sin_addr   = (*(struct in_addr *)(hp->h_addr_list[0]));
  }
  mysocket.sin_family      = AF_INET;
  
  if ((sockd = socket(AF_INET, SOCK_STREAM, 0)) < 0)  {
    syslog(LOG_ERR, "socket: %s", strerror(errno));
    exit(1);
  }

  // keepalive
  /*if (setsockopt(sockd, SOL_SOCKET, SO_KEEPALIVE, (char *)&one, sizeof(one)) < 0) {
    psyslog(LOG_ERR, "setsockopt: %s", strerror(errno));
    exit(1);
    }*/

  if (setsockopt(sockd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))) {
    syslog(LOG_ERR, "setsockopt: %s", strerror(errno));
    exit(1);
  }
  
  if (bind(sockd, (struct sockaddr *)&mysocket, sizeof(mysocket)) < 0)  {
    syslog(LOG_ERR, "bind: %s", strerror(errno));
    exit(1);
  }
  
  if (listen(sockd, 128) == -1) {
    syslog(LOG_ERR, "listen: %s", strerror(errno));
    exit(1);
  }
  if (getsockname(sockd, (struct sockaddr *)&mysocket, &len) < 0)  {
    syslog(LOG_ERR, "getsockname: %s", strerror(errno));
    exit(1);
  }
  
  for( ; ; ) {
    signal_unblock(SIGALRM);
    signal_unblock(SIGCHLD);
    addrlen = sizeof(struct sockaddr_in);
    if ((connfd = accept(sockd, (struct sockaddr *) &child, &addrlen)) < 0) {
      if (errno != EINTR) 
	syslog(LOG_ERR, "accept: %s", strerror(errno));
      continue;
    }
    signal_block(SIGCHLD);
    signal_block(SIGALRM);


    // maximum number of childs reached?
    if (child_cnt(1) > max_childs) {
      syslog(LOG_ERR, "Maximum number of childs (%d) reached.", max_childs);
      continue;
    }
    
    // fork the child
    if (fork() == 0) {
      signal_action_flags(SIGCHLD, SIG_DFL, 0);
      signal_action(SIGALRM, SIG_IGN);
      signal_action(SIGHUP, SIG_IGN);
      signal_action(SIGTERM, SIG_IGN);
      signal_action(SIGINT, SIG_IGN);
      signal_action(SIGQUIT, SIG_IGN);
      signal_action(SIGABRT, SIG_IGN);
      signal_action(SIGPIPE, SIG_IGN);
      signal_action(SIGSEGV, exit);
      signal_action(SIGILL, exit);
      close(sockd);
      alarm(0);
      //      reset_pid_vars();
      umask(077);
      
      // nonblocking?
      if (blocking_flag == FALSE) {
	val = fcntl(connfd, F_GETFL, 0);
	fcntl(connfd, F_SETFL, val | O_NONBLOCK);
      }
      
      (*func)(connfd);
      _exit(0);
    } else {
      child_cnt(0);
    }
    close(connfd);
  }
}


void alias_add(char *nummer, char *name)
{
    if (Ahead == NULL) {
        Atail = malloc(sizeof(ALIAS));
        Ahead = Atail;
        Ahead->next = NULL;
        Ahead->prev = NULL;
        Ahead->nummer = strdup(nummer);
        Ahead->name = strdup(name);
    } else {
        Atail->next = malloc(sizeof(ALIAS));
        Atail->next->nummer = strdup(nummer);
        Atail->next->name = strdup(name);
        Atail->next->prev = Atail;
        Atail = Atail->next;
        Atail->next = NULL;
    }
}


char *alias_search(char *nummer)
{
    ALIAS *Alist;

    if (Ahead != NULL)
        for (Alist = Ahead; Alist != NULL; Alist = Alist->next)
            if (!strcmp(nummer, Alist->nummer))
                return strdup(Alist->name);

    return strdup(nummer);
}


void alias_free()
{
    ALIAS *Alist, *Atmp;

    if (Ahead == NULL)
        return;

    for (Alist = Ahead; Alist != NULL;) {
        Atmp = Alist->next;
        free(Alist->nummer);
        free(Alist->name);
        free(Alist);
        Alist = Atmp;
    }
    Ahead = NULL;
    Atail = NULL;
}


void alias_read(char *file)
{
    FILE *f;
    char *p, buf[1024], nummer[1024], name[1024], *h, *d;
    long line = 0;

    f = fopen(file, "r");
    if (f == NULL) {
        fprintf(stderr, "%s: %s\n", file, strerror(errno));
        exit(1);
    }
    while (fgets(buf, sizeof(buf), f)) {
        line++;
        buf[strlen(buf) - 1] = 0;
        if (buf[0] == 0)
            continue;
        if ((p = (char *) strchr(buf, '#')) != NULL)
            *p = 0;

        p = trim(buf);
        if (*p == 0)
            continue;
        h = nummer;
        d = name;
        *h = *d = 0;
        while (*p && !isspace(*p))
            *h++ = *p++;
        *h = 0;
        if (*p == 0) {
            fprintf(stderr, "%s[%ld]: invalid line\n", file, line);
            continue;
        }
        while (*p && isspace(*p))
            p++;
        while (*p)
            *d++ = *p++;
        *d = 0;
        if (name[0] == 0) {
            fprintf(stderr, "%s[%ld]: invalid line\n", file, line);
            continue;
        }
        alias_add(nummer, name);
    }
    fclose(f);
}


void openfritzlog(int fd)
{
    FILE *fr, *fw;
    int acnt = 0;
    char **arr = NULL;
    char *src, buf[1024];
    HISTORY *Hlist;

    if (stat(alias_file, &st) == -1) {
        fprintf(stderr, "%s: %m\n", alias_file);
        exit(1);
    }
    mtime = st.st_mtime;
    alias_read(alias_file);

    fr = fopen(log_file, "r");
    if (fr == NULL) {
        perror(log_file);
        exit(1);
    }
    fw = fdopen(fd, "w");
    if (fw == NULL) {
        perror("socket");
        exit(1);
    }
    // first read the whole phone-list-file
    while (fgets(buf, sizeof(buf), fr)) {
        arr = wordlist(buf, arr, &acnt, is_space);
        src = alias_search(arr[2]);
        fprintf(fw, "+ %s %s %s\n", arr[0], arr[1], src);
        fflush(fw);
        free(src);
    }
    while (3) {
        // phone-list-file reset ?
        if (ftell(fr) > getfilesize(log_file)) {
            fclose(fr);
            sleep(5);
            fr = fopen(log_file, "r");
            if (fr == NULL) {
                perror(log_file);
                exit(1);
            }
        }

        fgets(buf, sizeof(buf), fr);
        // phone-list-file end reached ?
        if (feof(fr)) {
            clearerr(fr);
            usleep(350);
            readln(fd);   // socket connection alive ?
            continue;
        }
        arr = wordlist(buf, arr, &acnt, is_space);
        // reread alias-file ?
        if (stat(alias_file, &st) == -1) {
            fprintf(stderr, "%s: %m\n", alias_file);
            exit(1);
        }
        if (st.st_mtime > mtime) {
            mtime = st.st_mtime;
            alias_free();
            alias_read(alias_file);
            for (Hlist = Htail; Hlist != NULL; Hlist = Hlist->prev) {
                src = alias_search(Hlist->nummer);
                free(Hlist->nummer);
                Hlist->nummer = src;
            }
        }
        src = alias_search(arr[2]);
        fprintf(fw, "%s %s %s\n", arr[0], arr[1], src);
        fflush(fw);
        free(src);
    }
}

int main(int argc, char **argv)
{
    int port = DEFAULT_PORT, opt;
    char *bind_to_host = NULL;
    extern char *optarg;

    snprintf(log_file, sizeof(log_file), "%s/.xphone/log", getenv("HOME"));
    snprintf(alias_file, sizeof(alias_file), "%s/.xphone/log", getenv("HOME"));

    while ((opt = getopt(argc, argv, "b:a:l:p:h")) != -1) {
        switch (opt) {
            case 'b':
                bind_to_host = strdup(optarg);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'l':
                strcpy(log_file, optarg);
                break;
            case 'a':
                strcpy(alias_file, optarg);
                break;
            case 'h':
                usage(bind_to_host, port, log_file, alias_file);
                exit(0);
            default:
                usage(bind_to_host, port, log_file, alias_file);
                exit(1);
        }
    }

    daemonize(TRUE);
    signal_action(SIGCHLD, signal_hdl__child);
    signal_action(SIGSEGV, signal_hdl__exit);
    signal_action(SIGINT, signal_hdl__exit);
    signal_action(SIGTERM, signal_hdl__exit);
    signal_action(SIGPIPE, signal_hdl__pipe);
    broker(bind_to_host, port, openfritzlog, FALSE, 256);
}
