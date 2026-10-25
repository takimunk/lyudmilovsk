#include "Weather/LyudWind.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Plants/GameCalendar.h"
#include "WindModel.h"

namespace
{
	TAutoConsoleVariable<float> CVarWindSpeed(TEXT("lyud.Wind.Speed"), -1.f, TEXT("Force the mean wind speed, m/s at 10 m (negative: the calendar's weather)."));
	TAutoConsoleVariable<float> CVarWindFrom(TEXT("lyud.Wind.From"), -1.f, TEXT("Force the bearing the wind comes from, degrees (270 = off the sea; negative: the calendar's)."));
	TAutoConsoleVariable<float> CVarWindGust(TEXT("lyud.Wind.Gustiness"), -1.f, TEXT("Force the gust spread, share of the mean (negative: the calendar's)."));
	TAutoConsoleVariable<int32> CVarWindDebug(TEXT("lyud.Wind.Debug"), 0, TEXT("1: print the wind on screen."));

	const TCHAR* CollectionPath = TEXT("/Game/Weather/MPC_LyudWeather.MPC_LyudWeather");

	LyudWeather::FWindParams ToParams(const FLyudWindSettings& S)
	{
		LyudWeather::FWindParams P;
		P.PrevailingFromDeg = S.PrevailingFromDeg;
		P.DirectionSwingDeg = S.DirectionSwingDeg;
		P.MedianSpeed = S.MedianSpeed;
		P.SpeedLogSigma = S.SpeedLogSigma;
		P.MaxSpeed = S.MaxSpeed;
		P.NightFactor = S.NightFactor;
		P.GustinessDay = S.GustinessDay;
		P.GustinessNight = S.GustinessNight;
		P.SeaBreeze = S.SeaBreeze;
		P.SeaBearingDeg = S.SeaBearingDeg;
		P.Seed = uint32(S.Seed);
		return P;
	}

	/** Compass bearing the wind comes from -> world unit vector it blows toward (north +Y, east -X). */
	FVector Toward(float FromDeg)
	{
		const float R = FMath::DegreesToRadians(FromDeg + 180.f);
		return FVector(-FMath::Sin(R), FMath::Cos(R), 0.f);
	}
}

ULyudWindSubsystem* ULyudWindSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudWindSubsystem>() : nullptr;
}

bool ULyudWindSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor || WorldType == EWorldType::GamePreview;
}

void ULyudWindSubsystem::Initialize(FSubsystemCollectionBase& Collection_)
{
	Super::Initialize(Collection_);
}

TStatId ULyudWindSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudWindSubsystem, STATGROUP_Tickables);
}

FDateTime ULyudWindSubsystem::WindTime() const
{
	static IConsoleVariable* SkyOffset = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.SkyHourOffset"));
	const float Offset = SkyOffset ? SkyOffset->GetFloat() : 0.f;
	return UGameCalendar::NowIn(GetWorld()) + FTimespan::FromHours(Offset);
}

double ULyudWindSubsystem::NowHours() const
{
	const FDateTime T = WindTime();
	return LyudWeather::HoursSince2000(T.GetYear(), T.GetMonth(), T.GetDay(), T.GetTimeOfDay().GetTotalHours());
}

const ULyudWindSubsystem::FState& ULyudWindSubsystem::State() const
{
	const double Hours = NowHours();
	const float ForceSpeed = CVarWindSpeed.GetValueOnGameThread();
	const float ForceFrom = CVarWindFrom.GetValueOnGameThread();
	const float ForceGust = CVarWindGust.GetValueOnGameThread();
	// a second of game time changes nothing worth recomputing; forced values apply at once
	const bool bForceChanged = LastForce[0] != ForceSpeed || LastForce[1] != ForceFrom || LastForce[2] != ForceGust;
	if (FMath::Abs(Hours - Cached.Hours) < 1.0 / 3600.0 && !bForceChanged)
	{
		return Cached;
	}
	LastForce[0] = ForceSpeed;
	LastForce[1] = ForceFrom;
	LastForce[2] = ForceGust;

	const LyudWeather::FWindParams P = ToParams(Settings);
	const LyudWeather::FWindState S = LyudWeather::Regime(Hours, P);
	const LyudWeather::FWindState Sea = LyudWeather::SeaStateWind(Hours, P);
	FState& C = Cached;
	C.Hours = Hours;
	C.Speed = ForceSpeed >= 0.f ? ForceSpeed : S.Speed;
	C.FromDeg = ForceFrom >= 0.f ? FMath::Fmod(ForceFrom, 360.f) : S.FromDeg;
	C.Gustiness = ForceGust >= 0.f ? ForceGust : S.Gustiness;
	C.Day = S.Day;
	C.Mean = Toward(C.FromDeg) * C.Speed;
	// a forced wind is one the sea has been in for hours
	const float SeaSpeed = ForceSpeed >= 0.f ? ForceSpeed : Sea.Speed;
	const float SeaFrom = ForceFrom >= 0.f ? C.FromDeg : Sea.FromDeg;
	C.Sea = Toward(SeaFrom) * SeaSpeed;
	return C;
}

FLyudWindSample ULyudWindSubsystem::GetWindAt(const FVector& Location, float HeightAboveGroundM, bool bOverWater) const
{
	const FState& S = State();
	LyudWeather::FWindState W;
	W.Gustiness = S.Gustiness;
	const LyudWeather::FGust G = LyudWeather::GustAt(W, Location.X * 0.01, Location.Y * 0.01, DriftX, DriftY, GustClock);
	const FVector Dir = S.Mean.GetSafeNormal2D(UE_SMALL_NUMBER, FVector(0.0, 1.0, 0.0)).RotateAngleAxisRad(G.Veer, FVector::UpVector);
	FLyudWindSample Out;
	Out.Gust = G.Factor;
	Out.Speed = S.Speed * G.Factor * LyudWeather::HeightFactor(HeightAboveGroundM, bOverWater);
	Out.Velocity = Dir * Out.Speed;
	return Out;
}

FVector ULyudWindSubsystem::GetMeanWind() const { return State().Mean; }
float ULyudWindSubsystem::GetMeanSpeed() const { return State().Speed; }
float ULyudWindSubsystem::GetFromDegrees() const { return State().FromDeg; }
float ULyudWindSubsystem::GetGustiness() const { return State().Gustiness; }
float ULyudWindSubsystem::GetDayStir() const { return State().Day; }
FVector ULyudWindSubsystem::GetSeaStateWind() const { return State().Sea; }

void ULyudWindSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const FState& S = State();
	// the air drifts downwind with the mean wind; the gusts ride on it
	const double Period = LyudWeather::GustPeriod;
	DriftX = FMath::Fmod(DriftX + S.Mean.X * DeltaTime + Period, Period);
	DriftY = FMath::Fmod(DriftY + S.Mean.Y * DeltaTime + Period, Period);
	GustClock = FMath::Fmod(GustClock + DeltaTime, 10240.0);

	if (!bCollectionLooked)
	{
		bCollectionLooked = true;
		Collection = LoadObject<UMaterialParameterCollection>(nullptr, CollectionPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (Collection)
	{
		const FVector Dir = S.Mean.GetSafeNormal2D(UE_SMALL_NUMBER, FVector(0.0, 1.0, 0.0));
		UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("Wind"), FLinearColor(Dir.X, Dir.Y, S.Speed, S.Gustiness));
		UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("WindDrift"), FLinearColor(DriftX, DriftY, GustClock, S.Day));
	}
	if (CVarWindDebug.GetValueOnGameThread() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(0x5157494E, 0.f, FColor::Cyan, DebugReport());
	}
}

FString ULyudWindSubsystem::DebugReport() const
{
	const FState& S = State();
	const FVector Sea = S.Sea;
	return FString::Printf(TEXT("wind %s: from %.0f deg %.1f m/s, gusts %.2f, day %.2f; sea state %.1f m/s from %.0f deg; drift %.0f,%.0f m"),
		*WindTime().ToString(TEXT("%Y-%m-%d %H:%M")), S.FromDeg, S.Speed, S.Gustiness, S.Day, Sea.Size(),
		FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(Sea.X, -Sea.Y)) + 360.0, 360.0), DriftX, DriftY);
}
