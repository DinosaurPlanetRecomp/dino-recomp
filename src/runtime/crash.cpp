#include "crash.hpp"

#include <map>
#include <stack>
#include <thread>

#include "recomp.h"

struct GameFunc {
    const char* name;
    gpr address;
};

static uint8_t *rdram = nullptr;
static std::map<std::thread::id, recomp_context*> thread_context_map;
thread_local static std::stack<GameFunc> game_func_stack;

extern "C" void recomp_enter_function(const char* name, gpr address) {
    game_func_stack.push({name, address});
}

extern "C" void recomp_exit_function(void) {
    game_func_stack.pop();
}

namespace dino::runtime {

void crash_register_rdram(uint8_t *_rdram) {
    rdram = _rdram;
}

void crash_register_thread_context(std::thread::id id, recomp_context *ctx) {
    thread_context_map.emplace(id, ctx);
}

}

static void dump_game_stack(void) {
    fprintf(stderr, "**** GAME BACKTRACE START ****\n");

    while (!game_func_stack.empty()) {
        GameFunc& func = game_func_stack.top();
        if (func.name != nullptr) {
            fprintf(stderr, "0x%08lx %s\n", func.address, func.name);
        } else {
            fprintf(stderr, "0x%08lx\n", func.address);
        }
        game_func_stack.pop();
    }

    fprintf(stderr, "**** GAME BACKTRACE END ****\n");
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
#include <sys/prctl.h>

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

    char thread_name[16] = {0};
    if (prctl(PR_GET_NAME, thread_name) == 0) {
        fprintf(stderr, "Thread %s\n", thread_name);
    }

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

    dump_game_stack();
    dump_mips_context();
}

namespace dino::runtime {

void crash_setup_handler() {
    std::signal(SIGSEGV, fatal_signal_handler);
    std::signal(SIGFPE, fatal_signal_handler);
    std::signal(SIGILL, fatal_signal_handler);
}

}

#elif defined(_WIN32) /* --------------- Windows --------------- */

#include <config/config.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

static LONG WINAPI unhandled_exception_handler(EXCEPTION_POINTERS *ep) {
    EXCEPTION_RECORD *ex = ep->ExceptionRecord;

    fprintf(stderr, "Crash! Code 0x%08lX", ex->ExceptionCode);

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
        fprintf(stderr, " (%s)", name);
    }

    if (ex->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
    {
        ULONG_PTR type = ex->ExceptionInformation[0];
        ULONG_PTR addr = ex->ExceptionInformation[1];

        fprintf(stderr, " @ 0x%llx [%s]",
            addr,
            type == 0 ? "read" :
            type == 1 ? "write" :
            type == 8 ? "execute" :
            "unknown"
        );
    }

    fprintf(stderr, "\n");

    PWSTR thread = nullptr;
    GetThreadDescription(GetCurrentThread(), &thread);
    if (thread && wcslen(thread) != 0) {
        fprintf(stderr, "Thread %ls\n", thread);
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

        fprintf(stderr, "**** BACKTRACE START ****\n");

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
                fprintf(stderr, "%s", module.ImageName);
            }

            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;

            DWORD64 diff = 0;
            if (SymFromAddr(GetCurrentProcess(), frame.AddrPC.Offset, &diff, symbol)) {
                fprintf(stderr, "(%s+0x%llx) [0x%llx]", symbol->Name, diff, frame.AddrPC.Offset);
            } else {
                fprintf(stderr, "(unknown) [0x%llx]", frame.AddrPC.Offset);
            }

            fprintf(stderr, "\n");
        }

        fprintf(stderr, "**** BACKTRACE END ****\n");
    }

    dump_mips_context();

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
