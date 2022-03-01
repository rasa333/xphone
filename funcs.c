#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <syslog.h>
#include <ctype.h>
#include <sys/stat.h>

#include "defs.h"
#include "history.h"

#define TRUE   1
#define FALSE  0

extern HISTORY *Hhead;
extern HISTORY *Htail;


int is_space(char c)
{
    return isspace(c);
}

char **wordlist(char *str, char **arg, int *cnt, int (*trunc)(char c))
{
    char *tmp = (char *) malloc(strlen(str) + 1);
    register int i = 0, j = 0;

    if (arg == NULL) {
        arg = (char **) malloc(sizeof(char *));
        arg[0] = NULL;
    }

    while (*str) {
        while (*str && (*trunc)(*str) && *str != '"')
            str++;

        if (*str == 0)
            goto start;

        while (*str && !(*trunc)(*str) && *str != '"')
            tmp[i++] = *str++;
        if (*str == '"') {
            str++;
            while (*str && *str != '"')
                tmp[i++] = *str++;
            str++;
        }
        tmp[i] = 0;
        if (arg[j] == NULL)
            arg[j] = (char *) malloc(i + 1);
        else
            arg[j] = (char *) realloc(arg[j], i + 1);
        strcpy(arg[j++], tmp);
        i = 0;

        start:

        if (j > *cnt) {
            arg = (char **) realloc(arg, sizeof(char *) * (j + 1));
            arg[j] = NULL;
        }
    }
    if (j < *cnt) {
        for (i = j; i < *cnt; i++) {
            free(arg[i]);
            arg[i] = NULL;
        }
        arg = (char **) realloc(arg, sizeof(char *) * (j + 1));
    }
    *cnt = j;
    free(tmp);

    return arg;
}


/*
 * frees a char array (char **); last element must be NULL
 */

char **free_char_array(char **a)
{
    int i;

    if (a == NULL)
        return a;

    for (i = 0; a[i] != NULL; i++) {
        free(a[i]);
    }
    free(a);
    a = NULL;

    return a;
}


int array_cnt(char **arr)
{
    int i;

    if (arr == NULL)
        return -1;

    for (i = 0; arr[i] != NULL; i++);

    return i;
}


char *array2pup(char **arr)
{
    int i, len = 0;
    char *new;

    if (arr == NULL || array_cnt(arr) == 1)
        return NULL;

    for (i = 0; arr[i] != NULL; i++)
        len += strlen(arr[i]);
    len += i + 1;

    new = malloc(len);

    *new = 0;
    for (i = array_cnt(arr) - 2; i != -1; i--) {
        strcat(new, arr[i]);
        if (i != 0)
            strcat(new, "|");
    }

    return new;
}


static void _history_del_node(HISTORY *x)
{
    if (x == Hhead) {
        Hhead = x->next;
        Hhead->prev = NULL;
    } else {
        if (x == Htail) {
            Htail = x->prev;
            Htail->next = NULL;
        } else {
            x->next->prev = x->prev;
            x->prev->next = x->next;
        }
    }
}

void history_del(HISTORY *x)
{
    free(x->nummer);
    free_char_array(x->time_list);
    _history_del_node(x);
    free(x);
}


void history_add(char *zeit, char *nummer)
{
    HISTORY *Hlist;
    int cnt;

    if (Hhead != NULL)
        for (Hlist = Hhead; Hlist != NULL; Hlist = Hlist->next)
            if (!strcmp(Hlist->nummer, nummer)) {
                Hlist->time_list = add_to_list(Hlist->time_list, zeit);
                if (array_cnt(Hlist->time_list) > 20)
                    Hlist->time_list = del_from_list(Hlist->time_list, Hlist->time_list[0]);
                if (Hhead == Htail)
                    return;
                _history_del_node(Hlist);
                Htail->next = Hlist;
                Htail->next->prev = Htail;
                Htail = Htail->next;
                Htail->next = NULL;
                return;
            }

    if (Hhead == NULL) {
        Htail = (HISTORY *) malloc(sizeof(HISTORY));
        Hhead = Htail;
        Hhead->next = NULL;
        Hhead->prev = NULL;
        Hhead->nummer = (char *) strdup(nummer);
        Hhead->time_list = NULL;
        Hhead->time_list = (char **) add_to_list(Hhead->time_list, zeit);
    } else {
        Htail->next = (HISTORY *) malloc(sizeof(HISTORY));
        Htail->next->nummer = (char *) strdup(nummer);
        Htail->next->time_list = NULL;
        Htail->next->time_list = add_to_list(Htail->next->time_list, zeit);
        Htail->next->prev = Htail;
        Htail = Htail->next;
        Htail->next = NULL;
        for (Hlist = Hhead, cnt = 0; Hlist != NULL; Hlist = Hlist->next, cnt++);
        if (cnt > HISTORY_MAX)
            history_del(Hhead);
    }
}


char *gethostipbyname(char *name)
{
    struct hostent *hp;

    if ((hp = gethostbyname(name)) == NULL)
        return NULL;

    return inet_ntoa(*(struct in_addr *) (hp->h_addr_list[0]));
}


int connect_to_host(char *host, int port, int blocking)
{
    struct sockaddr_in serv_addr;
    char *ip = gethostipbyname(host);
    int sockfd, val;

    if (ip == NULL) {
        return -1;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        shutdown(sockfd, 2);
        close(sockfd);
        return -1;
    }

    /* nonblocking please */
    if (!blocking) {
        val = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, val | O_NONBLOCK);
    }

    return sockfd;
}


char *readln(int fd)
{
    char tmp[1024], ch, *pbuf = NULL;
    struct timeval tv;
    fd_set fdset_read;
    int n, size = 0, ready;

    do {
        tv.tv_usec = 50;
        tv.tv_sec = 0;

        FD_ZERO(&fdset_read);
        FD_SET(fd, &fdset_read);
        ready = select(fd + 1, &fdset_read, NULL, NULL, &tv);
        if (ready < 0) {
            syslog(LOG_NOTICE, "select: %s", strerror(errno));
            exit(1);
        }

        // server receive tcp-data
        if (FD_ISSET(fd, &fdset_read)) {
            n = read(fd, &ch, 1);
            if (n < 1) {
                if (n == 0)
                    exit(0);
                syslog(LOG_NOTICE, "read: %s", strerror(errno));
                exit(1);
            }
            tmp[size++] = ch;
            if (ch == '\n') {
                tmp[size] = 0;
                pbuf = dstrcat(pbuf, tmp);
                return pbuf;
            }
        }
    } while (ready);

    return NULL;
}


inline char *trim(char *s)
{
    char *p;

    while (isspace(*s))
        s++;

    if (!*s)
        return s;

    p = s;
    while (*p)
        p++;
    p--;
    while (isspace(*p))
        p--;
    *(p + 1) = 0;

    return s;
}

char **add_to_list(char **list, char *str)
{
    int i = 0;

    if (list == NULL) {
        list = malloc(sizeof(char *) * 2);
    } else {
        for (; list[i] != NULL; i++);
        list = realloc(list, sizeof(char *) * (i + 2));
    }
    list[i] = strdup(str);
    list[i + 1] = NULL;

    return list;
}

char **del_index_from_list(char **list, int i)
{
    if (list == NULL)
        return list;

    if (list[i] != NULL) {
        free(list[i]);
        do
            list[i] = list[i + 1];
        while (list[i++] != NULL);

        if (list[0] == NULL) {
            free(list);
            list = NULL;
        } else
            list = realloc(list, sizeof(char *) * (i + 1));
    }

    return list;
}

char **del_from_list(char **list, char *str)
{
    int i = 0;

    if (list == NULL || str == NULL)
        return list;

    for (; list[i] != NULL && strcmp(list[i], str); i++);

    return del_index_from_list(list, i);
}

inline char *dstrcat(char *buf, char *add)
{
    if (add == NULL)
        return buf;

    if (buf == NULL) {
        buf = malloc(strlen(add) + 1);
        *buf = 0;
    } else
        buf = realloc(buf, strlen(buf) + strlen(add) + 1);

    strcat(buf, add);

    return buf;
}

// return file-size or -1 in case of error

inline off_t getfilesize(char *name)
{
    struct stat st;

    return stat(name, &st) ? (off_t) -1 : (off_t) st.st_size;
}

void daemonize(int verbose_flag)
{
    switch (fork()) {
        case -1:
            syslog(LOG_ERR, "Couldn't fork.");
            fprintf(stderr, "Couldn't fork.\n");
            exit(2);
        case 0:
            /* We are the child */
            break;
        default:
            /* We are the parent */
            sleep(1);
            _exit(0);
    }

    setsid();

    switch (fork()) {
        case -1:
            syslog(LOG_ERR, "Couldn't fork.");
            exit(2);
        case 0:
            /* We are the child */
            break;
        default:
            /* We are the parent */
            sleep(1);
            _exit(0);
    }

    if (verbose_flag == TRUE)
        fprintf(stderr, "* *  >> [process id %d]\n", getpid());

    chdir("/");

    umask(077);
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
}
