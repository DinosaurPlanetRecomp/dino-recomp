#include "patches.h"
#include "reasset.h"
#include "reasset/reasset_fst.h"

#include "PR/ultratypes.h"
#include "PR/os.h"
#include "sys/fs.h"
#include "sys/memory.h"
#include "macros.h"

extern s32 __fstAddress;
extern s32 __file1Address;

void read_from_rom(u32 romAddr, u8* dst, s32 size);

extern OSIoMesg romcopy_OIMesg;
extern OSMesg D_800B2E48[1];
extern OSMesgQueue romcopy_mesgq;
extern OSMesg D_800B2E68[16];
extern OSMesgQueue D_800B2EA8;
extern Fs *gFST;
extern u32 gLastFSTIndex;

RECOMP_PATCH void init_filesystem(void)
{
    s32 size;

    osCreateMesgQueue(&D_800B2EA8, D_800B2E68, ARRAYCOUNT(D_800B2E68));
    osCreateMesgQueue(&romcopy_mesgq, D_800B2E48, ARRAYCOUNT(D_800B2E48));

    osCreatePiManager(0x96, &D_800B2EA8, D_800B2E68, ARRAYCOUNT(D_800B2E68));

    // A4AA0 - A4970
    size = (s32)&__file1Address - (s32)&__fstAddress;

    gFST = (Fs *)mmAlloc(size, COLOUR_TAG_GREY, NULL);
    read_from_rom((u32)&__fstAddress, (u8 *)gFST, size);

    // @recomp: Run reasset system
    reasset_run();
}

RECOMP_PATCH void *read_alloc_file(u32 id, u32 a1)
{
    void *data;

    if (id > (u32)gFST->fileCount)
        return NULL;

    // @recomp: Rewrite to use reasset
    u32 size = reasset_fst_get_file_size(id);

    data = mmAlloc(size, COLOUR_TAG_GREY, NULL);
    if (data == NULL)
        return NULL;

    reasset_fst_read_from_file(id, data, 0, size);

    return data;
}

RECOMP_PATCH s32 read_file(u32 id, void *dest)
{
    if (id > (u32)gFST->fileCount)
        return NULL;

    // @recomp: Rewrite to use reasset
    u32 size = reasset_fst_get_file_size(id);
    reasset_fst_read_from_file(id, dest, 0, size);

    return size;
}

RECOMP_PATCH s32 read_file_region(u32 id, void *dst, u32 offset, s32 size)
{
    if (size == 0 || id > (u32)gFST->fileCount)
        return 0;

    // @recomp: Rewrite to use reasset
    gLastFSTIndex = id + 1;

    reasset_fst_read_from_file(id, dst, offset, size);

    return size;
}
