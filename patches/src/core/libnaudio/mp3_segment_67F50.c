#include "patches.h"
#include "patches/mp3.h"
#include "recomp_funcs.h"

// TODO: get from decomp headers
struct mp3vars {
	/*0x00*/ s32 romaddr;
	/*0x04*/ struct asistream *stream;
	/*0x08*/ ENVMIX_STATE *em_state;
	/*0x0c*/ s16 em_pan;
	/*0x0e*/ s16 em_volume;
	/*0x10*/ s16 em_cvolL;
	/*0x12*/ s16 em_cvolR;
	/*0x14*/ s16 em_dryamt;
	/*0x16*/ s16 em_wetamt;
	/*0x18*/ u16 em_lratl;
	/*0x1a*/ s16 em_lratm;
	/*0x1c*/ s16 em_ltgt;
	/*0x1e*/ u16 em_rratl;
	/*0x20*/ s16 em_rratm;
	/*0x22*/ s16 em_rtgt;
	/*0x24*/ s16 em_first;
	/*0x28*/ s32 em_delta;
	/*0x2c*/ s32 em_segEnd;
	/*0x30*/ s32 filesize;
	/*0x34*/ s32 dmaoffset;
	/*0x38*/ u16 *var8009c3c8;
	/*0x3c*/ s32 var8009c3cc;
	/*0x40*/ s32 var8009c3d0;
	/*0x44*/ u32 *var8009c3d4[1];
	/*0x48*/ u32 var8009c3d8;
	/*0x4c*/ void *dmafunc;
	/*0x50*/ u32 state;
	/*0x54*/ u32 currentvol;
	/*0x58*/ u32 var8009c3e8;
	/*0x5c*/ s16 currentpan;
	/*0x5e*/ s16 targetpan;
	/*0x60*/ u8 statetimer;
	/*0x61*/ u8 dualchannel;
};

extern struct mp3vars D_800BFF00;
extern void func_8006804C(struct mp3vars*);
extern void mp3_dma(void);

static s32 recomp_lastMp3Vol = 0x7fff;

void recomp_update_mp3_volume(void) {
    D_800BFF00.currentvol = recomp_lastMp3Vol * (recomp_get_dialog_volume() / 100.0f);
    func_8006804C(&D_800BFF00); // update_mp3_vars
}

RECOMP_PATCH void func_80067650(s32 vol, s32 a1) {
    // @recomp: Save last MP3 volume so we can use it if dialog volume is changed mid-playback
    recomp_lastMp3Vol = vol;

    // @recomp: Factor in recomp dialog volume
    vol *= (recomp_get_dialog_volume() / 100.0f);

    if (vol < 0) {
        D_800BFF00.currentvol = 0;
    } else {
        D_800BFF00.currentvol = vol;
        if (vol > 0x7fff) {
            D_800BFF00.currentvol = 0x7fff;
        }
    }

    D_800BFF00.var8009c3e8 = a1;
}

RECOMP_PATCH void mp3_play_file(s32 romaddr, s32 filesize) {
    if (D_800BFF00.dmafunc != NULL) {
        // @recomp: Save last MP3 volume so we can use it if dialog volume is changed mid-playback
        recomp_lastMp3Vol = 0x7fff;

        D_800BFF00.dmaoffset = 0;
        D_800BFF00.var8009c3e8 = 0;
        // @recomp: Factor in recomp dialog volume
        D_800BFF00.currentvol = 0x7fff * (recomp_get_dialog_volume() / 100.0f);
        D_800BFF00.statetimer = 5;
        D_800BFF00.romaddr = romaddr;
        D_800BFF00.filesize = filesize;
        mp3_dma();
        D_800BFF00.state = 4;
    }
}
