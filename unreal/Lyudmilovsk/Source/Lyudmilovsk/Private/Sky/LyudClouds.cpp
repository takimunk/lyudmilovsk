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
	/** The cloud material's extinction per metre at density 1 (its Sigma). */
	constexpr float CloudSigmaPerM = 0.04f;

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
		// how flat the genus is (the weather engine gives it continuously; presets take the genus's own)
		switch (G)
		{
		case ELyudCloudType::Cirrostratus: case ELyudCloudType::Stratus: L.Sheet = 1.f; break;
		case ELyudCloudType::Altostratus: case ELyudCloudType::Nimbostratus: L.Sheet = 0.95f; break;
		case ELyudCloudType::Stratocumulus: L.Sheet = 0.5f; break;
		case ELyudCloudType::Altocumulus: case ELyudCloudType::AltocumulusLenticular: L.Sheet = 0.1f; break;
		default: L.Sheet = 0.f; break;
		}
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
	// No switch on the genus: the weather's Type flips at thresholds, while Sheet, Cover, Base, Top and Density change
	// smoothly, so the shape is blended from them and a change of genus never shows as a jump.
	//   heaped (Sheet 0): cumulus -> congestus -> cumulonimbus by thickness; altocumulus in the middle layer
	//   Sheet 0.5: stratocumulus
	//   sheet (Sheet 1): stratus -> nimbostratus by thickness; altostratus in the middle layer
	struct FRow
	{
		float Cell, Sheet, Round, Anvil, Erosion, Stretch, Ragged, Cluster, Dark, Boil, Shafts;
		FRow Lerp(const FRow& O, float K) const
		{
			auto M = [K](float A, float B) { return A + (B - A) * K; };
			return { M(Cell, O.Cell), M(Sheet, O.Sheet), M(Round, O.Round), M(Anvil, O.Anvil), M(Erosion, O.Erosion), M(Stretch, O.Stretch),
				M(Ragged, O.Ragged), M(Cluster, O.Cluster), M(Dark, O.Dark), M(Boil, O.Boil), M(Shafts, O.Shafts) };
		}
	};
	//                       cell  sheet round anvil eros  strch rag   clust dark  boil  shafts
	static const FRow Cu     = { 1.4f, 0.f,  0.85f, 0.f, 0.75f, 1.4f, 0.05f, 0.35f, 0.3f, 1.f,  0.f };
	static const FRow CuCon  = { 2.6f, 0.f,  0.55f, 0.f, 0.7f,  1.2f, 0.1f,  0.6f,  0.5f, 1.4f, 0.5f };
	static const FRow Cb     = { 6.f,  0.f,  0.f,   1.f, 0.55f, 1.f,  0.3f,  0.f,   0.8f, 1.2f, 1.f };
	static const FRow Sc     = { 1.6f, 0.12f, 0.55f, 0.f, 0.5f, 1.5f, 0.08f, 0.f,   0.45f, 0.6f, 0.2f };
	static const FRow St     = { 4.f,  0.92f, 0.2f, 0.f, 0.15f, 1.5f, 0.08f, 0.f,   0.4f, 0.3f, 0.3f };
	static const FRow Ns     = { 5.f,  0.85f, 0.2f, 0.f, 0.25f, 1.3f, 0.45f, 0.f,   0.85f, 0.5f, 1.f };
	static const FRow Ac     = { 0.8f, 0.15f, 0.6f, 0.f, 0.55f, 1.6f, 0.02f, 0.45f, 0.2f, 0.6f, 0.f };
	static const FRow As     = { 6.f,  0.95f, 0.1f, 0.f, 0.12f, 1.5f, 0.05f, 0.f,   0.35f, 0.3f, 0.4f };

	FShape S;
	// the cover itself fades a layer in and out (the genus may read None while a trace of it is left)
	const bool bDraw = !IsHigh(L.Type) && L.Cover > 0.f;
	const float Cover = bDraw ? L.Cover : 0.f;
	const float ThickM = FMath::Max(L.TopM - L.BaseM, 50.f);
	const float ThickKm = ThickM * 1e-3f;
	const float Sheet = FMath::Clamp(L.Sheet, 0.f, 1.f);
	auto Smooth = [](float A, float B, float X) { const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f); return T * T * (3.f - 2.f * T); };

	// low: heaped by thickness, then toward the deck by Sheet (through stratocumulus at 0.5)
	const FRow Heaped = Cu.Lerp(CuCon, Smooth(1.2f, 3.5f, ThickKm)).Lerp(Cb, Smooth(4.5f, 7.5f, ThickKm));
	const FRow Deck = St.Lerp(Ns, Smooth(1.f, 2.5f, ThickKm));
	const FRow Low = Sheet < 0.5f ? Heaped.Lerp(Sc, Sheet * 2.f) : Sc.Lerp(Deck, (Sheet - 0.5f) * 2.f);
	// the middle layer: altocumulus .. altostratus
	const FRow Mid = Ac.Lerp(As, Sheet);
	const FRow R = Low.Lerp(Mid, Smooth(1800.f, 2600.f, L.BaseM) * (1.f - Smooth(4.5f, 7.5f, ThickKm)));

	// rain shafts only where something falls: light rain greys the air under the base, a downpour hangs curtains
	const float Shafts = R.Shafts * FMath::Clamp(PrecipRate / 4.f, 0.f, 1.f);
	// shower cells make heaped layers clumpier
	const float Cluster = FMath::Clamp(R.Cluster + (1.f - Sheet) * ShowerCells * 0.5f, 0.f, 1.f);
	// decks: the weather's density is how much light they let through, so it sets their optical depth whatever their
	// thickness (an altostratus 2 km thick still shows a watery sun; a nimbostratus is dark); heaps keep it as it is
	const float Density0 = FMath::Clamp(L.Density, 0.f, 1.f);
	const float LowTau = Sheet < 0.5f ? 8.f + 22.f * Density0
		: FMath::Lerp(8.f + 22.f * Density0, FMath::Lerp(6.f + 14.f * Density0, 25.f + 35.f * Density0, Smooth(1.f, 2.5f, ThickKm)), (Sheet - 0.5f) * 2.f);
	// an altostratus is thin: the sun shows through it as a dim washed-out disk ("as through ground glass")
	const float Tau = FMath::Lerp(LowTau, 0.3f + 1.2f * Density0, Smooth(1800.f, 2600.f, L.BaseM));
	const float DeckDensity = FMath::Clamp(Tau / (CloudSigmaPerM * ThickM), 0.f, 1.f);
	const float Density = FMath::Lerp(Density0, DeckDensity, Smooth(0.25f, 0.75f, Sheet) * (1.f - Smooth(4.5f, 7.5f, ThickKm)));

	S.A = FVector4f(Cover, L.BaseM * 1e-3f, (L.BaseM + ThickM) * 1e-3f, Density);
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
		// fibres and veil from Sheet (continuous); the grain of cirrocumulus eases in and out with its genus
		const float Veil = FMath::Clamp(High.Sheet, 0.f, 1.f);
		const float Grain = High.Type == G::Cirrocumulus ? 1.f : 0.f;
		HB = FVector4f((1.f - Grain) * (1.f - 0.65f * Veil), Grain * (1.f - Veil), Veil, FMath::Clamp(Target.Halo, 0.f, 1.f));
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
		Mid.BaseM > 0.f ? Mid.BaseM * 1e-3f : 3.5f, Mid.TopM > 0.f ? Mid.TopM * 1e-3f : 4.5f, 4.f) : FVector4f(0.f, LensA.Y, LensA.Z, 4.f);
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

	// the direct sun under an overcast: the cloud shadow map dims it per pixel, but under a full deck it must be gone
	// everywhere (no crisp shadows in a snowfall). Each layer lets through 1 - cover x (1 - its transmittance along the
	// sun); that counts only where the decks close up (broken clouds keep their own moving shadows and sunny gaps).
	{
		const float SinAlt = FMath::Max(FMath::Sin(FMath::DegreesToRadians(Sky->SunAltitude())), 0.12f);
		float Through = 1.f, Closed = 0.f, Diffuse = 1.f, DeckBase = 1e9f;
		for (int32 I = 0; I < 2; ++I)
		{
			const FShape& S = Shape[I];
			const float ThickM = FMath::Max(S.A.Z - S.A.Y, 0.05f) * 1000.f;
			const float Tau = S.A.W * CloudSigmaPerM * ThickM / SinAlt;
			const float Cover = FMath::Clamp(S.A.X, 0.f, 1.f);
			Through *= 1.f - Cover * (1.f - FMath::Exp(-Tau));
			// how closed the layer is: a deck (sheet) at full cover; heaps never close up the sky this way
			const float LayerClosed = FMath::SmoothStep(0.75f, 0.97f, Cover) * FMath::SmoothStep(0.2f, 0.6f, S.B.Y + S.B.W);
			Closed = FMath::Max(Closed, LayerClosed);
			// the light that diffuses through a deck (two-stream: ~1 / (1 + 0.11 tau) for cloud droplets), straight down
			const float TauV = S.A.W * CloudSigmaPerM * ThickM;
			Diffuse *= 1.f - Cover * (1.f - 1.f / (1.f + 0.11f * TauV));
			if (LayerClosed > 0.5f)
			{
				DeckBase = FMath::Min(DeckBase, S.A.Y);
			}
		}
		// a cirrostratus veil takes a little of the sun's crispness too
		Through *= 1.f - 0.25f * HighA.X * HighB.Z * HighA.Z;
		Sky->SetOvercastSunScale(FMath::Lerp(1.f, FMath::Max(Through, 0.02f), Closed));
		SunThrough = Sky->GetOvercastSunScale();
		SkyLightThrough = Diffuse;
		DeckClosed = Closed;
		// a closed deck reaches the horizon: trace it out to where its base sinks under the earth's curve
		// (sqrt(2 R h)), so no strip of bright sky is left over the sea under it
		DeckTraceKm = DeckBase < 1e8f ? FMath::Lerp(60.f, FMath::Clamp(FMath::Sqrt(2.f * 6371.f * FMath::Max(DeckBase, 0.05f)) + 45.f, 60.f, 230.f), Closed) : 60.f;
	}

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
	// the ray march goes this far from the eye; the shader fades the clouds out before it
	const float TraceKm = FMath::RoundToFloat(DeckTraceKm / 5.f) * 5.f;
	if (Clouds)
	{
		if (Clouds->TracingMaxDistanceMode != EVolumetricCloudTracingMaxDistanceMode::DistanceFromPointOfView || !FMath::IsNearlyEqual(Clouds->TracingMaxDistance, TraceKm))
		{
			Clouds->TracingMaxDistanceMode = EVolumetricCloudTracingMaxDistanceMode::DistanceFromPointOfView;
			Clouds->SetTracingMaxDistance(TraceKm);
		}
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
		CloudMid->SetVectorParameterValue(TEXT("Misc"), FLinearColor(Time, TraceKm, Flicker * 40.f, 0.f));
		CloudMid->SetVectorParameterValue(TEXT("Flash"), Vec(FlashAt));
		// a closed deck's underside: the diffuse light through it (as ALyudRainFx::SkyLuminance lights the fog under it)
		const float SunAlt = Sky->SunAltitude();
		const float Day = FMath::SmoothStep(-7.f, 8.f, SunAlt);
		const float Sin = FMath::Max(FMath::Sin(FMath::DegreesToRadians(FMath::Max(SunAlt, 0.f))), 0.15f);
		const float Glow = (Sky->SunIlluminance * Sin / PI * 0.55f * SkyLightThrough * Day + 0.006f) * DeckClosed;
		CloudMid->SetVectorParameterValue(TEXT("DeckGlow"), Sky->SunColor * Glow);
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
	const ALyudSky* Sky = SkyActor.Get();
	const FString Mat = Sky ? FString::Printf(TEXT(" | material %s ready %d visible %d"), Sky->Clouds && Sky->Clouds->GetMaterial() ? *Sky->Clouds->GetMaterial()->GetName() : TEXT("none"),
		Sky->IsCloudMaterialReady() ? 1 : 0, Sky->Clouds && Sky->Clouds->IsVisible() ? 1 : 0) : FString(TEXT(" | no sky"));
	return FString::Printf(TEXT("CLOUDS sun %.2f | %s | %s | %s | lens %.2f showers %.2f rain %.1f | slab %.2f+%.2f km | drawn cover %.2f %.2f high %.2f"),
		SunThrough, *Line(TEXT("low"), Target.Low), *Line(TEXT("mid"), Target.Mid), *Line(TEXT("high"), Target.High), Target.Lenticular,
		Target.ShowerCells, Target.PrecipRate, SlabBottom, SlabHeight, Shape[0].A.X, Shape[1].A.X, HighA.X) + Mat;
}
