#include <windows.h>
#include <GL/gl.h>
#include "glext.h" // contains type definitions for all modern OpenGL functions
#include "config.h"
#include "utils.h"

#ifdef MINIFIED_SHADERS
#include "shaders.inl"
#endif


// Define the modern OpenGL functions to load from the driver

#define glCreateShaderProgramv ((PFNGLCREATESHADERPROGRAMVPROC)wglGetProcAddress("glCreateShaderProgramv"))
#define glUseProgram ((PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram"))
#define glProgramUniform4fv ((PFNGLPROGRAMUNIFORM4FVPROC)wglGetProcAddress("glProgramUniform4fv"))

static GLuint fragShader;

void intro_init(void) {
    #ifndef MINIFIED_SHADERS
    const char* shader_frag = load_shader("shader.frag");
    #endif

    // Create a fragment shader program, the default vertex shader will
    // be used (?)
    fragShader = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &shader_frag);

    #ifndef MINIFIED_SHADERS
    free(shader_frag);
    #endif

    #ifdef DEBUG
    if(!check_shader(fragShader)) {
        ExitProcess(1);
    }
    #endif

    glUseProgram(fragShader);
}


// Paramaters to pass to the fragment shader at each frame as an array of vec4s
static GLfloat params[4*1] = {(float)XRES, (float)YRES, 0.f, 0.f};

void intro_do(GLfloat time) {
    params[2] = time;
    glProgramUniform4fv(fragShader, 0, 1, params);
    glRects(-1, -1, 1, 1);
}