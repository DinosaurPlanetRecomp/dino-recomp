#include "patches.h"

#include "libnaudio/n_libaudio.h"

#include "recomp/dlls/engine/5_AMSEQ_recomp.h"

// size:0x24C
typedef struct {
    u8 unk0;
    u8 _unk1[0x4 - 0x1];
    N_ALCSPlayer unk4;
    ALCSeq unk90;
    void *unk188;
    u8 _unk18C[0x190 - 0x18C];
    s16 unk190;
    u8 _unk192[0x24C - 0x192];
} AMSEQBss0;

extern AMSEQBss0 **_bss_0;

extern u16 _bss_28;
extern u16 _bss_2A;
extern u16 _bss_2C;

RECOMP_PATCH void dll_5_func_F74(u32 arg0) {
    AMSEQBss0* temp_v0;

    if (arg0 > 0x100) {
        arg0 = 0x100;
    }
    _bss_28 = arg0;
    _bss_2C = (_bss_28 * _bss_2A) >> 8;
    temp_v0 = _bss_0[0];
    n_alCSPSetVol(&temp_v0->unk4, (s16) ((temp_v0->unk190 >> 4) * _bss_2C));
    temp_v0 = _bss_0[1];
    n_alCSPSetVol(&temp_v0->unk4, (s16) ((temp_v0->unk190 >> 4) * _bss_2C));
    temp_v0 = _bss_0[2];
    n_alCSPSetVol(&temp_v0->unk4, (s16) ((temp_v0->unk190 >> 4) * _bss_2C));
    // @recomp: Fix missing volume set for music player #4
    temp_v0 = _bss_0[3];
    n_alCSPSetVol(&temp_v0->unk4, (s16) ((temp_v0->unk190 >> 4) * _bss_2C));
}
