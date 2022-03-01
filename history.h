
#define HISTORY_MAX    20

struct history {
  char           *nummer;
  char          **time_list;
  struct history *next;
  struct history *prev;
};
typedef struct history HISTORY;

extern void history_del(HISTORY *x);
extern void history_add(char *zeit, char *nummer);
