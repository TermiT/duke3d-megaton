//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include "FrameAnimationDecorator.h"
#include <Rocket/Core.h>
#include <Rocket/Core/Element.h>
#include <Rocket/Core/SystemInterface.h>
#include "ShellOpenGL.h"
#include <math.h>

FrameAnimationDecorator::~FrameAnimationDecorator() {
}

bool FrameAnimationDecorator::Initialise(const Rocket::Core::String& image_source, const Rocket::Core::String& image_path, int num_frames, int frame_width, float frame_delay, bool reverse) {
	m_texture = LoadTexture(image_source, image_path);
	m_num_frames = num_frames;
	m_frame_width = frame_width;
	m_frame_delay = frame_delay;
	m_reverse = reverse;
	m_init_time = 	Rocket::Core::GetSystemInterface()->GetElapsedTime();
	return m_texture != -1;
}

Rocket::Core::DecoratorDataHandle FrameAnimationDecorator::GenerateElementData(Rocket::Core::Element *element) {
	return NULL;
}

void FrameAnimationDecorator::ReleaseElementData(Rocket::Core::DecoratorDataHandle element_data) {
}

void FrameAnimationDecorator::RenderElement(Rocket::Core::Element *element, Rocket::Core::DecoratorDataHandle element_data) {
	Rocket::Core::Vector2f position = element->GetAbsoluteOffset(Rocket::Core::Box::PADDING);
	Rocket::Core::Vector2f size = element->GetBox().GetSize(Rocket::Core::Box::PADDING);
	
	float delta = Rocket::Core::GetSystemInterface()->GetElapsedTime() - m_init_time;
	int frame = ((int)ceilf(delta/m_frame_delay))%m_num_frames;
	if (m_reverse) {
		frame = m_num_frames-frame-1;
	}
	float frame_w = 1.0f / m_num_frames;
	float l = frame_w * frame;
	float r = l + frame_w;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, (GLuint) GetTexture(m_texture)->GetHandle(element->GetRenderInterface()));

	glColor4ub(255, 255, 255, 255);
	glBegin(GL_QUADS);

		glTexCoord2f(l, 0);
		glVertex2f(position.x, position.y);

		glTexCoord2f(l, 1);
		glVertex2f(position.x, position.y + size.y);

		glTexCoord2f(r, 1);
		glVertex2f(position.x + size.x, position.y + size.y);

		glTexCoord2f(r, 0);
		glVertex2f(position.x + size.x, position.y);

	glEnd();
}
