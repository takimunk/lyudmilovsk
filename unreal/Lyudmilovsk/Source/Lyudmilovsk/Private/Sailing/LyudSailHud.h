// The helmsman's HUD and view (docs/yacht_school.md "Engine: the boats"). The boat (ALyudDinghy) fills FLyudSailHudData
// once a frame while the hero has the helm; SLyudSailHud only draws it (a wind rose with the no-go zone, the point of
// sail, speed, heel, sheet and board gauges, the tutor's hint, the lesson / race panel with its countdown and the next
// mark). ULyudSailCamera rolls the first-person view with a share of the heel and tumbles it into the water on a capsize.
#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "Widgets/SLeafWidget.h"
#include "LyudSailHud.generated.h"

class ALyudDinghy;

/** Everything the HUD draws, in the boat's own terms. */
struct FLyudSailHudData
{
	// the boat now
	float TwaRad = 0.f;            // true wind angle, + = over the starboard side
	float TwsMs = 0.f;
	float AwaRad = 0.f;            // apparent
	float AwsMs = 0.f;
	float SpeedMs = 0.f;
	float PolarMs = 0.f;           // what a good crew makes at this angle in this wind
	float HeelDeg = 0.f;
	float Sheet = 0.5f;            // 0 hard in .. 1 right out
	float IdealSheet = 0.5f;
	float Board = 1.f;             // 0 up .. 1 down
	float Hike = 0.f;              // 0 in .. 1 right out (to windward)
	float DepthM = 100.f;
	float Luff = 0.f;
	float TrimError = 0.f;
	float BeatRad = 0.75f;         // the no-go zone's half width
	float Water = 0.f;
	float Righting = 0.f;
	int32 Point = 0;               // LyudSail::EPoint
	int32 State = 0;               // LyudSail::EState
	bool bByTheLee = false;
	bool bPlaning = false;
	bool bShallow = false;
	bool bEasySheet = false;
	// the words
	FText PointName;
	FText Hint;
	float HintLeft = 0.f;
	FText Keys;                    // the key map, for a while after boarding
	float KeysLeft = 0.f;
	// the school's task
	FText Title;
	TArray<FText> Lines;
	float Countdown = -1.f;
	FVector2D MarkM = FVector2D::ZeroVector;   // world metres; zero = none
	bool bHasMark = false;
	float MarkBearingRad = 0.f;    // off the bow, + to starboard
	float MarkDistM = 0.f;
	// lyud.Sail.Debug
	FString Debug;
	/** A screen is up (the diary, the menu): nothing is drawn. */
	bool bHidden = false;
};

class SLyudSailHud : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudSailHud) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, ALyudDinghy* InBoat);

	static TSharedPtr<SLyudSailHud> AddTo(ALyudDinghy* InBoat);
	static void Remove(const TSharedPtr<SLyudSailHud>& Hud);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(64.f, 64.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Culling, FSlateWindowElementList& Out,
	                      int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

protected:
	virtual bool ComputeVolatility() const override { return true; }

private:
	TWeakObjectPtr<ALyudDinghy> Boat;
	/** The labels, looked up once. */
	FText WordHeel, WordSheet, WordBoard, WordWater, WordKn, WordMs, WordM, WordShallow, WordCapsized, WordPlaning, WordWind, WordAuto;
};

namespace LyudSailHud
{
	/** lyud.Sail.Hud: 0 off, 1 the rose, the speed and the words, 2 the gauges too. */
	int32 Mode();
	/** The words of a key; while the strings tables do not have it yet, the English fallback (never the bare key). */
	FText Word(const FString& Key, const TCHAR* Fallback);
}

/** The helmsman's view: rolled with a share of the heel (eased, capped), dropped to the water and tipped on a capsize. */
UCLASS()
class ULyudSailCamera : public UCameraModifier
{
	GENERATED_BODY()

public:
	/** What the boat asks for (deg, cm); eased here. */
	float WantRoll = 0.f;
	float WantPitch = 0.f;
	float WantDrop = 0.f;
	virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;

private:
	float Roll = 0.f;
	float Pitch = 0.f;
	float Drop = 0.f;
};
