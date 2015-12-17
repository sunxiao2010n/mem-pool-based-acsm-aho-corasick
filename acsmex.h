#ifndef _acsmex_H_
#define _acsmex_H_

#include <sys/types.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

#define ASCIITABLE_SIZE    (256)     

#define PATTERN_MAXLEN   (1024) 

#define acsmex_FAIL_STATE  (-1)     


typedef struct acsmex_queue_s {
    struct acsmex_queue_s  *prev;
    struct acsmex_queue_s  *next;
} acsmex_queue_t;

typedef struct {
    int          state;
    acsmex_queue_t queue;
} acsmex_state_queue_t;


typedef struct acsmex_pattern_s {
    u_char        *string;
    size_t         len;
    void* (*callback) (void*);
    struct acsmex_pattern_s *next;
} acsmex_pattern_t;


typedef struct {
    int next_state[ASCIITABLE_SIZE];
    int fail_state;
    /* output */
    acsmex_pattern_t *match_list;
} acsmex_state_node_t;


typedef struct {
    unsigned max_state;
    unsigned num_state;

    acsmex_pattern_t    *patterns;
    acsmex_state_node_t *state_table;

    void *pool;

    acsmex_state_queue_t work_queue;
    acsmex_state_queue_t free_queue;

    unsigned no_case;
} acsmex_context_t;


#define acsmex_tolower(c)      (u_char) ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)

#define acsmex_strlen(s)       strlen((const char *) s)


#define NO_CASE 0x01

acsmex_context_t *acsmex_alloc(int flag);
void acsmex_free(acsmex_context_t *ctx);

int acsmex_add_pattern(acsmex_context_t *ctx, u_char *string, size_t len);
int acsmex_compile(acsmex_context_t *ctx);
int acsmex_search(acsmex_context_t *ctx, u_char *string, size_t len);
int acsmex_add_pattern_callback(acsmex_context_t *ctx, u_char *string, size_t len, void*(*callback)(void* args)) ;

/* user defined calllbacks */
#define godefault(x)

void acsmex_set_mm_pool(MM* mem_pool);

#endif /* _acsmex_H_ */
