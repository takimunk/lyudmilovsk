#include "Plants/GameCalendar.h"

#include "Engine/World.h"
#include "LyudSave.h"

FDateTime UGameCalendar::Now() const
{
	return StartDate + FTimespan::FromSeconds(GameSeconds(GetWorld()) * TimeScale + OffsetSeconds);
}

void UGameCalendar::SetNow(const FDateTime& When)
{
	OffsetSeconds = (When - StartDate).GetTotalSeconds() - GameSeconds(GetWorld()) * TimeScale;
}

void UGameCalendar::SetTimeScale(float Scale)
{
	const FDateTime Was = Now();
	TimeScale = FMath::Max(Scale, 0.f);
	SetNow(Was);
}

void UGameCalendar::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (ULyudSaveSubsystem* Saves = InWorld.IsGameWorld() ? ULyudSaveSubsystem::Get(&InWorld) : nullptr)
	{
		Saves->RegisterSaveObject(TEXT("Calendar"), this);
	}
}

float UGameCalendar::DayOfYear() const
{
	return DayOfYear(Now());
}

float UGameCalendar::DayOfYear(const FDateTime& Date)
{
	return Date.GetDayOfYear() + (float)(Date.GetTimeOfDay().GetTotalHours() / 24.0);
}

FDateTime UGameCalendar::NowIn(const UWorld* World)
{
	if (const UGameCalendar* C = World ? World->GetSubsystem<UGameCalendar>() : nullptr)
	{
		return C->Now();
	}
	return GetDefault<UGameCalendar>()->StartDate;
}

double UGameCalendar::GameSeconds(const UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		return 0.0;
	}
	const ULyudSaveSubsystem* Save = ULyudSaveSubsystem::Get(World);
	return Save ? Save->GetGameTimeSeconds() : World->GetTimeSeconds();
}
