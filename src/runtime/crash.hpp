#pragma once

#include <cstdint>
#include <thread>

#include "recomp.h"

namespace dino::runtime {

void crash_register_rdram(uint8_t *rdram);
void crash_register_thread_context(std::thread::id id, recomp_context *ctx);

void crash_setup_handler();

}