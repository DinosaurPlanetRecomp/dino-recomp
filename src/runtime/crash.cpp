#include "crash.hpp"

#include <map>
#include <thread>

#include "recomp.h"

static uint8_t *rdram = nullptr;
static std::map<std::thread::id, recomp_context*> thread_context_map;

namespace dino::runtime {
    void crash_register_rdram(uint8_t *_rdram) {
        rdram = _rdram;
    }

    void crash_register_thread_context(std::thread::id id, recomp_context *ctx) {
        thread_context_map.emplace(id, ctx);
    }
}

static void dump_mips_context(void) {
    auto ctx_find_it = thread_context_map.find(std::this_thread::get_id());
    if (ctx_find_it != thread_context_map.end()) {
        recomp_context* ctx = ctx_find_it->second;

        fprintf(stderr, "**** MIPS START ****\n");

        gpr* gregs = &ctx->r0;
        for (int i = 0; i < 32; i++) {
            fprintf(stderr, "r%-2d = 0x%-16lx ", i, gregs[i]);

            if (((i + 1) % 4) == 0) {
                fprintf(stderr, "\n");
            }
        }
        fpr* fregs = &ctx->f0;
        for (int i = 0; i < 32; i++) {
            fprintf(stderr, "f%-2d = %-19f", i, fregs[i].fl);

            if (((i + 1) % 4) == 0) {
                fprintf(stderr, "\n");
            }
        }
        fprintf(stderr, "hi = 0x%lx\n", ctx->hi);
        fprintf(stderr, "lo = 0x%lx\n", ctx->lo);
        fprintf(stderr, "status_reg = 0x%x\n", ctx->status_reg);
        fprintf(stderr, "mips3_float_mode = %d\n", ctx->mips3_float_mode);

        fprintf(stderr, "**** MIPS END ****\n");
    }

    if (rdram != NULL) {
        static gpr gPiManagerArray_addr = 0x800bfd40 + 0xFFFFFFFF00000000;
        static const char* objInterfaceFuncNames[] = {
            "setup",
            "control",
            "print",
            "update",
            "free"
        };

        for (int i = 0; i < 5; i++) {
            int32_t objID = MEM_W(gPiManagerArray_addr, i * 4);
            if (objID != -1) {
                fprintf(stderr, "Fault in object: (%s) (%d)\n", objInterfaceFuncNames[i], objID);
            }
        }
    }

    
}

#if defined(__linux__) /* --------------- Linux --------------- */

#include <csignal>
#include <cstdio>
#include <execinfo.h>
#include <link.h>

static constexpr size_t MAX_STACKTRACE_DEPTH = 64;

static void fatal_signal_handler(int signal) {
    std::signal(signal, SIG_DFL);

    fprintf(stderr, "Crash! ");
    const char* signame = "";
    switch (signal) {
        case SIGSEGV:
            signame = "SIGSEGV";
            break;
        case SIGFPE:
            signame = "SIGFPE";
            break;
        case SIGILL:
            signame = "SIGILL";
            break;
    }
    fprintf(stderr, "Signal %d (%s)\n", signal, signame);

    void* trace[MAX_STACKTRACE_DEPTH];
    size_t trace_depth = backtrace(trace, MAX_STACKTRACE_DEPTH);
    fprintf(stderr, "**** BACKTRACE START ****\n");
    //backtrace_symbols_fd(trace, trace_depth, STDERR_FILENO);
    // Adapted from backtrace_symbols_fd
    for (int i = 0; i < trace_depth; i++) {
        Dl_info info = {};
        link_map* linkMap;
        if (dladdr1(trace[i], &info, (void**)&linkMap, RTLD_DL_LINKMAP) && info.dli_fname && info.dli_fname[0] != '\0') {
            fprintf(stderr, "%s", info.dli_fname);

            if (info.dli_sname != NULL || linkMap->l_addr != 0) {
                fprintf(stderr, "(");

                if (info.dli_sname != NULL) {
                    fprintf(stderr, "%s", info.dli_sname);
                } else {
                    info.dli_saddr = (void*)linkMap->l_addr;
                }

                size_t diff;
                if (trace[i] >= (void*)info.dli_saddr) {
                    fprintf(stderr, "+0x");
                    diff = (uintptr_t)trace[i] - (uintptr_t)info.dli_saddr;
                } else {
                    fprintf(stderr, "-0x");
                    diff = (uintptr_t)info.dli_saddr - (uintptr_t)trace[i];
                }

                fprintf(stderr, "%lx)", diff);
            }

            fprintf(stderr, " [0x%lx]\n", (uintptr_t)trace[i]);
        } else {
            fprintf(stderr, "(unknown) [0x%lx]\n", (uintptr_t)trace[i]);
        }
    }

    fprintf(stderr, "**** BACKTRACE END ****\n");

    dump_mips_context();
}

namespace dino::runtime {

void crash_setup_handler() {
    std::signal(SIGSEGV, fatal_signal_handler);
    std::signal(SIGFPE, fatal_signal_handler);
    std::signal(SIGILL, fatal_signal_handler);
}

}

#else /* --------------- Unsupported --------------- */

void setup_crash_handler() {
    fprintf(stderr, "Failed to setup crash handler! (unsupported platform)\n");
    
}

#endif
