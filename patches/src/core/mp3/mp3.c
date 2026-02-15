#include "patches.h"
#include "patches/mp3.h"
#include "recomp_funcs.h"

#include "mp3/mp3_internal.h"

extern struct mp3vars g_Mp3Vars;
extern void mp3_update_vars(struct mp3vars*);
extern void mp3_dma(void);

static s32 recomp_lastMp3Vol = 0x7fff;

void recomp_update_mp3_volume(void) {
    g_Mp3Vars.currentvol = recomp_lastMp3Vol * (recomp_get_dialog_volume() / 100.0f);
    mp3_update_vars(&g_Mp3Vars);
}

RECOMP_PATCH void mp3_set_volume(s32 vol, s32 a1) {
    // @recomp: Save last MP3 volume so we can use it if dialog volume is changed mid-playback
    recomp_lastMp3Vol = vol;

    // @recomp: Factor in recomp dialog volume
    vol *= (recomp_get_dialog_volume() / 100.0f);

    if (vol < 0) {
        g_Mp3Vars.currentvol = 0;
    } else {
        g_Mp3Vars.currentvol = vol;
        if (vol > 0x7fff) {
            g_Mp3Vars.currentvol = 0x7fff;
        }
    }

    g_Mp3Vars.var8009c3e8 = a1;
}

RECOMP_PATCH void mp3_play_file(s32 romaddr, s32 filesize) {
    if (g_Mp3Vars.dmafunc != NULL) {
        // @recomp: Save last MP3 volume so we can use it if dialog volume is changed mid-playback
        recomp_lastMp3Vol = 0x7fff;

        g_Mp3Vars.dmaoffset = 0;
        g_Mp3Vars.var8009c3e8 = 0;
        // @recomp: Factor in recomp dialog volume
        g_Mp3Vars.currentvol = 0x7fff * (recomp_get_dialog_volume() / 100.0f);
        g_Mp3Vars.statetimer = 5;
        g_Mp3Vars.romaddr = romaddr;
        g_Mp3Vars.filesize = filesize;
        mp3_dma();
        g_Mp3Vars.state = 4;
    }
}
