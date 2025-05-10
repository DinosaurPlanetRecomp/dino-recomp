#include "patches.h"
#include "sys/scheduler.h"

extern UnkSchedStruct D_800918D0;
extern s32 gRetraceCounter32;
extern s32 gCurRSPTaskCounter;
extern s32 gCurRDPTaskCounter;
extern u64 gRetraceCounter64;

extern void __scHandleRSP(OSSched *sc);

RECOMP_PATCH void __scHandleRetrace(OSSched *sc) {
    OSScTask *rspTask = NULL;
    OSScClient *client;
    s32 state;
    OSScTask *sp = NULL;
    OSScTask *dp = NULL;
    OSScTask *unkTask;

    if (sc->curRSPTask) {
        gCurRSPTaskCounter++;
    }

    if (sc->curRDPTask) {
        gCurRDPTaskCounter++;
    }

    // @recomp: Patch out RSP/RDP stall check. This will get tripped during RT64 ubershader compilation 
    // and during any random game stutters. It's not necessary to keep this check for recomp, but we'll
    // add some prints to at least leave in some insight.
    if ((gCurRSPTaskCounter > 10) && (sc->curRSPTask)) {
        recomp_eprintf("RSP stall! gCurRSPTaskCounter=%d\n", gCurRSPTaskCounter);
        // @recomp:
        // HACK: Sometimes opening the recomp controls screen lags hard enough to "stall" the RSP. Running
        // __scHandleRSP here seems to avoid the hardlock in *most cases*. This is not a perfect fix! 
        recomp_eprintf("Running __scHandleRSP to avoid hardlock\n");
        __scHandleRSP(sc);
    }

    if ((gCurRDPTaskCounter > 10) && (sc->curRDPTask)) {
        recomp_eprintf("RDP stall! gCurRDPTaskCounter=%d\n", gCurRDPTaskCounter);
    }

    // Read the task command queue and schedule tasks
    while (osRecvMesg(&sc->cmdQ, (OSMesg *)&rspTask, OS_MESG_NOBLOCK) != -1)
        __scAppendList(sc, rspTask);

    state = ((sc->curRSPTask == NULL) << 1) | (sc->curRDPTask == NULL);
    if (__scSchedule(sc, &sp, &dp, state) != state)
        __scExec(sc, sp, dp);

    gRetraceCounter64++;
    gRetraceCounter32++;
    sc->frameCount++;

    if ((sc->unkTask) && (sc->frameCount >= 2)) {
        unkTask = sc->unkTask;
        if (unkTask->msgQ) {
            if ((unkTask->unk0x68) || (unkTask->msg)) {
                osSendMesg(unkTask->msgQ, unkTask->msg, OS_MESG_BLOCK);
            } else {
                osSendMesg(unkTask->msgQ, (OSMesg)&D_800918D0, OS_MESG_BLOCK);
            }
        }
        sc->frameCount = 0;
        sc->unkTask = NULL;
    }

    for (client = sc->clientList; client != NULL; client = client->next) {
        if (client->unk0x0 == 1) {
            // Only run this on even calls to this function
            if (gRetraceCounter64 % 2 == 0) {
                osSendMesg(client->msgQ, sc, OS_MESG_NOBLOCK);
                if (sc->audioListHead) {
                    func_8003B9C0(sc);
                }
            }
        } else if (client->unk0x0 == 2) {
            osSendMesg(client->msgQ, sc, OS_MESG_NOBLOCK);
        }
    }
}
