#include "patches.h"
#include "patches/5_amseq.h"

#include "libnaudio/n_libaudio.h"
#include "game/gamebits.h"
#include "sys/asset.h"
#include "sys/main.h"
#include "sys/memory.h"
#include "sys/pi.h"

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
extern ALSeqFile *sSeqFiles[2];

extern u16 sVolumeGameOption;
extern u16 sUnkVolume;
extern u16 sCurrGlobalVolume;

extern void amseq_determine_vols(u8 arg0, u8 arg1, u16 arg2, u16 arg3, u16 arg4, s8* arg5);

static u16 recomp_currMusVolume = 256;
static u16 recomp_currAmbVolume = 256;

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

    // @recomp: Overwrite recomp options
    recomp_currMusVolume = sCurrGlobalVolume;
    recomp_currAmbVolume = sCurrGlobalVolume;
}

void recomp_amseq_set_volume_option(u32 musVolume, u32 ambVolume) {
    AMSEQPlayer* player;

    if (musVolume > 256) {
        musVolume = 256;
    }
    if (ambVolume > 256) {
        ambVolume = 256;
    }
    sVolumeGameOption = MAX(musVolume, ambVolume);
    sCurrGlobalVolume = (sVolumeGameOption * sUnkVolume) >> 8;

    recomp_currMusVolume = (musVolume * sUnkVolume) >> 8;
    recomp_currAmbVolume = (ambVolume * sUnkVolume) >> 8;

    player = sSeqPlayers[0];
    n_alCSPSetVol(&player->seqp, (s16) ((player->volume >> 4) * recomp_currAmbVolume));
    player = sSeqPlayers[1];
    n_alCSPSetVol(&player->seqp, (s16) ((player->volume >> 4) * recomp_currAmbVolume));
    player = sSeqPlayers[2];
    n_alCSPSetVol(&player->seqp, (s16) ((player->volume >> 4) * recomp_currMusVolume));
    player = sSeqPlayers[3];
    n_alCSPSetVol(&player->seqp, (s16) ((player->volume >> 4) * recomp_currMusVolume));
}

RECOMP_PATCH void amseq_start_next_sequence(u8 playerNo) {
    AMSEQPlayer* player;
    N_ALCSPlayer* seqp;
    s32 idx;
    s32 offset;
    s32 seqFileIdx;
    s8 channelVolumes[16]; // has to be at least 16
    s32 temp;

    player = sSeqPlayers[playerNo];
    if (player->nextSeqID == 0) {
        // Next sequence not set, nothing to start, let current sequence continue (if any)
        return;
    }
    seqp = &player->seqp;
    if (n_alCSPGetState(seqp) != AL_STOPPED) {
        n_alCSPStop(seqp);
        player->currentSeqID = 0;
        player->unk0 &= ~0x1;
        return;
    }
    if (((playerNo == 2) || (playerNo == 3)) && (mainGetBits(BIT_DisableMusic) != 0)) {
        player->currentSeqID = 0;
        player->unk0 &= ~0x1;
        return;
    }
    // "startSequencer %d\n"
    player->currentSeqID = player->nextSeqID;
    player->nextSeqID = 0;
    if (playerNo >= 2) {
        seqFileIdx = 1;
    } else {
        seqFileIdx = 0;
    }
    idx = player->currentSeqID - 1;
    offset = sSeqFiles[seqFileIdx]->seqArray[idx].offset - (u8*)piRomGetSectionPtr(AUDIO_BIN, 0);
    if (player->midiData != NULL) {
        mmFree(player->midiData);
    }
    player->midiData = mmAlloc(sSeqFiles[seqFileIdx]->seqArray[idx].len, ALLOC_TAG_SEQ_COL, NULL);
    if (player->midiData != NULL) {
        // "download sequence %d:%d\n"
        assetRomLoadSection(player->midiData, AUDIO_BIN, offset, sSeqFiles[seqFileIdx]->seqArray[idx].len);
        n_alCSeqNew(&player->seq, (u8* ) player->midiData);
        n_alCSPSetSeq(seqp, &player->seq);
        n_alCSPPlay(seqp);
        player->bpm = player->nextBPM;
        if (player->bpm != 0) {
            n_alCSPSetTempo(seqp, (s32) ((1.0f / (f32) player->bpm) * 6e7f));
        }
        player->volUpRate = player->nextVolUpRate;
        player->volDownRate = player->nextVolDownRate;
        if (player->volUpRate) {
            player->volume = 0;
            player->targetVolume = player->targetVolume;
        } else {
            player->volume = player->targetVolume = player->targetVolume;
        }
        // @recomp: Global volume per player type
        n_alCSPSetVol(seqp, (player->volume >> 4) * (playerNo >= 2 ? recomp_currMusVolume : recomp_currAmbVolume));
        player->unk192 = player->unk200;
        player->seqp.chanMask = player->unk192;
        player->unk194 = player->unk204;
        player->unk196 = player->unk206;
        player->unk198 = player->unk208;
        amseq_determine_vols(0, player->seqp.maxChannels, player->unk196, player->unk198, player->unk194, channelVolumes);
        for (idx = 0, temp = 1; idx < player->seqp.maxChannels; idx++, temp <<= 1) {
            switch (channelVolumes[idx]) {
                case -1:
                    func_80075E38(seqp, idx);
                    n_alCSPSetAmSeqChlVol(seqp, idx, 0);
                    player->channelVolumes[idx] = 0;
                    break;
                case -2:
                    break;
                case -3:
                    channelVolumes[idx] = (s8) player->channelVolumes[idx];
                    /* fallthrough */
                default:
                    func_80075E9C(seqp, idx);
                    n_alCSPSetAmSeqChlVol(seqp, idx, channelVolumes[idx]);
                    player->channelVolumes[idx] = channelVolumes[idx] << 4;
                    break;
            }
        }
        player->unk0 |= 1;
    } else {
        // "AUDIO: MIDI sequence buffer failed.\n"
    }
}

RECOMP_PATCH void amseq_update_fade(u8 playerNo) {
    AMSEQPlayer* player;
    N_ALCSPlayer* seqp;
    s32 volume;
    s32 temp_s7;
    s32 chanMask;
    s32 chan;

    player = sSeqPlayers[playerNo];
    seqp = &player->seqp;
    if (n_alCSPGetState(seqp) == AL_PLAYING) {
        if (player->nextSeqID == 0) {
            if (player->targetVolume != player->volume) {
                if (player->volUpRate == 0) {
                    player->volume = player->targetVolume;
                    if (player->targetVolume == 0) {
                        n_alCSPStop(seqp);
                    }
                } else if (player->volume < player->targetVolume) {
                    player->volume += (player->volUpRate * gUpdateRate);
                    if (player->targetVolume < player->volume) {
                        player->volume = player->targetVolume;
                    }
                } else {
                    player->volume -= (player->volDownRate * gUpdateRate);
                    if (player->targetVolume >= player->volume) {
                        player->volume = player->targetVolume;
                        if (player->targetVolume == 0) {
                            n_alCSPStop(seqp);
                        }
                    }
                }
                // @recomp: Global volume per player type
                n_alCSPSetVol(seqp, (player->volume >> 4) * (playerNo >= 2 ? recomp_currMusVolume : recomp_currAmbVolume));
            }
        }
        temp_s7 = player->unk196 & ~player->unk198;
        if (temp_s7 != 0) {
            chan = 0;
            chanMask = 1;
            while (chan < player->seqp.maxChannels) {
                if (temp_s7 & chanMask) {
                    volume = player->channelVolumes[chan];
                    if (player->unk194 & chanMask) {
                        // fade in channel
                        func_80075E9C(seqp, chan);
                        player->unk192 |= chanMask;
                        player->seqp.chanMask = player->unk192;
                        volume += (player->volUpRate * gUpdateRate);
                        if (volume >= (127 << 4)) {
                            volume = (127 << 4);
                            player->unk196 &= ~chanMask;
                        }
                    } else {
                        // fade out channel
                        volume -= (player->volDownRate * gUpdateRate);
                        if (volume <= 0) {
                            volume = 0;
                            player->unk196 &= ~chanMask;
                            func_80075E38(seqp, chan);
                            player->unk192 |= chanMask;
                            player->seqp.chanMask = player->unk192;
                        }
                    }
                    player->channelVolumes[chan] = volume;
                    n_alCSPSetAmSeqChlVol(seqp, chan, (volume >> 4));
                }
                chan += 1;
                chanMask <<= 1;
            }
        }
    }
}
