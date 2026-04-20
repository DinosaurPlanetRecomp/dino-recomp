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

typedef int ReAssetBool;

typedef enum {
    REASSET_LOGLEVEL_WARNING = 0,
    REASSET_LOGLEVEL_INFO = 1,
    REASSET_LOGLEVEL_DEBUG = 2
} ReAssetLogLevel;

typedef enum {
    REASSET_STAGE_UNINITIALIZED,
    REASSET_STAGE_FST_SET,
    REASSET_STAGE_SET,
    REASSET_STAGE_MODIFY,
    REASSET_STAGE_RESOLVE,
    REASSET_STAGE_COMMITTED
} ReAssetStage;

extern ReAssetStage reassetStage;
extern const char *DINO_FS_FILENAMES[NUM_FILES];

void reasset_run(void);

void reasset_assert_stage_set_call(const char *functionName);
void reasset_assert_stage_get_call(const char *functionName);
void reasset_assert_stage_delete_call(const char *functionName);
void reasset_assert_stage_iterator_call(const char *functionName);
void reasset_assert_stage_link_call(const char *functionName);
void reasset_assert_stage_get_resolve_map_call(const char *functionName);

_Bool reasset_is_debug_logging_enabled(void);

void reasset_assert(_Bool condition, const char *fmt, ...);
void reasset_assert_no_exit(_Bool condition, const char *fmt, ...);
void reasset_log_debug(const char *fmt, ...);
void reasset_log_info(const char *fmt, ...);
void reasset_log_warning(const char *fmt, ...);
void reasset_log_error(const char *fmt, ...);
void reasset_error(const char *fmt, ...);
void reasset_error_box(const char *fmt, ...);

const char* reasset_alloc_sprintf(const char *fmt, ...);
