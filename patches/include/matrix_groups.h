#pragma once

#include "PR/ultratypes.h"
#include "game/objects/object.h"
#include "sys/math.h"

// Cameras: 0x00000010 - 0x0000001F
#define CAMERA_MTX_GROUP_ID_START 0x00000010

// Object modgfx calls: 0x00005000 - 0x00005FFF
#define OBJ_MODGFX_MTX_GROUP_ID_START 0x00005000
// Object print (fallback group for stuff rendered during the print func): 0x00006000 - 0x00006FFF
#define OBJ_PRINT_AUTO_MTX_GROUP_ID_START 0x00006000
// Object shadows: 0x00007000 - 0x00007FFF
#define OBJ_SHADOW_MTX_GROUP_ID_START 0x00007000
// Object shadowtex models: 0x00008000 - 0x00008FFF
#define OBJ_SHADOWTEX_MODEL_MTX_GROUP_ID_START 0x00008000

// Single groups for graphics DLLs (eventually should be more granular per DLL)
#define NEWDAY_MTX_GROUP_ID 0x0000F000
#define NEWSTARS_MTX_GROUP_ID 0x0000F001
#define NEWCLOUDS_MTX_GROUP_ID 0x0000F002
#define PROJGFX_MTX_GROUP_ID 0x0000F003
#define MINIC_MTX_GROUP_ID 0x0000F004
#define MODGFX_MTX_GROUP_ID 0x0000F005
#define WATERFX_MTX_GROUP_ID 0x0000F006

// Object models (min length: 180 * 16): 0x00100000 - 0x00101000
// Note: The most models in a vanilla object is 12
#define OBJ_MODEL_MTX_GROUP_ID_START 0x00100000
#define OBJ_MODEL_MTX_GROUP_MAX_MODELS 16
// Object linked object models (min length: 180 * 16): 0x00200000 - 0x00201000
#define OBJ_LINKEDOBJ_MODEL_MTX_GROUP_ID_START 0x00200000

// Expgfx particles (min length: 30000): 0x04000000 - 0x04008000
#define EXPGFX_MTX_GROUP_ID_START 0x04000000

// Block shapes (unique per global grid coord and layer): 0x10000000 - 0x11000000
#define BLOCK_SHAPE_MTX_GROUP_ID_START 0x10000000

extern _Bool recomp_frameInterpActive;
extern MtxF *recomp_objParentMtx;

MtxF* recomp_model_instance_setup_absolute_matrices(ModelInstance *modelInst, s32 count);

u32 recomp_obj_get_matrix_group(Object *obj, _Bool *skipInterpolation);
void recomp_set_skip_camera_interpolation(_Bool skip);
