#include "patches.h"

#include "libnaudio/n_synthInternals.h"
#include "sys/audio.h"

#include "audio.h"

extern Acmd *__am_ACMDList[NUM_ACMD_LISTS];
extern s32 gCurAcmdList;

s32 last_audio_cmdlist_size = 0;

s32 __n_nextSampleTime(ALPlayer **client);
s32 _n_timeToSamplesNoRound(s32 micros);

RECOMP_PATCH Acmd *n_alAudioFrame(Acmd *cmdList, s32 *cmdLen, s16 *outBuf, s32 outLen) {
	ALPlayer    *client;
	Acmd        *cmdlEnd = cmdList;
	Acmd        *cmdPtr;
	s32         nOut;
	s16         *lOutBuf = outBuf;

	if (n_syn->head == 0) {
		*cmdLen = 0;
		return cmdList;         /* nothing to do */
	}

	/*
	 * run down list of clients and execute callback if needed this
	 * subframe. Here we do all the work for the frame at the
	 * start. Time offsets that occur before the next frame are
	 * executed "early".
	 */

	/*
	 * paramSamples = time of next parameter change.
	 * curSamples = current sample time.
	 * so paramSamples - curSamples is the time until the next parameter change.
	 * if the next parameter change occurs within this frame time (outLen),
	 * then call back the client that contains the parameter change.
	 * Note, paramSamples must be rounded down to 16 sample boundary for use
	 * during the client handler.
	 */

	for (n_syn->paramSamples = __n_nextSampleTime(&client);
			n_syn->paramSamples - n_syn->curSamples < outLen;
			n_syn->paramSamples = __n_nextSampleTime(&client)) {
		n_syn->paramSamples &= ~0xf;
		client->samplesLeft += _n_timeToSamplesNoRound((*client->handler)(client));
	}

	/* for safety's sake, always store paramSamples aligned to 16 sample boundary.
	 * this way, if an voice handler routine gets called outside the ALVoiceHandler
	 * routine (alSynAllocVoice) it will get timestamped with an aligned value and
	 * will be processed immediately next audio frame.
	 */
	n_syn->paramSamples &= ~0xf;

	/*
	 * Now build the command list in small chunks
	 */
	while (outLen > 0) {
		nOut = MIN(n_syn->maxOutSamples, outLen);

		cmdPtr = cmdlEnd;
		n_syn->sv_dramout = (s32) lOutBuf;

		cmdlEnd = n_alSavePull(n_syn->curSamples, cmdPtr);

		outLen -= nOut;
		lOutBuf += nOut << 1;     /* For Stereo */
		n_syn->curSamples += nOut;
	}

	*cmdLen = (s32) (cmdlEnd - cmdList);

	_n_collectPVoices(); /* collect free physical voices */

    // @recomp: Print if the command list overflows its buffer
    u32 cmdlistSize = (u32)cmdlEnd - (u32)__am_ACMDList[gCurAcmdList];
    last_audio_cmdlist_size = cmdlistSize;
    if (cmdlistSize > 0x4000) {
        recomp_eprintf("Acmd list overflow! at: %p size: %x\n", cmdlEnd, cmdlistSize);
    }

	return cmdlEnd;
}
