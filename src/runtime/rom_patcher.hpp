#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dino::runtime {

std::vector<uint8_t> patch_rom(std::span<const uint8_t> unpatched_rom);

}