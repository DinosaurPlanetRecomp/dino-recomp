#include "patches.h"
#include "reasset.h"
#include "reasset/reasset_fst.h"

#include "PR/ultratypes.h"
#include "PR/os.h"
#include "sys/pi.h"
#include "sys/memory.h"
#include "macros.h"

extern OSIoMesg romcopy_OIMesg;
extern OSMesg D_800B2E48[1];
extern OSMesgQueue romcopy_mesgq;
extern OSMesg D_800B2E68[16];
extern OSMesgQueue D_800B2EA8;
extern Fs *gFST;
extern u32 gLastFSTIndex;

RECOMP_PATCH void piInit(void) {
    s32 size;

    osCreateMesgQueue(&D_800B2EA8, D_800B2E68, ARRAYCOUNT(D_800B2E68));
    osCreateMesgQueue(&romcopy_mesgq, D_800B2E48, ARRAYCOUNT(D_800B2E48));

    osCreatePiManager(OS_PRIORITY_PIMGR, &D_800B2EA8, D_800B2E68, ARRAYCOUNT(D_800B2E68));

    // A4AA0 - A4970
    size = __file1Address - __fstAddress;

    gFST = (Fs *)mmAlloc(size, COLOUR_TAG_GREY, NULL);
    romCopy((u32)__fstAddress, (u8 *)gFST, size);

    // @recomp: Run reasset system
    reasset_run();
}

RECOMP_PATCH void *piRomLoad(u32 id, u32 a1) {
    void *data;

    if (id > gFST->fileCount)
        return NULL;

    // @recomp: Rewrite to use reasset
    u32 size = reasset_fst_get_file_size(id);

    data = mmAlloc(size, COLOUR_TAG_GREY, NULL);
    if (data == NULL)
        return NULL;

    reasset_fst_read_from_file(id, data, 0, size);

    return data;
}

RECOMP_PATCH s32 piRomLoadToDest(u32 id, void *dest) {
    if (id > gFST->fileCount)
        return NULL;

    // @recomp: Rewrite to use reasset
    u32 size = reasset_fst_get_file_size(id);
    reasset_fst_read_from_file(id, dest, 0, size);

    return size;
}

RECOMP_PATCH s32 piRomLoadSection(u32 id, void *dst, u32 offset, s32 size) {
    if (size == 0 || id > gFST->fileCount)
        return 0;

    // @recomp: Rewrite to use reasset
    gLastFSTIndex = id + 1;

    reasset_fst_read_from_file(id, dst, offset, size);

    return size;
}
