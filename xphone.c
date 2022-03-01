#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <forms.h>
#include <string.h>
#include <libnotify/notify.h>

#include "defs.h"
#include "history.h"

HISTORY *Hhead = NULL;
HISTORY *Htail = NULL;


FL_OBJECT *history_quit = NULL;
FL_OBJECT *history_pup[HISTORY_MAX];
FL_FORM *history_form;


void usage(char *host, int port, int x, int y)
{
    printf("Usage: xphone [options]\n");
    printf("    -h          this help\n");
    printf("    -x pixel    x-coords for history-button [%d]\n", x);
    printf("    -y pixel    y-coords for history-button [%d]\n", y);
    printf("    -s host     xphone_broker host [%s]\n", host);
    printf("    -p port     xphone_broker port [%d]\n", port);
}

void history_display()
{
    HISTORY *Hlist;
    int i = 0, j = 0;

    history_form = fl_bgn_form(FL_FLAT_BOX, 450, 460);
    history_quit = fl_add_button(FL_NORMAL_BUTTON, 180, 430, 80, 25, "Schliessen");
    for (Hlist = Htail; Hlist != NULL; Hlist = Hlist->prev) {
        history_pup[j] = fl_add_button(FL_MENU_BUTTON, 0, 1 + i, 110, 20,
                                       Hlist->time_list[array_cnt(Hlist->time_list) - 1]);
        fl_set_object_lsize(history_pup[j], FL_TINY_SIZE);
        fl_set_object_color(history_pup[j++], FL_PALEGREEN, FL_PALEGREEN);
        fl_add_text(FL_NORMAL_TEXT, 110, 1 + i, 210, 20, Hlist->nummer);
        i += 20;
    }
    fl_end_form();
    fl_show_form(history_form, FL_PLACE_FREE, FL_NOBORDER, "");
}


int main(int argc, char **argv)
{
    int fd, port = DEFAULT_PORT;
    char *host = strdup(DEFAULT_HOST);
    int acnt = 0, i;
    char **arr = NULL;
    char line[1024], date[1024], *p;
    FL_FORM *form;
    FL_OBJECT *obj, *history_but;
    HISTORY *Hlist;
    int x = 1900, y = 1060, opt;
    extern char *optarg;

    while ((opt = getopt(argc, argv, "x:y:p:s:h")) != -1) {
        switch (opt) {
            case 'x':
                x = atoi(optarg);
                break;
            case 'y':
                y = atoi(optarg);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 's':
                host = strdup(optarg);
                break;
            case 'h':
                usage(host, port, x, y);
                exit(0);
            default:
                usage(host, port, x, y);
                exit(1);
        }
    }

    fd = connect_to_host(host, port, FALSE);
    if (fd == -1) {
        perror("socket");
        exit(1);
    }
    fl_initialize(&argc, argv, "xphone", 0, 0);
    daemonize(TRUE);

    form = fl_bgn_form(FL_UP_BOX, 20, 20);
    history_but = fl_add_button(FL_NORMAL_BUTTON, 0, 0, 20, 20, " ");
    fl_set_object_color(history_but, FL_BLACK, FL_BLACK);
    fl_end_form();
    fl_show_form(form, FL_PLACE_HOTSPOT, FL_NOBORDER, "");
    fl_set_form_position(form, x, y);
    fl_check_forms();

    for (;;) {
        p = readln(fd);
        if (p != NULL) {
            arr = wordlist(p, arr, &acnt, is_space);
            if (p[0] == '+' && arr != NULL && acnt == 4) {
                sprintf(date, "%s %s", arr[1], arr[2]);
                history_add(date, arr[3]);
            } else if (acnt == 3) {
                sprintf(date, "%s %s", arr[0], arr[1]);
                history_add(date, arr[2]);
                snprintf(line, sizeof(line), "(%s) %s", arr[1], arr[2]);
                notify_init("xphone");
                NotifyNotification *n = notify_notification_new (line, "", 0);
                notify_notification_set_timeout(n, 20000); // 20 seconds
                notify_notification_show(n, 0);
                fl_check_forms();
            }
            free(p);
            p = NULL;
            acnt = 0;
        }
        obj = fl_check_forms();
        if (obj == history_but) {
            fl_deactivate_form(form);
            history_display();
        }
        if (history_quit != NULL && obj == history_quit) {
            fl_hide_form(history_form);
            fl_free_form(history_form);
            fl_activate_form(form);
            history_quit = NULL;
        }
        if (history_quit != NULL) {
            for (Hlist = Htail, i = 0; Hlist != NULL; Hlist = Hlist->prev, i++) {
                if (obj == history_pup[i]) {
                    char *s = array2pup(Hlist->time_list);
                    int x;
                    if (s == NULL)
                        break;
                    x = fl_defpup(FL_ObjWin(obj), s);
                    fl_dopup(x);
                    free(s);
                }
            }
        }
        continue;
    }
}
