#include "patches.h"
#include "matrix_groups.h"

#include "PR/gbi.h"
#include "game/objects/object.h"
#include "sys/camera.h"
#include "sys/main.h"
#include "sys/rand.h"
#include "sys/segment_13D0.h"
#include "sys/segment_1D900.h"

#include "recomp/dlls/engine/13_expgfx_recomp.h"

typedef struct {
    s16 unk0;
    s16 unk2;
    s16 unk4;
    s16 unk6;
    s16 unk8;
    s16 unkA;
    s16 unkC;
    u8 unkE;
    u8 unkF;
} UnkBss0Struct_Unk0;

typedef struct {
    UnkBss0Struct_Unk0 unk0[4];
    SRT unk40;
    Vec3f unk58;
    f32 unk64;
    f32 unk68;
    f32 unk6C;
    f32 unk70;
    f32 unk74;
    f32 unk78;
    union {
        u32 unk7C;
        struct {
            u32 unk7C_31: 1;
            u32 unk7C_30: 1;
            u32 unk7C_29: 1;
            u32 unk7C_28: 1;
            u32 unk7C_27: 1;
            u32 unk7C_26: 1;
            u32 unk7C_25: 1;
            u32 unk7C_24: 1;
            u32 unk7C_23: 1;
            u32 unk7C_22: 1;
            u32 unk7C_21: 1;
            u32 unk7C_20: 1; // & 0x100000
            u32 unk7C_19: 1;
            u32 unk7C_18: 1;
            u32 unk7C_17: 1;
            u32 unk7C_16: 1;
            u32 unk7C_15: 1;
            u32 unk7C_14: 1;
            u32 unk7C_13: 1;
            u32 unk7C_12: 1;
            u32 unk7C_11: 1;
            u32 unk7C_10: 1;
            u32 unk7C_9: 1;
            u32 unk7C_8: 1;
            u32 unk7C_7: 1;
            u32 unk7C_6: 1;
            u32 unk7C_5: 1;
            u32 unk7C_4: 1;
            u32 unk7C_3: 1;
            u32 unk7C_2: 1;
            u32 unk7C_1: 1;
            u32 unk7C_0: 1;
        } bits;
    };
    u32 unk80;
    u16 unk84;
    u16 unk86;
    u16 unk88;
    u16 unk8A : 7;
    u16 unk8A_2 : 1;
    u16 unk8A_3 : 4;
    u16 unk8A_4 : 2;
    u16 unk8A_5 : 1;
    u16 unk8A_6 : 1;
    u8 unk8C;
    u8 unk8D;
    u8 unk8E;
} UnkBss0Struct;

typedef struct {
    Object *unk0;
    Object *unk4;
    Texture *unk8;
    u16 unkC;
} UnkBss190Struct;

typedef struct {
    Texture *unk0;
    s32 unk4; // lifetime?
    s32 unk8; // texture ID
    s32 unkC;
} UnkBss370Struct;

/*0x3C*/ extern u8 _data_3C[30];
/*0x64*/ extern u8 _data_64;
/*0x68*/ extern u8 _data_68;
/*0x70*/ extern DLTri _data_70[2];

/*0x0*/ extern UnkBss0Struct *_bss_0[30];
/*0x78*/ extern u32 _bss_78[30];
/*0xF0*/ extern s8 _bss_F0[30];
/*0x110*/ extern u32 _bss_110;
/*0x118*/ extern Object *_bss_118[30];
/*0x190*/ extern UnkBss190Struct _bss_190[30];
/*0x370*/ extern UnkBss370Struct _bss_370[8];

extern void dll_13_func_52B4(Gfx** gdl);

RECOMP_PATCH s32 dll_13_func_1080(Object* obj, Gfx** gdl, Mtx** mtxs, Vertex** vertices, u8 arg4, s32 arg5, s32 arg6) {
    Camera* camera;
    Object* temp_s3;
    s32 i; // sp12C
    UnkBss0Struct* var_s0;
    SRT sp110;
    Object* temp_s4;
    f32 temp_fs0;
    Vec3f spFC;
    //s32 pad2;
    Vec3f spEC;
    f32 var_fv0;
    s32 temp_a0;
    s32 temp_a1;
    s32 var_s2;
    //s32 pad;
    Texture* spD4;
    s32 j;
    u8 spCF;
    u8 spCE;
    u8 spCD;
    u8 temp_v0_5;
    s16 spCA;
    s16 spC8;
    s16 spC6;

    func_8001F81C(&spCF, &spCE, &spCD);
    spCA = (s16)((0xFF - spCF) / 3) + spCF;
    spC8 = (s16)((0xFF - spCE) / 3) + spCE;
    spC6 = (s16)((0xFF - spCD) / 3) + spCD;
    if (func_80000824(-1) == 1) {
        return 0;
    }

    for (i = 0; i < 30; i++) {
        camera = get_camera();
        dl_clear_geometry_mode(gdl, G_CULL_BACK);
        if ((_data_64 != 1 || i != 29) && (_bss_F0[i] == 0 || _data_3C[i] == 0)) {
            continue;
        }

        dl_set_env_color(gdl, 0xFF, 0xFF, 0xFF, 0xFF);
        var_s0 = _bss_0[i] - 1;
        for (j = 0; j < 30; j++) {
            var_s0 += 1;
            temp_s3 = _bss_190[var_s0->unk8A].unk0;
            temp_s4 = _bss_190[var_s0->unk8A].unk4;
            spD4 = _bss_190[var_s0->unk8A].unk8;
            if ((_data_64 != 1 || i != 29 || obj == temp_s3) && (_bss_78[i] & (1 << j))) {
                if (var_s0->unk8A_4 == 0 && var_s0->unk8A_5 && var_s0->unk0[2].unk6 != -1 && !var_s0->unk8A_6 && (arg4 != 0 || !(var_s0->unk7C & 0x40000)) && (arg4 != 1 || (var_s0->unk7C & 0x40000))) {
                    var_s0->unk8A_5 = 0;
                    //pad2 = var_s0->unk0[1].unk6;
                    temp_a0 = var_s0->unk0[1].unk6 / 2;
                    if (var_s0->unk7C & 0x800000) {
                        var_fv0 = (f32) var_s0->unk0[0].unk6 / (f32) var_s0->unk0[1].unk6;
                        if (var_fv0 < 0.0f) {
                            var_fv0 = 0.0f;
                        } else if (var_fv0 > 1.0f) {
                            var_fv0 = 1.0f;
                        }
                        temp_v0_5 = var_s0->unk0[0].unkF;
                        var_s2 = ((temp_v0_5 - 0xFF) * var_fv0) + temp_v0_5;
                    } else if (var_s0->unk7C & 0x200) {
                        var_fv0 = (f32) var_s0->unk0[0].unk6 / (f32) var_s0->unk0[1].unk6;
                        if (var_fv0 < 0.0f) {
                            var_fv0 = 0.0f;
                        } else if (var_fv0 > 1.0f) {
                            var_fv0 = 1.0f;
                        }
                        var_s2 = var_s0->unk0[0].unkF * var_fv0;
                    } else {
                        if ((var_s0->unk80 & 0x400000) && var_s0->unk0[0].unk6 <= temp_a0) {
                            var_fv0 = (f32) var_s0->unk0[0].unk6 / (f32) temp_a0;
                            if (var_fv0 < 0.0f) {
                                var_fv0 = 0.0f;
                            } else if (var_fv0 > 1.0f) {
                                var_fv0 = 1.0f;
                            }
                            var_s2 = var_s0->unk0[0].unkF * var_fv0;
                        } else if ((var_s0->unk7C & 0x100) && var_s0->unk0[0].unk6 <= temp_a0) {
                            var_fv0 = (f32) var_s0->unk0[0].unk6 / (f32) temp_a0;
                            if (var_fv0 < 0.0f) {
                                var_fv0 = 0.0f;
                            } else if (var_fv0 > 1.0f) {
                                var_fv0 = 1.0f;
                            }
                            var_s2 = var_s0->unk0[0].unkF * var_fv0;
                        } else if (var_s0->unk7C & 0x100) {
                            var_fv0 = (f32) ((temp_a0 - var_s0->unk0[0].unk6) + temp_a0) / (f32) temp_a0;
                            if (var_fv0 < 0.0f) {
                                var_fv0 = 0.0f;
                            } else if (var_fv0 > 1.0f) {
                                var_fv0 = 1.0f;
                            }
                            var_s2 = var_s0->unk0[0].unkF * var_fv0;
                        } else {
                            var_s2 = var_s0->unk0[0].unkF;
                        }
                    }
                    sp110.transl.x = 0.0f;
                    sp110.transl.y = 0.0f;
                    sp110.transl.z = 0.0f;
                    sp110.scale = 1.0f;
                    if ((var_s0->unk7C & 0x20000) && !(var_s0->unk80 & 0x30000000)) {
                        sp110.roll = var_s0->unk40.roll * gUpdateRateF;
                        sp110.pitch = var_s0->unk40.pitch * gUpdateRateF;
                        sp110.yaw = var_s0->unk40.yaw * gUpdateRateF;
                        rotate_vec3(&sp110, var_s0->unk58.f);
                    }
                    sp110.roll = 0;
                    sp110.pitch = 0;
                    sp110.yaw = 0;
                    if (!(var_s0->unk7C & 0x04000000)) {
                        if (var_s0->unk7C & 4) {
                            if (temp_s3 != NULL) {
                                sp110.yaw = temp_s3->srt.yaw;
                                sp110.pitch = temp_s3->srt.pitch;
                                sp110.roll = temp_s3->srt.roll;
                            } else {
                                sp110.yaw = var_s0->unk40.yaw;
                                sp110.pitch = var_s0->unk40.pitch;
                                sp110.roll = var_s0->unk40.roll;
                            }
                        }
                        if (temp_s3 != NULL) {
                            if (temp_s3->parent != NULL) {
                                sp110.yaw += temp_s3->parent->srt.yaw;
                            }
                        }
                    }
                    spFC.f[0] = var_s0->unk58.f[0];
                    spFC.f[1] = var_s0->unk58.f[1];
                    spFC.f[2] = var_s0->unk58.f[2];
                    rotate_vec3(&sp110, spFC.f);
                    spEC.f[0] = 0.0f;
                    spEC.f[1] = 0.0f;
                    spEC.f[2] = 0.0f;
                    if (!(var_s0->unk7C & 1)) {
                        if (temp_s3 != NULL) {
                            spEC.f[0] = temp_s3->globalPosition.f[0];
                            spEC.f[1] = temp_s3->globalPosition.f[1];
                            spEC.f[2] = temp_s3->globalPosition.f[2];
                        } else {
                            spEC.f[0] = var_s0->unk40.transl.f[0];
                            spEC.f[1] = var_s0->unk40.transl.f[1];
                            spEC.f[2] = var_s0->unk40.transl.f[2];
                            if (temp_s4 != NULL) {
                                transform_point_by_object_matrix(&var_s0->unk40.transl, &spEC, temp_s4->matrixIdx);
                            }
                        }
                    }
                    sp110.scale = 1.0f;
                    sp110.roll = 0;
                    sp110.pitch = 0;
                    sp110.yaw = 0;
                    sp110.transl.x = spEC.f[0] + spFC.f[0];
                    sp110.transl.y = spEC.f[1] + spFC.f[1];
                    sp110.transl.z = spEC.f[2] + spFC.f[2];
                    if ((var_s0->unk7C & 0x20000) && !(var_s0->unk7C & 0x04000000) && !(var_s0->unk80 & 0x30000000)) {
                        sp110.transl.x += var_s0->unk40.transl.f[0];
                        sp110.transl.y += var_s0->unk40.transl.f[1];
                        sp110.transl.z += var_s0->unk40.transl.f[2];
                    }
                    temp_fs0 = var_s0->unk84 * 0.000015259022f;
                    if (var_s0->unk7C & 0x400000) {
                        // FAKE
                        //if (i){}
                        var_fv0 = temp_fs0 * 0.5f;
                        sp110.scale = (var_fv0 / rand_next(1, 10)) + var_fv0;
                    } else {
                        sp110.scale = temp_fs0;
                    }
                    if (!(var_s0->unk7C & 0x04000000)) {
                        sp110.roll = 0;
                        sp110.pitch = 0;
                        if (var_s0->unk7C & 0x02000000) {
                            sp110.yaw = 0;
                        } else if (var_s0->unk7C & 0x80000) {
                            sp110.yaw = 0x10000 - camera->srt.yaw;
                            sp110.pitch = camera->srt.pitch;
                        } else {
                            sp110.yaw = 0x10000 - camera->srt.yaw;
                        }
                    }
                    if ((spEC.f[0] > 65534.0f) || (spEC.f[0] < -65534.0f)) {
                        spEC.f[0] = -gWorldX;
                    }
                    if ((spEC.f[1] > 65534.0f) || (spEC.f[1] < -65534.0f)) {
                        spEC.f[1] = 0.0f;
                    }
                    if ((spEC.f[2] > 65534.0f) || (spEC.f[2] < -65534.0f)) {
                        spEC.f[2] = -gWorldZ;
                    }
                    // @recomp: Tag particle matrix
                    //          The game already comes up with a reasonably unique ID for each particle instance, so use that.
                    //          We could also scope by object to really make sure it's unique but so far this seems fine.
                    gEXMatrixGroupDecomposedNormal((*gdl)++, var_s0->unk0[2].unk6 + EXPGFX_MTX_GROUP_ID_START, 
                        G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
                    camera_setup_object_srt_matrix(gdl, mtxs, &sp110, 1.0f, 0/*.0f*/, NULL);
                    if ((temp_s3 != NULL) && (var_s0->unk80 & 0x80)) {
                        var_s2 = (temp_s3->opacity * var_s2) >> 8;
                    }
                    if (var_s0->unk80 & 0x01000000) {
                        dl_set_prim_color(gdl, spCF, spCE, spCD, var_s2);
                    } else if (var_s0->unk80 & 0x01000000) {
                        dl_set_prim_color(gdl, spCA, spC8, spC6, var_s2);
                    } else {
                        dl_set_prim_color(gdl, 0xFF, 0xFF, 0xFF, var_s2);
                    }
                    gSPGeometryMode(*gdl, 0xFFFFFF, G_SHADING_SMOOTH | G_SHADE | G_ZBUFFER);
                    dl_apply_geometry_mode(gdl);
                    tex_gdl_set_textures(gdl, spD4, NULL, 0, 0, 0, 0);
                    if (var_s0->unk80 & 0x40) {
                        if (var_s0->unk80 & 0x20) {
                            gDPSetCombineLERP(*gdl, 1, 0, SHADE, 0, 0, 0, 0, 1, COMBINED, 0, PRIMITIVE, 0, COMBINED, 0, PRIMITIVE, 0);
                            dl_apply_combine(gdl);
                        } else {
                            gDPSetCombineLERP(*gdl, 1, 0, ENVIRONMENT, 0, 1, 0, ENVIRONMENT, 0, COMBINED, 0, PRIMITIVE, 0, COMBINED, 0, PRIMITIVE, 0);
                            dl_apply_combine(gdl);
                        }
                        if (var_s0->unk7C & 0x10) {
                            gDPSetOtherMode(
                                *gdl,
                                G_AD_PATTERN | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_NONE | G_CYC_2CYCLE | G_PM_NPRIMITIVE,
                                G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_CLD_SURF2
                            );
                            dl_apply_other_mode(gdl);
                        } else if (var_s0->unk80 & 0x800000) {
                            gDPSetOtherMode(
                                *gdl,
                                G_AD_PATTERN | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_NONE | G_CYC_2CYCLE | G_PM_NPRIMITIVE,
                                G_AC_NONE | G_ZS_PRIM | G_RM_NOOP | G_RM_ZB_CLD_SURF2
                            );
                            dl_apply_other_mode(gdl);
                        } else {
                            gDPSetOtherMode(
                                *gdl,
                                G_AD_PATTERN | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_NONE | G_CYC_2CYCLE | G_PM_NPRIMITIVE,
                                G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_ZB_CLD_SURF2
                            );
                            dl_apply_other_mode(gdl);
                        }
                    } else {
                        if (var_s0->unk80 & 0x20) {
                            gDPSetCombineMode(*gdl, G_CC_MODULATEIDECALA, G_CC_MODULATEIA_PRIM2);
                            dl_apply_combine(gdl);
                        } else {
                            gDPSetCombineLERP(*gdl, TEXEL0, 0, ENVIRONMENT, 0, 0, 0, 0, TEXEL0, COMBINED, 0, PRIMITIVE, 0, COMBINED, 0, PRIMITIVE, 0);
                            dl_apply_combine(gdl);
                        }
                        if (var_s0->unk7C & 0x10) {
                            gDPSetOtherMode(
                                *gdl,
                                G_AD_PATTERN | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_PERSP | G_CYC_2CYCLE | G_PM_NPRIMITIVE,
                                G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_CLD_SURF2
                            );
                            dl_apply_other_mode(gdl);
                        } else if (var_s0->unk80 & 0x800000) {
                            gDPSetOtherMode(
                                *gdl,
                                G_AD_PATTERN | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_PERSP | G_CYC_2CYCLE | G_PM_NPRIMITIVE,
                                G_AC_NONE | G_ZS_PRIM | G_RM_NOOP | G_RM_ZB_CLD_SURF2
                            );
                            dl_apply_other_mode(gdl);
                        } else {
                            gDPSetOtherMode(
                                *gdl,
                                G_AD_PATTERN | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_PERSP | G_CYC_2CYCLE | G_PM_NPRIMITIVE,
                                G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_ZB_CLD_SURF2
                            );
                            dl_apply_other_mode(gdl);
                        }
                    }
                    gSPVertex((*gdl)++, OS_PHYSICAL_TO_K0(var_s0), 4, 0);
                    dl_triangles(gdl, _data_70, 2);
                    // @recomp: Pop matrix tag
                    gEXPopMatrixGroup((*gdl)++, G_MTX_MODELVIEW);
                }
            }
        }
    }
    func_8001F848(gdl);
    if (_data_68 != 0) {
        dll_13_func_52B4(gdl);
        _data_68 = 0;
    }

    return 0;
}
