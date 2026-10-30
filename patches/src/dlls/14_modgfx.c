#include "patches.h"
#include "matrix_groups.h"

#include "dlls/engine/14_modgfx.h"
#include "sys/camera.h"
#include "sys/lfx.h"
#include "sys/rand.h"
#include "sys/segment_13D0.h"
#include "sys/lighting.h"
#include "macros.h"

#include "recomp/dlls/engine/14_modgfx_recomp.h"

typedef struct {
    Object* unk0;
    Object* unk4;
    s16* unk8;
    SRT unkC;
    Vec3f unk24;
    Vec3f unk30[4];
    f32 unk60;
    f32 unk64;
    f32 unk68;
    Vec3f unk6C;
    Vtx* unk78[3];
    DLTri* unk84[3];
    DLTri* unk90;
    DLTri* unk94;
    Texture* unk98;
    ModgfxStruct_0* unk9C;
    LightAction* unkA0;
    s32 unkA4;
    s32 unkA8;
    f32 unkAC[4];
    f32 unkBC;
    f32 unkC0;
    f32 unkC4;
    f32 unkC8;
    f32 unkCC;
    f32 unkD0;
    f32 unkD4;
    u8 _unkD8[0xE6 - 0xD8];
    s16 unkE6;
    u8 padE8;
    s16 unkEA;
    s16 unkEC;
    s16 unkEE[7];
    s16 unkFC;
    s16 unkFE;
    s16 unk100;
    s16 unk102;
    s16 unk104;
    s16 unk106; // yaw
    s16 unk108; // pitch
    s16 unk10A; // roll
    s16 unk10C;
    s16 unk10E;
    s16 unk110;
    u16 pad112;
    s32 unk114[3];
    s16 unk120;
    s16 unk122;
    s16 unk124;
    u8 unk126;
    u8 _unk127[0x12C - 0x127];
    DLTri* unk12C;
    u8 unk130; // 0 or 1, indexes unk78
    u8 unk131;
    u8 unk132;
    u8 unk133;
    u8 unk134;
    u8 unk135;
    u8 unk136;
    u8 unk137;
    u8 unk138;
    s8 unk139; // keeps track of count for unk9C
    u8 unk13A;
    u8 unk13B;
    u8 unk13C;
    u8 unk13D;
    u8 unk13E;
    u8 unk13F;
} ModgfxInstance;

/*0x0*/ extern ModgfxInstance* bss_0[496];

extern void modgfx_func_4EDC(ModgfxInstance* arg0, u8 arg1);

RECOMP_PATCH s32 modgfx_Func6(Gfx** gdl, Mtx** mtxs, Vertex** vtxs, u8 arg3, Object* obj) {
    s32 idx;
    s32 i;
    DLTri* var_s6;
    s32 texFormat = 0;
    UNUSED s32 counter = 0;
    s32 tmem = 0;
    f32 magnitude;
    s32 var_s0;
    s32 var_s3;
    Vtx* var_s7;
    SRT srt;
    f32 sp284[3];
    f32 sp26C[6];
    s32 frameIdx;
    Vec3f sp25C;
    Camera* camera;
    u8 mixFrameNo = 0;
    u8 baseFrameNo = 0;
    u8 maskt = 0;
    u8 masks = 0;
    s32 someVar;
    Texture *baseTex;
    Texture* mixTex;
    s16 sp234[] = {
        0x0003, 0x0002, 0x0401, 0x0400, 0x0302, 0x0301, 0x0300, 0x0200, 0x0201
    };
    u8 skylightR;
    u8 skylightG;
    u8 skylightB;

    lightGetSkyLight(&skylightR, &skylightG, &skylightB);
    if (func_80000824(-1) == 1) {
        return 1;
    }
    camera = camGet();
    for (i = 0; i < ARRAYCOUNT_S(bss_0); i++) {
        if (bss_0[i] == NULL) {
            continue;
        }
        if (bss_0[i]->unk10C == -1) {
            continue;
        }
        if (arg3 && !(bss_0[i]->unkA4 & 0x2000)) {
            continue;
        }
        if (arg3 && (obj != bss_0[i]->unk4)) {
            continue;
        }
        if (!arg3 && (bss_0[i]->unkA4 & 0x2000)) {
            continue;
        }
        
        if (bss_0[i]->unkA4 & 0x800) {
            bss_0[i]->unk13E = 0;
        }

        counter++;

        idx = i;
        someVar = 0;

        var_s7 = bss_0[idx]->unk78[bss_0[idx]->unk130];
        if (bss_0[idx]->unkA4 & 0x40000) {
            modgfx_func_4EDC(bss_0[idx], 0);
        }
        var_s6 = bss_0[idx]->unk84[bss_0[idx]->unk130];
        srt.transl.x = 0.0f;
        srt.transl.y = 0.0f;
        srt.transl.z = 0.0f;
        srt.scale = 1.0f;
        srt.roll = 0;
        srt.pitch = 0;
        sp284[0] = bss_0[idx]->unk60;
        sp284[1] = bss_0[idx]->unk64;
        sp284[2] = bss_0[idx]->unk68;
        if ((bss_0[idx]->unkA4 & 4) && ((sp284[0] + sp284[1] + sp284[2]) == 0.0f)) {
            someVar = 1;
        }
        if ((bss_0[idx]->unkA4 & 4) && (someVar == 0)) {
            if (bss_0[idx]->unk4 != NULL) {
                srt.yaw = bss_0[idx]->unk4->srt.yaw;
                srt.pitch = bss_0[idx]->unk4->srt.pitch;
                srt.roll = bss_0[idx]->unk4->srt.roll;
                mathRotateRPY(&srt, sp284);
            }
        }
        sp25C.x = 0.0f;
        sp25C.y = 0.0f;
        sp25C.z = 0.0f;
        if (!(bss_0[idx]->unkA4 & 1)) {
            if (bss_0[idx]->unk4 != NULL) {
                sp25C.x = bss_0[idx]->unk4->globalPosition.x;
                sp25C.y = bss_0[idx]->unk4->globalPosition.y;
                sp25C.z = bss_0[idx]->unk4->globalPosition.z;
            } else {
                sp25C.x = bss_0[idx]->unkC.transl.x;
                sp25C.y = bss_0[idx]->unkC.transl.y;
                sp25C.z = bss_0[idx]->unkC.transl.z;
                camTransformPointByObjectMatrix(&bss_0[idx]->unkC.transl, &sp25C, bss_0[idx]->unk135);
            }
        }
        if ((sp25C.x > 65534.0f) || (sp25C.x < -65534.0f)) {
            STUBBED_PRINTF("\t modgfx x %f\n", &sp25C.x);
            sp25C.x = -gWorldX;
        }
        if ((sp25C.y > 65534.0f) || (sp25C.y < -65534.0f)) {
            sp25C.y = 0.0f;
        }
        if ((sp25C.z > 65534.0f) || (sp25C.z < -65534.0f)) {
            STUBBED_PRINTF("\t modgfx z %f\n", &sp25C.z);
            sp25C.z = -gWorldZ;
        }
        srt.transl.x = sp284[0] + sp25C.x;
        srt.transl.y = sp284[1] + sp25C.y;
        srt.transl.z = sp284[2] + sp25C.z;
        if (bss_0[idx]->unkA4 & 0x400000) {
            srt.scale = (bss_0[idx]->unkD4 * 0.5f) + ((bss_0[idx]->unkD4 * 0.5f) / (f32) mathRnd(1, 10));
        } else {
            srt.scale = bss_0[idx]->unkD4 * 0.01f;
        }
        if (bss_0[idx]->unkA4 & 0x80000) {
            srt.roll = bss_0[idx]->unk4->srt.roll;
            srt.pitch = bss_0[idx]->unk4->srt.pitch;
            srt.yaw = bss_0[idx]->unk4->srt.yaw;
        } else if (someVar != 0 && bss_0[idx]->unk4 != NULL) {
            srt.roll = bss_0[idx]->unk106 + bss_0[idx]->unk4->srt.roll;
            srt.pitch = bss_0[idx]->unk108 + bss_0[idx]->unk4->srt.pitch;
            srt.yaw = bss_0[idx]->unk10A + bss_0[idx]->unk4->srt.yaw;
        } else if (someVar != 0) {
            srt.roll = bss_0[idx]->unk106 + bss_0[idx]->unkC.roll;
            srt.pitch = bss_0[idx]->unk108 + bss_0[idx]->unkC.pitch;
            srt.yaw = bss_0[idx]->unk10A + bss_0[idx]->unkC.yaw;
        } else {
            srt.roll = bss_0[idx]->unk106;
            srt.pitch = bss_0[idx]->unk108;
            srt.yaw = bss_0[idx]->unk10A;
        }
        if ((bss_0[idx]->unkA4 & 0x1000) && (bss_0[idx]->unk4 != NULL)) {
            sp26C[3] = 0.0f; // wat
            sp26C[4] = 0.0f;
            sp26C[5] = -1.0f;
            sp26C[0] = camera->tx - bss_0[idx]->unk4->globalPosition.x;
            sp26C[1] = 0.0f;
            sp26C[2] = camera->tz - bss_0[idx]->unk4->globalPosition.z;
            magnitude = sqrtf(SQ(sp26C[0]) + SQ(sp26C[2]));
            if (magnitude != 0/*.0f*/) {
                sp26C[0] /= magnitude;
                sp26C[2] /= magnitude;
            }
            srt.yaw += (s16) (f32) mathAtan2f(sp26C[0], sp26C[2]);
        }

        // @recomp: Tag modgfx matrix
        //          The game already comes up with a reasonably unique ID for each modgfx instance, so use that.
        //          We could also scope by object to really make sure it's unique but so far this seems fine.
        gEXMatrixGroupDecomposed((*gdl)++, bss_0[idx]->unk10C + MODGFX_MTX_GROUP_ID_START, G_EX_PUSH, G_MTX_MODELVIEW, 
            /*pos*/ G_EX_COMPONENT_INTERPOLATE, 
            /*rot*/ G_EX_COMPONENT_INTERPOLATE, 
            /*scale*/ G_EX_COMPONENT_INTERPOLATE, 
            /*skew*/ G_EX_COMPONENT_INTERPOLATE, 
            /*persp*/ G_EX_COMPONENT_INTERPOLATE, 
            /*vert*/ G_EX_COMPONENT_INTERPOLATE, 
            /*tile*/ G_EX_COMPONENT_INTERPOLATE, 
            /*order*/ G_EX_ORDER_LINEAR, 
            /*edit*/ G_EX_EDIT_NONE, 
            /*tc*/ G_EX_COMPONENT_SKIP, // TODO: figure out how to support texture animation looping 
            /*lookat*/ G_EX_COMPONENT_AUTO);

        camSetupObjectSRTMatrix(gdl, mtxs, &srt, 1.0f, 0/*.0f*/, NULL);
        if (bss_0[idx]->unk98 != NULL && bss_0[idx]->unk98->next != NULL && bss_0[idx]->unk132 != 0) {
            bss_0[idx]->unk133 -= 1;
            if (bss_0[idx]->unk133 <= 0) {
                bss_0[idx]->unk133 = 0x3C / (s32) bss_0[idx]->unk132;
                bss_0[idx]->unk131++;
                if (bss_0[idx]->unk131 >= (bss_0[idx]->unk98->animDuration >> 8)) {
                    bss_0[idx]->unk131 = 0;
                }
            }
        }
        if (bss_0[idx]->unk98 != NULL) {
            texFormat = TEX_FORMAT(bss_0[idx]->unk98->format);
        }
        gSPLoadGeometryMode(*gdl, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
        dlApplyGeometryMode(gdl);
        if (bss_0[idx]->unkA4 & 0x10000000) {
            dlSetPrimColor(gdl, skylightR, skylightG, skylightB, 255);
        } else if ((bss_0[idx]->unk4 != NULL) && (bss_0[idx]->unkA4 & 0x4000)) {
            dlSetPrimColor(gdl, 255, 255, 255, bss_0[idx]->unk4->opacityWithFade);
        } else {
            dlSetPrimColor(gdl, 255, 255, 255, 255);
        }
        if (bss_0[idx]->unk98 != NULL) {
            tmem = bss_0[idx]->unk98->sizeBytes >> 3;
            baseFrameNo = bss_0[idx]->unk131;
            mixFrameNo = baseFrameNo + 1;
            if (mixFrameNo >= (bss_0[idx]->unk98->animDuration >> 8)) {
                mixFrameNo = 0;
            }
            if (bss_0[idx]->unk98->sizeBytes > 0x800) {
                if (bss_0[idx]->unkA4 & 0x01000000) {
                    // STUBBED_PRINTF("modgfx can't TEXMIX framesize %d\n", 0); // default.dol
                    bss_0[idx]->unkA4 ^= 0x01000000;
                    bss_0[idx]->unkA4 |= 0x04000000;
                }
            }
            if (bss_0[idx]->unkA4 & 0x05000000) {
                switch (bss_0[idx]->unk98->width) {
                    case 128:
                        masks = 7;
                        break;
                    case 64:
                        masks = 6;
                        break;
                    case 32:
                        masks = 5;
                        break;
                    case 16:
                        masks = 4;
                        break;
                    case 8:
                        masks = 3;
                        break;
                }
                switch (bss_0[idx]->unk98->height) {
                    case 128:
                        maskt = 7;
                        break;
                    case 64:
                        maskt = 6;
                        break;
                    case 32:
                        maskt = 5;
                        break;
                    case 16:
                        maskt = 4;
                        break;
                    case 8:
                        maskt = 3;
                    break;
                }
            }
        }
        if ((bss_0[idx]->unkA4 & 0x01000000) && ((bss_0[idx]->unk13E != 0) || (bss_0[idx]->unkA4 & 0x400))) {
            mixTex = bss_0[idx]->unk98;
            for (frameIdx = 0; frameIdx < mixFrameNo; frameIdx++) {
                mixTex = mixTex->next;
            }
            dlSetEnvColor(gdl, 0xFF, 0xFF, 0xFF, 0xFF - (bss_0[idx]->unk133 * bss_0[idx]->unk134));
            gDPSetCombineLERP(*gdl,
                TEXEL1, TEXEL0, ENV_ALPHA, TEXEL0, TEXEL1, TEXEL0, ENVIRONMENT, TEXEL0, 
                COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
            dlApplyCombine(gdl);
            switch (texFormat) {
            case TEX_FORMAT_RGBA32:
                gDPLoadMultiBlockS((*gdl)++,
                    /*timg*/mixTex + 1,
                    /*tmem*/tmem,
                    /*rtile*/1,
                    /*fmt*/G_IM_FMT_RGBA,
                    /*siz*/G_IM_SIZ_32b,
                    /*width*/mixTex->width,
                    /*height*/mixTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            case TEX_FORMAT_RGBA16:
                gDPLoadMultiBlockS((*gdl)++,
                    /*timg*/mixTex + 1,
                    /*tmem*/tmem,
                    /*rtile*/1,
                    /*fmt*/G_IM_FMT_RGBA,
                    /*siz*/G_IM_SIZ_16b,
                    /*width*/mixTex->width,
                    /*height*/mixTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            case TEX_FORMAT_I8:
                gDPLoadMultiBlockS((*gdl)++,
                    /*timg*/mixTex + 1,
                    /*tmem*/tmem,
                    /*rtile*/1,
                    /*fmt*/G_IM_FMT_I,
                    /*siz*/G_IM_SIZ_8b,
                    /*width*/mixTex->width,
                    /*height*/mixTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            case TEX_FORMAT_I4:
                gDPLoadMultiBlockS((*gdl)++,
                    /*timg*/mixTex + 1,
                    /*tmem*/tmem,
                    /*rtile*/1,
                    /*fmt*/G_IM_FMT_I,
                    /*siz*/G_IM_SIZ_4b,
                    /*width*/mixTex->width,
                    /*height*/mixTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            case TEX_FORMAT_IA16:
                gDPLoadMultiBlockS((*gdl)++,
                    /*timg*/mixTex + 1,
                    /*tmem*/tmem,
                    /*rtile*/1,
                    /*fmt*/G_IM_FMT_IA,
                    /*siz*/G_IM_SIZ_16b,
                    /*width*/mixTex->width,
                    /*height*/mixTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            case TEX_FORMAT_IA8:
                gDPLoadMultiBlockS((*gdl)++,
                    /*timg*/mixTex + 1,
                    /*tmem*/tmem,
                    /*rtile*/1,
                    /*fmt*/G_IM_FMT_IA,
                    /*siz*/G_IM_SIZ_8b,
                    /*width*/mixTex->width,
                    /*height*/mixTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            case TEX_FORMAT_IA4:
                gDPLoadMultiBlockS((*gdl)++,
                    /*timg*/mixTex + 1,
                    /*tmem*/tmem,
                    /*rtile*/1,
                    /*fmt*/G_IM_FMT_IA,
                    /*siz*/G_IM_SIZ_4b,
                    /*width*/mixTex->width,
                    /*height*/mixTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            }
        } else if (bss_0[idx]->unkA4 & 0x02000000) {
            gDPSetCombineLERP(*gdl, 1, 0, SHADE, 0, 1, 0, SHADE, 0, COMBINED, 0, PRIMITIVE, 0, COMBINED, 0, PRIMITIVE, 0);
            dlApplyCombine(gdl);
        } else if (bss_0[idx]->unkA4 & 0x04000000) {
            gDPSetCombineMode(*gdl, G_CC_MODULATEIA, G_CC_MODULATEIA_PRIM2);
            dlApplyCombine(gdl);
        }
        var_s3 = 0;
        if ((bss_0[idx]->unkA4 & 0x05000000) && ((bss_0[idx]->unk13E != 0) || (bss_0[idx]->unkA4 & 0x400))) {
            baseTex = bss_0[idx]->unk98;
            for (frameIdx = 0; frameIdx < baseFrameNo; frameIdx++) {
                baseTex = baseTex->next;
            }
            switch (texFormat) {
            case TEX_FORMAT_RGBA32:
                gDPLoadTextureBlockS((*gdl)++,
                    /*timg*/baseTex + 1,
                    /*fmt*/G_IM_FMT_RGBA,
                    /*siz*/G_IM_SIZ_32b,
                    /*width*/baseTex->width,
                    /*height*/baseTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            case TEX_FORMAT_RGBA16:
                gDPLoadTextureBlockS((*gdl)++,
                    /*timg*/baseTex + 1,
                    /*fmt*/G_IM_FMT_RGBA,
                    /*siz*/G_IM_SIZ_16b,
                    /*width*/baseTex->width,
                    /*height*/baseTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            case TEX_FORMAT_I8:
                gDPLoadTextureBlockS((*gdl)++,
                    /*timg*/baseTex + 1,
                    /*fmt*/G_IM_FMT_I,
                    /*siz*/G_IM_SIZ_8b,
                    /*width*/baseTex->width,
                    /*height*/baseTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            case TEX_FORMAT_I4:
                gDPLoadTextureBlockS((*gdl)++,
                    /*timg*/baseTex + 1,
                    /*fmt*/G_IM_FMT_I,
                    /*siz*/G_IM_SIZ_4b,
                    /*width*/baseTex->width,
                    /*height*/baseTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            case TEX_FORMAT_IA16:
                gDPLoadTextureBlockS((*gdl)++,
                    /*timg*/baseTex + 1,
                    /*fmt*/G_IM_FMT_IA,
                    /*siz*/G_IM_SIZ_16b,
                    /*width*/baseTex->width,
                    /*height*/baseTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            case TEX_FORMAT_IA8:
                gDPLoadTextureBlockS((*gdl)++,
                    /*timg*/baseTex + 1,
                    /*fmt*/G_IM_FMT_IA,
                    /*siz*/G_IM_SIZ_8b,
                    /*width*/baseTex->width,
                    /*height*/baseTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            case TEX_FORMAT_IA4:
                gDPLoadTextureBlockS((*gdl)++,
                    /*timg*/baseTex + 1,
                    /*fmt*/G_IM_FMT_IA,
                    /*siz*/G_IM_SIZ_4b,
                    /*width*/baseTex->width,
                    /*height*/baseTex->height,
                    /*pal*/0,
                    /*cms*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*cmt*/G_TX_NOMIRROR | G_TX_WRAP,
                    /*masks*/masks,
                    /*maskt*/maskt,
                    /*shifts*/G_TX_NOLOD,
                    /*shiftt*/G_TX_NOLOD
                );
                break;
            default:
                texDPTextures(gdl, baseTex, NULL, 0, 0, 0, TRUE);
                var_s3 = 1;
                break;
            }
        }
        if (var_s3 == 0) {
            if (bss_0[idx]->unkA4 & 0x100) {
                gDPSetOtherMode(
                    *gdl,
                    G_AD_PATTERN | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_PERSP | G_CYC_2CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_AA_ZB_XLU_INTER2
                );
                dlApplyOtherMode(gdl);
            } else if ((bss_0[idx]->unkA4 & 0x10) && (bss_0[idx]->unkA4 & 0x80)) {
                gDPSetOtherMode(
                    *gdl,
                    G_AD_PATTERN | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_PERSP | G_CYC_2CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_AA_XLU_SURF2
                );
                dlApplyOtherMode(gdl);
            } else if (bss_0[idx]->unkA4 & 0x80) {
                gDPSetOtherMode(
                    *gdl,
                    G_AD_PATTERN | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_PERSP | G_CYC_2CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_AA_ZB_XLU_SURF2
                );
                dlApplyOtherMode(gdl);
            } else if (bss_0[idx]->unkA4 & 0x10) {
                gDPSetOtherMode(
                    *gdl,
                    G_AD_PATTERN | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_PERSP | G_CYC_2CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_CLD_SURF2
                );
                dlApplyOtherMode(gdl);
            } else {
                gDPSetOtherMode(
                    *gdl,
                    G_AD_PATTERN | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_PERSP | G_CYC_2CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_ZB_CLD_SURF2
                );
                dlApplyOtherMode(gdl);
            }
        }
        if ((bss_0[idx]->unk13E != 0) || (bss_0[idx]->unkA4 & 0x400)) {
            for (var_s0 = 0; var_s0 < bss_0[idx]->unk136; var_s0++) {
                gSPVertex((*gdl)++, OS_PHYSICAL_TO_K0(var_s7), bss_0[idx]->unk138, 0);
                if (bss_0[idx]->unkA4 & 0x08000000) {
                    dlTriangles(gdl, var_s6, bss_0[idx]->unkEC / bss_0[idx]->unk136);
                } else {
                    dlTriangles(gdl, var_s6, bss_0[idx]->unkEC);
                }
                var_s7 += bss_0[idx]->unk137;
                if (bss_0[idx]->unkA4 & 0x08000000) {
                    var_s6 += bss_0[idx]->unkEC / bss_0[idx]->unk136;
                }
            }
        }
        texRenderReset();
        bss_0[idx]->unk130 = 1 - bss_0[idx]->unk130;
    }
    return 0;
}
