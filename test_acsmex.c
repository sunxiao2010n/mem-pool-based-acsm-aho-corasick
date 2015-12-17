
#include <stdio.h>
#include <mm.h>
#include <sys/time.h>
#include "acsmex.h"
#include "test_acsmex.h"
#include <stdio.h>

#define debug_printf(x,y...)
#define _P(x,y...)  printf(x,##y)

int actest_mm_global_init(size_t size, const char* key, void** ptr)
{
    void *tmp_ptr = NULL;
    _P("mm size:%d:%s\n", size, key);
    tmp_ptr = mm_core_create(size, key);
    if ( tmp_ptr == NULL )
    {
        _P("actest_mm_global_init() failed: %s", mm_lib_error_get());
        return -1;
    }
    *ptr = tmp_ptr;
    return 0;
}

int actest_mm_shared_init ( size_t size, const char* key, void** ptr)
{
    MM *mm;
    size_t s;
    if ( key == NULL || size == 0)
    {
        _P("key is is not null");
        return -1;
    }
    mm = mm_create(size, key);
    //_P("actest_mm_shared_init size=%ld key=%s mm=%p", size, key, mm);
    if ( mm == NULL )
    {
        _P("actest_mm_shared_init() failed: %s", mm_lib_error_get());
        return -1;
    }
    s = mm_available(mm);
    if ( s == 0 )
    {
        _P("actest_mm_shared_init() mm_available(): %s", mm_lib_error_get());
        return -1;
    }
    //_P("actest_mm_shared_init available=%ld", s);
    *ptr = mm;

    return 0;
}

void* actest_mm_malloc ( MM* mm, int size)
{
    int s;
    void* p;
    mm_lock ( mm, MM_LOCK_RW);
    s = mm_available(mm);
    if ( s <= size)
    {
        _P("no need allocate ; or not enough memory\n");
        return NULL;
    }
    p = mm_malloc ( mm, size);
    mm_unlock(mm);
    return p;
}

void actest_mm_free ( MM* mm, void *ptr)
{
    mm_free ( mm, ptr);
}

void actest_mm_destroy ( MM *mm)
{
    _P("actest_mm_destroy mm=%p", mm);
    mm_destroy ( mm );
}

void actest_mm_available ( MM* mm){
        size_t s = mm_available(mm);
        debug_printf("mm available is %d\n", s);
}

static char *log_patterns[] = {"Good luck",NULL};
char *good_luck_text =
        "Deep Packet Inspection (DPI, also called complete packet inspection "
        "and Information eXtraction or IX) is a form of computer network packet filtering "
        "that examines the data part (and possibly also the header) of a packet as it passes "
        "an inspection point, searching for protocol non-compliance, viruses, spam, intrusions, "
        "or defined criteria to decide whether the packet may pass or if it needs to be routed "
        "to a different destination, or, for the purpose of collecting statistical information. "
        "There are multiple headers for IP packets; network equipment only needs to use "
        "the first of these (the IP header) for normal operation, but use of the second header "
        "(TCP, UDP etc.) is normally considered to be shallow packet inspection (usually called "
        "Stateful Packet Inspection) despite this definition."
        "Good luck!";
char *bad_luck_text =
        "Deep Packet Inspection (DPI, also called complete packet inspection "
        "and Information eXtraction or IX) is a form of computer network packet filtering "
        "that examines the data part (and possibly also the header) of a packet as it passes "
        "an inspection point, searching for protocol non-compliance, viruses, spam, intrusions, "
        "or defined criteria to decide whether the packet may pass or if it needs to be routed "
        "to a different destination, or, for the purpose of collecting statistical information. "
        "There are multiple headers for IP packets; network equipment only needs to use "
        "the first of these (the IP header) for normal operation, but use of the second header "
        "(TCP, UDP etc.) is normally considered to be shallow packet inspection (usually called "
        "Stateful Packet Inspection) despite this definition."
        "Bad luck!";
struct timeval g_time;

int LOGTIME(char *string,unsigned int begin)
{
    struct timeval tv;
    unsigned int nowtime;
    gettimeofday(&tv, NULL);
    nowtime = tv.tv_sec * 1000000 + tv.tv_usec;
    _P("%s: time spend [%u.%06u]\n",string,(nowtime-begin)/1000000,(nowtime-begin));
    return 0;
}

MM* pool;
int main()
{
    int ii;
    u_char         **input;
    acsmex_context_t  *ctx;

    gettimeofday(&g_time,NULL);
    int begintime = g_time.tv_sec*1000000 + g_time.tv_usec;

    actest_mm_shared_init(67096536,"mm_key_file",&pool);
    actest_mm_available(pool);

    LOGTIME("mm_shared_init ", begintime);

    acsmex_set_mm_pool(pool);

    ctx = acsmex_alloc(NO_CASE);
    if (ctx == NULL) {
        fprintf(stderr, "acsmex_alloc() error.\n");
        return -1;
    }

    actest_mm_available(pool);
    input = (u_char**) log_patterns;
    while(*input) {
        if (acsmex_add_pattern_callback(ctx, *input, acsmex_strlen(*input),((void*)3)) != 0) {
            fprintf(stderr, "acsmex_add_pattern() with pattern \"%s\" error.\n",
                    *input);
            return -1;
        }
        input++;
    }

    actest_mm_available(pool);
    debug_printf("after add_pattern: max_state=%d\n", ctx->max_state);

    if (acsmex_compile(ctx) != 0) {
        fprintf(stderr, "acsmex_compile() error.\n");
        return -1;
    }

    LOGTIME("acsmex_init ", begintime);

    acsmex_search(ctx, (u_char *)good_luck_text, acsmex_strlen(good_luck_text));

    for(ii=100; ii>0; ii--){
        acsmex_search(ctx, (u_char *)bad_luck_text, acsmex_strlen(bad_luck_text));
    }
    _P("search length %ld \n",strlen(bad_luck_text));

    LOGTIME("acsmex_search 100 times", begintime);

    acsmex_free(ctx);
    actest_mm_available(pool);

    return 0;
}
