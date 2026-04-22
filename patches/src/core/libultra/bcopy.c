#include "patches.h"
#include "recomp_funcs.h"

#undef bcopy

RECOMP_PATCH void bcopy(const void *src, void *dst, int length) {
    // Use reimplemented version
    recomp_bcopy(src, dst, length);
}
