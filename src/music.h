#pragma once

#include "config.h"

#define MUSIC_DURATION INTRO_DURATION // seconds
#define SAMPLE_RATE 44100 // 44.1 kHz (typical for audio)
#define NUM_CHANNELS 2    // stereo
#define BIT_DEPTH 32      // 32 bits per sample (float)

#define NUM_SAMPLES (MUSIC_DURATION * SAMPLE_RATE)
#define BYTE_RATE (SAMPLE_RATE * NUM_CHANNELS * BIT_DEPTH / 8)
#define SAMPLE_ALIGNMENT (NUM_CHANNELS * BIT_DEPTH / 8)
#define MUSIC_DATA_BYTES (MUSIC_DURATION * SAMPLE_RATE * NUM_CHANNELS * BIT_DEPTH / 8)
#define MUSIC_BUFFER_SIZE (NUM_SAMPLES * NUM_CHANNELS)


void music_init(float* buffer);