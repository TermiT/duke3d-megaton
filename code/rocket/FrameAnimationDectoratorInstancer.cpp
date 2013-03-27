//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include "FrameAnimationDecoratorInstancer.h"
#include "FrameAnimationDecorator.h"
#include <math.h>

Rocket::Core::Decorator* FrameAnimationDecoratorInstancer::InstanceDecorator(const Rocket::Core::String& name, const Rocket::Core::PropertyDictionary& properties) {
	const Rocket::Core::Property *image_source_property = properties.GetProperty("image-src");
	const Rocket::Core::Property *num_frames_property = properties.GetProperty("num-frames");
	const Rocket::Core::Property *delay_property = properties.GetProperty("delay");

	Rocket::Core::String image_source = image_source_property->Get< Rocket::Core::String >();
	int num_frames = num_frames_property->Get<int>();
	float delay = delay_property->Get<float>();

	FrameAnimationDecorator *decorator = new FrameAnimationDecorator();
	if (decorator->Initialise(image_source, image_source_property->source, num_frames, 0, fabs(delay), delay < 0)) {
		return decorator;
	}
	decorator->RemoveReference();
	ReleaseDecorator(decorator);
	return NULL;
}

void FrameAnimationDecoratorInstancer::ReleaseDecorator(Rocket::Core::Decorator* decorator) {
	delete decorator;
}

void FrameAnimationDecoratorInstancer::Release() {
	delete this;
}

FrameAnimationDecoratorInstancer::FrameAnimationDecoratorInstancer() {
	RegisterProperty("image-src", "").AddParser("string");
	RegisterProperty("num-frames", "10").AddParser("number");
	RegisterProperty("delay", "0.05").AddParser("number");
}

FrameAnimationDecoratorInstancer::~FrameAnimationDecoratorInstancer() {
}
