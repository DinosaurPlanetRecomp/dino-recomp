#include <cstring>

#include "librecomp/helpers.hpp"
#include "recomp.h"

extern "C" void bcopy_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) srcAddr = _arg<0, PTR(void)>(rdram, ctx);
    PTR(void) dstAddr = _arg<1, PTR(void)>(rdram, ctx);
    int length = _arg<2, int>(rdram, ctx);

    int src = srcAddr - 0xFFFFFFFF80000000;
    int dst = dstAddr - 0xFFFFFFFF80000000;

    if ((src % 4) == 0 && (dst % 4) == 0) {
        // Word copies
        int alignedLength = length & ~3;
        void *srcPtr = (int8_t*)(rdram + src);
        void *dstPtr = (int8_t*)(rdram + dst);
        memcpy(dstPtr, srcPtr, alignedLength);

        int lengthMisalignment = length & 3;
        for (size_t i = 0; i < lengthMisalignment; i++) {
            MEM_B(i + alignedLength, dstAddr) = MEM_B(i + alignedLength, srcAddr);
        }
    } else if ((src % 2) == 0 && (dst % 2) == 0) {
        // Half copies
        int alignedLength = length & ~1;
        for (size_t i = 0; i < alignedLength; i += 2) {
            MEM_HU(i, dstAddr) = MEM_HU(i, srcAddr);
        }

        int lengthMisalignment = length & 1;
        for (size_t i = 0; i < lengthMisalignment; i++) {
            MEM_B(i + alignedLength, dstAddr) = MEM_B(i + alignedLength, srcAddr);
        }
    } else {
        // Byte copies
        for (size_t i = 0; i < length; i++) {
            MEM_B(i, dstAddr) = MEM_B(i, srcAddr);
        }
    }
}

extern "C" void bcopy_zero(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) dstAddr = _arg<0, PTR(void)>(rdram, ctx);
    int length = _arg<1, int>(rdram, ctx);

    int dst = dstAddr - 0xFFFFFFFF80000000;

    if ((dst % 4) == 0) {
        // Word copies
        int alignedLength = length & ~3;
        void *dstPtr = (int8_t*)(rdram + dst);
        memset(dstPtr, 0, alignedLength);

        int lengthMisalignment = length & 3;
        for (size_t i = 0; i < lengthMisalignment; i++) {
            MEM_B(i + alignedLength, dstAddr) = 0;
        }
    } else if ((dst % 2) == 0) {
        // Half copies
        int alignedLength = length & ~1;
        for (size_t i = 0; i < alignedLength; i += 2) {
            MEM_HU(i, dstAddr) = 0;
        }

        int lengthMisalignment = length & 1;
        for (size_t i = 0; i < lengthMisalignment; i++) {
            MEM_B(i + alignedLength, dstAddr) = 0;
        }
    } else {
        // Byte copies
        for (size_t i = 0; i < length; i++) {
            MEM_B(i, dstAddr) = 0;
        }
    }
}
