#include "patches.h"
#include "recomp_funcs.h"

#undef bzero

RECOMP_PATCH void bzero(void *dst, int length) {
    // Use reimplemented version
    recomp_bzero(dst, length);
}
