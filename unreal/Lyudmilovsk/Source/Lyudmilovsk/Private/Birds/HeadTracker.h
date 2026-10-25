#pragma once

#include "CoreMinimal.h"

/**
 * AirPods head pose for the player bird. On a Mac the game starts tools/headtracker's helper app
 * (Saved/HeadTracker/LyudHeadTracker.app; the first flight builds it with tools/headtracker/build.sh) and reads the attitude it
 * sends over a localhost UDP socket; anywhere else, or without the helper or AirPods, it simply never
 * tracks. Angles in degrees in Unreal's sense: yaw + right, pitch + up, roll + right ear down.
 */
class FHeadTracker
{
public:
	~FHeadTracker() { Stop(); }

	void Start();
	void Stop();
	/** Reads what arrived; call once per frame. */
	void Poll();

	/** Tracking now (a sample in the last half second). */
	bool IsTracking() const;
	/** The head relative to where it was when tracking began (the helper's own zero). */
	FRotator Head() const { return bFake ? Fake : Pose; }
	/** What the helper last said (connected, disconnected, denied, unavailable...), for the debug line. */
	const FString& State() const { return LastState; }

	/** Developer stand-in for AirPods (Bird.Script, lyud.Bird.FakeHead): a pose to report as if tracked. */
	void SetFake(const FRotator& FakePose, bool bOn) { Fake = FakePose; bFake = bOn; }

private:
	void Launch();

	int Socket = -1;
	bool bLaunched = false, bBuilding = false;
	double LastLaunchTry = 0.0;
	FRotator Pose = FRotator::ZeroRotator;
	double LastSample = -1e9;
	FString LastState = TEXT("off");
	FRotator Fake = FRotator::ZeroRotator;
	bool bFake = false;
};
