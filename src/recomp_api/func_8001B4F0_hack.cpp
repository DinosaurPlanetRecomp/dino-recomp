#include <cassert>
#include <cstdint>

#include "recomp.h"

static const uint32_t SR_FR = 0x04000000;

// With the way func_8001B4F0 is recompiled, we need to track whether the function
// already ran it's return routine and prevent it from being ran more than once.
thread_local bool func_8001B4F0_returned = false;

extern "C" void recomp_on_func_8001B4F0_entry(uint8_t* rdram, recomp_context* ctx) {
    func_8001B4F0_returned = false;
    cop0_status_write(ctx, cop0_status_read(ctx) | SR_FR);
}

extern "C" int recomp_did_func_8001B4F0_return() {
    return func_8001B4F0_returned ? 1 : 0;
}

extern "C" void recomp_on_func_8001B4F0_ret(uint8_t* rdram, recomp_context* ctx) {
    assert(func_8001B4F0_returned == false);
    func_8001B4F0_returned = true;
    cop0_status_write(ctx, cop0_status_read(ctx) & ~SR_FR);
}
