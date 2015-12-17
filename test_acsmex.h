

#ifndef __ACTEST_MM_SHARED__
#define __ACTEST_MM_SHARED__

#include <mm.h>

#define SHM_GETPTR(base, offset) ((void*)((char *)(base) + (size_t)(offset)))
#define SHM_GETOFF(base, ptr)    ((size_t)((char *)(ptr) - (char *)(base)))
#define SHM_GETPTR_N(base, offset)  \
        offset==NULL?NULL:((void*)((char *)(base) + (size_t)(offset)))
#define SHM_GETOFF_P(base, ptr)    ((void*)((char *)(ptr) - (char *)(base)))

int actest_mm_shared_init ( size_t size, const char *key, void **ptr );
void* actest_mm_malloc ( MM* mm, int size);
void actest_mm_free( MM* mm, void*);
void actest_mm_destroy ( MM *mm);
int actest_mm_global_init(size_t size, const char* key, void** ptr);
void actest_mm_available ( MM* mm);
#endif

