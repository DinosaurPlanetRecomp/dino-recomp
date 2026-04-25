#pragma once

#include <cstdint>
#include <cstddef>

namespace dino::runtime {

void queue_samples(int16_t* audio_data, size_t sample_count);
size_t get_frames_remaining();
void set_frequency(uint32_t freq);
bool reset_audio(uint32_t output_freq);

}