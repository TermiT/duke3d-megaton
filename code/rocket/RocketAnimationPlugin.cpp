//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include "RocketAnimationPlugin.h"
#include <Rocket/Core.h>
#include <Rocket/Core/Element.h>
#include "PluginUtils.h"
#include <math.h>

enum AnimationPhase {
	AnimatonPhaseNotYetStarted,
	AnimationPhaseActive,
	AnimationPhaseFinished,
};

struct BasicAnimationTracker {
	Rocket::Core::Element *e;
	BasicAnimationActuator *actuator;
	float duration;
	float time, starting_time;
	float position;

	AnimationType type;
	AnimationPhase phase;
	BasicAnimationTracker(Rocket::Core::Element *element, AnimationType type, float duration, BasicAnimationActuator *actuator):
		type(type),
		duration(duration),
		phase(AnimatonPhaseNotYetStarted),
		actuator(actuator),
		e(element)
	{
	}
	
	virtual ~BasicAnimationTracker(){};

	virtual void ApplyAnimation() {
		actuator->Apply(e, position);
	}

	virtual void StartAnimation() {
		position = 0.0f;
		starting_time = time;
		actuator->Init(e);
	}

	virtual void UpdateAnimationPosition() {
		float offset = time - starting_time;
		int cycle = (int)(offset/duration);
		if (cycle != 0 && type == AnimationTypeOnce) {
			phase = AnimationPhaseFinished;
		} else {
			float cycle_start = cycle * duration;
			float cycle_offset = offset - cycle_start;
			if (type == AnimationTypeBounce && cycle%2) {
				position = 1.0f - cycle_offset/duration;
			} else {
				position = cycle_offset/duration;
			}
		}
	}

	virtual void Update() {
		time = Rocket::Core::GetSystemInterface()->GetElapsedTime();
		if (phase == AnimatonPhaseNotYetStarted) {
			StartAnimation();
			phase = AnimationPhaseActive;
		} 
		if (phase == AnimationPhaseActive) {
			UpdateAnimationPosition();
		} else if (phase == AnimationPhaseFinished) {
			return;
		}
		ApplyAnimation();
	}

	virtual void Reset() {
		actuator->Reset(e);
	}

};

static void
SetElementData(Rocket::Core::Element *element, BasicAnimationTracker *user_data) {
	SetElementUserData(element, (BasicAnimationTracker*)user_data, "__animation_data");
}

static BasicAnimationTracker*
GetElementData(Rocket::Core::Element *element) {
	return (BasicAnimationTracker*)GetElementUserData(element, "__animation_data");
}

RocketAnimationPlugin::RocketAnimationPlugin() {
}

RocketAnimationPlugin::~RocketAnimationPlugin() {
}

void RocketAnimationPlugin::OnInitialise() {
}

void RocketAnimationPlugin::OnShutdown() {
}

void RocketAnimationPlugin::OnDocumentOpen(Rocket::Core::Context* context, const Rocket::Core::String& document_path) {
}

void RocketAnimationPlugin::OnDocumentLoad(Rocket::Core::ElementDocument* document) {
}

// Called when a document is unloaded from its context. This is called after the document's 'unload' event.
void RocketAnimationPlugin::OnDocumentUnload(Rocket::Core::ElementDocument* document) {
}

// Called when a new context is created.
void RocketAnimationPlugin::OnContextCreate(Rocket::Core::Context* context) {
}

// Called when a context is destroyed.
void RocketAnimationPlugin::OnContextDestroy(Rocket::Core::Context* context) {
}

// Called when a new element is created
void RocketAnimationPlugin::OnElementCreate(Rocket::Core::Element* element) {
}

// Called when an element is destroyed.
void RocketAnimationPlugin::OnElementDestroy(Rocket::Core::Element* element) {
	BasicAnimationTracker *ad = GetElementData(element);
	if (ad != NULL) {
		m_animations.remove(ad);
		delete ad;
	}
}

void RocketAnimationPlugin::UpdateAnimation() {
	for (std::list<BasicAnimationTracker*>::iterator i = m_animations.begin(); i != m_animations.end(); i++) {
		(*i)->Update();
	}
}

void RocketAnimationPlugin::AnimateElement(Rocket::Core::Element *element, AnimationType type, float duration, BasicAnimationActuator *actuator) {
	BasicAnimationTracker *animation = GetElementData(element);
	if (animation == NULL && !element->IsClassSet("noanim")) {
		animation = new BasicAnimationTracker(element, type, duration, actuator);
		SetElementData(element, animation);
		m_animations.push_back(animation);
	}
}

void RocketAnimationPlugin::CancelAnimation(Rocket::Core::Element *element, bool reset) {
	BasicAnimationTracker *animation = GetElementData(element);
	if (animation != NULL) {
		if (reset) {
			animation->Reset();
		}
		m_animations.remove(animation);
		delete animation;
		SetElementData(element, NULL);
	}
}

bool RocketAnimationPlugin::HasAnimation(Rocket::Core::Element *element) {
	return GetElementData(element) != NULL;
}
