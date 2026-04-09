#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <malloc.h>
#include <GL/gl.h>
#include "glext.h"
#include "utils.h"


// adapted from: https://learn.microsoft.com/en-us/windows/win32/Debug/retrieving-the-last-error-code
void error_exit(const char* file, int line) {
    // Retrieve the system error message for the last-error code
    LPSTR lpMsgBuf = NULL;
    DWORD dw = GetLastError(); 

    DWORD msgSize = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&lpMsgBuf,
        0, NULL
    );
    
    if (msgSize == 0) {
        MessageBox(NULL, "FormatMessage failed", "Error", MB_OK);
        ExitProcess(dw);
    }

    size_t bufSize = 256 + msgSize + 1; // +256 at the beginning for file and line number
    char* msg = (char*)malloc(bufSize);
    if (!msg) {
        LocalFree(lpMsgBuf);
        ExitProcess(dw);
    }

    wsprintf(msg, "At %s:%d:\n%s", file, line, (LPCSTR)lpMsgBuf);
    MessageBox(NULL, msg, "Error", MB_OK);

    free(msg);
    LocalFree(lpMsgBuf);
    ExitProcess(dw);
}

const char* base_name(const char* path) {
    const char* file = path;
    while (*path != '\0') {
        if (*path == '\\' || *path == '/') {
            file = path + 1;
        }
        path++;
    }
    return file;
}

BOOL write_file(HANDLE hFile, LPCVOID data, DWORD nbBytes, PDWORD nbWrittenTotal) {
    const BYTE* p = (const BYTE*)data;
    if(nbWrittenTotal) {
        *nbWrittenTotal = 0;
    }
    while (nbBytes > 0) {
        DWORD nbWritten = 0;
        if (!WriteFile(hFile, p, nbBytes, &nbWritten, NULL)) {
            return FALSE; // error
        }
        p += nbWritten;
        nbBytes -= nbWritten;
        if(nbWrittenTotal) {
            *nbWrittenTotal += nbWritten;
        }
    }
    return TRUE; // success
}

BOOL read_file(HANDLE hFile, LPVOID buffer, DWORD nbBytes, PDWORD nbReadTotal) {
    BYTE* p = (BYTE*)buffer;
    if(nbReadTotal) {
        *nbReadTotal = 0;
    }
    while (nbBytes > 0) {
        DWORD nbRead = 0;
        if (!ReadFile(hFile, p, nbBytes, &nbRead, NULL)) {
            return FALSE; // error
        }
        if (nbRead == 0) {
            break; // unexpected EOF
        }
        p += nbRead;
        nbBytes -= nbRead;
        if(nbReadTotal) {
            *nbReadTotal += nbRead;
        }
    }
    return TRUE; // success
}

char* load_file(const char* path, PDWORD loadedSize) {
    HANDLE hFile = CreateFile(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size)) {
        CloseHandle(hFile);
        return NULL;
    }

    DWORD fileSize = (DWORD)size.QuadPart;
    char* buffer = (char*)malloc(fileSize + 1); // +1 for null termination
    if (!buffer) {
        CloseHandle(hFile);
        return NULL;
    }

    DWORD totalRead;
    if(!read_file(hFile, buffer, fileSize, &totalRead)) {
        CloseHandle(hFile);
        free(buffer);
        return NULL;
    }
    buffer[totalRead] = '\0';

    CloseHandle(hFile);

    if (loadedSize != NULL) {
        *loadedSize = totalRead;
    }

    return buffer;
}

char* load_shader(const char* filename) {
    char buf[1024];
    wsprintf(buf, ".\\src\\shaders\\%s", filename);
    char* source = load_file(buf, NULL);
    if(!source) {
        wsprintf(buf, "Failed to load shader: %s", filename);
        MessageBox(NULL, buf, "Error", MB_OK);
        ExitProcess(1);
    }
    return source;
}

#define glGetProgramiv ((PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv"))
#define glGetProgramInfoLog ((PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog"))

BOOL check_shader(GLuint shader) {
    GLuint result;
    glGetProgramiv(shader, GL_LINK_STATUS, &result);
    if(!result) {
        GLint infoLength;
        glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &infoLength);
        GLchar* info = (GLchar*)malloc(infoLength * sizeof(GLchar));
        if(!info) {
            MessageBox(NULL, "Check shader malloc failed.", "Error", MB_OK);
            ExitProcess(1);
        }
        glGetProgramInfoLog(shader, infoLength, NULL, info);
        MessageBox(NULL, info, "Shader error", MB_OK);
        free(info);
        return FALSE;
    }
    return TRUE;
}
