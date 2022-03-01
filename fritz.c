#include <signal.h>
#include <syslog.h>

#include "defs.h"
#include "history.h"
#include "linphone/core.h"


HISTORY *Hhead = NULL;
HISTORY *Htail = NULL;
char log_file[1024];

static bool_t running = TRUE;

static void stop(int signum)
{
    running = FALSE;
}

int auto_answer = FALSE;
int linphonec_camera_enabled = FALSE;
int answer_call = FALSE;
int real_early_media_sending = FALSE;
int vcap_enabled = FALSE;
char caller_name[1024];
long callid = 0;

#define INT_TO_VOIDPTR(i) ((void*)(intptr_t)(i))
#define VOIDPTR_TO_INT(p) ((int)(intptr_t)(p))

void linphonec_call_identify(LinphoneCall *call)
{
    static int callid = 1;
    linphone_call_set_user_pointer (call, INT_TO_VOIDPTR(callid));
    callid++;
}


static void linphonec_set_caller(const char *caller)
{
    snprintf(caller_name, sizeof(caller_name) - 1, "%s", caller);
}


static void linphonec_call_state_changed(LinphoneCore *lc, LinphoneCall *call, LinphoneCallState st, const char *msg)
{
    char *from = linphone_call_get_remote_address_as_string(call);
    //  int id=VOIDPTR_TO_INT(linphone_call_get_user_pointer(call));
    FILE *f;
    char datetime[256], *p, *number;
    time_t t;
    struct tm *tm;

    switch (st) {
        case LinphoneCallIncomingReceived:
            linphonec_call_identify(call);
            //    linphone_call_enable_camera (call,linphonec_camera_enabled);
            //    id=VOIDPTR_TO_INT(linphone_call_get_user_pointer(call));
            linphonec_set_caller(from);
            number = from;
            t = time(NULL);
            tm = localtime(&t);
            snprintf(datetime, sizeof(datetime), "%2.2d.%2.2d.%d %2.2d:%2.2d", tm->tm_mday, tm->tm_mon + 1,
                     tm->tm_year + 1900, tm->tm_hour, tm->tm_min);

            p = strchr(number, ':');
            if (p != NULL)
                number = p + 1;
            p = strchr(number, '@');
            if (p != NULL)
                *p = 0;
            f = fopen(log_file, "a");
            if (f == NULL) {
                perror(log_file);
                exit(1);
            }
            fprintf(f, "%s %s\n", datetime, number);
            fclose(f);
            break;
        default:
            break;
    }
    ms_free(from);
}


char setting_password[256], setting_identity[256];

static void fritz_config_read(char *file)
{
    FILE *f;
    char buf[1024], *p, *tr, *re;
    int cnt = 0;

    f = fopen(file, "r");
    if (f == NULL) {
        perror(file);
        exit(1);
    }
    while (fgets(buf, sizeof(buf), f)) {
        cnt++;
        if ((p = strchr(buf, '\n')) != NULL)
            *p = 0;
        if ((p = strchr(buf, '#')) != NULL)
            *p = 0;
        if ((p = strchr(buf, '=')) == NULL) {
            syslog(LOG_ERR, "%s: line %d: expected '='", file, cnt);
            exit(1);
        }
        *p = 0;
        tr = buf;
        re = p + 1;
        if (!strcasecmp(tr, "id")) {
            strcpy(setting_identity, re);
        } else if (!strcasecmp(tr, "pw")) {
            strcpy(setting_password, re);
        }
    }
    fclose(f);
}


LinphoneCore *lc;

void usage(char *config_file, char *log_file)
{
    printf("Usage: xphone [options]\n");
    printf("    -h          this help\n");
    printf("    -c file     authentication config file [%s]\n", config_file);
    printf("    -l file     log file [%s]\n", log_file);
}



int main(int argc, char *argv[])
{
    LinphoneCoreVTable vtable = {0};
    LinphoneProxyConfig *proxy_cfg;
    LinphoneAddress *from;
    LinphoneAuthInfo *info;
    char *identity = NULL;
    char *password = NULL;
    char config_file[1024];
    int opt;
    const char *server_addr;
    extern char *optarg;

    setting_identity[0] = 0;
    setting_password[0] = 0;
    config_file[0] = 0;
    log_file[0] = 0;

    snprintf(config_file, sizeof(config_file), "%s/.xphone/config", getenv("HOME"));
    snprintf(log_file, sizeof(log_file), "%s/.xphone/log", getenv("HOME"));
    while ((opt = getopt(argc, argv, "c:l:h")) != -1) {
        switch (opt) {
            case 'c':
                strcpy(config_file, optarg);
                break;
            case 'l':
                strcpy(log_file, optarg);
                break;
            case 'h':
                usage(config_file, log_file);
                exit(0);
            default:
                usage(config_file, log_file);
                exit(1);
        }
    }

    fritz_config_read(config_file);
    /* takes   sip uri  identity from the command line arguments */
    if (argc > 1) {
        identity = argv[1];
    }

    /* takes   password from the command line arguments */
    if (argc > 2) {
        password = argv[2];
    } else {
        identity = setting_identity;
        password = setting_password;
    }
    daemonize(TRUE);
    signal(SIGINT, stop);

#ifdef DEBUG_LOGS
    linphone_core_enable_logs(NULL); /*enable liblinphone logs.*/
#endif
    /*
     Fill the LinphoneCoreVTable with application callbacks.
     All are optional. Here we only use the registration_state_changed callbacks
     in order to get notifications about the progress of the registration.
     */

    vtable.call_state_changed = linphonec_call_state_changed;
    //	vtable.registration_state_changed=registration_state_changed;

    /*
     Instanciate a LinphoneCore object given the LinphoneCoreVTable
    */
    lc = linphone_core_new(&vtable, NULL, NULL, NULL);

    /*create proxy config*/
    proxy_cfg = linphone_core_create_proxy_config(lc);
    /*parse identity*/
    from = linphone_address_new(identity);
    if (from == NULL) {
        printf("%s not a valid sip uri, must be like sip:toto@sip.linphone.org \n", identity);
        goto end;
    }
    if (password != NULL) {
        info = linphone_auth_info_new(linphone_address_get_username(from), NULL, password, NULL, NULL,
                                      NULL); /*create authentication structure from identity*/
        linphone_core_add_auth_info(lc, info); /*add authentication info to LinphoneCore*/
    }

    // configure proxy entries
    linphone_proxy_config_set_identity_address(proxy_cfg, from); /*set identity with user name and domain*/
    server_addr = linphone_address_get_domain(from); /*extract domain address from identity*/
    linphone_proxy_config_set_server_addr(proxy_cfg, server_addr); /* we assume domain = proxy server address*/
    linphone_proxy_config_enable_register(proxy_cfg, TRUE); /*activate registration for this proxy config*/
    linphone_address_unref(from); /*release resource*/

    linphone_core_add_proxy_config(lc, proxy_cfg); /*add proxy config to linphone core*/
    linphone_core_set_default_proxy_config(lc, proxy_cfg); /*set to default proxy*/


    /* main loop for receiving notifications and doing background linphonecore work: */
    while (running) {
        linphone_core_iterate(lc); /* first iterate initiates registration */
        usleep(50000);
    }

    proxy_cfg = linphone_core_get_default_proxy_config(lc); /* get default proxy config*/
    linphone_proxy_config_edit(proxy_cfg); /*start editing proxy configuration*/
    linphone_proxy_config_enable_register(proxy_cfg, FALSE); /*de-activate registration for this proxy config*/
    linphone_proxy_config_done(proxy_cfg); /*initiate REGISTER with expire = 0*/

    while (linphone_proxy_config_get_state(proxy_cfg) != LinphoneRegistrationCleared) {
        linphone_core_iterate(lc); /*to make sure we receive call backs before shutting down*/
        usleep(50000);
    }

    end:
    printf("Shutting down...\n");
    linphone_core_destroy(lc);
    printf("Exited\n");
    return 0;
}
