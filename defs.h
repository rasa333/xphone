#include <sys/types.h>


#define DEFAULT_PORT 5069
#define DEFAULT_HOST "localhost"

//#define FALSE  0
//#define TRUE   1


extern char **wordlist(char *str, char **arg, int *cnt, int (*trunc)(char c));
extern char **free_char_array(char **a);
extern char *array2pup(char **arr);
extern int array_cnt(char **arr);
extern char *gethostipbyname(char *name);
extern int connect_to_host(char *host, int port, int blocking);
extern char *readln(int fd);
extern char *trim(char *s);
extern char **add_to_list(char **list, char *str);
extern char **del_from_list(char **list, char *str);
extern char *dstrcat(char *buf, char *add);
extern int is_space(char c);
extern off_t getfilesize(char *name);
extern void daemonize(int verbose_flag);
