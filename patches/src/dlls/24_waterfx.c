#include "patches.h"
#include "matrix_groups.h"

#include "PR/ultratypes.h"
#include "PR/gbi.h"
#include "PR/os.h"
#include "gbi_extra.h"
#include "sys/camera.h"
#include "sys/gfx/texture.h"
#include "sys/math.h"
#include "sys/map.h"

#include "recomp/dlls/engine/24_waterfx_recomp.h"

#define MAX_CIRCULAR_RIPPLES 30
#define MAX_MOVEMENT_RIPPLES 30
#define MAX_SPLASHES 10
#define MAX_SPLASH_PARTICLES 30

// size: 0x1C
typedef struct {
/*00*/ f32 x;
/*04*/ f32 y;
/*08*/ f32 z;
/*0C*/ f32 unkC;
/*10*/ f32 scale;
/*14*/ s16 yaw;
/*16*/ s16 alpha;
/*18*/ s16 decayRate;
} CircularWaterRipple;

// size: 0x5C
typedef struct {
/*00*/ f32 x;
/*04*/ f32 y;
/*08*/ f32 z;
/*0C*/ f32 unkC[6];
/*24*/ f32 unk24[6];
/*3C*/ f32 unk3C[6];
/*54*/ s16 alpha;
/*56*/ s16 unk56;
/*58*/ u8 particleCount;
} WaterSplash;

// size: 0x1C
typedef struct {
/*00*/ f32 x;
/*04*/ f32 y;
/*08*/ f32 z;
/*0C*/ f32 unkC;
/*10*/ f32 scale;
/*14*/ s16 alpha;
/*16*/ s16 yaw;
/*18*/ u8 hide;
} MovementWaterRipple;

// size: 0x14
typedef struct {
/*00*/ f32 xVel;
/*04*/ f32 zVel;
/*08*/ f32 speed; // lateral only
/*0C*/ f32 yVel;
/*10*/ s16 unk10;
/*12*/ s8 splashIdx; // index of the linked water splash instance
} WaterSplashParticle;

extern s32 waterfx_make_splash_particles(WaterSplash *splash, s32 splashIdx);

extern Vtx *sCircularRippleVerts;
extern DLTri *sCircularRippleTris;
extern Vtx *sWaterSplashVerts;
extern DLTri *sWaterSplashTris;
extern Vtx *sWaterSplashPartVerts;
extern DLTri *sWaterSplashPartTris;
extern Vtx *sMovementRippleVerts;
extern DLTri *sMovementRippleTris;
extern s32 sNumCircularRipples;
extern CircularWaterRipple *sCircularRipples;
extern s32 sNumWaterSplashes;
extern WaterSplash *sWaterSplashes;
extern s32 sNumMovementRipples;
extern MovementWaterRipple *sMovementRipples;
extern s32 sNumWaterSplashParticles;
extern WaterSplashParticle *sWaterSplashParticles;
extern Texture *sCircularWaterRippleTex;
extern Texture *sWaterSplashTex;
extern Texture *sWaterSplashParticleTex;
extern Texture *sMovementWaterRippleTex;
extern f32 sCircularRippleScale;

static u8 recomp_skipCircRipInterp[MAX_CIRCULAR_RIPPLES];
static u8 recomp_skipSplashInterp[MAX_SPLASHES];
static u8 recomp_skipMovRipInterp[MAX_MOVEMENT_RIPPLES];

RECOMP_PATCH void waterfx_print(Gfx** gdl, Mtx** mtxs) {
    s32 i;
    SRT srt;
    CircularWaterRipple* circRipple;
    WaterSplash* splash;
    MovementWaterRipple *movRipple;
    WaterSplashParticle* splashPart;

    if ((sNumCircularRipples != 0) || (sNumMovementRipples != 0) || (sNumWaterSplashes != 0) || (sNumWaterSplashParticles != 0)) {
        gSPLoadGeometryMode(*gdl, G_SHADE | G_ZBUFFER | G_SHADING_SMOOTH);
        dl_apply_geometry_mode(gdl);
        gDPSetCombineLERP(*gdl, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, COMBINED, COMBINED, 0, SHADE, 0);
        dl_apply_combine(gdl);
        gDPSetOtherMode(*gdl, 
            G_AD_PATTERN | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_PERSP | G_CYC_2CYCLE | G_PM_NPRIMITIVE, 
            G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_ZB_CLD_SURF2);
        dl_apply_other_mode(gdl);

        // Draw circular water ripples
        if (sNumCircularRipples != 0) {
            gSPDisplayList((*gdl)++, OS_PHYSICAL_TO_K0(sCircularWaterRippleTex->gdl));
        }
        for (i = 0; i < MAX_CIRCULAR_RIPPLES; i++) {
            if (sCircularRipples[i].alpha != 0) {
                circRipple = &sCircularRipples[i];
                dl_set_prim_color(gdl, 0xFF, 0xFF, 0xFF, circRipple->alpha);
                srt.transl.x = circRipple->x;
                srt.transl.y = circRipple->y;
                srt.transl.z = circRipple->z;
                srt.scale = circRipple->scale;
                srt.yaw = circRipple->yaw;
                srt.roll = 0;
                srt.pitch = 0;
                camera_setup_object_srt_matrix(gdl, mtxs, &srt, 1.0f, 0.0f, NULL);
                // @recomp: Tag matrices
                if (recomp_skipCircRipInterp[i]) {
                    recomp_skipCircRipInterp[i] = FALSE;
                    gEXMatrixGroupSkipAll((*gdl)++, WATERFX_CIRC_RIPPLE_MTX_GROUP_ID_START + i, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
                } else {
                    gEXMatrixGroupSimpleVerts((*gdl)++, WATERFX_CIRC_RIPPLE_MTX_GROUP_ID_START + i, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
                }
                gSPVertex((*gdl)++, OS_PHYSICAL_TO_K0(&sCircularRippleVerts[i << 2]), 4, 0);
                dl_triangles(gdl, &sCircularRippleTris[i << 1], 2);
            }
        }

        // Draw 3D water splashes
        if (sNumWaterSplashes != 0) {
            gSPDisplayList((*gdl)++, OS_PHYSICAL_TO_K0(sWaterSplashTex->gdl));
        }
        for (i = 0; i < MAX_SPLASHES; i++) {
            if (sWaterSplashes[i].alpha != 0) {
                splash = &sWaterSplashes[i];
                dl_set_prim_color(gdl, 0xFF, 0xFF, 0xFF, splash->alpha);
                srt.transl.x = splash->x;
                srt.transl.y = splash->y;
                srt.transl.z = splash->z;
                srt.scale = 0.01f;
                srt.yaw = 0;
                srt.roll = 0;
                srt.pitch = 0;
                camera_setup_object_srt_matrix(gdl, mtxs, &srt, 1.0f, 0.0f, NULL);
                // @recomp: Tag matrices
                if (recomp_skipSplashInterp[i]) {
                    gEXMatrixGroupSkipAll((*gdl)++, WATERFX_SPLASH_MTX_GROUP_ID_START + i, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
                } else {
                    gEXMatrixGroupSimpleVerts((*gdl)++, WATERFX_SPLASH_MTX_GROUP_ID_START + i, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
                }
                gSPVertex((*gdl)++, OS_PHYSICAL_TO_K0(&sWaterSplashVerts[i * 14]), 14, 0);
                dl_triangles(gdl, &sWaterSplashTris[i * 12], 12);
            }
        }

        // Draw water splash particles
        if (sNumWaterSplashParticles != 0) {
            gSPDisplayList((*gdl)++, OS_PHYSICAL_TO_K0(sWaterSplashParticleTex->gdl));
            dl_set_prim_color(gdl, 0xFF, 0xFF, 0xFF, 0xB4);
        }
        for (i = 0; i < MAX_SPLASH_PARTICLES; i++) {
            if (sWaterSplashParticles[i].splashIdx != -1 && sWaterSplashParticles[i].unk10 != 0) {
                splashPart = &sWaterSplashParticles[i];
                srt.transl.x = sWaterSplashes[splashPart->splashIdx].x;
                srt.transl.y = sWaterSplashes[splashPart->splashIdx].y;
                srt.transl.z = sWaterSplashes[splashPart->splashIdx].z;
                srt.scale = 0.01f;
                srt.yaw = 0;
                srt.roll = 0;
                srt.pitch = 0;
                camera_setup_object_srt_matrix(gdl, mtxs, &srt, 1.0f, 0.0f, NULL);
                // @recomp: Tag matrices
                if (recomp_skipSplashInterp[splashPart->splashIdx]) {
                    gEXMatrixGroupSkipAll((*gdl)++, WATERFX_SPLASH_PART_MTX_GROUP_ID_START + i, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
                } else {
                    gEXMatrixGroupSimpleVerts((*gdl)++, WATERFX_SPLASH_PART_MTX_GROUP_ID_START + i, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
                }
                gSPVertex((*gdl)++, OS_PHYSICAL_TO_K0(&sWaterSplashPartVerts[i << 2]), 4, 0);
                dl_triangles(gdl, &sWaterSplashPartTris[i << 1], 2);
            }
        }

        // Draw movement water ripples
        if (sNumMovementRipples != 0) {
            gSPDisplayList((*gdl)++, OS_PHYSICAL_TO_K0(sMovementWaterRippleTex->gdl));
        }
        for (i = 0; i < MAX_MOVEMENT_RIPPLES; i++) {
            if (sMovementRipples[i].alpha != 0 && sMovementRipples[i].hide == FALSE) {
                movRipple = &sMovementRipples[i];
                dl_set_prim_color(gdl, 0xFF, 0xFF, 0xFF, movRipple->alpha);
                srt.transl.x = movRipple->x;
                srt.transl.y = movRipple->y;
                srt.transl.z = movRipple->z;
                srt.scale = movRipple->scale;
                srt.yaw = movRipple->yaw;
                srt.roll = 0;
                srt.pitch = 0;
                camera_setup_object_srt_matrix(gdl, mtxs, &srt, 1.0f, 0.0f, NULL);
                // @recomp: Tag matrices
                if (recomp_skipMovRipInterp[i]) {
                    recomp_skipMovRipInterp[i] = FALSE;
                    gEXMatrixGroupSkipAll((*gdl)++, WATERFX_MOV_RIPPLE_MTX_GROUP_ID_START + i, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
                } else {
                    gEXMatrixGroupSimpleVerts((*gdl)++, WATERFX_MOV_RIPPLE_MTX_GROUP_ID_START + i, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
                }
                gSPVertex((*gdl)++, OS_PHYSICAL_TO_K0(&sMovementRippleVerts[i << 2]), 4, 0);
                dl_triangles(gdl, &sMovementRippleTris[i << 1], 2);
            }
        }
        tex_render_reset();

        // @recomp: Enable interp for splashes. Do this late since splash parts access this field after
        //          we process splash matrices (so we can't re-enable there).
        for (i = 0; i < MAX_SPLASHES; i++) {
            recomp_skipSplashInterp[i] = FALSE;
        }
    }
}

RECOMP_PATCH void waterfx_spawn_splash(f32 x, f32 y, f32 z, f32 size) {
    WaterSplash* splash;
    f32 temp_fs1;
    f32 temp_ft4;
    f32 temp_fv0;
    f32 temp_fv0_2;
    s16 s;
    s16 angle;
    s32 idx;
    Vtx* vtx;
    s32 i;

    if (size == 0.0f) {
        size = 4.0f;
    }
    for (idx = 0; idx < MAX_SPLASHES && sWaterSplashes[idx].particleCount != 0; idx++) {}
    if (idx >= MAX_SPLASHES) {
        return;
    }

    splash = &sWaterSplashes[idx];
    vtx = &sWaterSplashVerts[idx * 14];
    temp_fs1 = size * 100.0f;
    vtx->v.ob[0] = temp_fs1;
    vtx->v.ob[1] = 0;
    vtx->v.ob[2] = 0;
    vtx->v.cn[0] = 0xFF;
    vtx->v.cn[1] = 0;
    vtx->v.cn[2] = 0;
    vtx->v.cn[3] = 0xFF;
    vtx->v.tc[0] = qu105(0);
    vtx->v.tc[1] = qu105(0);
    vtx++;
    vtx->v.ob[0] = temp_fs1;
    vtx->v.ob[1] = 0;
    vtx->v.ob[2] = 0;
    vtx->v.cn[0] = 0xFF;
    vtx->v.cn[1] = 0;
    vtx->v.cn[2] = 0;
    vtx->v.cn[3] = 0xFF;
    vtx->v.tc[0] = qu105(0);
    vtx->v.tc[1] = qu105(29);
    vtx++;
    splash->unkC[0] = 4.0f;
    splash->unk24[0] = 4.0f;
    splash->unk3C[0] = 0.0f;
    temp_ft4 = 2.0f * (splash->unkC[0] * splash->unk24[0]);
    angle = 0x2AAA;
    if (temp_ft4 > 0.0f) {
        temp_ft4 = 1.0f / temp_ft4;
        splash->unkC[0] *= temp_ft4;
        splash->unk24[0] *= temp_ft4;
    }
    s = qu105(0);
    i = 1;
    do {
        temp_fv0 = fcos16_precise(angle);
        temp_fv0_2 = fsin16_precise(angle);
        splash->unkC[i] = 4.0f * temp_fv0;
        splash->unk24[i] = 4.0f;
        splash->unk3C[i] = 4.0f * temp_fv0_2;
        temp_ft4 = SQ(splash->unkC[i]) + SQ(splash->unk24[i]) + SQ(splash->unk3C[i]);
        if (temp_ft4 > 0.0f) {
            temp_ft4 = 1.0f / temp_ft4;
            splash->unkC[i] *= temp_ft4;
            splash->unk24[i] *= temp_ft4;
            splash->unk3C[i] *= temp_ft4;
        }
        s += qu105(96);
        vtx->v.ob[1] = 0;
        vtx->v.cn[3] = 0xFF;
        vtx->v.tc[0] = s;
        vtx->v.tc[1] = qu105(0);
        vtx[1].v.ob[1] = 0;
        vtx[1].v.cn[3] = 0xFF;
        vtx[1].v.tc[0] = s;
        if (0) { } // @fake
        vtx[1].v.tc[1] = qu105(29);
        vtx->v.ob[0] = temp_fs1 * temp_fv0;
        vtx->v.ob[2] = temp_fs1 * temp_fv0_2;
        vtx[1].v.ob[0] = vtx->v.ob[0];
        vtx[1].v.ob[2] = vtx->v.ob[2];
        angle += 0x2AAA;
        vtx += 2;
        i++;
    } while (i != 6);
    s += qu105(96);
    vtx->v.ob[0] = temp_fs1;
    vtx->v.ob[2] = 0;
    vtx->v.ob[1] = 0;
    vtx->v.cn[3] = 0xFF;
    vtx->v.cn[2] = 0;
    vtx->v.cn[1] = 0;
    vtx->v.cn[0] = 0xFF;
    vtx->v.tc[0] = s;
    vtx->v.tc[1] = qu105(0);
    vtx++;
    vtx->v.ob[0] = temp_fs1;
    vtx->v.ob[2] = 0;
    vtx->v.ob[1] = 0;
    vtx->v.cn[3] = 0xFF;
    vtx->v.cn[2] = 0;
    vtx->v.cn[1] = 0;
    vtx->v.cn[0] = 0xFF;
    vtx->v.tc[0] = s;
    vtx->v.tc[1] = qu105(29);
    splash->alpha = 0xFF;
    splash->x = x;
    splash->y = y;
    splash->z = z;
    sNumWaterSplashes += 1;
    splash->particleCount = waterfx_make_splash_particles(&sWaterSplashes[idx], idx);

    // @recomp: Skip first frame of interp
    recomp_skipSplashInterp[idx] = TRUE;
}

RECOMP_PATCH void waterfx_spawn_movement_ripple(f32 x, f32 y, f32 z, s16 yaw, f32 arg4) {
    MovementWaterRipple *ripple;
    s32 idx;
    Vtx *vtx;

    for (idx = 0; idx < MAX_MOVEMENT_RIPPLES && sMovementRipples[idx].alpha != 0; idx++) {}
    if (idx >= MAX_MOVEMENT_RIPPLES) {
        return;
    }

    vtx = &sMovementRippleVerts[idx * 4];
    vtx->v.ob[0] = -200;
    vtx->v.ob[1] = 0;
    vtx->v.ob[2] = 400;
    vtx->v.cn[3] = -1;
    vtx->v.tc[0] = qu105(0);
    vtx->v.tc[1] = qu105(0);
    vtx++;
    vtx->v.ob[0] = -200;
    vtx->v.ob[1] = 0;
    vtx->v.ob[2] = -200;
    vtx->v.cn[3] = -1;
    vtx->v.tc[0] = qu105(0);
    vtx->v.tc[1] = qu105(31);
    vtx++;
    vtx->v.ob[0] = 200;
    vtx->v.ob[1] = 0;
    vtx->v.ob[2] = -200;
    vtx->v.cn[3] = -1;
    vtx->v.tc[0] = qu105(63);
    vtx->v.tc[1] = qu105(31);
    vtx++;
    vtx->v.ob[0] = 200;
    vtx->v.ob[1] = 0;
    vtx->v.ob[2] = 400;
    vtx->v.cn[3] = -1;
    vtx->v.tc[0] = qu105(63);
    vtx->v.tc[1] = qu105(0);
    ripple = &sMovementRipples[idx];
    ripple->x = x;
    ripple->y = y;
    ripple->z = z;
    ripple->unkC = arg4;
    ripple->scale = 0.01f;
    ripple->alpha = 128;
    ripple->yaw = yaw;
    ripple->hide = FALSE;
    sNumMovementRipples += 1;

    // @recomp: Skip first frame of interp
    recomp_skipMovRipInterp[idx] = TRUE;
}

RECOMP_PATCH void waterfx_spawn_circular_ripple(f32 x, f32 y, f32 z, s16 yaw, f32 arg4, s32 decayRate) {
    s32 idx;
    s32 vtxIdx;
    Vtx* vtx;

    for (idx = 0; idx < MAX_CIRCULAR_RIPPLES && sCircularRipples[idx].alpha != 0; idx++) {}
    if (idx >= MAX_CIRCULAR_RIPPLES) {
        return;
    }

    vtxIdx = idx * 4;
    vtx = &sCircularRippleVerts[vtxIdx];
    vtx->v.ob[0] = -200;
    vtx->v.ob[1] = 0;
    vtx->v.ob[2] = 200;
    vtx->v.cn[3] = 0xFF;
    vtx->v.tc[0] = qu105(0);
    vtx->v.tc[1] = qu105(0);
    vtx = &sCircularRippleVerts[vtxIdx + 1];
    vtx->v.ob[0] = -200;
    vtx->v.ob[1] = 0;
    vtx->v.ob[2] = -200;
    vtx->v.cn[3] = 0xFF;
    vtx->v.tc[0] = qu105(0);
    vtx->v.tc[1] = qu105(63);
    vtx = &sCircularRippleVerts[vtxIdx + 2];
    vtx->v.ob[0] = 200;
    vtx->v.ob[1] = 0;
    vtx->v.ob[2] = -200;
    vtx->v.cn[3] = 0xFF;
    vtx->v.tc[0] = qu105(63);
    vtx->v.tc[1] = qu105(63);
    vtx = &sCircularRippleVerts[vtxIdx + 3];
    vtx->v.ob[0] = 200;
    vtx->v.ob[1] = 0;
    vtx->v.ob[2] = 200;
    vtx->v.cn[3] = 0xFF;
    vtx->v.tc[0] = qu105(63);
    vtx->v.tc[1] = qu105(0);
    sCircularRipples[idx].unkC = arg4;
    sCircularRipples[idx].alpha = 0xFF;
    sCircularRipples[idx].x = x;
    sCircularRipples[idx].y = y;
    sCircularRipples[idx].z = z;
    sCircularRipples[idx].yaw = yaw;
    sCircularRipples[idx].scale = sCircularRippleScale;
    sCircularRipples[idx].decayRate = decayRate;
    sNumCircularRipples += 1;

    // @recomp: Skip first frame of interp
    recomp_skipCircRipInterp[idx] = TRUE;
}
