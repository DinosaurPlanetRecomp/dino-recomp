#include "patches.h"
#include "stdarg.h"
#include "recomp_funcs.h"

#include "reasset.h"
#include "reasset/reasset_fst.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"

#include "reasset/types/reasset_music_actions.h"

#include "sys/fs.h"

RECOMP_DECLARE_EVENT(reasset_on_fst_set());
RECOMP_DECLARE_EVENT(reasset_on_set());
RECOMP_DECLARE_EVENT(reasset_on_modify());
RECOMP_DECLARE_EVENT(reasset_on_resolve());
RECOMP_DECLARE_EVENT(reasset_on_committed());

ReAssetStage reassetStage = REASSET_STAGE_UNINITIALIZED;

const char *DINO_FS_FILENAMES[NUM_FILES] = {
/*00*/ "AUDIO.tab",
/*01*/ "AUDIO.bin",
/*02*/ "SFX.tab",
/*03*/ "SFX.bin",
/*04*/ "AMBIENT.tab",
/*05*/ "AMBIENT.bin",
/*06*/ "MUSIC.tab",
/*07*/ "MUSIC.bin",
/*08*/ "MPEG.tab",
/*09*/ "MPEG.bin",
/*0A*/ "MUSICACTIONS.bin",
/*0B*/ "CAMACTIONS.bin",
/*0C*/ "LACTIONS.bin",
/*0D*/ "ANIMCURVES.bin",
/*0E*/ "ANIMCURVES.tab",
/*0F*/ "OBJSEQ2CURVE.tab",
/*10*/ "FONTS.bin",
/*11*/ "CACHEFON.bin",
/*12*/ "CACHEFON2.bin",
/*13*/ "GAMETEXT.bin",
/*14*/ "GAMETEXT.tab",
/*15*/ "GLOBALMAP.bin",
/*16*/ "TABLES.bin",
/*17*/ "TABLES.tab",
/*18*/ "SCREENS.bin",
/*19*/ "SCREENS.tab",
/*1A*/ "FILE_1A.bin",
/*1B*/ "FILE_1B.bin",
/*1C*/ "VOXMAP.tab",
/*1D*/ "VOXMAP.bin",
/*1E*/ "WARPTAB.bin",
/*1F*/ "MAPS.bin",
/*20*/ "MAPS.tab",
/*21*/ "MAPINFO.bin",
/*22*/ "MAPSETUP.ind",
/*23*/ "MAPSETUP.tab",
/*24*/ "TEX1.bin",
/*25*/ "TEX1.tab",
/*26*/ "TEXTABLE.bin",
/*27*/ "TEX0.bin",
/*28*/ "TEX0.tab",
/*29*/ "BLOCKS.bin",
/*2A*/ "BLOCKS.tab",
/*2B*/ "TRKBLK.bin",
/*2C*/ "HITS.bin",
/*2D*/ "HITS.tab",
/*2E*/ "MODELS.tab",
/*2F*/ "MODELS.bin",
/*30*/ "MODELIND.bin",
/*31*/ "MODANIM.tab",
/*32*/ "MODANIM.bin",
/*33*/ "ANIM.tab",
/*34*/ "ANIM.bin",
/*35*/ "AMAP.tab",
/*36*/ "AMAP.bin",
/*37*/ "BITTABLE.bin",
/*38*/ "WEAPONDATA.bin",
/*39*/ "VOXOBJ.tab",
/*3A*/ "VOXOBJ.bin",
/*3B*/ "MODLINES.bin",
/*3C*/ "MODLINES.tab",
/*3D*/ "SAVEGAME.bin",
/*3E*/ "SAVEGAME.tab",
/*3F*/ "OBJSEQ.bin",
/*40*/ "OBJSEQ.tab",
/*41*/ "OBJECTS.tab",
/*42*/ "OBJECTS.bin",
/*43*/ "OBJINDEX.bin",
/*44*/ "OBJEVENT.bin",
/*45*/ "OBJHITS.bin",
/*46*/ "DLLS.bin",
/*47*/ "DLLS.tab",
/*48*/ "DLLSIMPORTTAB.bin",
/*49*/ "ENVFXACT.bin"
};

static void reasset_types_init(void) {
    reasset_music_actions_init();
}

static void reasset_types_repack(void) {
    reasset_music_actions_repack();
}

void reasset_run(void) {
    reasset_namespace_init();
    reasset_id_init();
    reasset_resolve_map_init();

    reasset_fst_init();

    reasset_log("[reasset] == FST Set ==\n");
    reassetStage = REASSET_STAGE_FST_SET;
    reasset_on_fst_set();

    reasset_types_init();

    reasset_log("[reasset] == Set ==\n");
    reassetStage = REASSET_STAGE_SET;
    reasset_on_set();

    reasset_log("[reasset] == Modify ==\n");
    reassetStage = REASSET_STAGE_MODIFY;
    reasset_on_modify();

    reasset_log("[reasset] == Resolve ==\n");
    reasset_types_repack();
    reasset_fst_rebuild();
    reassetStage = REASSET_STAGE_RESOLVE;
    reasset_on_resolve();

    reasset_log("[reasset] == Committed ==\n");
    reassetStage = REASSET_STAGE_COMMITTED;
    reasset_on_committed();
}

void reasset_assert_stage_set_call(const char *functionName) {
    reasset_assert(reassetStage == REASSET_STAGE_SET, 
        "[reasset] %s can only be called during the set stage.", functionName);
}

void reasset_assert_stage_get_call(const char *functionName) {
    reasset_assert(reassetStage == REASSET_STAGE_MODIFY, 
        "[reasset] %s can only be called during the modify stage.", functionName);
}

void reasset_assert_stage_link_call(const char *functionName) {
    reasset_assert(reassetStage == REASSET_STAGE_SET, 
        "[reasset] %s can only be called during the set stage.", functionName);
}

void reasset_assert_stage_get_resolve_map_call(const char *functionName) {
    reasset_assert(reassetStage == REASSET_STAGE_RESOLVE || reassetStage == REASSET_STAGE_COMMITTED, 
        "[reasset] %s can only be called during the resolve and committed stages.", functionName);
}

void reasset_assert(_Bool condition, const char *fmt, ...) {
    va_list args;
	va_start(args, fmt);

    if (!condition) {
        recomp_exit_with_error(recomp_vsprintf_helper(fmt, args));
    }

    va_end(args);
}

void reasset_assert_no_exit(_Bool condition, const char *fmt, ...) {
    va_list args;
	va_start(args, fmt);

    if (!condition) {
        recomp_error_message_box(recomp_vsprintf_helper(fmt, args));
    }

    va_end(args);
}

void reasset_log(const char *fmt, ...) {
    va_list args;
	va_start(args, fmt);

    //TODO: debug logging option
    //s32 enableDebugLogging = recomp_get_config_u32("logging") == DEBUG_LOGGING_ON;
    //if (enableDebugLogging) {
        recomp_vprintf(fmt, args);
    //}

    va_end(args);
}

void reasset_log_warning(const char *fmt, ...) {
    va_list args;
	va_start(args, fmt);

    recomp_veprintf(fmt, args);

    va_end(args);
}

void reasset_log_error(const char *fmt, ...) {
    va_list args;
	va_start(args, fmt);

    recomp_veprintf(fmt, args);

    va_end(args);
}

void reasset_error(const char *fmt, ...) {
    va_list args;
	va_start(args, fmt);

    recomp_exit_with_error(recomp_vsprintf_helper(fmt, args));

    va_end(args);
}

void reasset_error_box(const char *fmt, ...) {
    va_list args;
	va_start(args, fmt);

    recomp_error_message_box(recomp_vsprintf_helper(fmt, args));

    va_end(args);
}
