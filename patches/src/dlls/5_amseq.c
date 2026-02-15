#include "patches.h"

#include "libnaudio/n_libaudio.h"

#include "recomp/dlls/engine/5_AMSEQ_recomp.h"

// size:0x24C
typedef struct {
/*000*/ u8 unk0;
/*004*/ N_ALCSPlayer seqp;
/*090*/ ALCSeq seq;
/*118*/ void *midiData;
/*18C*/ u8 currentSeqID;
/*18E*/ s16 bpm;
/*190*/ s16 volume;
/*192*/ u16 unk192;
/*194*/ u16 unk194; // enabled channels?
/*196*/ u16 unk196; // dirty channels?
/*198*/ u16 unk198; // ignore channels?
/*19A*/ u8 volUpRate; // volume per tick
/*19B*/ u8 volDownRate; // volume per tick
/*19C*/ s16 channelVolumes[16];
/*1BC*/ u8 _unk1BC[0x1FC - 0x1BC];
/*1FC*/ u8 nextSeqID; // current music/ambient id?
/*1FE*/ s16 nextBPM;
/*200*/ u16 unk200;
/*202*/ s16 targetVolume;
/*204*/ u16 unk204;
/*206*/ u16 unk206;
/*208*/ u16 unk208;
/*20A*/ u8 nextVolUpRate; // volume per tick
/*20B*/ u8 nextVolDownRate; // volume per tick
/*20C*/ u8 _unk20C[0x24C - 0x20C];
} AMSEQPlayer;

extern AMSEQPlayer **sSeqPlayers;

extern u16 sVolumeGameOption;
extern u16 sUnkVolume;
extern u16 sCurrGlobalVolume;

RECOMP_PATCH void amseq_set_volume_option(u32 volume) {
    AMSEQPlayer* player;

    if (volume > 256) {
        volume = 256;
    }
    sVolumeGameOption = volume;
    sCurrGlobalVolume = (sVolumeGameOption * sUnkVolume) >> 8;
    player = sSeqPlayers[0];
    n_alCSPSetVol(&player->seqp, (s16) ((player->volume >> 4) * sCurrGlobalVolume));
    player = sSeqPlayers[1];
    n_alCSPSetVol(&player->seqp, (s16) ((player->volume >> 4) * sCurrGlobalVolume));
    player = sSeqPlayers[2];
    n_alCSPSetVol(&player->seqp, (s16) ((player->volume >> 4) * sCurrGlobalVolume));
    // @recomp: Fix missing volume set for music player #4
    player = sSeqPlayers[3];
    n_alCSPSetVol(&player->seqp, (s16) ((player->volume >> 4) * sCurrGlobalVolume));
}
