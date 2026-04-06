#include <windows.h>
#include <GL/gl.h>
#include "glext.h" // contains type definitions for all modern OpenGL functions
#include "config.h"
#include "utils.h"
#include "music.h"


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

#define SAMPLES_PER_INVOC 256
#define MAX_AMPLITUDE (32767/16) // must be less or equal than 32767

#ifdef MINIFIED_SHADERS
extern const char* music_comp;
#endif

static GLint cpuMusicBuffer[MUSIC_BUFFER_SIZE];
static GLfloat params[4*1] = {(float)SAMPLE_RATE, (float)MAX_AMPLITUDE, 0, 0};

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
    glNamedBufferStorage(gpuMusicBuffer, sizeof(cpuMusicBuffer), NULL, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gpuMusicBuffer);

    glUseProgram(musicShader);
    glUniform4fv(0, 1, params);
    glDispatchCompute(CEIL_DIV(NUM_SAMPLES, 64*SAMPLES_PER_INVOC), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glGetNamedBufferSubData(gpuMusicBuffer, 0, sizeof(cpuMusicBuffer), cpuMusicBuffer);
    for(unsigned int i = 0; i < MUSIC_BUFFER_SIZE; i++) {
        buffer[i] = (short)cpuMusicBuffer[i];
    }
}