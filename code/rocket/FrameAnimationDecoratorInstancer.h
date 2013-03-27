//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#ifndef FRAME_ANIMATION_DECORATOR_INSTANCER_H
#define FRAME_ANIMATION_DECORATOR_INSTANCER_H

#include <Rocket/Core/DecoratorInstancer.h>

class FrameAnimationDecoratorInstancer: public Rocket::Core::DecoratorInstancer {
public:
	// Instances a decorator given the property tag and attributes from the RCSS file.
	// @param[in] name The type of decorator desired.
	// @param[in] properties All RCSS properties associated with the decorator.
	// @return The decorator if it was instanced successful, NULL if an error occured.
	virtual Rocket::Core::Decorator* InstanceDecorator(const Rocket::Core::String& name, const Rocket::Core::PropertyDictionary& properties);
	// Releases the given decorator.
	// @param[in] decorator Decorator to release.
	virtual void ReleaseDecorator(Rocket::Core::Decorator* decorator);

	// Releases the instancer.
	virtual void Release();

	FrameAnimationDecoratorInstancer();
	virtual ~FrameAnimationDecoratorInstancer();
};

#endif /* FRAME_ANIMATION_DECORATOR_INSTANCER_H */
