#pragma once

#include "PR/ultratypes.h"
#include "sys/fs.h"

extern Fs *gFST;
extern u32 gLastFSTIndex;
extern s32 __fstAddress;
extern s32 __file1Address;

void read_from_rom(u32 romAddr, u8* dst, s32 size);

// // // //

#ifndef MAX
#define MAX(a, b)				((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b)				((a) < (b) ? (a) : (b))
#endif

typedef enum {
    REASSET_STAGE_UNINITIALIZED,
    REASSET_STAGE_FST_SET,
    REASSET_STAGE_SET,
    REASSET_STAGE_MODIFY,
    REASSET_STAGE_POST,
    REASSET_STAGE_COMMITTED
} ReAssetStage;

extern ReAssetStage reassetStage;
extern const char *DINO_FS_FILENAMES[NUM_FILES];

void reasset_run(void);

void reasset_assert(_Bool condition, const char *fmt, ...);
void reasset_assert_no_exit(_Bool condition, const char *fmt, ...);
void reasset_log(const char *fmt, ...);
void reasset_log_error(const char *fmt, ...);
void reasset_error_box(const char *fmt, ...);
