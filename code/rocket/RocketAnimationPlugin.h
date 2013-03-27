//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#ifndef ROCKET_ANIMATION_PLUGIN_H
#define ROCKET_ANIMATION_PLUGIN_H

#include <Rocket/Core.h>
#include <Rocket/Core/Plugin.h>
#include <list>

enum AnimationType {
	AnimationTypeOnce,
	AnimationTypeCycle,
	AnimationTypeBounce,
};

struct BasicAnimationTracker;

struct BasicAnimationActuator {
	virtual void Init(Rocket::Core::Element *element) = 0;
	virtual void Apply(Rocket::Core::Element *element, float position) = 0;
	virtual void Stop(Rocket::Core::Element *element) = 0;
	virtual void Reset(Rocket::Core::Element *element) = 0;
};

class RocketAnimationPlugin: public Rocket::Core::Plugin {
public:
	RocketAnimationPlugin();
	virtual ~RocketAnimationPlugin();

	// Called when Rocket is initialised.
	virtual void OnInitialise();
	
	// Called when Rocket shuts down.
	virtual void OnShutdown();

	// Called when a document load request occurs, before the document's file is opened.
	virtual void OnDocumentOpen(Rocket::Core::Context* context, const Rocket::Core::String& document_path);

	// Called when a document is successfully loaded from file or instanced, initialised and added to its context. This is called before the document's 'load' event.
	virtual void OnDocumentLoad(Rocket::Core::ElementDocument* document);

	// Called when a document is unloaded from its context. This is called after the document's 'unload' event.
	virtual void OnDocumentUnload(Rocket::Core::ElementDocument* document);

	// Called when a new context is created.
	virtual void OnContextCreate(Rocket::Core::Context* context);

	// Called when a context is destroyed.
	virtual void OnContextDestroy(Rocket::Core::Context* context);

	// Called when a new element is created.
	virtual void OnElementCreate(Rocket::Core::Element* element);

	// Called when an element is destroyed.
	virtual void OnElementDestroy(Rocket::Core::Element* element);

	virtual void AnimateElement(Rocket::Core::Element *element, AnimationType type, float duration, BasicAnimationActuator *actuator);
	virtual void CancelAnimation(Rocket::Core::Element *element, bool reset = true);
	virtual bool HasAnimation(Rocket::Core::Element *element);

	virtual void UpdateAnimation();

private:
	std::list<BasicAnimationTracker*> m_animations;
};


#endif /* ROCKET_ANIMATION_PLUGIN_H */
