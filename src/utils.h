#pragma once

#include <windows.h>
#include <GL/gl.h>

const char* base_name(const char* path);
void error_exit(const char* file, int line);

#if defined(DEBUG) || defined(CAPTURE)
    #define ERROR_EXIT() error_exit(base_name(__FILE__), __LINE__)
#else
    #define ERROR_EXIT() ExitProcess(1)
#endif

BOOL write_file(HANDLE hFile, LPCVOID data, DWORD nbBytes, PDWORD nbWrittenTotal);
BOOL read_file(HANDLE hFile, LPVOID buffer, DWORD nbBytes, PDWORD nbReadTotal);
char* load_file(const char* path, PDWORD loadedSize);

char* load_shader(const char* filename);
BOOL check_shader(GLuint shader);
