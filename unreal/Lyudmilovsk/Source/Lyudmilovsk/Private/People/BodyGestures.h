// The gestures of the townsfolk's manners (PeopleManners.h, docs/town_manners.md): what each manner does with the
// arms, the head and the back on top of the clip, and what it puts in the hands. No clips: a gesture is a few goals for
// the hands worked out from the time since it began (Eval), reached by the arms in the animation worker (Apply), so it
// goes with any pose and any body, melts into the next one, and a left-hander does it with the left hand.
#pragma once

#include "CoreMinimal.h"
#include "BonePose.h"
#include "People/LyudBodyAnim.h"
#include "PeopleManners.h"

namespace LyudGesture
{
	/** What somebody's body is doing under the gesture. */
	struct FBody
	{
		bool bSeated = false;
		bool bWalking = false;
		bool bLeftHanded = false;
		/** Body size against the clips' body (ANpcBody::GetBodyScale). */
		float Scale = 1.f;
		/** Their own number: people side by side aren't in step. */
		uint32 Seed = 0;
	};

	/** The manner's pose T seconds after it began, with Left seconds to go (a negative Left: no end in sight). */
	void Eval(LyudPeople::EManner Manner, float T, float Left, const FBody& Body, FLyudGesturePose& Out);

	/** A clip that does the manner better than the goals can, for a body standing still (NAME_None: none). */
	FName ClipFor(LyudPeople::EManner Manner);

	/** In the animation worker, after the clip and the feet: the back, the head, the arms, the fingers. */
	void Apply(FCompactPose& Pose, const FLyudGesturePose& G, FLyudGestureBones& Bones);

	/** How a prop sits in the hands. */
	enum class EHold : uint8
	{
		Upright,    // in the main hand, standing up whatever the hand does (an umbrella, a cone, a cup)
		Hanging,    // from the main hand, hanging down (a bag)
		Palm,       // lying in the main hand's palm along the fingers, its face out of the palm (a phone)
		Fingers,    // between the main hand's fingers, sticking out to the thumb's side (a cigarette, a brush)
		TwoHands,   // between both hands, its length from hand to hand, its face toward the head (a paper, a camera)
		Pointing,   // in the main hand, pointing on past the other hand (a rod)
	};
	EHold HoldOf(LyudPeople::EProp Prop);
}
