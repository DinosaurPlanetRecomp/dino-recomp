#pragma once

// Original values from the game
#define MAIN_GFX_BUF_SIZE 0x8CA0
#define MAIN_MTX_BUF_SIZE 0x11300
#define MAIN_VTX_BUF_SIZE 0x12C0
#define MAIN_POL_BUF_SIZE 0x320

// Increase buffer sizes in recomp
#define RECOMP_MAIN_GFX_BUF_SIZE ((int)(MAIN_GFX_BUF_SIZE * 2))
#define RECOMP_MAIN_MTX_BUF_SIZE ((int)(MAIN_MTX_BUF_SIZE * 2))
#define RECOMP_MAIN_VTX_BUF_SIZE ((int)(MAIN_VTX_BUF_SIZE * 2))
#define RECOMP_MAIN_POL_BUF_SIZE ((int)(MAIN_POL_BUF_SIZE * 2))
