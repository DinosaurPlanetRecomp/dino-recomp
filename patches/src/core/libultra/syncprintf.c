#include "patches.h"
#include "recomp_funcs.h"

// @recomp: Hook up game printf to recomp logging
RECOMP_PATCH void* proutSyncPrintf(void* dst, const char* buf, s32 size) {
    recomp_puts(buf, size);
    return (void*)1;
}
