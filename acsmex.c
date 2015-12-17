#include <mm.h>
#include "acsmex.h"

#define debug_printf(x,y...)
#define _P(x,y...)  printf(x,##y)
#include "test_acsmex.h"

#define acsmex_queue_init(q, pl)                                                    \
    (q)->prev = SHM_GETOFF_P(pl,q);                                                            \
    (q)->next = SHM_GETOFF_P(pl,q)

#define acsmex_queue_empty(h, pl)                                                   \
    ( (h) == SHM_GETPTR(pl, (h)->prev) )

#define acsmex_queue_pool_insert_head(h, x, pl)                                          \
    (x)->next = (h)->next;                                                    \
    ((acsmex_queue_t*)SHM_GETPTR(pl,(x)->next))->prev = SHM_GETOFF_P(pl,x);                                            \
    (x)->prev = SHM_GETOFF_P(pl,h);                                                            \
    (h)->next = SHM_GETOFF_P(pl,x)

#define acsmex_queue_pool_insert_tail(h, x, pl)                                          \
    (x)->prev = (h)->prev;                                                    \
    ((acsmex_queue_t*)SHM_GETPTR(pl,(x)->prev))->next = SHM_GETOFF_P(pl,x);                                        \
    (x)->next = SHM_GETOFF_P(pl,h);                                                            \
    (h)->prev = SHM_GETOFF_P(pl,x)

#define acsmex_queue_head(h)                                                    \
    (h)->next

#define acsmex_queue_last(h)                                                    \
    (h)->prev

#define acsmex_queue_sentinel(h)                                                \
    (h)

#define acsmex_queue_next(q)                                                    \
    (q)->next

#define acsmex_queue_prev(q)                                                    \
    (q)->prev

#define acsmex_queue_remove(x)                                               \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next

#define acsmex_queue_pool_remove(x, pl)                                               \
	((acsmex_queue_t*)SHM_GETPTR(pl,(x)->next))->prev = (x)->prev;                                              \
	((acsmex_queue_t*)SHM_GETPTR(pl,(x)->prev))->next = (x)->next

#define acsmex_queue_data(q, type, link)                                        \
    (type *) ((u_char *) q - offsetof(type, link))

MM* pl;

void acsmex_set_mm_pool(MM* mem_pool)
{
    pl = mem_pool;
    return ;
}

static acsmex_state_queue_t *acsmex_alloc_state_queue(acsmex_context_t *ctx)
{
    acsmex_queue_t       *q;
    acsmex_state_queue_t *sq;

    if (acsmex_queue_empty(&ctx->free_queue.queue, pl)) {
        sq = actest_mm_malloc(pl, sizeof(acsmex_state_queue_t));
    }
    else {
        q = SHM_GETPTR(pl, acsmex_queue_last(&ctx->free_queue.queue));
        acsmex_queue_pool_remove(q, pl);
        sq = acsmex_queue_data(q, acsmex_state_queue_t, queue);
    }

    return sq;
}

static void acsmex_free_state_queue(acsmex_context_t *ctx, acsmex_state_queue_t *sq)
{
	acsmex_queue_pool_remove(&sq->queue, pl);
	acsmex_queue_pool_insert_tail(&ctx->free_queue.queue, &sq->queue, pl);

    sq->state = acsmex_FAIL_STATE;
}

static int acsmex_add_state(acsmex_context_t *ctx, int state)
{
    acsmex_state_queue_t *sq;

    sq = acsmex_alloc_state_queue(ctx);
    if (sq == NULL) {
        return -1;
    }

    sq->state = state;
    acsmex_queue_pool_insert_head(&ctx->work_queue.queue, &sq->queue, pl);

    return 0;
}

static int acsmex_next_state(acsmex_context_t *ctx)
{
    int                 state;
    acsmex_queue_t       *q;
    acsmex_state_queue_t *sq;

    if (acsmex_queue_empty(&ctx->work_queue.queue, pl)) {
        return acsmex_FAIL_STATE;
    }

    q = SHM_GETPTR(pl, acsmex_queue_last(&ctx->work_queue.queue));
    acsmex_queue_pool_remove(q, pl);
    sq = acsmex_queue_data(q, acsmex_state_queue_t, queue);

    state = sq->state;

    acsmex_free_state_queue(ctx, sq);

    return state;
}


static void acsmex_free_state(acsmex_context_t *ctx)
{
    acsmex_queue_t       *q;
    //acsmex_state_queue_t *sq;

    while ((q = SHM_GETPTR(pl, acsmex_queue_last(&ctx->free_queue.queue)))) {
        if (q == acsmex_queue_sentinel(&ctx->free_queue.queue)) {
            break;
        }

        acsmex_queue_pool_remove(q, pl);
        //sq = acsmex_queue_data(q, acsmex_state_queue_t, queue);

        //actest_mm_free(pl, sq);
        actest_mm_free(pl, q);
    }
}


static int acsmex_state_add_match_pattern(acsmex_context_t *ctx, 
        int state, acsmex_pattern_t *p)
{
    acsmex_pattern_t *copy;
    acsmex_state_node_t *p_state_table;

    copy = actest_mm_malloc(pl, sizeof(acsmex_pattern_t));
    if (copy == NULL) {
        return -1;
    }
    memcpy(copy, p, sizeof(acsmex_pattern_t));

    p_state_table = SHM_GETPTR(pl, ctx->state_table);
    copy->next = p_state_table[state].match_list;

    p_state_table[state].match_list = SHM_GETOFF_P(pl, copy);

    return 0;
}


static int acsmex_state_union_match_pattern(acsmex_context_t *ctx, 
        int state, int fail_state)
{
	void *pattern_ptr;
	acsmex_pattern_t *p;

	pattern_ptr = ((acsmex_state_node_t*)SHM_GETPTR(pl, ctx->state_table))[fail_state].match_list;
	p = SHM_GETPTR_N(pl, pattern_ptr);

    while (p) {
        acsmex_state_add_match_pattern(ctx, state, p);
        p=SHM_GETPTR_N(pl, p->next);
    }

    return 0;
}

acsmex_context_t *acsmex_alloc(int flag){
    int no_case = 0;
    acsmex_context_t *ctx;

    if (flag & NO_CASE) {
        no_case = 1;
    }

    ctx = actest_mm_malloc(pl, sizeof(acsmex_context_t));
    if (ctx == NULL) {
        return NULL;
    }

    memset(ctx,0,sizeof(acsmex_context_t));
    ctx->no_case = no_case;
    ctx->max_state = 1;
    ctx->num_state = 0;

    acsmex_queue_init(&ctx->work_queue.queue,pl);
    acsmex_queue_init(&ctx->free_queue.queue,pl);

    return ctx;
}

void acsmex_free(acsmex_context_t *ctx)
{
    unsigned        i;
    acsmex_pattern_t *p, *op;

    acsmex_state_node_t *p_state_table=(acsmex_state_node_t*)SHM_GETPTR(pl, ctx->state_table);

    for (i = 0; i <= ctx->num_state; i++) {
        if (p_state_table[i].match_list) {
            p = SHM_GETPTR_N(pl,p_state_table[i].match_list);
            
            while(p) {
                op = p;
                p = (acsmex_pattern_t*)SHM_GETPTR_N(pl, p->next);

                actest_mm_free(pl, op);
            }
        }
    }
    
    p = (acsmex_pattern_t*)SHM_GETPTR_N(pl, ctx->patterns);
    while (p) {
        op = p;
        p = (acsmex_pattern_t*)SHM_GETPTR_N(pl, p->next);

        if (op->string) {
            actest_mm_free(pl, SHM_GETPTR(pl, op->string));
        }

        actest_mm_free(pl, op);
    }

    actest_mm_free(pl, SHM_GETPTR(pl, ctx->state_table));
    
    actest_mm_free(pl, ctx);
}


int acsmex_add_pattern_callback(acsmex_context_t *ctx, u_char *string, size_t len, void*(*callback)(void* args))
{
    u_char ch;
    size_t i;
    acsmex_pattern_t *p;
    char *s_pattern=NULL;

    p = actest_mm_malloc(pl,sizeof(acsmex_pattern_t));
    if (p == NULL) {
        return -1;
    }
    memset(p,0,sizeof(acsmex_pattern_t));

    if(callback) p->callback = callback;
    
    p->len = len;

    if (len > 0) {
    	s_pattern = actest_mm_malloc(pl,len);
        if (s_pattern == NULL) {
            return -1;
        }

        for(i = 0; i < len; i++) {
            ch = string[i];
            s_pattern[i] = ctx->no_case ? acsmex_tolower(ch) : ch;
        }
    }

    p->string=SHM_GETOFF_P(pl,s_pattern);
    p->next = ctx->patterns;
    ctx->patterns = SHM_GETOFF_P(pl,p);

    debug_printf("add_pattern: \"%.*s\"\n", (int)p->len, string);

    ctx->max_state += len;


    return 0;
}

int acsmex_compile(acsmex_context_t *ctx)
{
    int state, new_state, r, s;
    u_char ch;
    unsigned int i, j, k;
    acsmex_pattern_t *p;
    u_char* pstring;

    acsmex_state_node_t *ctx_state_table = (acsmex_state_node_t*)
    		actest_mm_malloc(pl,ctx->max_state * sizeof(acsmex_state_node_t));

    if (ctx_state_table == NULL) {
        return -1;
    }
    ctx->state_table = SHM_GETOFF_P(pl, ctx_state_table);

    for (i = 0; i < ctx->max_state; i++) {

    	ctx_state_table[i].fail_state = 0;
    	ctx_state_table[i].match_list = NULL;

        for (j = 0; j < ASCIITABLE_SIZE; j++) {
        	ctx_state_table[i].next_state[j] = acsmex_FAIL_STATE;
        }
    }

    debug_printf("goto function\n");

    /* Construction of the goto function */
    new_state = 0;
    p = SHM_GETPTR_N(pl, ctx->patterns);
    while (p) {
        state = 0;
        j = 0;

        pstring = SHM_GETPTR(pl, p->string);

        while(j < p->len) {

            ch = pstring[j];
            if (ctx_state_table[state].next_state[ch] == acsmex_FAIL_STATE) {
                break;
            }

            state = ctx_state_table[state].next_state[ch];
            j++;
        }

        for (k = j; k < p->len; k++) {
            new_state = ++ctx->num_state;
            ch = pstring[k];

            debug_printf("add_match_pattern: state=%d, new_state=%d\n", state, new_state);
            ctx_state_table[state].next_state[ch] = new_state;
            state = new_state;
        }

        debug_printf("add_match_pattern: state=%d, s=%.*s\n", state, (int)p->len, pstring);

        acsmex_state_add_match_pattern(ctx, state, p);

        p = SHM_GETPTR_N(pl, p->next);
    }

    for (j = 0; j < ASCIITABLE_SIZE; j++) {
        if (ctx_state_table[0].next_state[j] == acsmex_FAIL_STATE) {
        	ctx_state_table[0].next_state[j] = 0;
        }
    }

    debug_printf("failure function\n");

    /* Construction of the failure function */
    for (j = 0; j < ASCIITABLE_SIZE; j++) {
        if (ctx_state_table[0].next_state[j] != 0) {
            s = ctx_state_table[0].next_state[j];

            if (acsmex_add_state(ctx, s) != 0) {
                return -1;
            }

            ctx_state_table[s].fail_state = 0;
        }
    }

    while (!acsmex_queue_empty(&ctx->work_queue.queue, pl)) {

        r = acsmex_next_state(ctx);
        debug_printf("next_state: r=%d\n", r);

        for (j = 0; j < ASCIITABLE_SIZE; j++) {
            if (ctx_state_table[r].next_state[j] != acsmex_FAIL_STATE) {
                s = ctx_state_table[r].next_state[j];

                acsmex_add_state(ctx, s);

                state = ctx_state_table[r].fail_state;

                while(ctx_state_table[state].next_state[j] == acsmex_FAIL_STATE) {
                    state = ctx_state_table[state].fail_state;
                }

                ctx_state_table[s].fail_state = ctx_state_table[state].next_state[j];
                debug_printf("fail_state: f[%d] = %d\n", s, ctx_state_table[s].fail_state);

                acsmex_state_union_match_pattern(ctx, s, ctx_state_table[s].fail_state);
            }
            else {
                state = ctx_state_table[r].fail_state;
                ctx_state_table[r].next_state[j] = ctx_state_table[state].next_state[j];
            }
        }
    }

    acsmex_free_state(ctx);
    debug_printf("end of compile function\n");

    return 0;
}


int acsmex_search(acsmex_context_t *ctx, u_char *text, size_t len)
{
    int state = 0;
    u_char *p, *last, ch;
    void* (*callback) (void*);
    size_t match_off;
    acsmex_pattern_t *p_match_list;
    acsmex_state_node_t* ctx_state_table;
    //void* ret=NULL;

    p = text;
    last = text + len;
    ctx_state_table = SHM_GETPTR(pl, ctx->state_table);

    while (p < last) {
        ch = ctx->no_case ? acsmex_tolower((*p)) : (*p);

        while (ctx_state_table[state].next_state[ch] == acsmex_FAIL_STATE) {
            state = ctx_state_table[state].fail_state;
        }

        state = ctx_state_table[state].next_state[ch];

        if (ctx_state_table[state].match_list) {
        	match_off = (size_t)((acsmex_state_node_t*)SHM_GETPTR(pl, ctx->state_table))[state].match_list;
        	p_match_list = SHM_GETPTR(pl, match_off);
        	callback = p_match_list->callback;
            switch((int)(size_t)callback){
            case 1:
            	break;
            case 2:
            	break;
            case 3:
            	_P("%s\n","test case matched");
            	break;
            default:
            	;
            }
        }
        p++;
    }
    return 0;
}
