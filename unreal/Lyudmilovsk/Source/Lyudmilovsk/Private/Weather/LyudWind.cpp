#include "Weather/LyudWind.h"

#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Dev/LyudDev.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "LandscapeProxy.h"
#include "LyudText.h"
#include "Materials/MaterialParameterCollection.h"
#include "Plants/GameCalendar.h"
#include "Sea/LyudSea.h"
#include "Terrain/LyudTerrain.h"
#include "Weather/LyudWindsock.h"
#include "WeatherModel.h"
#include "WindField.h"
#include "WindModel.h"

namespace
{
	TAutoConsoleVariable<float> CVarWindSpeed(TEXT("lyud.Wind.Speed"), -1.f, TEXT("Force the mean wind speed, m/s at 10 m (negative: the calendar's weather)."));
	TAutoConsoleVariable<float> CVarWindFrom(TEXT("lyud.Wind.From"), -1.f, TEXT("Force the bearing the wind comes from, degrees (270 = off the sea; negative: the calendar's)."));
	TAutoConsoleVariable<float> CVarWindGust(TEXT("lyud.Wind.Gustiness"), -1.f, TEXT("Force the gust spread, share of the mean (negative: the calendar's)."));
	TAutoConsoleVariable<int32> CVarWindDebug(TEXT("lyud.Wind.Debug"), 0, TEXT("1: print the wind on screen; 2: also draw the local wind round the camera (arrows coloured by exposure, ticks of rising and sinking air)."));
	TAutoConsoleVariable<int32> CVarWindShifts(TEXT("lyud.Wind.Shifts"), 1, TEXT("1: the direction swings a few degrees every several minutes and the area breathes; 0: the weather's steady wind."));
	TAutoConsoleVariable<int32> CVarWindRelief(TEXT("lyud.Wind.Relief"), 1, TEXT("1: GetWindAt takes the land's lee and speed-up in; 0: the open-air wind as before (GetLocalWind always has the land)."));
	TAutoConsoleVariable<float> CVarWindReliefBudget(TEXT("lyud.Wind.ReliefBudgetMs"), 0.6f,
		TEXT("At the start of play the wind's relief grid is read from the land for at most this many ms a frame (it always reads a few cells)."));

	const TCHAR* CollectionPath = TEXT("/Game/Weather/MPC_LyudWeather.MPC_LyudWeather");

	/** The relief grid: the cell (m), the sea round the land (m), and the most cells it may have (two floats each). */
	constexpr double ReliefCellM = 16.0;
	constexpr double ReliefMarginM = 128.0;
	constexpr int32 ReliefMaxCells = 90000;
	/** The town's houses and the pier are put up in code in the first moments of play: the land is read after them. */
	constexpr float ReliefDelayS = 4.f;
	/** The top of an old pine and spruce forest over its floor, m (the trees have no collision: the terrain mask says where they stand). */
	constexpr float ForestCrownM = 16.f;
	/** The sea's datum where there is no sea actor, cm (tools/world_map.py SEA_Z). */
	constexpr float WindDatumCm = -4300.f;

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

	float WrapWindDeg(float Deg)
	{
		const float D = FMath::Fmod(Deg, 360.f);
		return D < 0.f ? D + 360.f : D;
	}

	/** The strongest gust to expect in a wind of this mean and gustiness, m/s (as LyudWeather::FWindOutlook::GustSpeed). */
	float GustSpeedOf(float Speed, float Gustiness)
	{
		return Speed * (1.f + 2.2f * Gustiness * 0.8f);
	}

	bool WindCameraAt(const UWorld* World, FVector& Out)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			Out = PC->PlayerCameraManager->GetCameraLocation();
			return true;
		}
		return false;
	}

	FLyudLocalWind ToLocalWind(const LyudWeather::FLocalWind& L)
	{
		FLyudLocalWind Out;
		Out.Velocity = FVector(L.VX, L.VY, L.VZ);
		Out.Speed = L.Speed;
		Out.FromDegrees = L.FromDeg;
		Out.Gust = L.Gust;
		Out.Exposure = L.Exposure;
		Out.Shelter = L.Shelter;
		Out.Updraft = L.Updraft;
		Out.Turbulence = L.Turbulence;
		Out.HeightM = L.HeightM;
		Out.FetchM = L.FetchM;
		Out.bOverWater = L.bOverWater;
		return Out;
	}

	/**
	 * One column of the static world at X, Y (cm): the highest thing there (a roof, the pier's deck, the ground itself)
	 * and the ground under it (the landscape, or what is tagged LyudSeaBed like the railway's bank: as the sea's depth
	 * map reads it). Simple collision, as the bird's air does: the houses built in code have boxes, not triangles.
	 */
	bool TraceWindColumn(const UWorld* World, double X, double Y, double TopZ, double BottomZ, const FCollisionQueryParams& Params,
		float& OutGround, float& OutTop, bool& bOutGround)
	{
		static TArray<FHitResult> Hits;   // the game thread's, kept to spare an allocation per cell
		Hits.Reset();
		bOutGround = false;
		bool bAny = false;
		World->LineTraceMultiByObjectType(Hits, FVector(X, Y, TopZ), FVector(X, Y, BottomZ), FCollisionObjectQueryParams(ECC_WorldStatic), Params);
		for (const FHitResult& H : Hits)
		{
			const float Z = float(H.ImpactPoint.Z);
			if (!bAny || Z > OutTop)
			{
				OutTop = Z;
				bAny = true;
			}
			const AActor* Actor = H.GetActor();
			const UPrimitiveComponent* Comp = H.GetComponent();
			if ((Actor && Actor->IsA<ALandscapeProxy>()) || (Comp && Comp->ComponentHasTag(TEXT("LyudSeaBed"))))
			{
				if (!bOutGround || Z > OutGround)
				{
					OutGround = Z;
					bOutGround = true;
				}
			}
		}
		return bAny;
	}

	void SetWindCVar(const TCHAR* Name, float V)
	{
		if (IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			C->Set(V, ECVF_SetByConsole);
		}
	}

	/** A forced wind for the developer panel: Speed m/s from FromDeg; negative = the calendar's. */
	void DevWind(float Speed, float FromDeg)
	{
		SetWindCVar(TEXT("lyud.Wind.Speed"), Speed);
		SetWindCVar(TEXT("lyud.Wind.From"), FromDeg);
	}

	bool DevWindIs(float Speed, float FromDeg)
	{
		return FMath::IsNearlyEqual(CVarWindSpeed.GetValueOnGameThread(), Speed, 0.01f) && FMath::IsNearlyEqual(CVarWindFrom.GetValueOnGameThread(), FromDeg, 0.01f);
	}

	void LogWindReport(UWorld* World)
	{
		const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(World);
		if (!Wind)
		{
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("WIND %s"), *Wind->DebugReport());
		UE_LOG(LogTemp, Display, TEXT("WIND words: %s"), *Wind->DescribeWind().ToString());
		FVector Cam;
		if (WindCameraAt(World, Cam))
		{
			const FLyudLocalWind L = Wind->GetLocalWind(Cam);
			UE_LOG(LogTemp, Display, TEXT("WIND at the camera (%.0f, %.0f, %.0f): %.1f m/s from %.0f deg, gust x%.2f, exposure %.2f, shelter %.2f, updraft %+.2f m/s, turbulence %.2f m/s, %.1f m over %s, fetch %.0f m, relief %s"),
				Cam.X, Cam.Y, Cam.Z, L.Speed, L.FromDegrees, L.Gust, L.Exposure, L.Shelter, L.Updraft, L.Turbulence, L.HeightM,
				L.bOverWater ? TEXT("water") : TEXT("land"), L.FetchM, Wind->IsReliefReady() ? TEXT("ready") : TEXT("not read yet"));
			float In = 0.f, Factor = 1.f;
			bool bLull = false;
			if (Wind->GetNextGust(Cam, 60.f, In, Factor, bLull))
			{
				UE_LOG(LogTemp, Display, TEXT("WIND next %s x%.2f in %.0f s"), bLull ? TEXT("lull") : TEXT("gust"), Factor, In);
			}
			else
			{
				UE_LOG(LogTemp, Display, TEXT("WIND no gust or lull worth a word within 60 s"));
			}
		}
		const FLyudWindOutlook O = Wind->GetOutlook(3.f);
		UE_LOG(LogTemp, Display, TEXT("WIND next 3 h: %.1f..%.1f m/s (mean %.1f, force %d), gusts to %.1f (force %d), from %.0f deg turning %+.0f, trend %+.2f m/s per h; a kite breeze (4..9 m/s) in %.1f h"),
			O.MinSpeed, O.MaxSpeed, O.MeanSpeed, O.Force, O.GustSpeed, O.ForceMax, O.FromDegrees, O.TurnDegrees, O.Trend,
			Wind->HoursUntilWind(4.f, 9.f, 1.f, true));
		if (const ULyudWindTells* Tells = ULyudWindTells::Get(World))
		{
			UE_LOG(LogTemp, Display, TEXT("WIND %s"), *Tells->DebugReport());
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GWindReport(TEXT("lyud.Wind.Report"),
		TEXT("lyud.Wind.Report: logs the wind, its swing, the local wind at the camera and the outlook (lines start with WIND)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			LogWindReport(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs GWindRebuild(TEXT("lyud.Wind.RebuildRelief"),
		TEXT("lyud.Wind.RebuildRelief: reads the land for the local wind again (after the ground or the houses changed)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(World))
			{
				Wind->RebuildRelief();
			}
		}));

	// Developer mode (F7, Actions): the winds a kite, a dinghy and the tell-tales want to be tried in.
	LyudDev::FAutoAction GDevWindCalendar(TEXT("wind.calendar"), TEXT("wind"), TEXT("dev.action.wind.calendar"),
		[](UWorld* W) { DevWind(-1.f, -1.f); }, [](UWorld* W) { return CVarWindSpeed.GetValueOnGameThread() < 0.f && CVarWindFrom.GetValueOnGameThread() < 0.f; });
	LyudDev::FAutoAction GDevWindCalm(TEXT("wind.calm"), TEXT("wind"), TEXT("dev.action.wind.calm"),
		[](UWorld* W) { DevWind(0.6f, 270.f); }, [](UWorld* W) { return DevWindIs(0.6f, 270.f); });
	LyudDev::FAutoAction GDevWindKite(TEXT("wind.kite"), TEXT("wind"), TEXT("dev.action.wind.kite"),
		[](UWorld* W) { DevWind(6.f, 270.f); }, [](UWorld* W) { return DevWindIs(6.f, 270.f); });
	LyudDev::FAutoAction GDevWindFresh(TEXT("wind.fresh"), TEXT("wind"), TEXT("dev.action.wind.fresh"),
		[](UWorld* W) { DevWind(10.f, 250.f); }, [](UWorld* W) { return DevWindIs(10.f, 250.f); });
	LyudDev::FAutoAction GDevWindGale(TEXT("wind.gale"), TEXT("wind"), TEXT("dev.action.wind.gale"),
		[](UWorld* W) { DevWind(18.f, 290.f); }, [](UWorld* W) { return DevWindIs(18.f, 290.f); });
	LyudDev::FAutoAction GDevWindOffshore(TEXT("wind.offshore"), TEXT("wind"), TEXT("dev.action.wind.offshore"),
		[](UWorld* W) { DevWind(7.f, 90.f); }, [](UWorld* W) { return DevWindIs(7.f, 90.f); });
	LyudDev::FAutoAction GDevWindArrows(TEXT("wind.arrows"), TEXT("wind"), TEXT("dev.action.wind.arrows"),
		[](UWorld* W) { SetWindCVar(TEXT("lyud.Wind.Debug"), CVarWindDebug.GetValueOnGameThread() >= 2 ? 0.f : 2.f); },
		[](UWorld* W) { return CVarWindDebug.GetValueOnGameThread() >= 2; });
	LyudDev::FAutoAction GDevWindReport(TEXT("wind.report"), TEXT("wind"), TEXT("dev.action.wind.report"), [](UWorld* W) { LogWindReport(W); });
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

void ULyudWindSubsystem::Deinitialize()
{
	Relief.Reset();
	bReliefReady = false;
	bReliefStarted = false;
	Super::Deinitialize();
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
	const int32 ShiftsOn = CVarWindShifts.GetValueOnGameThread() != 0 ? 1 : 0;
	// a second of game time changes nothing worth recomputing in the weather; forced values apply at once
	// (a weather preset plays another hour's episode: that counts as forced too)
	const float Shift = float(LyudWeather::GetHourShift());
	const bool bForceChanged = LastForce[0] != ForceSpeed || LastForce[1] != ForceFrom || LastForce[2] != ForceGust || LastShift != Shift;
	LastShift = Shift;
	FState& C = Cached;
	const bool bWeatherStale = FMath::Abs(Hours - C.Hours) >= 1.0 / 3600.0 || bForceChanged;
	if (!bWeatherStale && Hours == C.ShiftHours && LastShiftsOn == ShiftsOn)
	{
		return C;
	}
	const LyudWeather::FWindParams P = ToParams(Settings);
	if (bWeatherStale)
	{
		LastForce[0] = ForceSpeed;
		LastForce[1] = ForceFrom;
		LastForce[2] = ForceGust;

		const LyudWeather::FWindState S = LyudWeather::Regime(Hours, P);
		const LyudWeather::FWindState SeaWind = LyudWeather::SeaStateWind(Hours, P);
		C.Hours = Hours;
		C.SteadySpeed = ForceSpeed >= 0.f ? ForceSpeed : S.Speed;
		C.SteadyFromDeg = ForceFrom >= 0.f ? FMath::Fmod(ForceFrom, 360.f) : S.FromDeg;
		C.Gustiness = ForceGust >= 0.f ? ForceGust : S.Gustiness;
		C.Day = S.Day;
		C.Weather = S.Weather;
		// a forced wind is one the sea has been in for hours; the waves do not follow the swing of a few minutes
		const float SeaSpeed = ForceSpeed >= 0.f ? ForceSpeed : SeaWind.Speed;
		const float SeaFrom = ForceFrom >= 0.f ? C.SteadyFromDeg : SeaWind.FromDeg;
		C.Sea = Toward(SeaFrom) * SeaSpeed;
	}
	// The slow swing rides on the weather's wind and is worked out for the very moment, every frame the clock has
	// moved: the calendar runs at real time (UGameCalendar::TimeScale 1), a swing takes minutes, so between two frames
	// the direction moves by hundredths of a degree and never steps, even though the weather under it is a second old.
	C.ShiftHours = Hours;
	LastShiftsOn = ShiftsOn;
	C.Shift = FLyudWindShift();
	C.Speed = C.SteadySpeed;
	C.FromDeg = C.SteadyFromDeg;
	if (ShiftsOn)
	{
		LyudWeather::FWindState R;
		R.Hours = Hours;
		R.FromDeg = C.SteadyFromDeg;
		R.Speed = C.SteadySpeed;
		const FVector To = Toward(C.SteadyFromDeg);
		R.DirX = float(To.X);
		R.DirY = float(To.Y);
		R.Gustiness = C.Gustiness;
		R.Day = C.Day;
		R.Weather = C.Weather;
		LyudWeather::FWindShift Sh = LyudWeather::ShiftAt(Hours, R, P.Seed);
		if (ForceSpeed >= 0.f)
		{
			// a forced speed is the speed asked for; a forced direction still swings (lyud.Wind.Shifts 0 pins it)
			Sh.Factor = 1.f;
		}
		const LyudWeather::FWindState Now = LyudWeather::Shifted(R, Sh);
		C.Speed = Now.Speed;
		C.FromDeg = WrapWindDeg(Now.FromDeg);
		C.Shift.VeerDegrees = Sh.VeerDeg;
		C.Shift.Factor = Sh.Factor;
		C.Shift.RangeDegrees = Sh.RangeDeg;
		C.Shift.PeriodMinutes = Sh.PeriodMin;
		C.Shift.NextTurnMinutes = Sh.NextTurnMin;
	}
	C.Mean = Toward(C.FromDeg) * C.Speed;
	return C;
}

void ULyudWindSubsystem::FillWindState(const FState& S, LyudWeather::FWindState& Out) const
{
	Out.Hours = S.Hours;
	Out.FromDeg = S.FromDeg;
	Out.Speed = S.Speed;
	const FVector To = Toward(S.FromDeg);
	Out.DirX = float(To.X);
	Out.DirY = float(To.Y);
	Out.Gustiness = S.Gustiness;
	Out.Day = S.Day;
	Out.Weather = S.Weather;
}

const LyudWeather::FWindTerrain* ULyudWindSubsystem::ReadyRelief() const
{
	return bReliefReady && Relief.IsValid() ? Relief.Get() : nullptr;
}

float ULyudWindSubsystem::GetWaterZ() const
{
	const ALyudSea* S = SeaRef.Get();
	return S ? S->SeaLevel() : WindDatumCm;
}

float ULyudWindSubsystem::WaterZM() const
{
	return GetWaterZ() * 0.01f;
}

bool ULyudWindSubsystem::IsReliefReady() const
{
	return bReliefReady;
}

FLyudWindSample ULyudWindSubsystem::GetWindAt(const FVector& Location, float HeightAboveGroundM, bool bOverWater) const
{
	const FState& S = State();
	LyudWeather::FWindState W;
	FillWindState(S, W);
	const double XM = Location.X * 0.01, YM = Location.Y * 0.01;
	const LyudWeather::FGust G = LyudWeather::GustAt(W, XM, YM, DriftX, DriftY, GustClock);
	const FVector Dir = S.Mean.GetSafeNormal2D(UE_SMALL_NUMBER, FVector(0.0, 1.0, 0.0)).RotateAngleAxisRad(G.Veer, FVector::UpVector);
	FLyudWindSample Out;
	Out.Gust = G.Factor;
	Out.Speed = S.Speed * G.Factor * LyudWeather::HeightFactor(HeightAboveGroundM, bOverWater);
	if (CVarWindRelief.GetValueOnGameThread() != 0)
	{
		if (const LyudWeather::FWindTerrain* T = ReadyRelief())
		{
			// what the land does to the speed at that height over this very ground: the lee of the cliff, of the forest
			// and of houses, the squeeze over a crest. The height's profile and the water's freer air stay the caller's.
			const float Water = WaterZM();
			const double Surface = FMath::Max(T->GroundAt(XM, YM), Water);
			const LyudWeather::FLocalWind L = LyudWeather::LocalWindAt(W, T, XM, YM, Surface + FMath::Max(HeightAboveGroundM, 0.3f), Water,
				DriftX, DriftY, GustClock);
			Out.Speed *= L.Exposure;
		}
	}
	Out.Velocity = Dir * Out.Speed;
	return Out;
}

void ULyudWindSubsystem::GetWindsAt(TConstArrayView<FVector> Locations, float HeightAboveGroundM, bool bOverWater, TArray<FLyudWindSample>& Out) const
{
	Out.SetNum(Locations.Num(), EAllowShrinking::No);
	for (int32 I = 0; I < Locations.Num(); ++I)
	{
		Out[I] = GetWindAt(Locations[I], HeightAboveGroundM, bOverWater);
	}
}

FLyudLocalWind ULyudWindSubsystem::LocalAt(const LyudWeather::FWindState& W, double XM, double YM, double ZM, float HeightM) const
{
	const float Water = WaterZM();
	if (const LyudWeather::FWindTerrain* T = ReadyRelief())
	{
		double Z = ZM;
		if (HeightM >= 0.f)
		{
			// a thing of a known height stands on its own ground (a deck, a roof) or, where the coarse grid's ground
			// comes out higher than that (a steep bank), on the grid's
			Z = FMath::Max(ZM, double(FMath::Max(T->GroundAt(XM, YM), Water))) + HeightM;
		}
		return ToLocalWind(LyudWeather::LocalWindAt(W, T, XM, YM, Z, Water, DriftX, DriftY, GustClock));
	}
	// the land is not read yet: open flat land (the model's ground is then at Z = 0), at the height given or the standard 10 m
	return ToLocalWind(LyudWeather::LocalWindAt(W, nullptr, XM, YM, HeightM >= 0.f ? FMath::Max(HeightM, 0.3f) : 10.0, Water, DriftX, DriftY, GustClock));
}

FLyudLocalWind ULyudWindSubsystem::GetLocalWind(const FVector& Location) const
{
	LyudWeather::FWindState W;
	FillWindState(State(), W);
	return LocalAt(W, Location.X * 0.01, Location.Y * 0.01, Location.Z * 0.01, -1.f);
}

FLyudLocalWind ULyudWindSubsystem::GetLocalWindAbove(const FVector& GroundLocation, float HeightM) const
{
	LyudWeather::FWindState W;
	FillWindState(State(), W);
	return LocalAt(W, GroundLocation.X * 0.01, GroundLocation.Y * 0.01, GroundLocation.Z * 0.01, FMath::Max(HeightM, 0.f));
}

void ULyudWindSubsystem::GetLocalWinds(TConstArrayView<FVector> Locations, TArray<FLyudLocalWind>& Out) const
{
	Out.SetNum(Locations.Num(), EAllowShrinking::No);
	LyudWeather::FWindState W;
	FillWindState(State(), W);
	for (int32 I = 0; I < Locations.Num(); ++I)
	{
		const FVector& L = Locations[I];
		Out[I] = LocalAt(W, L.X * 0.01, L.Y * 0.01, L.Z * 0.01, -1.f);
	}
}

FVector ULyudWindSubsystem::GetApparentWind(const FVector& Location, const FVector& BodyVelocityCmS) const
{
	return GetLocalWind(Location).Velocity - BodyVelocityCmS * 0.01;
}

FLyudWindShift ULyudWindSubsystem::GetShift() const { return State().Shift; }
float ULyudWindSubsystem::GetSteadyFromDegrees() const { return State().SteadyFromDeg; }

float ULyudWindSubsystem::GetGustAhead(const FVector& Location, float Seconds) const
{
	LyudWeather::FWindState W;
	FillWindState(State(), W);
	return LyudWeather::GustAhead(W, Location.X * 0.01, Location.Y * 0.01, DriftX, DriftY, GustClock, FMath::Max(Seconds, 0.f));
}

bool ULyudWindSubsystem::GetNextGust(const FVector& Location, float WithinSeconds, float& OutInSeconds, float& OutFactor, bool& bOutLull) const
{
	LyudWeather::FWindState W;
	FillWindState(State(), W);
	const LyudWeather::FGustEvent E = LyudWeather::NextGust(W, Location.X * 0.01, Location.Y * 0.01, DriftX, DriftY, GustClock,
		FMath::Clamp(WithinSeconds, 1.f, 120.f));
	OutInSeconds = E.InSeconds;
	OutFactor = E.Factor;
	bOutLull = E.bLull;
	return E.InSeconds >= 0.f;
}

int32 ULyudWindSubsystem::GetBeaufort() const { return BeaufortOf(State().Speed); }

int32 ULyudWindSubsystem::BeaufortOf(float SpeedMs)
{
	return FMath::Clamp(LyudWeather::Beaufort(FMath::Max(SpeedMs, 0.f)), 0, 12);
}

FString ULyudWindSubsystem::BeaufortKey(int32 Force)
{
	return FString::Printf(TEXT("wind.force.%d"), FMath::Clamp(Force, 0, 12));
}

FString ULyudWindSubsystem::CompassKey(float FromDegrees)
{
	static const TCHAR* const Points[8] = { TEXT("n"), TEXT("ne"), TEXT("e"), TEXT("se"), TEXT("s"), TEXT("sw"), TEXT("w"), TEXT("nw") };
	const int32 I = ((FMath::RoundToInt(WrapWindDeg(FromDegrees) / 45.f) % 8) + 8) % 8;
	return FString::Printf(TEXT("wind.from.%s"), Points[I]);
}

FText ULyudWindSubsystem::DescribeWind() const
{
	const FState& S = State();
	const int32 Force = BeaufortOf(S.Speed);
	const int32 GustForce = BeaufortOf(GustSpeedOf(S.Speed, S.Gustiness));
	if (Force == 0 && GustForce <= 1)
	{
		return LyudText::Get(TEXT("wind.describe.calm"));
	}
	const FString From = LyudText::Get(CompassKey(S.FromDeg)).ToString();
	const FString Name = LyudText::Get(BeaufortKey(Force)).ToString();
	if (GustForce <= Force)
	{
		return LyudText::Get(TEXT("wind.describe.steady"), { FStringFormatArg(From), FStringFormatArg(Force), FStringFormatArg(Name) });
	}
	return LyudText::Get(TEXT("wind.describe"), { FStringFormatArg(From), FStringFormatArg(Force), FStringFormatArg(Name), FStringFormatArg(GustForce) });
}

FLyudWindOutlook ULyudWindSubsystem::GetOutlook(float SpanHours) const
{
	const FState& S = State();
	const LyudWeather::FWindOutlook O = LyudWeather::Outlook(NowHours(), FMath::Clamp(double(SpanHours), 0.25, 48.0), ToParams(Settings));
	FLyudWindOutlook Out;
	Out.MinSpeed = O.MinSpeed;
	Out.MaxSpeed = O.MaxSpeed;
	Out.MeanSpeed = O.MeanSpeed;
	Out.GustSpeed = O.GustSpeed;
	Out.FromDegrees = O.FromDeg;
	Out.TurnDegrees = O.TurnDeg;
	Out.Trend = O.Trend;
	Out.Force = O.Force;
	Out.ForceMax = O.ForceMax;
	// a forced wind holds for as long as it is forced
	if (CVarWindSpeed.GetValueOnGameThread() >= 0.f)
	{
		Out.MinSpeed = Out.MaxSpeed = Out.MeanSpeed = S.SteadySpeed;
		Out.GustSpeed = GustSpeedOf(S.SteadySpeed, S.Gustiness);
		Out.Trend = 0.f;
		Out.Force = BeaufortOf(Out.MeanSpeed);
		Out.ForceMax = BeaufortOf(Out.GustSpeed);
	}
	if (CVarWindFrom.GetValueOnGameThread() >= 0.f)
	{
		Out.FromDegrees = S.SteadyFromDeg;
		Out.TurnDegrees = 0.f;
	}
	return Out;
}

float ULyudWindSubsystem::HoursUntilWind(float MinSpeed, float MaxSpeed, float HoldHours, bool bDaylight) const
{
	const float Forced = CVarWindSpeed.GetValueOnGameThread();
	if (Forced >= 0.f)
	{
		return Forced >= MinSpeed && Forced <= MaxSpeed ? 0.f : -1.f;
	}
	return float(LyudWeather::NextSpell(NowHours(), ToParams(Settings), MinSpeed, MaxSpeed, FMath::Max(double(HoldHours), 0.0), 48.0, bDaylight));
}

FVector ULyudWindSubsystem::GetMeanWind() const { return State().Mean; }
float ULyudWindSubsystem::GetMeanSpeed() const { return State().Speed; }
float ULyudWindSubsystem::GetFromDegrees() const { return State().FromDeg; }
float ULyudWindSubsystem::GetGustiness() const { return State().Gustiness; }
float ULyudWindSubsystem::GetDayStir() const { return State().Day; }
FVector ULyudWindSubsystem::GetSeaStateWind() const { return State().Sea; }

void ULyudWindSubsystem::RebuildRelief()
{
	Relief.Reset();
	bReliefReady = false;
	bReliefStarted = false;
	ReliefNext = 0;
	ReliefSpentMs = 0.0;
	ReliefWait = ReliefDelayS;
}

void ULyudWindSubsystem::StepRelief(float DeltaTime, float BudgetMs)
{
	UWorld* World = GetWorld();
	if (bReliefReady || !World || !World->IsGameWorld() || !World->HasBegunPlay())
	{
		return;
	}
	const ALyudSea* SeaActor = SeaRef.Get();
	const double Datum = SeaActor ? SeaActor->GetActorLocation().Z : double(WindDatumCm);
	if (!bReliefStarted)
	{
		ReliefWait += DeltaTime;
		if (ReliefWait < ReliefDelayS)
		{
			return;
		}
		FBox Box(ForceInit);
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			Box += It->GetComponentsBoundingBox(true);
		}
		if (!Box.IsValid)
		{
			// no ground yet (a level still streaming in, a room with no outdoors): look again in a couple of seconds
			ReliefWait = ReliefDelayS - 2.f;
			return;
		}
		Relief = MakePimpl<LyudWeather::FWindTerrain>();
		LyudWeather::FWindTerrain& T = *Relief.Get();
		const double SizeX = (Box.Max.X - Box.Min.X) * 0.01 + 2.0 * ReliefMarginM;
		const double SizeY = (Box.Max.Y - Box.Min.Y) * 0.01 + 2.0 * ReliefMarginM;
		// a bigger map gets bigger cells: the grid stays two small float arrays
		T.Cell = ReliefCellM;
		while ((SizeX / T.Cell + 1.0) * (SizeY / T.Cell + 1.0) > double(ReliefMaxCells))
		{
			T.Cell *= 1.25;
		}
		T.NX = FMath::Max(2, FMath::CeilToInt(float(SizeX / T.Cell)));
		T.NY = FMath::Max(2, FMath::CeilToInt(float(SizeY / T.Cell)));
		T.X0 = Box.Min.X * 0.01 - ReliefMarginM + T.Cell * 0.5;
		T.Y0 = Box.Min.Y * 0.01 - ReliefMarginM + T.Cell * 0.5;
		T.SeaZ = float(Datum * 0.01);
		T.Ground.assign(size_t(T.NX) * size_t(T.NY), T.SeaZ - 10.f);
		T.Cover.assign(size_t(T.NX) * size_t(T.NY), 0.f);
		ReliefTopZ = Box.Max.Z + 8000.0;
		ReliefNext = 0;
		ReliefSpentMs = 0.0;
		bReliefStarted = true;
	}
	if (!Relief.IsValid())
	{
		return;
	}
	LyudWeather::FWindTerrain& T = *Relief.Get();
	const int32 Total = T.NX * T.NY;
	const double StartTime = FPlatformTime::Seconds();
	const double EndTime = StartTime + FMath::Max(BudgetMs, 0.f) * 0.001;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LyudWindRelief), false);
	// the sea's surface and its water volume are not ground
	if (SeaActor)
	{
		Params.AddIgnoredActor(SeaActor);
		if (const AActor* Volume = SeaActor->GetWaterVolume())
		{
			Params.AddIgnoredActor(Volume);
		}
	}
	const ULyudTerrainMask* Mask = ULyudTerrainMask::Get();
	const bool bForest = Mask && Mask->HasForestFloor();
	const double BottomZ = Datum - 8000.0;
	const double Quarter = T.Cell * 25.0;   // a quarter of a cell, cm
	int32 Done = 0;
	while (ReliefNext < Total)
	{
		const int32 IX = ReliefNext % T.NX, IY = ReliefNext / T.NX;
		const double CX = (T.X0 + IX * T.Cell) * 100.0, CY = (T.Y0 + IY * T.Cell) * 100.0;
		float Ground = 0.f, Top = 0.f;
		bool bGround = false;
		float GroundM = T.SeaZ - 10.f, CoverM = 0.f;
		if (TraceWindColumn(World, CX - Quarter, CY - Quarter, ReliefTopZ, BottomZ, Params, Ground, Top, bGround))
		{
			// what stands there is measured from the ground, or from the datum where the ground is the sea's bed (the pier)
			const float Base = bGround ? FMath::Max(Ground, float(Datum)) : float(Datum);
			float Cover = FMath::Max(Top - Base, 0.f);
			if (bGround)
			{
				if (Ground > Datum - 200.0)
				{
					// land: a second column across the cell, since a house rarely stands on one point of it; a thin thing
					// (a pylon, a lamp) caught by one of the two then counts for half
					float Ground2 = 0.f, Top2 = 0.f;
					bool bGround2 = false;
					if (TraceWindColumn(World, CX + Quarter, CY + Quarter, ReliefTopZ, BottomZ, Params, Ground2, Top2, bGround2) && bGround2)
					{
						Cover = 0.5f * (Cover + FMath::Max(Top2 - FMath::Max(Ground2, float(Datum)), 0.f));
						Ground = 0.5f * (Ground + Ground2);
					}
				}
				GroundM = Ground * 0.01f;
			}
			CoverM = Cover * 0.01f;
		}
		if (bForest && bGround && Ground > Datum)
		{
			CoverM = FMath::Max(CoverM, Mask->GetForestFloor(FVector(CX, CY, 0.0)) * ForestCrownM);
		}
		// kerbs, benches and boulders are the ground's own roughness, not a lee
		T.Ground[size_t(ReliefNext)] = GroundM;
		T.Cover[size_t(ReliefNext)] = CoverM < 1.f ? 0.f : FMath::Min(CoverM, 45.f);
		ReliefNext++;
		if ((++Done & 7) == 0 && FPlatformTime::Seconds() > EndTime)
		{
			break;
		}
	}
	ReliefSpentMs += (FPlatformTime::Seconds() - StartTime) * 1000.0;
	if (ReliefNext >= Total)
	{
		bReliefReady = true;
		UE_LOG(LogTemp, Log, TEXT("LyudWind: relief grid %dx%d of %.0f m cells read (%.0f ms of traces in all, %.1f ms a frame at most)"),
			T.NX, T.NY, T.Cell, ReliefSpentMs, BudgetMs);
	}
}

void ULyudWindSubsystem::DrawDebugArrows() const
{
	const UWorld* World = GetWorld();
	FVector Cam;
	if (!WindCameraAt(World, Cam))
	{
		return;
	}
	LyudWeather::FWindState W;
	FillWindState(State(), W);
	const LyudWeather::FWindTerrain* T = ReadyRelief();
	const float Water = WaterZM();
	constexpr int32 Half = 5;
	constexpr double StepM = 12.0;
	// the arrows stand on the grid's own lattice so that they do not swim as the camera moves
	const double BaseX = FMath::RoundToDouble(Cam.X * 0.01 / StepM) * StepM, BaseY = FMath::RoundToDouble(Cam.Y * 0.01 / StepM) * StepM;
	for (int32 IY = -Half; IY <= Half; ++IY)
	{
		for (int32 IX = -Half; IX <= Half; ++IX)
		{
			const double XM = BaseX + IX * StepM, YM = BaseY + IY * StepM;
			// 2 m over the ground or the water: where a person, a kite's first metres and a dinghy's sail are
			const double SurfaceM = T ? double(FMath::Max(T->GroundAt(XM, YM), Water)) : Cam.Z * 0.01 - 3.5;
			const LyudWeather::FLocalWind L = LyudWeather::LocalWindAt(W, T, XM, YM, T ? SurfaceM + 2.0 : 2.0, Water, DriftX, DriftY, GustClock);
			const FVector At(XM * 100.0, YM * 100.0, (SurfaceM + 2.0) * 100.0);
			// red in a dead lee, yellow half sheltered, green open, cyan where the land speeds the wind up
			const FColor Colour = L.Exposure > 1.08f ? FColor::Cyan
				: FLinearColor::LerpUsingHSV(FLinearColor::Red, FLinearColor::Green, FMath::Clamp(L.Exposure, 0.f, 1.f)).ToFColor(true);
			const FVector Along = FVector(L.VX, L.VY, 0.0) * 40.0;   // 40 cm of arrow per m/s
			const FVector Tip = At + Along;
			const FVector Side = FVector(-Along.Y, Along.X, 0.0) * 0.18;
			DrawDebugLine(World, At, Tip, Colour, false, -1.f, 0, 2.f);
			DrawDebugLine(World, Tip, Tip - Along * 0.3 + Side, Colour, false, -1.f, 0, 2.f);
			DrawDebugLine(World, Tip, Tip - Along * 0.3 - Side, Colour, false, -1.f, 0, 2.f);
			if (FMath::Abs(L.Updraft) > 0.15f)
			{
				// rising air a white tick up, sinking air a magenta tick down, a metre per m/s
				DrawDebugLine(World, At, At + FVector(0.0, 0.0, L.Updraft * 100.0), L.Updraft > 0.f ? FColor::White : FColor::Magenta, false, -1.f, 0, 3.f);
			}
		}
	}
}

void ULyudWindSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// the sea is looked for now and then, never per query (ALyudSea::Get walks the actors while there is none)
	if (!SeaRef.IsValid())
	{
		SeaLookAge += DeltaTime;
		if (SeaLookAge > 2.f)
		{
			SeaLookAge = 0.f;
			SeaRef = ALyudSea::Get(this);
		}
	}
	const FState& S = State();
	// the air drifts downwind with the mean wind; the gusts ride on it
	const double Period = LyudWeather::GustPeriod;
	DriftX = FMath::Fmod(DriftX + S.Mean.X * DeltaTime + Period, Period);
	DriftY = FMath::Fmod(DriftY + S.Mean.Y * DeltaTime + Period, Period);
	GustClock = FMath::Fmod(GustClock + DeltaTime, 10240.0);

	StepRelief(DeltaTime, CVarWindReliefBudget.GetValueOnGameThread());

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
	const int32 Debug = CVarWindDebug.GetValueOnGameThread();
	if (Debug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(0x5157494E, 0.f, FColor::Cyan, DebugReport());
	}
	if (Debug >= 2)
	{
		DrawDebugArrows();
	}
}

FString ULyudWindSubsystem::DebugReport() const
{
	const FState& S = State();
	const FVector SeaWind = S.Sea;
	return FString::Printf(TEXT("wind %s: from %.0f deg %.1f m/s (force %d), gusts %.2f, day %.2f; swing %+.1f of %.0f deg x%.2f, a swing %.0f min, turns in %.1f min (steady from %.0f deg); sea state %.1f m/s from %.0f deg; drift %.0f,%.0f m; relief %s"),
		*WindTime().ToString(TEXT("%Y-%m-%d %H:%M")), S.FromDeg, S.Speed, BeaufortOf(S.Speed), S.Gustiness, S.Day,
		S.Shift.VeerDegrees, S.Shift.RangeDegrees, S.Shift.Factor, S.Shift.PeriodMinutes, S.Shift.NextTurnMinutes, S.SteadyFromDeg, SeaWind.Size(),
		FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(SeaWind.X, -SeaWind.Y)) + 360.0, 360.0), DriftX, DriftY,
		bReliefReady ? TEXT("ready") : (bReliefStarted ? TEXT("being read") : TEXT("not read")));
}
