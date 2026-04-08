#include <windows.h>
#include <GL/gl.h>
#include "glext.h" // contains type definitions for all modern OpenGL functions
#include "config.h"
#include "utils.h"
#include "music.h"

// Define the modern OpenGL functions to load from the driver

#define glCreateShaderProgramv ((PFNGLCREATESHADERPROGRAMVPROC)wglGetProcAddress("glCreateShaderProgramv"))
#define glUseProgram ((PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram"))
#define glUniform4fv ((PFNGLUNIFORM4FVPROC)wglGetProcAddress("glUniform4fv"))
#define glDispatchCompute ((PFNGLDISPATCHCOMPUTEPROC)wglGetProcAddress("glDispatchCompute"))
#define glCreateBuffers ((PFNGLCREATEBUFFERSPROC)wglGetProcAddress("glCreateBuffers"))
#define glNamedBufferStorage ((PFNGLNAMEDBUFFERSTORAGEPROC)wglGetProcAddress("glNamedBufferStorage"))
#define glBindBufferBase ((PFNGLBINDBUFFERBASEPROC)wglGetProcAddress("glBindBufferBase"))
#define glMemoryBarrier ((PFNGLMEMORYBARRIERPROC)wglGetProcAddress("glMemoryBarrier"))
#define glGetNamedBufferSubData ((PFNGLGETNAMEDBUFFERSUBDATAPROC)wglGetProcAddress("glGetNamedBufferSubData"))

#define CEIL_DIV(x, y) ((x) + (y) - 1) / (y)

#define MAX_AMPLITUDE (32767/16) // must be less or equal than 32767

#ifdef MINIFIED_SHADERS
extern const char* music_comp;
#endif

static GLfloat params[4*1] = {(float)SAMPLE_RATE, (float)MAX_AMPLITUDE, 0.f, 0.f};

void music_init(short* buffer) {
    #ifndef MINIFIED_SHADERS
    const char* music_comp = load_shader("music.comp");
    #endif

    GLuint musicShader = glCreateShaderProgramv(GL_COMPUTE_SHADER, 1, &music_comp);

    #ifndef MINIFIED_SHADERS
    free(music_comp);
    #endif

    #ifdef DEBUG
    if(!check_shader(musicShader)) {
        ExitProcess(1);
    }
    #endif

    GLuint gpuMusicBuffer;
    glCreateBuffers(1, &gpuMusicBuffer);
    glNamedBufferStorage(gpuMusicBuffer, MUSIC_DATA_BYTES, NULL, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gpuMusicBuffer);

    glUseProgram(musicShader);
    glUniform4fv(0, 1, params);
    // Dispatch one thread per sample, OpenGL guarantees a least 65535 workgroups
    glDispatchCompute(CEIL_DIV(NUM_SAMPLES, 1024), 1, 1);
    // Wait for shaders writes to be visible by getBufferSubData
    // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glMemoryBarrier.xhtml
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    glGetNamedBufferSubData(gpuMusicBuffer, 0, MUSIC_DATA_BYTES, (void*)buffer);
}