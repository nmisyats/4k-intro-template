#pragma once

#include <windows.h>

void init_capture();
void start_video_capture(HWND hwnd);
void end_video_capture(HWND hwnd);
void save_frame(int frame);
void save_audio(short* buffer, DWORD bytes, HWND hwnd);