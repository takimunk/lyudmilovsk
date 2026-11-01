#include "Sky/LyudSunGlare.h"

#include "Audio/LyudSoundscape.h"
#include "Brushes/SlateColorBrush.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "LyudGameMenu.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Rendering/DrawElements.h"
#include "Sky/LyudSky.h"
#include "Sound/SoundBase.h"
#include "TextureResource.h"
#include "UI/LyudGameScreen.h"
#include "Widgets/SLeafWidget.h"

namespace
{
	TAutoConsoleVariable<int32> CVarGlare(TEXT("lyud.Sun.Glare"), 1,
		TEXT("The sun blinds until the eye adapts: glare, squint, blinks, \"ай\", afterimages (0: the star as drawn, none of it; the Settings page stores it)."));
	TAutoConsoleVariable<float> CVarGain(TEXT("lyud.Sun.Glare.Gain"), 14.f,
		TEXT("The photosphere this many times brighter for the unadapted eye (1 = the adapted, detailed star)."));
	TAutoConsoleVariable<float> CVarHorizonGain(TEXT("lyud.Sun.Glare.HorizonGain"), 0.f,
		TEXT("Share of the glare gain the star keeps on the horizon (from 3 up to 20 degrees it grows to all of it): low, its light crosses so much air that it can be looked at."));
	TAutoConsoleVariable<float> CVarHorizonDisk(TEXT("lyud.Sun.Glare.HorizonDisk"), 0.3f,
		TEXT("The photosphere's brightness on the horizon (to 1 at 15 degrees), fresh or adapted: brighter, the setting star blew out to pale yellow on the tonemapper; so it stays orange, pink, red (1 = the sky's own)."));
	TAutoConsoleVariable<int32> CVarFreezeAtShut(TEXT("lyud.Sun.Glare.FreezeAtShut"), 0, TEXT("1: lyud.Sun.Glare.Freeze 1 is set when a blink shuts the eyes (screenshots of the blink)."));
	TAutoConsoleVariable<float> CVarBloom(TEXT("lyud.Sun.Glare.Bloom"), 2.5f,
		TEXT("The outdoor bloom this many times stronger while the unadapted star is in sight."));
	TAutoConsoleVariable<float> CVarHalo(TEXT("lyud.Sun.Glare.Halo"), 1.f, TEXT("The glow in the eye round the unadapted star and the veil over the view (0 = none)."));
	TAutoConsoleVariable<float> CVarAfter(TEXT("lyud.Sun.Glare.Afterimage"), 1.f, TEXT("The afterimages' strength (0 = none)."));
	TAutoConsoleVariable<int32> CVarBlink(TEXT("lyud.Sun.Glare.Blink"), 1, TEXT("The hero squints and blinks at the sun (0: never)."));
	TAutoConsoleVariable<int32> CVarVoice(TEXT("lyud.Sun.Glare.Voice"), 1, TEXT("The hero says \"ай\" when the sun blinds him (0: silent)."));
	TAutoConsoleVariable<float> CVarStare(TEXT("lyud.Sun.Glare.StareSeconds"), 3.f, TEXT("Seconds of the star in the middle of the view before the hero blinks and says \"ай\"."));
	TAutoConsoleVariable<float> CVarAdaptSec(TEXT("lyud.Sun.Glare.AdaptSeconds"), 6.f, TEXT("Seconds of staring on after that until the eye has adapted (the star as drawn)."));
	TAutoConsoleVariable<float> CVarRecoverSec(TEXT("lyud.Sun.Glare.RecoverSeconds"), 2.f, TEXT("Seconds looking away until the adaptation is (nearly) gone and the star blinds again."));
	TAutoConsoleVariable<float> CVarOwEvery(TEXT("lyud.Sun.Glare.OwEvery"), 20.f, TEXT("At most one \"ай\" in this many seconds."));
	TAutoConsoleVariable<float> CVarTraceKm(TEXT("lyud.Sun.Glare.TraceKm"), 30.f, TEXT("How far the traces toward the star look for land, trees and buildings in front of it, km."));
	TAutoConsoleVariable<float> CVarForceAdapt(TEXT("lyud.Sun.Glare.Adapt"), -1.f, TEXT("Holds the adaptation at this 0..1 when >= 0 (tests, tuning)."));
	TAutoConsoleVariable<int32> CVarFreeze(TEXT("lyud.Sun.Glare.Freeze"), 0, TEXT("1: the eye's state and the retina stop changing (screenshots of a blink or an afterimage)."));
	TAutoConsoleVariable<float> CVarFlashStep(TEXT("lyud.Sun.Glare.FlashStep"), 0.5f, TEXT("A flash blind's whiteout keeps this share of itself with each blink."));
	FAutoConsoleCommandWithWorldAndArgs CmdFlash(TEXT("lyud.Sun.Glare.FlashBlind"), TEXT("Blinded by a sudden light: lyud.Sun.Glare.FlashBlind [strength 0..1] (tests)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudSunGlare* G = ULyudSunGlare::Get(World))
			{
				G->BeginFlashBlind(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 1.f);
			}
		}));
	TAutoConsoleVariable<int32> CVarDebug(TEXT("lyud.Sun.Glare.Debug"), 0, TEXT("1: the glare state on screen (GLARE line)."));

	FAutoConsoleCommandWithWorld CmdGlareReset(TEXT("lyud.Sun.Glare.Reset"), TEXT("A fresh eye: no adaptation, no stare, no afterimages (tests)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ULyudSunGlare* G = ULyudSunGlare::Get(World))
			{
				G->ResetEye();
			}
		}));

	/** The retina map's cells (16:9; other aspects stretch it). */
	constexpr int32 GW = 192;
	constexpr int32 GH = 108;
	/** Hits per trace round: the centre and four points on the disk. */
	constexpr int32 Rays = 5;
	constexpr float TraceEvery = 0.1f;
	constexpr float UploadEvery = 1.f / 30.f;
	/** The blink: closing, shut, opening (s). */
	constexpr float BlinkClose = 0.07f, BlinkShut = 0.06f, BlinkOpen = 0.16f;
	constexpr float FlinchSeconds = 0.4f, FlinchDeg = -1.6f;

	float Smooth(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	/** The air's transmittance toward an altitude (deg): ALyudSky's AirTransmittance (sky_hlsl.py TRANS_HLSL). */
	FLinearColor Air(float AltDeg)
	{
		const float H = FMath::Max(AltDeg, 0.f);
		const float Am = 1.f / (FMath::Sin(FMath::DegreesToRadians(H)) + 0.50572f * FMath::Pow(H + 6.07995f, -1.6364f));
		const float Sink = Smooth(-6.f, 0.5f, AltDeg);
		return FLinearColor(FMath::Exp(-0.07f * (Am - 1.f)) * Sink, FMath::Exp(-0.14f * (Am - 1.f)) * Sink, FMath::Exp(-0.30f * (Am - 1.f)) * Sink);
	}

	float Luma(const FLinearColor& C) { return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B; }

	UTexture2D* MakeStatic(int32 W, int32 H, const TCHAR* Name, TFunctionRef<FLinearColor(float U, float V)> Pixel)
	{
		UTexture2D* T = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8, Name);
		if (!T)
		{
			return nullptr;
		}
		T->SRGB = true;
		T->Filter = TF_Bilinear;
		T->AddressX = TA_Clamp;
		T->AddressY = TA_Clamp;
		FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
		FColor* Data = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Y = 0; Y < H; Y++)
		{
			for (int32 X = 0; X < W; X++)
			{
				const FLinearColor C = Pixel((X + 0.5f) / W, (Y + 0.5f) / H);
				FColor P = FLinearColor(C.R, C.G, C.B, 1.f).ToFColor(true);
				P.A = (uint8)FMath::Clamp(FMath::RoundToInt(C.A * 255.f), 0, 255);
				Data[Y * W + X] = P;
			}
		}
		Mip.BulkData.Unlock();
		T->UpdateResource();
		return T;
	}

	/** The upper eyelid's edge in the lid picture (v from its top): an arch, highest in the middle. */
	float LidEdge(float U) { const float X = 2.f * U - 1.f; return 0.84f + 0.16f * X * X; }
	constexpr float LidHeight = 0.7f;   // of the screen's height
}

/** Draws ULyudSunGlare's paint: the eyelids, the retina's afterimages, the glow round the star, the veil. */
class SLyudSunGlareOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudSunGlareOverlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULyudSunGlare>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Owner = InArgs._Owner;
		SetVisibility(EVisibility::HitTestInvisible);
		White = FSlateColorBrush(FLinearColor::White);
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Clip, FSlateWindowElementList& Out,
		int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const ULyudSunGlare* G = Owner.Get();
		if (!G)
		{
			return LayerId;
		}
		const ULyudSunGlare::FPaint& P = G->GetPaint();
		const FVector2f S = FVector2f(Geo.GetLocalSize());
		const ESlateDrawEffect Pre = ESlateDrawEffect::PreMultipliedAlpha;
		auto Box = [&](const FSlateBrush* B, float X, float Y, float W, float H, const FLinearColor& C)
		{
			FSlateDrawElement::MakeBox(Out, LayerId, Geo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))), B, Pre, C);
		};
		// a flash blind: the view washed toward white
		if (P.Flash > 1e-3f)
		{
			const float F = FMath::Min(P.Flash, 1.f);
			Box(&White, 0.f, 0.f, S.X, S.Y, FLinearColor(F, F * 0.97f, F * 0.92f, 0.85f * F));
		}
		// the veil and the glow round the star: light scattered in the eye, over everything (additive: alpha 0)
		if (P.Veil > 1e-3f)
		{
			Box(&White, 0.f, 0.f, S.X, S.Y, FLinearColor(P.SunTint.R * P.Veil, P.SunTint.G * P.Veil, P.SunTint.B * P.Veil, 0.f));
		}
		if (P.Halo > 1e-3f && G->GetGlowTexture())
		{
			GlowBrush.SetResourceObject(G->GetGlowTexture());
			const float R = FMath::Max(P.SunRadiusUV * S.X * 7.f, S.Y * 0.35f);
			Box(&GlowBrush, P.SunUV.X * S.X - R, P.SunUV.Y * S.Y - R, 2.f * R, 2.f * R,
				FLinearColor(P.SunTint.R * P.Halo, P.SunTint.G * P.Halo, P.SunTint.B * P.Halo, 0.f));
		}
		// the eyelids (lit red from the front by a bright sun)
		if (P.Lids > 2e-3f && G->GetLidTexture(true))
		{
			const float Hi = LidHeight * S.Y;
			const float Up = -0.2f * S.Y + P.Lids * 0.74f * S.Y;
			const float Down = 1.2f * S.Y - P.Lids * 0.74f * S.Y;
			const FLinearColor Lid(P.LidTint.R, P.LidTint.G, P.LidTint.B, 1.f);
			LidBrush[0].SetResourceObject(G->GetLidTexture(true));
			LidBrush[1].SetResourceObject(G->GetLidTexture(false));
			Box(&LidBrush[0], -0.1f * S.X, Up - LidEdge(0.5f) * Hi, 1.2f * S.X, Hi, Lid);
			Box(&LidBrush[1], -0.1f * S.X, Down - (1.f - LidEdge(0.5f)) * Hi, 1.2f * S.X, Hi, Lid);
			// shut: nothing more of the screen shows than the light through the lids
			if (P.Lids > 0.98f)
			{
				Box(&White, 0.f, 0.f, S.X, S.Y, Lid);
			}
		}
		// the afterimages, fixed to the retina: where on the screen the sun was (on the lids too: they show on shut eyes)
		if (P.bRetina && G->GetRetinaTexture())
		{
			RetinaBrush.SetResourceObject(G->GetRetinaTexture());
			Box(&RetinaBrush, P.Wobble.X * S.X, P.Wobble.Y * S.Y, S.X, S.Y, FLinearColor::White);
		}
		return LayerId;
	}

private:
	TWeakObjectPtr<ULyudSunGlare> Owner;
	FSlateColorBrush White = FSlateColorBrush(FLinearColor::White);
	mutable FSlateBrush GlowBrush;
	mutable FSlateBrush LidBrush[2];
	mutable FSlateBrush RetinaBrush;
};

ULyudSunGlare* ULyudSunGlare::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudSunGlare>() : nullptr;
}

bool ULyudSunGlare::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId ULyudSunGlare::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudSunGlare, STATGROUP_Tickables);
}

void ULyudSunGlare::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// every level start is a new game unless a load restores the memory right after (as ULyudBreath)
	if (ULyudSunGlareMemory* M = ULyudSunGlareMemory::Get(&InWorld); M && M->ResetFor.Get() != &InWorld)
	{
		M->FlashesSeen.Reset();
		M->ResetFor = &InWorld;
	}
}

void ULyudSunGlare::Deinitialize()
{
	ShowOverlay(false);
	if (ALyudSky* S = Sky.Get())
	{
		S->SetGlare(1.f, 1.f);
	}
	Super::Deinitialize();
}

void ULyudSunGlare::ShowOverlay(bool bShow)
{
	UGameViewportClient* VP = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (bShow && !Overlay.IsValid() && VP)
	{
		Overlay = SNew(SLyudSunGlareOverlay).Owner(this);
		// under the swimmer's meter (58), the prompts (60), the game screen (85) and the menu (100)
		VP->AddViewportWidgetContent(Overlay.ToSharedRef(), 50);
	}
	else if (!bShow && Overlay.IsValid())
	{
		if (VP)
		{
			VP->RemoveViewportWidgetContent(Overlay.ToSharedRef());
		}
		Overlay.Reset();
	}
}

void ULyudSunGlare::MakeTextures()
{
	if (RetinaTex)
	{
		return;
	}
	Fast.SetNumZeroed(GW * GH);
	Bleach.SetNumZeroed(GW * GH);
	for (TArray<FColor>& U : Upload)
	{
		U.SetNumZeroed(GW * GH);
	}
	RetinaTex = MakeStatic(GW, GH, TEXT("T_SunGlareRetina"), [](float, float) { return FLinearColor(0.f, 0.f, 0.f, 0.f); });
	// premultiplied: white x coverage, tinted by the paint's colour
	auto LidPixel = [](float U, float V)
	{
		const float Edge = LidEdge(U);
		const float A = 1.f - Smooth(Edge - 0.05f, Edge, V);
		// a darker band of lashes just above the edge
		const float Lash = 1.f - 0.35f * Smooth(Edge - 0.12f, Edge - 0.04f, V) * (1.f - Smooth(Edge - 0.04f, Edge, V));
		const float C = A * Lash;
		return FLinearColor(C, C, C, A);
	};
	LidUpperTex = MakeStatic(128, 128, TEXT("T_SunGlareLidUp"), LidPixel);
	LidLowerTex = MakeStatic(128, 128, TEXT("T_SunGlareLidDown"), [&LidPixel](float U, float V) { return LidPixel(U, 1.f - V); });
	// the glow in the eye round a bright source: a core and a long 1/r^2-like tail (the eye's own stray light)
	GlowTex = MakeStatic(128, 128, TEXT("T_SunGlareGlow"), [](float U, float V)
	{
		const float R = FMath::Sqrt(FMath::Square(2.f * U - 1.f) + FMath::Square(2.f * V - 1.f));
		const float I = (0.55f * FMath::Exp(-R * R / 0.015f) + 0.3f / (1.f + FMath::Square(R / 0.1f)) + 0.08f / (1.f + FMath::Square(R / 0.35f)))
			* (1.f - Smooth(0.6f, 1.f, R));
		const float L = FMath::Min(I, 1.f);
		return FLinearColor(L, L, L, 0.f);
	});
}

void ULyudSunGlare::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	const float Dt = FMath::Min(DeltaTime, 0.25f);
	const bool bOn = CVarGlare.GetValueOnGameThread() > 0;
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	const ULyudGameScreen* Screen = ULyudGameScreen::Get(World);
	const bool bHold = UGameplayStatics::IsGamePaused(World) || (Menu && Menu->IsOpen()) || (Screen && Screen->IsOpen());
	if (!bOn)
	{
		if (ALyudSky* S = Sky.Get())
		{
			S->SetGlare(1.f, 1.f);
		}
		ShowOverlay(false);
		ResetEye();
		return;
	}
	MakeTextures();
	ShowOverlay(true);
	if (!Look(Dt))
	{
		Paint = FPaint();
		return;
	}
	if (bHold)
	{
		// a menu or the game screen over the view: the eye waits, nothing is drawn over the menu
		Paint = FPaint();
		return;
	}
	if (CVarFreeze.GetValueOnGameThread() == 0)
	{
		Time += Dt;
		UpdateEye(Dt);
		UpdateRetina(Dt);
	}
	UploadRetina();

	ALyudSky* S = Sky.Get();
	const float Alt = S->SunAltitude();
	const float AltShape = S->IsSpaceView() ? 1.f : FMath::Lerp(FMath::Clamp(CVarHorizonGain.GetValueOnGameThread(), 0.f, 1.f), 1.f, Smooth(3.f, 20.f, Alt));
	const float LowDisk = S->IsSpaceView() ? 1.f : FMath::Lerp(FMath::Clamp(CVarHorizonDisk.GetValueOnGameThread(), 0.05f, 1.f), 1.f, Smooth(1.f, 15.f, Alt));
	const float Fresh = 1.f - Smooth(0.f, 1.f, Adapt);
	const float Gain = LowDisk * (1.f + (FMath::Max(CVarGain.GetValueOnGameThread(), 1.f) - 1.f) * AltShape * Fresh);
	const float BloomK = 1.f + (FMath::Max(CVarBloom.GetValueOnGameThread(), 1.f) - 1.f) * Fresh * FMath::Clamp(InEye * 1.5f, 0.f, 1.f);
	S->SetGlare(Gain, BloomK);

	const float HaloK = FMath::Max(CVarHalo.GetValueOnGameThread(), 0.f);
	Paint.SunUV = SunUV;
	Paint.SunRadiusUV = SunRadiusUV;
	Paint.SunTint = SunTint;
	Paint.Halo = HaloK * 0.55f * InEye * Fresh * AltShape;
	Paint.Veil = HaloK * 0.05f * InEye * Fresh * AltShape * (0.4f + 0.6f * InGaze / FMath::Max(InEye, 1e-3f));
	Paint.Lids = Lids;
	Paint.Flash = Flash;
	// the lids are dark; shut, facing a bright sun, the light through them is red
	const float Through = FMath::Clamp(InEye * (0.3f + 0.7f * Fresh) + Flash, 0.f, 1.f) * Smooth(0.8f, 1.f, Lids);
	Paint.LidTint = FLinearColor(0.012f + 0.1f * Through, 0.006f + 0.012f * Through, 0.005f + 0.005f * Through, 1.f);
	Paint.bRetina = bRetinaLive;
	Paint.Wobble = FVector2f(0.0018f * FMath::Sin(Time * 1.7f) + 0.0007f * FMath::Sin(Time * 4.1f), 0.0014f * FMath::Cos(Time * 1.3f)) * FMath::Clamp(MaxBleach * 2.f, 0.f, 1.f);

	if (CVarDebug.GetValueOnGameThread() > 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(int32(0x5A11), 0.f, FColor::Yellow, GetDebugLine());
	}
}

bool ULyudSunGlare::Look(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World->GetFirstPlayerController();
	APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!Cam)
	{
		return false;
	}
	ALyudSky* S = Sky.Get();
	if (!S)
	{
		if (TActorIterator<ALyudSky> It(World); It)
		{
			Sky = *It;
			S = *It;
		}
	}
	if (!S)
	{
		return false;
	}
	FVector2D Size(16.0, 9.0);
	if (UGameViewportClient* VP = World->GetGameViewport())
	{
		VP->GetViewportSize(Size);
	}
	Aspect = Size.Y > 1.0 ? float(Size.X / Size.Y) : 16.f / 9.f;

	const FVector Eye = Cam->GetCameraLocation();
	const FRotationMatrix M(Cam->GetCameraRotation());
	const FVector F = M.GetUnitAxis(EAxis::X), R = M.GetUnitAxis(EAxis::Y), U = M.GetUnitAxis(EAxis::Z);
	const FVector Sun = S->SunDirection();
	const float Fwd = (float)FVector::DotProduct(Sun, F);
	GazeDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Fwd, -1.f, 1.f)));
	const float RadDeg = S->SunAngularDiameterDeg * 0.5f;
	const float TanH = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Cam->GetFOVAngle(), 5.f, 170.f) * 0.5f));
	const float TanV = TanH / Aspect;
	float Edge = 1.f;
	if (Fwd > 0.05f)
	{
		SunUV = FVector2f(0.5f + 0.5f * float(FVector::DotProduct(Sun, R)) / Fwd / TanH, 0.5f - 0.5f * float(FVector::DotProduct(Sun, U)) / Fwd / TanV);
		SunRadiusUV = 0.5f * FMath::Tan(FMath::DegreesToRadians(RadDeg)) / TanH / Fwd;
		// how far outside the screen the disk is, in screen widths (its glow still reaches in a little)
		const float Out = FMath::Max(FMath::Max(-SunUV.X, SunUV.X - 1.f), FMath::Max(-SunUV.Y, SunUV.Y - 1.f) / Aspect) - SunRadiusUV;
		Edge = Out;
	}
	bSunOnScreen = Edge < 0.f;
	const float OnScreen = Fwd > 0.05f ? 1.f - Smooth(-0.02f, 0.2f, Edge) : 0.f;

	// what the land, the trees, the buildings let through: a few traces a few times a second, only near the view
	if (OnScreen > 0.f || GazeDeg < 70.f)
	{
		const double Now = World->GetTimeSeconds();
		if (Now >= NextTrace)
		{
			NextTrace = Now + TraceEvery;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(LyudSunGlare), false);
			if (APawn* Pawn = PC->GetPawn())
			{
				Params.AddIgnoredActor(Pawn);
				TArray<AActor*> Attached;
				Pawn->GetAttachedActors(Attached, true, true);
				Params.AddIgnoredActors(Attached);
			}
			const FVector A = FVector::CrossProduct(Sun, FMath::Abs(Sun.Z) < 0.95 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
			const FVector B = FVector::CrossProduct(Sun, A);
			const double Off = FMath::Tan(FMath::DegreesToRadians(RadDeg * 0.65));
			const FVector Dirs[Rays] = { Sun, (Sun + A * Off).GetSafeNormal(), (Sun - A * Off).GetSafeNormal(), (Sun + B * Off).GetSafeNormal(), (Sun - B * Off).GetSafeNormal() };
			const double Len = FMath::Max(CVarTraceKm.GetValueOnGameThread(), 0.1f) * 1e5;
			int32 Open = 0;
			for (const FVector& D : Dirs)
			{
				FHitResult Hit;
				Open += World->LineTraceSingleByChannel(Hit, Eye, Eye + D * Len, ECC_Visibility, Params) ? 0 : 1;
			}
			VisibleRaw = float(Open) / Rays;
		}
	}
	Visible = FMath::FInterpTo(Visible, VisibleRaw, DeltaTime, 10.f);

	// the star's brightness for the eye: the air (dims and reddens it low), the haze and the clouds in front
	const float Alt = S->SunAltitude();
	FLinearColor Tr = S->IsSpaceView() ? FLinearColor::White : Air(Alt);
	const float TrY = FMath::Max(Luma(Tr), 1e-4f);
	const float Weather = S->IsSpaceView() ? 1.f : S->GetOvercastSunScale() * FMath::Pow(S->GetSunHazeThrough(), 0.6f);
	Bright = FMath::Clamp(FMath::Pow(TrY, 0.6f) * Weather, 0.f, 1.f);
	InEye = Bright * Visible * OnScreen;
	InGaze = InEye * (1.f - Smooth(RadDeg * 0.6f, RadDeg + 14.f, GazeDeg));
	// its colour as the eye takes it: the red dwarf's (1, .26, .055) through the air, blown toward white when bright
	const float Gain = FMath::Max(CVarGain.GetValueOnGameThread(), 1.f) * (1.f - Smooth(0.f, 1.f, Adapt)) + Smooth(0.f, 1.f, Adapt);
	const FLinearColor C = FLinearColor(1.f * Tr.R, 0.26f * Tr.G, 0.055f * Tr.B) * (Gain * 3.f / FMath::Max(TrY, 0.05f) * FMath::Pow(TrY, 0.6f));
	const FLinearColor T(C.R / (1.f + C.R), C.G / (1.f + C.G), C.B / (1.f + C.B), 1.f);
	const float Top = FMath::Max3(T.R, T.G, T.B);
	SunTint = Top > 1e-4f ? FLinearColor(T.R / Top, T.G / Top, T.B / Top, 1.f) : FLinearColor::White;
	return true;
}

void ULyudSunGlare::UpdateEye(float Dt)
{
	const float StareSec = FMath::Max(CVarStare.GetValueOnGameThread(), 0.2f);
	const float AdaptSec = FMath::Max(CVarAdaptSec.GetValueOnGameThread(), 0.2f);
	const float RecoverSec = FMath::Max(CVarRecoverSec.GetValueOnGameThread(), 0.1f);
	const bool bBlinks = CVarBlink.GetValueOnGameThread() > 0;
	// the pain of a fresh eye in the light: the stare grows while the star stays in the middle of the view
	const float Pain = InGaze * (1.f - Adapt);
	if (Pain > 0.2f)
	{
		Stare = FMath::Min(Stare + Dt * FMath::Clamp(Pain * 1.5f, 0.f, 1.f), StareSec + 2.f);
	}
	else
	{
		Stare = FMath::Max(Stare - 1.5f * Dt, 0.f);
	}
	const bool bLooking = InGaze > 0.2f;
	if (!bAdapting && Stare >= StareSec)
	{
		// it hurts: a blink, "ай", and the eye starts to give in
		bAdapting = true;
		if (bBlinks)
		{
			BlinkT = 0.f;
		}
		Ow();
		NextBlink = Time + FMath::FRandRange(1.4f, 2.4f);
	}
	if (bAdapting && bLooking)
	{
		Adapt = FMath::Min(Adapt + Dt / AdaptSec, 1.f);
	}
	else if (!bLooking)
	{
		// looking away the eye opens up again: after RecoverSeconds a tenth of the adaptation is left
		Adapt *= FMath::Exp(-Dt * 2.3f / RecoverSec);
		if (Adapt < 0.08f && Stare <= 0.f)
		{
			bAdapting = false;
		}
	}
	const float Forced = CVarForceAdapt.GetValueOnGameThread();
	if (Forced >= 0.f)
	{
		Adapt = FMath::Clamp(Forced, 0.f, 1.f);
	}
	// a flash blind: a fresh eye, blinking every second or so, each blink takes the whiteout down a notch (below)
	if (Flash > 0.f)
	{
		Adapt = 0.f;
		if (bBlinks && BlinkT < 0.f && Time >= NextFlashBlink)
		{
			BlinkT = 0.f;
		}
	}
	// blinks keep coming while it still hurts
	if (bBlinks && bAdapting && bLooking && Adapt < 0.7f && Time >= NextBlink && BlinkT < 0.f)
	{
		BlinkT = 0.f;
		NextBlink = Time + FMath::FRandRange(1.5f, 3.f) / FMath::Max(1.f - Adapt, 0.3f);
	}
	if (!bBlinks && Flash > 0.f)
	{
		Flash = FMath::Max(Flash - Dt * 0.25f, 0.f);
	}
	// squinting into the light: more the longer it lasts, gone as the eye adapts
	const float Want = bBlinks ? FMath::Max3(Smooth(0.5f, StareSec, Stare) * Smooth(0.1f, 0.4f, InGaze) * (1.f - Adapt), 0.35f * Smooth(0.3f, 0.9f, InGaze) * (1.f - Adapt), 0.75f * FMath::Min(Flash, 1.f)) : 0.f;
	Squint = FMath::FInterpTo(Squint, Want, Dt, 5.f);
	float Blink = 0.f;
	if (BlinkT >= 0.f)
	{
		// a slow frame must not skip the shut eyes: the blink stops shut for at least a frame
		BlinkT = BlinkT < BlinkClose ? FMath::Min(BlinkT + Dt, BlinkClose) : BlinkT + Dt;
		Blink = BlinkT < BlinkClose ? BlinkT / BlinkClose
			: BlinkT < BlinkClose + BlinkShut ? 1.f
			: 1.f - (BlinkT - BlinkClose - BlinkShut) / BlinkOpen;
		if (BlinkT > BlinkClose + BlinkShut + BlinkOpen)
		{
			BlinkT = -1.f;
			Blink = 0.f;
			if (Flash > 0.f)
			{
				Flash *= FMath::Clamp(CVarFlashStep.GetValueOnGameThread(), 0.f, 0.95f);
				Flash = Flash < 0.05f ? 0.f : Flash;
				NextFlashBlink = Time + FMath::FRandRange(0.8f, 1.3f);
			}
		}
	}
	Lids = FMath::Max(Squint * 0.42f, FMath::Clamp(Blink, 0.f, 1.f));
	if (Blink >= 0.999f && CVarFreezeAtShut.GetValueOnGameThread() > 0)
	{
		CVarFreeze->Set(1, ECVF_SetByConsole);
	}

	// the flinch with the "ай": the head dips and comes back (the pitch put on is all taken off again)
	if (FlinchT >= 0.f)
	{
		FlinchT += Dt;
		const float K = FMath::Min(FlinchT / FlinchSeconds, 1.f);
		const float Want2 = FlinchDeg * FMath::Sin(PI * K) * (1.f - 0.3f * K);
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC && Cast<AHeroCharacter>(PC->GetPawn()))
		{
			FRotator Rot = PC->GetControlRotation();
			Rot.Pitch += Want2 - FlinchDone;
			PC->SetControlRotation(Rot);
		}
		FlinchDone = Want2;
		if (K >= 1.f)
		{
			FlinchT = -1.f;
			FlinchDone = 0.f;
		}
	}
}

void ULyudSunGlare::Ow()
{
	const double Now = Time;
	UE_LOG(LogTemp, Display, TEXT("GLARE ow (stare %.1f, in eye %.2f)"), Stare, InEye);
	FlinchT = 0.f;
	FlinchDone = 0.f;
	if (CVarVoice.GetValueOnGameThread() <= 0 || Now - LastOw < CVarOwEvery.GetValueOnGameThread())
	{
		return;
	}
	LastOw = Now;
	OwIndex = OwIndex % 3 + 1;
	ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this);
	USoundBase* V = LyudText::LoadVoice(FString::Printf(TEXT("line.hero.sun_ow.%d"), OwIndex));
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (Sound && V && PC && PC->PlayerCameraManager)
	{
		Sound->PlayVoiceAt(V, PC->PlayerCameraManager->GetCameraLocation(), 1.f);
	}
	else if (!bLoggedVoice)
	{
		bLoggedVoice = true;
		UE_LOG(LogTemp, Warning, TEXT("GLARE: no voice line.hero.sun_ow.%d (unreal/scripts/import_voice.py)"), OwIndex);
	}
}

void ULyudSunGlare::UpdateRetina(float Dt)
{
	const float Expo = Bright * Visible * (bSunOnScreen || SunRadiusUV > 0.f ? 1.f : 0.f) * (GazeDeg < 90.f ? 1.f : 0.f);
	if (Expo <= 1e-3f && MaxBleach < 1e-3f && MaxFast < 1e-3f)
	{
		return;
	}
	const float KAfter = FMath::Max(CVarAfter.GetValueOnGameThread(), 0.f);
	// the fast (positive) afterimage follows the light: up in a quarter second, down in under one
	const float FastUp = 1.f - FMath::Exp(-Dt / 0.25f), FastDown = 1.f - FMath::Exp(-Dt / 0.8f);
	const float R = FMath::Max(SunRadiusUV, 1e-4f);
	float MB = 0.f, MF = 0.f;
	const FLinearColor Tint = SunTint;
	FColor* Out = Upload[UploadIndex].GetData();
	for (int32 J = 0; J < GH; J++)
	{
		const float V = (J + 0.5f) / GH;
		for (int32 I = 0; I < GW; I++)
		{
			const int32 K = J * GW + I;
			const float U = (I + 0.5f) / GW;
			const float D = FMath::Sqrt(FMath::Square(U - SunUV.X) + FMath::Square((V - SunUV.Y) / Aspect)) / R;
			const float Disk = Expo > 0.f ? 1.f - Smooth(0.6f, 1.25f, D) : 0.f;
			const float E = Expo * Disk;
			float& F = Fast[K];
			float& B = Bleach[K];
			F += (E - F) * (E > F ? FastUp : FastDown);
			if (E > 0.05f)
			{
				// bleaching: saturates over several seconds of the star on the same spot
				B += Dt * E * (1.f - B) / 5.f;
			}
			else
			{
				// and comes back, slower after a long look
				B -= Dt * B / (1.6f + 5.f * B);
			}
			if (B < 1e-4f)
			{
				B = 0.f;
			}
			MB = FMath::Max(MB, B);
			MF = FMath::Max(MF, F);
			// nothing over the star itself where it shines now (unless the lids are over it)
			const float Here = (1.f - Smooth(1.05f, 1.5f, D)) * Visible * (1.f - Lids) * (Expo > 0.f ? 1.f : 0.f);
			const float Pos = FMath::Max(F - E, 0.f) * (1.f - Here) * KAfter;
			const float Neg = B * (1.f - Here) * (1.f - FMath::Min(Pos * 1.5f, 1.f)) * KAfter;
			// the bleached spot shows dark in the complementary colours: violet in the middle, green-teal round it
			const float A = FMath::Clamp(Neg * 0.85f, 0.f, 0.62f);
			const float Mid = Smooth(0.12f, 0.55f, B);
			const FLinearColor Comp = FMath::Lerp(FLinearColor(0.05f, 0.2f, 0.13f), FLinearColor(0.16f, 0.07f, 0.26f), Mid);
			const FLinearColor C = Comp * A + Tint * FMath::Min(Pos * 0.9f, 1.f);
			FColor P = FLinearColor(FMath::Min(C.R, 1.f), FMath::Min(C.G, 1.f), FMath::Min(C.B, 1.f), 1.f).ToFColor(true);
			P.A = (uint8)FMath::RoundToInt(A * 255.f);
			Out[K] = P;
		}
	}
	MaxBleach = MB;
	MaxFast = MF;
	bRetinaLive = true;
}

void ULyudSunGlare::UploadRetina()
{
	if (!bRetinaLive || !RetinaTex || !RetinaTex->GetResource())
	{
		return;
	}
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (Now < NextUpload)
	{
		return;
	}
	NextUpload = Now + UploadEvery;
	static const FUpdateTextureRegion2D Region(0, 0, 0, 0, GW, GH);
	RetinaTex->UpdateTextureRegions(0, 1, &Region, GW * sizeof(FColor), sizeof(FColor), (uint8*)Upload[UploadIndex].GetData(),
		[](uint8*, const FUpdateTextureRegion2D*) {});
	// the next frame fills the next buffer; the render thread still reads this one
	const int32 Next = (UploadIndex + 1) % UE_ARRAY_COUNT(Upload);
	FMemory::Memcpy(Upload[Next].GetData(), Upload[UploadIndex].GetData(), GW * GH * sizeof(FColor));
	UploadIndex = Next;
	if (MaxBleach < 1e-3f && MaxFast < 1e-3f)
	{
		// all faded: the cleared map was the last upload
		bRetinaLive = false;
	}
}

void ULyudSunGlare::ResetEye()
{
	Paint = FPaint();
	Stare = Adapt = Lids = Squint = Flash = 0.f;
	BlinkT = FlinchT = -1.f;
	bAdapting = false;
	for (float& V : Bleach) V = 0.f;
	for (float& V : Fast) V = 0.f;
	for (TArray<FColor>& U : Upload)
	{
		FMemory::Memzero(U.GetData(), U.Num() * sizeof(FColor));
	}
	MaxBleach = MaxFast = 0.f;
	// one more (cleared) upload, then the map is off
	bRetinaLive = RetinaTex != nullptr;
}

void ULyudSunGlare::BeginFlashBlind(float Strength)
{
	if (CVarGlare.GetValueOnGameThread() <= 0)
	{
		return;
	}
	Flash = FMath::Max(Flash, FMath::Clamp(Strength, 0.f, 1.f));
	Adapt = 0.f;
	bAdapting = false;
	Stare = 0.f;
	// the first blink comes at once, the "ай" with it
	NextFlashBlink = Time + 0.35f;
	UE_LOG(LogTemp, Display, TEXT("GLARE flash blind %.2f"), Flash);
	Ow();
}

bool ULyudSunGlare::FlashBlindOnce(FName Key, float Strength)
{
	ULyudSunGlareMemory* Memory = ULyudSunGlareMemory::Get(this);
	if (Memory && Memory->FlashesSeen.Contains(Key))
	{
		return false;
	}
	if (Memory)
	{
		Memory->FlashesSeen.Add(Key);
	}
	BeginFlashBlind(Strength);
	return true;
}

ULyudSunGlareMemory* ULyudSunGlareMemory::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudSunGlareMemory>() : nullptr;
}

void ULyudSunGlareMemory::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("SunGlare"), this);
	}
}

void ULyudSunGlareMemory::Deinitialize()
{
	if (ULyudSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULyudSaveSubsystem>() : nullptr)
	{
		Saves->UnregisterSaveObject(TEXT("SunGlare"));
	}
	Super::Deinitialize();
}

FString ULyudSunGlare::GetDebugLine() const
{
	const ALyudSky* S = Sky.Get();
	return FString::Printf(TEXT("GLARE in eye %.2f gaze %.2f (%.1f deg, on screen %d at %.2f,%.2f r %.3f) visible %.2f bright %.2f | stare %.2f adapt %.2f%s lids %.2f flash %.2f | after fast %.2f bleach %.2f | gain %.2f"),
		InEye, InGaze, GazeDeg, bSunOnScreen ? 1 : 0, SunUV.X, SunUV.Y, SunRadiusUV, Visible, Bright, Stare, Adapt, bAdapting ? TEXT(" (adapting)") : TEXT(""),
		Lids, Flash, MaxFast, MaxBleach, S ? S->GetGlareDiskGain() : 0.f);
}
