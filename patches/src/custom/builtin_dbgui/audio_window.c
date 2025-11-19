#include "dbgui.h"
#include "patches/n_synthesizer.h"

void dbgui_audio_window(s32 *open) {
    if (dbgui_begin("Audio Debug", open)) {
        dbgui_textf("Audio command list size: %x", recomp_lastAudioCmdlistSize);
    }
    dbgui_end();
}
