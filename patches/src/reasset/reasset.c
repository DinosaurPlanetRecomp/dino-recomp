#include "patches.h"
#include "stdarg.h"
#include "recomp_funcs.h"

#include "reasset.h"
#include "reasset/reasset_fst.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"
#include "reasset/reasset_iterator.h"

#include "reasset/files/reasset_anims.h"
#include "reasset/files/reasset_bits.h"
#include "reasset/files/reasset_blocks.h"
#include "reasset/files/reasset_maps.h"
#include "reasset/files/reasset_models.h"
#include "reasset/files/reasset_mpeg.h"
#include "reasset/files/reasset_music_actions.h"
#include "reasset/files/reasset_objects.h"
#include "reasset/files/reasset_sequences.h"
#include "reasset/files/reasset_textures.h"
#include "reasset/special/reasset_dlls.h"
#include "reasset/special/reasset_menus.h"

#include "libc/string.h"
#include "sys/fs.h"

RECOMP_DECLARE_EVENT(reasset_on_fst_set(void));
RECOMP_DECLARE_EVENT(reasset_on_set(void));
RECOMP_DECLARE_EVENT(reasset_on_modify(void));
RECOMP_DECLARE_EVENT(reasset_on_resolve(void));
RECOMP_DECLARE_EVENT(reasset_on_committed(void));

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

static void reasset_run_init(void) {
    reasset_anims_init();
    reasset_bits_init();
    reasset_blocks_init();
    reasset_maps_init();
    reasset_models_init();
    reasset_mpeg_init();
    reasset_music_actions_init();
    reasset_objects_init();
    reasset_sequences_init();
    reasset_textures_init();

    reasset_dlls_init();
    reasset_menus_init();
}

static void reasset_run_repack(void) {
    reasset_anims_repack();
    reasset_bits_repack();
    reasset_blocks_repack();
    reasset_maps_repack();
    reasset_models_repack();
    reasset_mpeg_repack();
    reasset_music_actions_repack();
    reasset_objects_repack();
    reasset_sequences_repack();
    reasset_textures_repack();

    reasset_dlls_repack();
    reasset_menus_repack();
}

static void reasset_run_patch(void) {
    reasset_models_patch();
    reasset_blocks_patch();
    reasset_maps_patch();
    reasset_objects_patch();
    reasset_sequences_patch();
    reasset_textures_patch();

    reasset_menus_patch();
}

static void reasset_run_cleanup(void) {
    reasset_anims_cleanup();
    reasset_bits_cleanup();
    reasset_blocks_cleanup();
    reasset_maps_cleanup();
    reasset_models_cleanup();
    reasset_mpeg_cleanup();
    reasset_music_actions_cleanup();
    reasset_objects_cleanup();
    reasset_sequences_cleanup();
    reasset_textures_cleanup();

    reasset_dlls_cleanup();
    reasset_menus_cleanup();
}

void reasset_run(void) {
    reasset_namespace_init();
    reasset_id_init();
    reasset_iterator_init();
    reasset_resolve_map_init();

    reasset_fst_init();

    reasset_log("[reasset] == FST Set ==\n");
    reassetStage = REASSET_STAGE_FST_SET;
    reasset_on_fst_set();

    reasset_run_init();

    reasset_log("[reasset] == Set ==\n");
    reassetStage = REASSET_STAGE_SET;
    reasset_on_set();

    reasset_log("[reasset] == Modify ==\n");
    reassetStage = REASSET_STAGE_MODIFY;
    reasset_on_modify();

    reasset_log("[reasset] == Resolve ==\n"); // TODO: rename to patch stage?
    reassetStage = REASSET_STAGE_RESOLVE;
    reasset_iterator_clear_all(); // Previous iterators are no longer valid
    reasset_run_repack();
    reasset_run_patch();
    reasset_fst_rebuild();
    reasset_on_resolve();

    reasset_log("[reasset] == Committed ==\n");
    reassetStage = REASSET_STAGE_COMMITTED;
    reasset_on_committed();

    reasset_run_cleanup();
}

void reasset_assert_stage_set_call(const char *functionName) {
    reasset_assert(reassetStage == REASSET_STAGE_SET || reassetStage == REASSET_STAGE_MODIFY, 
        "[reasset] %s can only be called during the set and modify stages.", functionName);
}

void reasset_assert_stage_get_call(const char *functionName) {
    reasset_assert(reassetStage == REASSET_STAGE_MODIFY || reassetStage == REASSET_STAGE_RESOLVE, 
        "[reasset] %s can only be called during the modify and resolve stages.", functionName);
}

void reasset_assert_stage_delete_call(const char *functionName) {
    reasset_assert(reassetStage == REASSET_STAGE_MODIFY, 
        "[reasset] %s can only be called during the modify stage.", functionName);
}

void reasset_assert_stage_iterator_call(const char *functionName) {
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

const char* reasset_alloc_sprintf(const char *fmt, ...) {
    va_list args;
	va_start(args, fmt);

    const char *temp = recomp_vsprintf_helper(fmt, args);
    u32 tempLen = strlen(temp);
    
    char *str = recomp_alloc(tempLen + 1);
    bcopy(temp, str, tempLen);
    str[tempLen] = '\0';

    va_end(args);

    return str;
}
