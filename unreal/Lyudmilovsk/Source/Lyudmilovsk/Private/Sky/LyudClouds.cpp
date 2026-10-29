#include "Sky/LyudClouds.h"

#include "Components/VolumetricCloudComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sky/LyudSky.h"
#include "Weather/LyudWeather.h"

static TAutoConsoleVariable<FString> CVarCloudPreset(TEXT("lyud.Clouds.Preset"), TEXT(""),
	TEXT("Forces a cloud sky: clear, cu, cucon, cb, sc, st, ns, ac, as, lens, ci, cc, cs, warmfront, coldfront, showers (empty or off = the weather's)."));
static TAutoConsoleVariable<float> CVarCloudCover(TEXT("lyud.Clouds.Cover"), 1.f, TEXT("Multiplies every cloud layer's cover (tests)."));
static TAutoConsoleVariable<float> CVarCloudDrift(TEXT("lyud.Clouds.Drift"), 1.f, TEXT("Multiplies how fast the clouds drift with their winds."));
static TAutoConsoleVariable<int32> CVarCloudDebug(TEXT("lyud.Clouds.Debug"), 0, TEXT("1 prints the cloud layers on screen and in the log every few seconds."));

namespace
{
	/** Seconds the shapes take to ease to a new genus or cover (about 63 percent of the way). */
	constexpr float EaseSeconds = 6.f;

	FVector4f Ease(const FVector4f& From, const FVector4f& To, float K)
	{
		return From + (To - From) * K;
	}

	/** A compass bearing the wind comes from -> the world unit vector it blows toward (north = +Y, east = -X). */
	FVector2D Downwind(float FromDeg)
	{
		const float To = FMath::DegreesToRadians(FromDeg + 180.f);
		return FVector2D(-FMath::Sin(To), FMath::Cos(To));
	}

	bool IsHigh(ELyudCloudType G)
	{
		return G == ELyudCloudType::Cirrus || G == ELyudCloudType::Cirrocumulus || G == ELyudCloudType::Cirrostratus;
	}

	FLyudCloudLayer Layer(ELyudCloudType G, float Cover, float BaseM, float TopM, float Density)
	{
		FLyudCloudLayer L;
		L.Type = G;
		L.Cover = Cover;
		L.BaseM = BaseM;
		L.TopM = TopM;
		L.Density = Density;
		return L;
	}
}

ULyudCloudSubsystem* ULyudCloudSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudCloudSubsystem>() : nullptr;
}

void ULyudCloudSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<ULyudWeatherSubsystem>();
}

bool ULyudCloudSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

TStatId ULyudCloudSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudCloudSubsystem, STATGROUP_Tickables);
}

void ULyudCloudSubsystem::SetOverride(const FLyudCloudSky& Sky)
{
	Override = Sky;
	bOverride = true;
}

void ULyudCloudSubsystem::ClearOverride()
{
	bOverride = false;
}

bool ULyudCloudSubsystem::SetPreset(const FString& Name)
{
	FLyudCloudSky S;
	if (!PresetSky(Name, S))
	{
		return false;
	}
	SetOverride(S);
	return true;
}

bool ULyudCloudSubsystem::PresetSky(const FString& InName, FLyudCloudSky& S)
{
	using G = ELyudCloudType;
	const FString Name = InName.ToLower();
	S = FLyudCloudSky();
	if (Name == TEXT("clear"))
	{
		S.High = Layer(G::Cirrus, 0.06f, 8800.f, 9400.f, 0.5f);
	}
	else if (Name == TEXT("cu"))
	{
		S.Low = Layer(G::Cumulus, 0.35f, 1100.f, 2300.f, 0.9f);
		S.High = Layer(G::Cirrus, 0.15f, 8800.f, 9400.f, 0.6f);
	}
	else if (Name == TEXT("cucon"))
	{
		S.Low = Layer(G::CumulusCongestus, 0.4f, 900.f, 5200.f, 1.f);
	}
	else if (Name == TEXT("cb"))
	{
		S.Low = Layer(G::Cumulonimbus, 0.45f, 900.f, 9500.f, 1.f);
		S.Mid = Layer(G::Altocumulus, 0.15f, 3500.f, 4000.f, 0.6f);
		S.ShowerCells = 0.5f;
		S.PrecipRate = 8.f;
		S.Thunder = 0.6f;
	}
	else if (Name == TEXT("sc"))
	{
		S.Low = Layer(G::Stratocumulus, 0.75f, 800.f, 1500.f, 0.85f);
	}
	else if (Name == TEXT("st"))
	{
		S.Low = Layer(G::Stratus, 0.98f, 250.f, 700.f, 0.8f);
	}
	else if (Name == TEXT("ns"))
	{
		S.Low = Layer(G::Nimbostratus, 1.f, 500.f, 4000.f, 1.f);
		S.PrecipRate = 3.f;
	}
	else if (Name == TEXT("ac"))
	{
		S.Mid = Layer(G::Altocumulus, 0.55f, 3200.f, 3800.f, 0.7f);
		S.High = Layer(G::Cirrus, 0.1f, 8800.f, 9400.f, 0.5f);
	}
	else if (Name == TEXT("as"))
	{
		S.Mid = Layer(G::Altostratus, 0.9f, 3000.f, 5000.f, 0.45f);
	}
	else if (Name == TEXT("lens"))
	{
		S.Mid = Layer(G::AltocumulusLenticular, 0.f, 3500.f, 4600.f, 0.8f);
		S.Lenticular = 0.6f;
		S.Low = Layer(G::Cumulus, 0.08f, 1200.f, 1900.f, 0.8f);
		S.UpperSpeed = 30.f;
	}
	else if (Name == TEXT("ci"))
	{
		S.High = Layer(G::Cirrus, 0.5f, 8500.f, 9500.f, 0.8f);
	}
	else if (Name == TEXT("cc"))
	{
		S.High = Layer(G::Cirrocumulus, 0.5f, 7500.f, 8000.f, 0.8f);
	}
	else if (Name == TEXT("cs"))
	{
		S.High = Layer(G::Cirrostratus, 0.9f, 8000.f, 9500.f, 0.7f);
		S.Halo = 0.8f;
	}
	else if (Name == TEXT("warmfront"))
	{
		S.High = Layer(G::Cirrostratus, 0.8f, 8000.f, 9500.f, 0.6f);
		S.Mid = Layer(G::Altostratus, 0.6f, 3000.f, 5500.f, 0.5f);
		S.Low = Layer(G::Stratus, 0.2f, 600.f, 900.f, 0.7f);
		S.Halo = 0.4f;
	}
	else if (Name == TEXT("coldfront"))
	{
		S.Low = Layer(G::Cumulonimbus, 0.75f, 700.f, 8500.f, 1.f);
		S.Mid = Layer(G::Altostratus, 0.4f, 3500.f, 5000.f, 0.6f);
		S.ShowerCells = 0.7f;
		S.PrecipRate = 10.f;
		S.Thunder = 0.4f;
		S.WindSpeed = 14.f;
		S.WindFromDeg = 290.f;
	}
	else if (Name == TEXT("showers"))
	{
		S.Low = Layer(G::CumulusCongestus, 0.4f, 800.f, 4500.f, 1.f);
		S.Mid = Layer(G::Altocumulus, 0.15f, 3600.f, 4100.f, 0.6f);
		S.ShowerCells = 0.3f;
		S.PrecipRate = 2.f;
	}
	else
	{
		return false;
	}
	return true;
}

FLyudCloudSky ULyudCloudSubsystem::FromWeather() const
{
	FLyudCloudSky S;
	const ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(GetWorld());
	if (!W)
	{
		return S;
	}
	const FLyudWeatherState St = W->GetState();
	S.Low = St.Low;
	S.Mid = St.Mid;
	S.High = St.High;
	S.Lenticular = St.Lenticular;
	S.ShowerCells = St.ShowerCells;
	S.WindFromDeg = St.WindFromDeg;
	S.WindSpeed = St.WindSpeed;
	S.UpperFromDeg = St.UpperFromDeg;
	S.UpperSpeed = St.UpperSpeed;
	S.PrecipRate = St.PrecipRate;
	S.Thunder = St.Thunder;
	S.Halo = St.Halo;
	return S;
}

ULyudCloudSubsystem::FShape ULyudCloudSubsystem::ShapeOf(const FLyudCloudLayer& L, float PrecipRate, float ShowerCells)
{
	using G = ELyudCloudType;
	FShape S;
	// cell km, sheet, top rounding, anvil | erosion, stretch, ragged base km, cluster | darkness, boil
	struct FRow { float Cell, Sheet, Round, Anvil, Erosion, Stretch, Ragged, Cluster, Dark, Boil; };
	FRow R = { 1.5f, 0.f, 0.6f, 0.f, 0.5f, 1.3f, 0.05f, 0.f, 0.3f, 0.6f };
	float Shafts = 0.f;
	switch (L.Type)
	{
	case G::Cumulus:          R = { 1.4f, 0.f, 0.85f, 0.f, 0.75f, 1.4f, 0.05f, 0.35f, 0.3f, 1.f }; break;
	case G::CumulusCongestus: R = { 2.6f, 0.f, 0.55f, 0.f, 0.7f, 1.2f, 0.1f, 0.6f, 0.5f, 1.4f }; Shafts = 0.5f; break;
	case G::Cumulonimbus:     R = { 7.f, 0.f, 0.35f, 1.f, 0.65f, 1.f, 0.35f, 0.9f, 0.8f, 1.2f }; Shafts = 1.f; break;
	case G::Stratocumulus:    R = { 1.8f, 0.35f, 0.6f, 0.f, 0.45f, 2.2f, 0.08f, 0.f, 0.45f, 0.6f }; break;
	case G::Stratus:          R = { 4.f, 0.92f, 0.2f, 0.f, 0.15f, 1.5f, 0.08f, 0.f, 0.4f, 0.3f }; Shafts = 0.3f; break;
	case G::Nimbostratus:     R = { 5.f, 0.85f, 0.2f, 0.f, 0.25f, 1.3f, 0.45f, 0.f, 0.85f, 0.5f }; Shafts = 1.f; break;
	case G::Altocumulus:      R = { 0.7f, 0.15f, 0.6f, 0.f, 0.6f, 1.6f, 0.02f, 0.05f, 0.2f, 0.6f }; break;
	case G::AltocumulusLenticular: R = { 0.9f, 0.1f, 0.7f, 0.f, 0.3f, 2.f, 0.02f, 0.2f, 0.2f, 0.2f }; break;
	case G::Altostratus:      R = { 6.f, 0.95f, 0.1f, 0.f, 0.12f, 1.5f, 0.05f, 0.f, 0.35f, 0.3f }; Shafts = 0.4f; break;
	default: break;
	}
	const bool bDraw = L.Type != G::None && !IsHigh(L.Type) && L.Cover > 0.f;
	const float Cover = bDraw ? L.Cover : 0.f;
	// rain shafts only where something falls: light rain greys the air under the base, a downpour hangs curtains
	Shafts *= FMath::Clamp(PrecipRate / 4.f, 0.f, 1.f);
	// shower cells make convective layers clumpier
	const float Cluster = FMath::Clamp(R.Cluster + (L.Type >= G::Cumulus ? ShowerCells * 0.5f : 0.f), 0.f, 1.f);
	S.A = FVector4f(Cover, L.BaseM * 1e-3f, FMath::Max(L.TopM, L.BaseM + 50.f) * 1e-3f, FMath::Clamp(L.Density, 0.f, 1.f));
	S.B = FVector4f(R.Cell, R.Sheet, R.Round, R.Anvil);
	S.C = FVector4f(R.Erosion, R.Stretch, R.Ragged, Cluster);
	S.E = FVector4f(Shafts, R.Dark, R.Boil, 0.f);
	return S;
}

ALyudSky* ULyudCloudSubsystem::FindSky()
{
	if (ALyudSky* Sky = SkyActor.Get())
	{
		return Sky;
	}
	if (UWorld* World = GetWorld())
	{
		TActorIterator<ALyudSky> It(World);
		if (It)
		{
			SkyActor = *It;
			bSnap = true;
			return *It;
		}
	}
	return nullptr;
}

void ULyudCloudSubsystem::OnStrike(FVector Location, float DistanceM)
{
	const float LowBase = Shape[0].A.Y, LowTop = Shape[0].A.Z;
	FlashAt = FVector4f(float(Location.X * 1e-5), float(Location.Y * 1e-5), FMath::Lerp(LowBase, LowTop, 0.3f), 3.f);
	FlashLeft = 0.35f;
}

void ULyudCloudSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ALyudSky* Sky = FindSky();
	if (!Sky)
	{
		return;
	}
	if (!bBound)
	{
		if (ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(GetWorld()))
		{
			W->OnLightning.AddUniqueDynamic(this, &ULyudCloudSubsystem::OnStrike);
			bBound = true;
		}
	}
	const FString Preset = CVarCloudPreset.GetValueOnGameThread();
	if (Preset != LastPreset)
	{
		// a forced sky shows at once (tests, the console)
		LastPreset = Preset;
		bSnap = true;
	}
	FLyudCloudSky Forced;
	if (!Preset.IsEmpty() && Preset != TEXT("off") && PresetSky(Preset, Forced))
	{
		Target = Forced;
	}
	else
	{
		Target = bOverride ? Override : FromWeather();
	}
	Apply(Sky, DeltaTime);
}

void ULyudCloudSubsystem::Apply(ALyudSky* Sky, float Dt)
{
	using G = ELyudCloudType;
	const float CoverK = FMath::Max(CVarCloudCover.GetValueOnGameThread(), 0.f);
	FLyudCloudLayer Low = Target.Low, Mid = Target.Mid, High = Target.High;
	Low.Cover *= CoverK;
	Mid.Cover *= CoverK;
	High.Cover *= CoverK;
	// a high genus in a lower slot (or the other way round) is still drawn where it belongs
	if (IsHigh(Low.Type)) { High = Low; Low = FLyudCloudLayer(); }
	if (IsHigh(Mid.Type)) { High = Mid; Mid = FLyudCloudLayer(); }

	const float K = bSnap ? 1.f : 1.f - FMath::Exp(-Dt / EaseSeconds);
	const FLyudCloudLayer* Vol[2] = { &Low, &Mid };
	for (int32 I = 0; I < 2; ++I)
	{
		const FShape T = ShapeOf(*Vol[I], Target.PrecipRate, Target.ShowerCells);
		FShape& S = Shape[I];
		// a layer that comes in from nothing takes its new shape at once and only grows its cover
		const bool bFresh = S.A.X < 0.01f;
		const float KS = bFresh ? 1.f : K;
		const float Cover = S.A.X + (T.A.X - S.A.X) * K;
		S.A = Ease(S.A, T.A, KS);
		S.A.X = Cover;
		S.B = Ease(S.B, T.B, KS);
		S.C = Ease(S.C, T.C, KS);
		S.E = Ease(S.E, T.E, KS);
	}

	// the high layer on the dome: fibres (Ci), grain (Cc), veil (Cs) and the halo in a veil
	FVector4f HA(0.f), HB(0.f);
	if (IsHigh(High.Type) && High.Cover > 0.f)
	{
		HA = FVector4f(High.Cover, (High.BaseM + High.TopM) * 0.5e-3f, FMath::Clamp(High.Density, 0.f, 1.f), 0.f);
		const bool bCi = High.Type == G::Cirrus, bCc = High.Type == G::Cirrocumulus, bCs = High.Type == G::Cirrostratus;
		HB = FVector4f(bCi ? 1.f : (bCs ? 0.35f : 0.f), bCc ? 1.f : 0.f, bCs ? 1.f : 0.f, FMath::Clamp(Target.Halo, 0.f, 1.f));
	}
	if (HighA.X < 0.01f || bSnap)
	{
		HighB = HB;
		HighA = FVector4f(HighA.X, HA.Y, HA.Z, 0.f);
	}
	const float HighCover = HighA.X + (HA.X - HighA.X) * K;
	HighA = Ease(HighA, HA, K);
	HighA.X = HighCover;
	HighB = Ease(HighB, HB, K);

	const bool bLens = Target.Lenticular > 0.f || Mid.Type == G::AltocumulusLenticular;
	const FVector4f LT = bLens ? FVector4f(FMath::Clamp(FMath::Max(Target.Lenticular, Mid.Type == G::AltocumulusLenticular ? Mid.Cover : 0.f), 0.f, 1.f),
		Mid.BaseM > 0.f ? Mid.BaseM * 1e-3f : 3.5f, Mid.TopM > 0.f ? Mid.TopM * 1e-3f : 4.5f, 6.f) : FVector4f(0.f, LensA.Y, LensA.Z, 6.f);
	LensA = (LensA.X < 0.01f || bSnap) ? FVector4f(LensA.X + (LT.X - LensA.X) * K, LT.Y, LT.Z, LT.W) : Ease(LensA, LT, K);
	bSnap = false;

	// drift: the low clouds with the wind a kilometre up (faster than at the ground and veered), the middle between
	// it and the upper wind, the high clouds with the upper wind
	const float DriftK = CVarCloudDrift.GetValueOnGameThread() * 1e-3f * Dt;
	const FVector2D DLow = Downwind(Target.WindFromDeg + 15.f), DUp = Downwind(Target.UpperFromDeg);
	const float SLow = Target.WindSpeed * 1.6f + 2.f;
	const FVector2D VLow = DLow * SLow, VUp = DUp * Target.UpperSpeed;
	const FVector2D VMid = FMath::Lerp(VLow, VUp, 0.6f);
	Drift[0] -= VLow * DriftK;
	Drift[1] -= VMid * DriftK;
	Drift[2] -= VUp * DriftK;
	for (FVector2D& D : Drift)
	{
		// keep the offsets small enough for float precision in the shader (the noise tiles every few hundred km anyway)
		D.X = FMath::Fmod(D.X, 4096.0);
		D.Y = FMath::Fmod(D.Y, 4096.0);
	}
	const FVector2D DMid = VMid.IsNearlyZero() ? DUp : VMid.GetSafeNormal();
	FlashLeft = FMath::Max(0.f, FlashLeft - Dt);
	const float Flicker = FlashLeft > 0.f ? (FMath::Frac(FlashLeft * 23.f) > 0.4f ? 1.f : 0.3f) : 0.f;

	// the volumetric slab, fitted round what is drawn (the shader keeps everything at its own altitude, so changing it
	// only clips; a tight slab spends the ray-march samples where the clouds are)
	float Bottom = 1e9f, Top = 0.f;
	for (int32 I = 0; I < 2; ++I)
	{
		const FShape& S = Shape[I];
		if (S.A.X > 0.005f)
		{
			Bottom = FMath::Min(Bottom, S.E.X > 0.f ? 0.f : S.A.Y - S.C.Z - 0.05f);
			Top = FMath::Max(Top, S.A.Z + 0.05f);
		}
	}
	if (LensA.X > 0.005f)
	{
		Bottom = FMath::Min(Bottom, LensA.Y - 0.25f);
		Top = FMath::Max(Top, LensA.Z + 0.25f);
	}
	UVolumetricCloudComponent* Clouds = Sky->Clouds;
	const bool bAny = Top > 0.f;
	if (Clouds)
	{
		if (Clouds->IsVisible() != bAny)
		{
			Clouds->SetVisibility(bAny);
		}
		if (bAny)
		{
			Bottom = FMath::Max(0.f, Bottom);
			const float Height = FMath::Max(0.3f, Top - Bottom);
			if (FMath::Abs(Bottom - SlabBottom) > 0.05f || FMath::Abs(Height - SlabHeight) > 0.05f)
			{
				SlabBottom = Bottom;
				SlabHeight = Height;
				Clouds->SetLayerBottomAltitude(Bottom);
				Clouds->SetLayerHeight(Height);
			}
		}
	}

	auto Vec = [](const FVector4f& V) { return FLinearColor(V.X, V.Y, V.Z, V.W); };
	if (UMaterialInstanceDynamic* CloudMid = Sky->GetCloudMID())
	{
		const TCHAR* Names[2][5] = { { TEXT("L0A"), TEXT("L0B"), TEXT("L0C"), TEXT("L0D"), TEXT("L0E") },
		                             { TEXT("L1A"), TEXT("L1B"), TEXT("L1C"), TEXT("L1D"), TEXT("L1E") } };
		const FVector2D Dirs[2] = { DLow, DMid };
		for (int32 I = 0; I < 2; ++I)
		{
			const FShape& S = Shape[I];
			CloudMid->SetVectorParameterValue(Names[I][0], Vec(S.A));
			CloudMid->SetVectorParameterValue(Names[I][1], Vec(S.B));
			CloudMid->SetVectorParameterValue(Names[I][2], Vec(S.C));
			CloudMid->SetVectorParameterValue(Names[I][3], FLinearColor(float(Drift[I].X), float(Drift[I].Y), float(Dirs[I].X), float(Dirs[I].Y)));
			CloudMid->SetVectorParameterValue(Names[I][4], Vec(S.E));
		}
		CloudMid->SetVectorParameterValue(TEXT("Lens"), Vec(LensA));
		CloudMid->SetVectorParameterValue(TEXT("LensDir"), FLinearColor(float(DUp.X), float(DUp.Y), 0.f, 0.f));
		const float Time = GetWorld() ? float(FMath::Fmod(GetWorld()->GetTimeSeconds(), 100000.0)) : 0.f;
		CloudMid->SetVectorParameterValue(TEXT("Misc"), FLinearColor(Time, 0.f, Flicker * 40.f, 0.f));
		CloudMid->SetVectorParameterValue(TEXT("Flash"), Vec(FlashAt));
	}
	if (UMaterialInstanceDynamic* SkyMid = Sky->GetSkyMID())
	{
		SkyMid->SetVectorParameterValue(TEXT("HighA"), Vec(HighA));
		SkyMid->SetVectorParameterValue(TEXT("HighB"), Vec(HighB));
		SkyMid->SetVectorParameterValue(TEXT("HighD"), FLinearColor(float(Drift[2].X), float(Drift[2].Y), float(DUp.X), float(DUp.Y)));
	}

	if (CVarCloudDebug.GetValueOnGameThread() > 0)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now >= DebugNext)
		{
			DebugNext = Now + 3.0;
			const FString R = DebugReport();
			UE_LOG(LogTemp, Display, TEXT("%s"), *R);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(int32(0x434C4F44), 3.5f, FColor::White, R);
			}
		}
	}
}

FString ULyudCloudSubsystem::DebugReport() const
{
	auto GenusName = [](ELyudCloudType G) { return StaticEnum<ELyudCloudType>()->GetNameStringByValue(int64(G)); };
	auto Line = [&](const TCHAR* Name, const FLyudCloudLayer& L)
	{
		return FString::Printf(TEXT("%s %s cover %.2f %0.f-%0.f m dens %.2f"), Name, *GenusName(L.Type), L.Cover, L.BaseM, L.TopM, L.Density);
	};
	return FString::Printf(TEXT("CLOUDS %s | %s | %s | lens %.2f showers %.2f rain %.1f | slab %.2f+%.2f km | drawn cover %.2f %.2f high %.2f"),
		*Line(TEXT("low"), Target.Low), *Line(TEXT("mid"), Target.Mid), *Line(TEXT("high"), Target.High), Target.Lenticular,
		Target.ShowerCells, Target.PrecipRate, SlabBottom, SlabHeight, Shape[0].A.X, Shape[1].A.X, HighA.X);
}
