#include "crash.hpp"

#include <inttypes.h>
#include <cstdio>
#include <stack>
#include <stdarg.h>

#include "config/config.hpp"
#include "recomp.h"

struct GameFunc {
    const char* name;
    gpr address;
};

static uint8_t *rdram = nullptr;
thread_local static recomp_context* thread_context = nullptr;
thread_local static std::stack<GameFunc> game_func_stack;

extern "C" void recomp_enter_function(const char* name, gpr address) {
    game_func_stack.push({name, address});
}

extern "C" void recomp_exit_function(void) {
    game_func_stack.pop();
}

namespace dino::runtime {

void crash_register_rdram(uint8_t *rdram_) {
    assert(rdram == nullptr);
    rdram = rdram_;
}

void crash_register_thread_context(recomp_context *ctx) {
    assert(thread_context == nullptr);
    thread_context = ctx;
}

}

struct CrashPrintContext {
    FILE* log_file;
};

static void crash_print_init_context(CrashPrintContext* context) {
    assert(context != nullptr);

    auto path = dino::config::get_app_folder_path() / "crash.log";
    context->log_file = fopen(path.c_str(), "w");
}

static void crash_print_deinit_context(CrashPrintContext* context) {
    assert(context != nullptr);

    if (context->log_file != nullptr) {
        fclose(context->log_file);
        context->log_file = nullptr;
    }
}

static void crash_printf(const CrashPrintContext* context, const char *format, ...) {
    va_list args, args2;
    va_start(args, format);
    va_copy(args2, args);

    vfprintf(stderr, format, args);
    if (context->log_file != nullptr) {
        vfprintf(context->log_file, format, args2);
    }

    va_end(args2);
    va_end(args);
}

static void dump_game_callstack(const CrashPrintContext* log_context) {
    if (!game_func_stack.empty()) {
        crash_printf(log_context, "**** GAME CALLSTACK START ****\n");

        while (!game_func_stack.empty()) {
            GameFunc& func = game_func_stack.top();
            if (func.name != nullptr) {
                crash_printf(log_context, "0x%08" PRIx32 " %s\n", (uint32_t)func.address, func.name);
            } else {
                crash_printf(log_context, "0x%08" PRIx32 "\n", (uint32_t)func.address);
            }
            game_func_stack.pop();
        }

        crash_printf(log_context, "**** GAME CALLSTACK END ****\n");
    }
}

static void dump_mips_context(const CrashPrintContext* log_context) {
    if (thread_context != nullptr) {
        recomp_context* ctx = thread_context;

        crash_printf(log_context, "**** MIPS START ****\n");

        gpr* gregs = &ctx->r0;
        for (int i = 0; i < 32; i++) {
            crash_printf(log_context, "r%-2d = 0x%-16" PRIx32 " ", i, (uint32_t)gregs[i]);

            if (((i + 1) % 4) == 0) {
                crash_printf(log_context, "\n");
            }
        }
        fpr* fregs = &ctx->f0;
        for (int i = 0; i < 32; i++) {
            crash_printf(log_context, "f%-2d = %-19f", i, fregs[i].fl);

            if (((i + 1) % 4) == 0) {
                crash_printf(log_context, "\n");
            }
        }
        crash_printf(log_context, "hi = 0x%" PRIx32 "\n", (uint32_t)ctx->hi);
        crash_printf(log_context, "lo = 0x%" PRIx32 "\n", (uint32_t)ctx->lo);
        crash_printf(log_context, "status_reg = 0x%" PRIx32 "\n", ctx->status_reg);
        crash_printf(log_context, "mips3_float_mode = %d\n", ctx->mips3_float_mode);

        crash_printf(log_context, "**** MIPS END ****\n");
    }
}

static void dump_object_info(const CrashPrintContext* log_context) {
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
                crash_printf(log_context, "Fault in object: (%s) (%" PRId32 ")\n", objInterfaceFuncNames[i], objID);
            }
        }
    }
}

static void common_crash_handler(const CrashPrintContext* log_context) {
    dump_object_info(log_context);
    dump_game_callstack(log_context);
    dump_mips_context(log_context);
}

#if defined(__linux__) /* --------------- Linux --------------- */

#include <signal.h>
#include <cstdio>
#include <execinfo.h>
#include <link.h>
#include <sys/prctl.h>

static constexpr size_t MAX_STACKTRACE_DEPTH = 64;

static void fatal_signal_handler(int signo, siginfo_t* info, void* extra) {
    // Restore default handler so we don't handle additional errors after this point
    signal(signo, SIG_DFL);

    CrashPrintContext log_context;
    crash_print_init_context(&log_context);

    crash_printf(&log_context, "Crash! ");
    const char* signame = "";
    const char* reason = nullptr;
    switch (signo) {
        case SIGSEGV:
            signame = "SIGSEGV";
            switch (info->si_code) {
                case SEGV_MAPERR:
                    reason = "SEGV_MAPERR";
                    break;
                case SEGV_ACCERR:
                    reason = "SEGV_ACCERR";
                    break;
                case SEGV_BNDERR:
                    reason = "SEGV_BNDERR";
                    break;
            }
            break;
        case SIGFPE:
            signame = "SIGFPE";
            switch (info->si_code) {
                case FPE_INTDIV:
                    reason = "FPE_INTDIV";
                    break;
                case FPE_FLTDIV:
                    reason = "FPE_FLTDIV";
                    break;
            }
            break;
        case SIGILL:
            signame = "SIGILL";
            break;
        case SIGBUS:
            signame = "SIGBUS";
            switch (info->si_code) {
                case BUS_ADRALN:
                    reason = "BUS_ADRALN";
                    break;
                case BUS_ADRERR:
                    reason = "BUS_ADRERR";
                    break;
            }
            break;
    }
    if (reason != nullptr) {
        crash_printf(&log_context, "Signal %d (%s) @ %p [%s]\n", signo, signame, info->si_addr, reason);
    } else {
        crash_printf(&log_context, "Signal %d (%s) @ %p [%d]\n", signo, signame, info->si_addr, info->si_code);
    }

    char thread_name[16] = {0};
    if (prctl(PR_GET_NAME, thread_name) == 0) {
        crash_printf(&log_context, "Thread: %s\n", thread_name);
    }

    void* trace[MAX_STACKTRACE_DEPTH];
    size_t trace_depth = backtrace(trace, MAX_STACKTRACE_DEPTH);
    crash_printf(&log_context, "**** BACKTRACE START ****\n");
    //backtrace_symbols_fd(trace, trace_depth, STDERR_FILENO);
    // Adapted from backtrace_symbols_fd
    for (int i = 0; i < trace_depth; i++) {
        Dl_info info = {};
        link_map* linkMap;
        if (dladdr1(trace[i], &info, (void**)&linkMap, RTLD_DL_LINKMAP) && info.dli_fname && info.dli_fname[0] != '\0') {
            crash_printf(&log_context, "%s", info.dli_fname);

            if (info.dli_sname != NULL || linkMap->l_addr != 0) {
                crash_printf(&log_context, "(");

                if (info.dli_sname != NULL) {
                    crash_printf(&log_context, "%s", info.dli_sname);
                } else {
                    info.dli_saddr = (void*)linkMap->l_addr;
                }

                size_t diff;
                if (trace[i] >= (void*)info.dli_saddr) {
                    crash_printf(&log_context, "+0x");
                    diff = (uintptr_t)trace[i] - (uintptr_t)info.dli_saddr;
                } else {
                    crash_printf(&log_context, "-0x");
                    diff = (uintptr_t)info.dli_saddr - (uintptr_t)trace[i];
                }

                crash_printf(&log_context, "%lx)", diff);
            }

            crash_printf(&log_context, " [0x%lx]\n", (uintptr_t)trace[i]);
        } else {
            crash_printf(&log_context, "(unknown) [0x%lx]\n", (uintptr_t)trace[i]);
        }
    }

    crash_printf(&log_context, "**** BACKTRACE END ****\n");

    common_crash_handler(&log_context);

    crash_print_deinit_context(&log_context);
}

namespace dino::runtime {

void crash_setup_handler() {
    struct sigaction action = {0};
    action.sa_flags = SA_SIGINFO; // include siginfo arg
    action.sa_sigaction = fatal_signal_handler;
    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGILL, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
}

}

#elif defined(_WIN32) /* --------------- Windows --------------- */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

static LONG WINAPI unhandled_exception_handler(EXCEPTION_POINTERS *ep) {
    EXCEPTION_RECORD *ex = ep->ExceptionRecord;

    CrashPrintContext log_context;
    crash_print_init_context(&log_context);

    crash_printf(&log_context, "Crash! Code 0x%08lX", ex->ExceptionCode);

    const char *name = nullptr;
    switch (ex->ExceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:
            name = "EXCEPTION_ACCESS_VIOLATION";
            break;

        case EXCEPTION_ILLEGAL_INSTRUCTION:
            name = "EXCEPTION_ILLEGAL_INSTRUCTION";
            break;

        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            name = "EXCEPTION_INT_DIVIDE_BY_ZERO";
            break;

        case EXCEPTION_STACK_OVERFLOW:
            name = "EXCEPTION_STACK_OVERFLOW";
            break;

        default: break;
    }

    if (name) {
        crash_printf(&log_context, " (%s)", name);
    }

    if (ex->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
    {
        ULONG_PTR type = ex->ExceptionInformation[0];
        ULONG_PTR addr = ex->ExceptionInformation[1];

        crash_printf(&log_context, " @ 0x%llx [%s]",
            addr,
            type == 0 ? "read" :
            type == 1 ? "write" :
            type == 8 ? "execute" :
            "unknown"
        );
    }

    crash_printf(&log_context, "\n");

    PWSTR thread = nullptr;
    GetThreadDescription(GetCurrentThread(), &thread);
    if (thread && wcslen(thread) != 0) {
        crash_printf(&log_context, "Thread: %ls\n", thread);
    }

    if (SymInitialize(GetCurrentProcess(), nullptr, TRUE)) {
        SymSetOptions(SYMOPT_LOAD_LINES);

        CONTEXT context {};
        context.ContextFlags = CONTEXT_FULL;
        RtlCaptureContext(&context);

        STACKFRAME frame {};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;

        crash_printf(&log_context, "**** BACKTRACE START ****\n");

        while (StackWalk(
            IMAGE_FILE_MACHINE_AMD64,
            GetCurrentProcess(),
            GetCurrentThread(),
            &frame,
            &context,
            nullptr,
            SymFunctionTableAccess,
            SymGetModuleBase,
            nullptr)
        ) {
            char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
            auto *symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);

            IMAGEHLP_MODULE module {};
            module.SizeOfStruct = sizeof(IMAGEHLP_MODULE);
            if (SymGetModuleInfo(GetCurrentProcess(), frame.AddrPC.Offset, &module)) {
                crash_printf(&log_context, "%s", module.ImageName);
            }

            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;

            DWORD64 diff = 0;
            if (SymFromAddr(GetCurrentProcess(), frame.AddrPC.Offset, &diff, symbol)) {
                crash_printf(&log_context, "(%s+0x%llx) [0x%llx]", symbol->Name, diff, frame.AddrPC.Offset);
            } else {
                crash_printf(&log_context, "(unknown) [0x%llx]", frame.AddrPC.Offset);
            }

            crash_printf(&log_context, "\n");
        }

        crash_printf(&log_context, "**** BACKTRACE END ****\n");
    }

    common_crash_handler(&log_context);

    crash_print_deinit_context(&log_context);

    auto path = dino::config::get_app_folder_path() / "crash.dmp";
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dump {};
        dump.ThreadId = GetCurrentThreadId();
        dump.ExceptionPointers = ep;
        dump.ClientPointers = FALSE;

        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            file,
            MiniDumpNormal,
            &dump,
            nullptr,
            nullptr
        );

        CloseHandle(file);
    }

    fprintf(stderr, "Crash dump written to: %ls\n", path.c_str());

    return EXCEPTION_EXECUTE_HANDLER;
}

namespace dino::runtime {

void crash_setup_handler() {
    SetUnhandledExceptionFilter(unhandled_exception_handler);
}

}

#else /* --------------- Unsupported --------------- */

namespace dino::runtime {

void crash_setup_handler() {
    fprintf(stderr, "Failed to setup crash handler! (unsupported platform)\n");
}

}

#endif
