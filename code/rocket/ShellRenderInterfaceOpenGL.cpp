/*
 * This source file is part of libRocket, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://www.librocket.com
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "ShellRenderInterfaceOpenGL.h"
#include <Rocket/Core.h>
#include "glguard.h"
#include "dnAPI.h"

struct GrabbedTexture {
	GLsizei width, height;
	GLvoid *pixels;
};

#define GL_CLAMP_TO_EDGE 0x812F

ShellRenderInterfaceOpenGL::ShellRenderInterfaceOpenGL():m_scale(1.0f),m_offset_x(0.0f),m_offset_y(0.0f),m_screen_height(1080) {
	InitGLGuard(this);
}

ShellRenderInterfaceOpenGL::~ShellRenderInterfaceOpenGL() {
	InitGLGuard(NULL);
}


// Called by Rocket when it wants to render geometry that it does not wish to optimise.
void ShellRenderInterfaceOpenGL::RenderGeometry(Rocket::Core::Vertex* vertices, int ROCKET_UNUSED(num_vertices), int* indices, int num_indices, const Rocket::Core::TextureHandle texture, const Rocket::Core::Vector2f& translation)
{
	glPushMatrix();
	glTranslatef(translation.x, translation.y, 0);


	if (!texture)
	{
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_TRIANGLES);
		for (int i = 0; i < num_indices; i++) {
			Rocket::Core::Vertex *v = &vertices[indices[i]];
			glColor4ub(v->colour.red, v->colour.green, v->colour.blue, v->colour.alpha);
			glVertex2f(v->position.x, v->position.y);
		}
		glEnd();
		glEnable(GL_TEXTURE_2D);
	}
	else
	{
		glVertexPointer(2, GL_FLOAT, sizeof(Rocket::Core::Vertex), &vertices[0].position);
		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Rocket::Core::Vertex), &vertices[0].colour);

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, (GLuint) texture);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, sizeof(Rocket::Core::Vertex), &vertices[0].tex_coord);

		glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, indices);
	}


	glPopMatrix();
}

// Called by Rocket when it wants to compile geometry it believes will be static for the forseeable future.		
Rocket::Core::CompiledGeometryHandle ShellRenderInterfaceOpenGL::CompileGeometry(Rocket::Core::Vertex* ROCKET_UNUSED(vertices), int ROCKET_UNUSED(num_vertices), int* ROCKET_UNUSED(indices), int ROCKET_UNUSED(num_indices), const Rocket::Core::TextureHandle ROCKET_UNUSED(texture))
{
	return (Rocket::Core::CompiledGeometryHandle) NULL;
}

// Called by Rocket when it wants to render application-compiled geometry.		
void ShellRenderInterfaceOpenGL::RenderCompiledGeometry(Rocket::Core::CompiledGeometryHandle ROCKET_UNUSED(geometry), const Rocket::Core::Vector2f& ROCKET_UNUSED(translation))
{
}

// Called by Rocket when it wants to release application-compiled geometry.		
void ShellRenderInterfaceOpenGL::ReleaseCompiledGeometry(Rocket::Core::CompiledGeometryHandle ROCKET_UNUSED(geometry))
{
}

// Called by Rocket when it wants to enable or disable scissoring to clip content.		
void ShellRenderInterfaceOpenGL::EnableScissorRegion(bool enable)
{
	if (enable)
		glEnable(GL_SCISSOR_TEST);
	else
		glDisable(GL_SCISSOR_TEST);
}

// Called by Rocket when it wants to change the scissor region.		
void ShellRenderInterfaceOpenGL::SetScissorRegion(int x, int y, int width, int height)
{
	float f_x = (x + m_offset_x) * m_scale;
	float f_y = (y - m_offset_y) * m_scale;
	float f_w = width * m_scale;
	float f_h = height * m_scale;
	glScissor((GLint)f_x, m_screen_height-(GLint)(f_y+f_h), (GLint)f_w, (GLint)f_h);
}

// Set to byte packing, or the compiler will expand our struct, which means it won't read correctly from file
#pragma pack(1) 
struct TGAHeader 
{
	char  idLength;
	char  colourMapType;
	char  dataType;
	short int colourMapOrigin;
	short int colourMapLength;
	char  colourMapDepth;
	short int xOrigin;
	short int yOrigin;
	short int width;
	short int height;
	char  bitsPerPixel;
	char  imageDescriptor;
};
// Restore packing
#pragma pack()

bool ShellRenderInterfaceOpenGL::LoadSpecialTexture(Rocket::Core::TextureHandle& texture_handle, Rocket::Core::Vector2i& texture_dimensions, const Rocket::Core::String& source) {
    int tile_no;
    if (sscanf(source.CString(), "tile:%d", &tile_no) == 1) {
        int w, h;
        if (dnGetTile(tile_no, &w, &h, NULL)) {
            std::vector<unsigned char> data;
            texture_dimensions.x = w;
            texture_dimensions.y = h;
            data.resize(texture_dimensions.x*texture_dimensions.y*4);
            dnGetTile(tile_no, &w, &h, &data[0]);
            return GenerateTexture(texture_handle, &data[0], texture_dimensions);
        }
    }
    return false;
}


// Called by Rocket when a texture is required by the library.		
bool ShellRenderInterfaceOpenGL::LoadTexture(Rocket::Core::TextureHandle& texture_handle, Rocket::Core::Vector2i& texture_dimensions, const Rocket::Core::String& source)
{
	Rocket::Core::FileInterface* file_interface = Rocket::Core::GetFileInterface();
	Rocket::Core::FileHandle file_handle = file_interface->Open(source);
	if (!file_handle)
	{
		return LoadSpecialTexture(texture_handle, texture_dimensions, source);
	}
	
	file_interface->Seek(file_handle, 0, SEEK_END);
	size_t buffer_size = file_interface->Tell(file_handle);
	file_interface->Seek(file_handle, 0, SEEK_SET);
	
	char* buffer = new char[buffer_size];
	file_interface->Read(buffer, buffer_size, file_handle);
	file_interface->Close(file_handle);

	TGAHeader header;
	memcpy(&header, buffer, sizeof(TGAHeader));
	
	int color_mode = header.bitsPerPixel / 8;
	int image_size = header.width * header.height * 4; // We always make 32bit textures 
	
	if (header.dataType != 2)
	{
		Rocket::Core::Log::Message(Rocket::Core::Log::LT_ERROR, "Only 24/32bit uncompressed TGAs are supported.");
		return false;
	}
	
	// Ensure we have at least 3 colors
	if (color_mode < 3)
	{
		Rocket::Core::Log::Message(Rocket::Core::Log::LT_ERROR, "Only 24 and 32bit textures are supported");
		return false;
	}
	
	const char* image_src = buffer + sizeof(TGAHeader);
	unsigned char* image_dest = new unsigned char[image_size];
	
	// Targa is BGR, swap to RGB and flip Y axis
	for (long y = 0; y < header.height; y++)
	{
		long read_index = y * header.width * color_mode;
		long write_index = ((header.imageDescriptor & 32) != 0) ? read_index : (header.height - y - 1) * header.width * color_mode;
		for (long x = 0; x < header.width; x++)
		{
			image_dest[write_index] = image_src[read_index+2];
			image_dest[write_index+1] = image_src[read_index+1];
			image_dest[write_index+2] = image_src[read_index];
			if (color_mode == 4)
				image_dest[write_index+3] = image_src[read_index+3];
			else
				image_dest[write_index+3] = 255;
			
			write_index += 4;
			read_index += color_mode;
		}
	}

	texture_dimensions.x = header.width;
	texture_dimensions.y = header.height;
	
	bool success = GenerateTexture(texture_handle, image_dest, texture_dimensions);
	
	delete [] image_dest;
	delete [] buffer;
	
	return success;
}

// Called by Rocket when a texture is required to be built from an internally-generated sequence of pixels.
bool ShellRenderInterfaceOpenGL::GenerateTexture(Rocket::Core::TextureHandle& texture_handle, const Rocket::Core::byte* source, const Rocket::Core::Vector2i& source_dimensions)
{
	GLuint texture_id = 0;
	glGenTextures(1, &texture_id);
	if (texture_id == 0)
	{
		printf("Failed to generate textures\n");
		return false;
	}

	UploadTexture(texture_id, source_dimensions.x, source_dimensions.y, source);
	m_textures[texture_id] = NULL;
	texture_handle = (Rocket::Core::TextureHandle) texture_id;


	return true;
}

// Called by Rocket when a loaded texture is no longer required.		
void ShellRenderInterfaceOpenGL::ReleaseTexture(Rocket::Core::TextureHandle texture_handle)
{
	glDeleteTextures(1, (GLuint*) &texture_handle);
	m_textures.erase(*((GLuint*) &texture_handle));
}

void ShellRenderInterfaceOpenGL::UploadTexture(GLuint texture_id, GLsizei width, GLsizei height, const GLvoid *pixels) {
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void ShellRenderInterfaceOpenGL::DownloadTexture(GLuint texture_id, GLsizei *width, GLsizei *height, GLvoid **pixels) {
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, height);

	*pixels = malloc((*width)*(*height)*4);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, *pixels);
}

#define DO_GRAB_TEXTURES 1

void ShellRenderInterfaceOpenGL::GrabTextures() {
#if DO_GRAB_TEXTURES
	for (std::map<GLuint, GrabbedTexture*>::iterator i = m_textures.begin(); i != m_textures.end(); i++) {
		GrabbedTexture *gt = new GrabbedTexture();
		DownloadTexture(i->first, &gt->width, &gt->height, &gt->pixels);
		i->second = gt;
	}
#endif
}

void ShellRenderInterfaceOpenGL::RestoreTextures() {
#if DO_GRAB_TEXTURES
	for (std::map<GLuint, GrabbedTexture*>::iterator i = m_textures.begin(); i != m_textures.end(); i++) {
		UploadTexture(i->first, i->second->width, i->second->height, i->second->pixels);
		free(i->second->pixels);
		delete i->second;
		i->second = NULL;
	}
#endif
}

bool ShellRenderInterfaceOpenGL::HasTexture(GLuint texture_id) {
	return m_textures.find(texture_id) != m_textures.end();
}

void ShellRenderInterfaceOpenGL::SetTransform(GLfloat scale, GLfloat xoffset, GLfloat yoffset, GLint height) {
	m_scale = scale;
	m_offset_x = xoffset;
	m_offset_y = yoffset;
	m_screen_height = height;
}
