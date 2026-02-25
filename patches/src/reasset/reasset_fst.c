#include "reasset_fst.h"

#include "patches.h"

#include "reasset.h"

#include "PR/ultratypes.h"
#include "PR/os.h"
#include "sys/fs.h"
#include "sys/memory.h"

typedef struct {
    void *data;
    u32 size;
    _Bool ownedByReAsset;
} FstExtEntry;

static FstExtEntry fstReplacements[NUM_FILES];
static Fs *originalFst;

void reasset_fst_init(void) {
    // Save vanilla FS.tab
    originalFst = gFST;
}

void reasset_fst_rebuild(void) {
    // Alloc new FS.tab and build it
    s32 size = (s32)&__file1Address - (s32)&__fstAddress;
    gFST = (Fs *)recomp_alloc(size);
    gFST->fileCount = originalFst->fileCount;

    u32 offset = 0;
    u32 i;
    for (i = 0; i < originalFst->fileCount; i++) {
        FstExtEntry *replacement = &fstReplacements[i];
        gFST->offsets[i] = offset;

        //reasset_log("[reasset] %04x -> %s.\n", (s32)&__file1Address + offset, DINO_FS_FILENAMES[i]);

        if (replacement->data != NULL) {
            offset += replacement->size;
        } else {
            offset += originalFst->offsets[i + 1] - originalFst->offsets[i];
        }

        offset = mmAlign2(offset);
    }

    gFST->offsets[i] = offset;

    reasset_log("[reasset] Rebuilt FS.tab.\n");
}

void reasset_fst_set_internal(s32 fileID, void *data, u32 size, _Bool ownedByReAsset) {
    reasset_assert(fileID >= 0 && fileID < NUM_FILES, "[reasset] File ID out of bounds: %d", fileID);

    FstExtEntry *replacement = &fstReplacements[fileID];
    if (replacement->data != NULL && replacement->ownedByReAsset) {
        if (replacement->data != data) {
            recomp_free(replacement->data);
        }
    }
    
    replacement->data = data;
    replacement->size = size;
    replacement->ownedByReAsset = ownedByReAsset;
}

RECOMP_EXPORT void reasset_fst_set(s32 fileID, const void *data, u32 size) {
    reasset_fst_set_internal(fileID, (void*)data, size, /*ownedByReAsset=*/FALSE);

    reasset_log("[reasset] Set FST file replacement for %s.\n", DINO_FS_FILENAMES[fileID]);
}

u32 reasset_fst_get_file_size(s32 fileID) {
    FstExtEntry *replacement = &fstReplacements[fileID];
    if (replacement->data != NULL) {
        return replacement->size;
    } else {
        return gFST->offsets[fileID + 1] - gFST->offsets[fileID];
    }
}

void reasset_fst_read_from_file(s32 fileID, void *dst, u32 offset, u32 size) {
    FstExtEntry *replacement = &fstReplacements[fileID];

    // Do bounds check
    u32 fileSize;
    if (replacement->data != NULL) {
        fileSize = replacement->size;
    } else {
        fileSize = gFST->offsets[fileID + 1] - gFST->offsets[fileID];
    }
    if (fileID == AMAP_TAB) {
        // HACK: The game reads out of bounds in AMAP.tab when reading the last couple entries. It doesn't actually
        //       use this data so we can cheat here and pretend it's big enough.
        fileSize += 0x14;
    } else if (fileID == ANIM_TAB) {
        fileSize += 0x4; // :(
    }
    if (!(*((s32*)&offset) >= 0 && offset < fileSize && (*((s32*)&offset) + size) >= 0 && (offset + size) <= fileSize)) {
        reasset_log_error(
            "[reasset] fst_ext_read_from_file(%s, %p, 0x%X, 0x%X) out of bounds read! file size: 0x%X", 
            DINO_FS_FILENAMES[fileID], dst, offset, size, fileSize);
    }

    // Read
    if (replacement->data != NULL) {
        // Read replacement file
        bcopy((u8*)replacement->data + offset, dst, size);
        //reasset_log("[reasset] Reading from FST file replacement for %d.\n", fileID);
    } else {
        // Read original ROM
        u32 fileOffset = originalFst->offsets[fileID];

        read_from_rom((u32)&__file1Address + fileOffset + offset, dst, size);
    }
}

void* reasset_fst_alloc_load_file(s32 fileID, u32 *outSize) {
    u32 size = reasset_fst_get_file_size(fileID);
    void *ptr = recomp_alloc(size);

    reasset_fst_read_from_file(fileID, ptr, 0, size);

    if (outSize != NULL) {
        *outSize = size;
    }

    return ptr;
}

s32 reasset_fst_audio_dma(void *dst, u32 romAddr, u32 size) {
    romAddr -= (u32)&__file1Address;
    
    s32 fileID = -1;
    u32 offset = 0;

    for (u32 i = 0; i < gFST->fileCount; i++) {
        u32 entryOffset = gFST->offsets[i];
        u32 entryEndOffset = gFST->offsets[i + 1];

        if (romAddr >= entryOffset && romAddr < entryEndOffset) {
            fileID = i;
            offset = romAddr - entryOffset;
            break;
        }
    }

    if (fileID == -1) {
        return FALSE;
    }

    FstExtEntry *replacement = &fstReplacements[fileID];
    if (replacement->data == NULL) {
        return FALSE;
    }

    //reasset_log("[reasset] [AUDIO DMA] Reading from FST file replacement for %d. %x   %x\n", fileID, offset, size);

    bcopy((u8*)replacement->data + offset, dst, size);
    return TRUE;
}
