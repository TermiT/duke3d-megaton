#include "glguard.h"
#include <stdio.h>

static
ShellRenderInterfaceOpenGL *s_render_interface = NULL;

void InitGLGuard(ShellRenderInterfaceOpenGL *render_interface) {
	s_render_interface = render_interface;
}

extern "C"
void APIENTRY g_glGenTextures(GLsizei n, GLuint *textures) {
}

extern "C"
void APIENTRY g_glDeleteTextures(GLsizei n, const GLuint *textures) {
	if (s_render_interface == NULL) {
		printf("[WARN] GL Guard is not initialized yet, but the game wants to delete textures\n");
	} else {
		for (GLsizei i = 0; i < n; i++) {
			if (s_render_interface->HasTexture(textures[i])) {
				printf("[ERR ] GL Guard: attempt to delete texture belonging to ShellRenderInterface\n");
			}
		}
	}
	glDeleteTextures(n, textures);
}
