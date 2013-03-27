// Sergei Shubin

#ifndef GLGUARD_H
#define GLGUARD_H

#include "glbuild.h"

#ifdef __cplusplus

#include "ShellRenderInterfaceOpenGL.h"

void InitGLGuard(ShellRenderInterfaceOpenGL *render_interface);

extern "C" {

#endif

void APIENTRY g_glGenTextures(GLsizei n, GLuint *textures);
void APIENTRY g_glDeleteTextures(GLsizei n, const GLuint *textures);

#ifdef __cplusplus
}
#endif

#endif /* GLDUARD_H */
