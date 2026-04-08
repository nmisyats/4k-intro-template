#pragma once

#include <windows.h>

void start_capture(void);
void finish_capture(void);
void capture_frame(void);
void save_audio(const float* buffer, DWORD nbBytes);