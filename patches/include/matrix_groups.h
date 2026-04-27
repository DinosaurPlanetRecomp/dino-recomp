#pragma once

#include "PR/ultratypes.h"
#include "game/objects/object.h"

// Cameras: 0x00000010 - 0x0000001F
#define CAMERA_MTX_GROUP_ID_START 0x00000010

// Object cameras (parent matrices): 0x00001000 - 0x00004FFF
#define OBJ_CAMERA_MTX_GROUP_ID_START 0x00001000

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

// Object models (min length: 180 * 8): 0x00100000 - 0x00101000
#define OBJ_MODEL_MTX_GROUP_ID_START 0x00100000
// Object linked object models (min length: 180 * 8): 0x00200000 - 0x00201000
#define OBJ_LINKEDOBJ_MODEL_MTX_GROUP_ID_START 0x00200000

// Block shapes: 0x01000000 - 0x03000000
#define BLOCK_SHAPE_MTX_GROUP_ID_START 0x01000000

// Expgfx particles (min length: 30000): 0x04000000 - 0x04008000
#define EXPGFX_MTX_GROUP_ID_START 0x04000000

u32 recomp_obj_get_matrix_group(Object *obj, _Bool *skipInterpolation);
void recomp_set_skip_camera_interpolation(_Bool skip);
