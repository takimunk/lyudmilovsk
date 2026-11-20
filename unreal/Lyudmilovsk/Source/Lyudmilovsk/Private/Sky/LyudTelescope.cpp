#include "Sky/LyudTelescope.h"

#include "Audio/LyudSoundscape.h"
#include "Birds/BirdBook.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "City/LyudCityThings.h"
#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "Rendering/DrawElements.h"
#include "Sky/Ephemeris.h"
#include "Sky/LyudRareSky.h"
#include "Sky/LyudSky.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "TelescopeModel.h"
#include "UI/LyudQuests.h"
#include "UI/LyudUIStyle.h"
#include "Weather/LyudWeather.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#if __has_include("Birds/BirdLife.h")
#include "Birds/BirdLife.h"
#define LYUD_SCOPE_HAS_BIRDS 1
#else
#define LYUD_SCOPE_HAS_BIRDS 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudScope, Log, All);

/** What the eyepiece draws this frame (the subsystem fills it, the widget paints it). Places are tangent-plane offsets from
 *  the middle (LyudScope::Project): x right, y up. */
struct FLyudEyepieceView
{
	/** The eyepiece circle's radius over the screen's height, and the tangent of half its field. */
	float RadiusShare = 0.46f;
	float TanHalfField = 0.13f;
	/** The blur of a thing at infinity, as a share of the field's width. */
	float BlurShare = 0.f;

	struct FDot
	{
		FVector2f At = FVector2f::ZeroVector;
		/** Across, in pixels of a 1080-high screen. */
		float Size = 2.f;
		FLinearColor Color = FLinearColor::White;
	};
	TArray<FDot> Dots;

	struct FPicture
	{
		FVector2f At = FVector2f::ZeroVector;
		/** The disk's diameter (tangent units) and the picture's width over the disk's. */
		float DiskTan = 0.f;
		float PictureOverDisk = 1.15f;
		/** Counter-clockwise, radians. */
		float Turn = 0.f;
		int32 Brush = 0;
		float Alpha = 1.f;
		/** The point it shrinks to when it is too small to draw, and its glare. */
		float DotSize = 5.f;
		FLinearColor Color = FLinearColor::White;
	};
	TArray<FPicture> Pictures;

	/** The solar filter's dark glass 0..1, the white-out 0..1, the eyepiece's black coming up 0..1. */
	float Filter = 0.f;
	float White = 0.f;
	float Open = 0.f;

	FSlateBrush Mask;
	FSlateBrush Dot;
	FSlateBrush Body[5];

	FText Caption;
	FText Hint;
	FText Power;
	FText Keys;
};

namespace LyudScopeImpl
{
	TAutoConsoleVariable<int32> CVarDof(TEXT("lyud.Scope.Dof"), 1, TEXT("The telescope's focus blurs the view with the camera's depth of field (0: only the planets and stars blur)."));
	TAutoConsoleVariable<int32> CVarDebug(TEXT("lyud.Scope.Debug"), 0, TEXT("Logs the telescope's state every second while somebody looks."));

	const float MountForward = 80.f;        // cm from the instrument's middle to the camera, along the tube
	const float TraceLength = 600000.f;     // 6 km
	const int32 PictureSize = 192;
	const int32 MaxStars = 320;

	double Wrap180(double A)
	{
		A = FMath::Fmod(A, 360.0);
		if (A > 180.0) { A -= 360.0; }
		if (A < -180.0) { A += 360.0; }
		return A;
	}

	LyudScope::FLog ToModel(const ULyudSkyLog& Log)
	{
		LyudScope::FLog L;
		L.Seen = static_cast<uint32>(Log.SeenBits);
		L.JupiterNights = Log.JupiterNights;
		L.LastJupiterNight = Log.LastJupiterNight;
		return L;
	}

	/** The eyepiece: what the sky material does not draw, the black round the field, the filter and the white-out. */
	class SEyepiece : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SEyepiece) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, TSharedPtr<FLyudEyepieceView> InView)
		{
			View = InView;
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(16.0, 16.0); }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer,
			const FWidgetStyle&, bool) const override
		{
			const FLyudEyepieceView* V = View.Get();
			if (!V)
			{
				return Layer;
			}
			const FVector2f Size = FVector2f(Geo.GetLocalSize());
			const FVector2f Mid = Size * 0.5f;
			const float R = V->RadiusShare * Size.Y;
			const float Scale = R / FMath::Max(V->TanHalfField, 1e-6f);
			const float Px = Size.Y / 1080.f;
			const float BlurPx = V->BlurShare * 2.f * R;
			const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
			auto Box = [&](int32 L, const FVector2f& At, const FVector2f& Sz, const FSlateBrush* Brush, const FLinearColor& Tint)
			{
				FSlateDrawElement::MakeBox(Out, L, Geo.ToPaintGeometry(Sz, FSlateLayoutTransform(At)), Brush, ESlateDrawEffect::None, Tint);
			};
			auto DotAt = [&](int32 L, const FVector2f& Tan, float Across, FLinearColor Tint)
			{
				const FVector2f At = Mid + FVector2f(Tan.X, -Tan.Y) * Scale;
				if ((At - Mid).SizeSquared() > (R + Across) * (R + Across))
				{
					return;
				}
				// out of focus a point spreads into a disk of the same light
				const float Wide = Across + BlurPx;
				Tint.A *= FMath::Clamp((Across * Across) / (Wide * Wide), 0.02f, 1.f) * V->Open;
				Box(L, At - FVector2f(Wide, Wide) * 0.5f, FVector2f(Wide, Wide), &V->Dot, Tint);
			};
			// the solar filter's dark glass over the game's own view
			if (V->Filter > 0.f)
			{
				Box(Layer, FVector2f::ZeroVector, Size, White, FLinearColor(0.04f, 0.012f, 0.f, 0.86f * V->Filter));
			}
			++Layer;
			for (const FLyudEyepieceView::FDot& D : V->Dots)
			{
				DotAt(Layer, D.At, D.Size * Px, D.Color);
			}
			++Layer;
			for (const FLyudEyepieceView::FPicture& P : V->Pictures)
			{
				const float DiskPx = P.DiskTan * Scale;
				FLinearColor Tint = P.Color;
				Tint.A = P.Alpha;
				// judged by the whole picture (Saturn's ring is 2.45 of its disk: at x60 the ring shows though the ball is ~4 px)
				if (DiskPx * P.PictureOverDisk < 6.f)
				{
					// too small to be a disk: a bright point with a little glare
					FLinearColor Glare = Tint;
					Glare.A *= 0.22f;
					DotAt(Layer, P.At, P.DotSize * 2.6f * Px, Glare);
					DotAt(Layer, P.At, P.DotSize * Px, Tint);
					continue;
				}
				const FVector2f At = Mid + FVector2f(P.At.X, -P.At.Y) * Scale;
				// the sharp picture gives way to a soft ball of its light as the focus goes
				const float Soft = FMath::Clamp(BlurPx / FMath::Max(DiskPx * 0.35f, 1.f), 0.f, 1.f);
				if (Soft > 0.f)
				{
					const float Wide = DiskPx * 1.2f + BlurPx;
					FLinearColor Ball = Tint;
					Ball.A *= Soft * FMath::Clamp((DiskPx * DiskPx) / (Wide * Wide) * 1.6f, 0.05f, 0.9f) * V->Open;
					Box(Layer, At - FVector2f(Wide, Wide) * 0.5f, FVector2f(Wide, Wide), &V->Dot, Ball);
				}
				if (Soft < 1.f && V->Body[P.Brush].GetResourceObject())
				{
					const float Side = DiskPx * P.PictureOverDisk;
					FSlateDrawElement::MakeRotatedBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2f(Side, Side), FSlateLayoutTransform(At - FVector2f(Side, Side) * 0.5f)),
						&V->Body[P.Brush], ESlateDrawEffect::None, -P.Turn, TOptional<FVector2f>(), FSlateDrawElement::RelativeToElement,
						FLinearColor(1.f, 1.f, 1.f, P.Alpha * (1.f - Soft) * V->Open));
				}
			}
			Layer += 2;
			// the black round the field: the mask's hole is 0.96 of its side
			const float Side = 2.f * R / 0.96f;
			const FVector2f Corner = Mid - FVector2f(Side, Side) * 0.5f;
			const FLinearColor Black(0.f, 0.f, 0.f, V->Open);
			Box(Layer, Corner, FVector2f(Side, Side), &V->Mask, FLinearColor(1.f, 1.f, 1.f, V->Open));
			if (Corner.X > 0.f)
			{
				Box(Layer, FVector2f(0.f, 0.f), FVector2f(Corner.X + 1.f, Size.Y), White, Black);
				Box(Layer, FVector2f(Corner.X + Side - 1.f, 0.f), FVector2f(Size.X - Corner.X - Side + 1.f, Size.Y), White, Black);
			}
			if (Corner.Y > 0.f)
			{
				Box(Layer, FVector2f(0.f, 0.f), FVector2f(Size.X, Corner.Y + 1.f), White, Black);
				Box(Layer, FVector2f(0.f, Corner.Y + Side - 1.f), FVector2f(Size.X, Size.Y - Corner.Y - Side + 1.f), White, Black);
			}
			++Layer;
			if (V->White > 0.f)
			{
				Box(Layer, FVector2f::ZeroVector, Size, White, FLinearColor(1.f, 0.97f, 0.9f, V->White));
			}
			return Layer + 1;
		}

	private:
		TSharedPtr<FLyudEyepieceView> View;
	};

	UTexture2D* MakeTexture(int32 Size, const TArray<FColor>& Pixels, const TCHAR* Name)
	{
		UTexture2D* T = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8, FName(Name));
		if (!T)
		{
			return nullptr;
		}
		T->SRGB = true;
		T->AddressX = TA_Clamp;
		T->AddressY = TA_Clamp;
		T->NeverStream = true;
		FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
		void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Pixels.GetData(), static_cast<SIZE_T>(Size) * Size * 4);
		Mip.BulkData.Unlock();
		T->UpdateResource();
		return T;
	}

	void FillTexture(UTexture2D* T, const TArray<FColor>& Pixels)
	{
		if (!T || !T->GetPlatformData() || T->GetPlatformData()->Mips.Num() == 0)
		{
			return;
		}
		FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
		void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Pixels.GetData(), static_cast<SIZE_T>(Pixels.Num()) * 4);
		Mip.BulkData.Unlock();
		T->UpdateResource();
	}

	// --- console

	FAutoConsoleCommandWithWorldAndArgs CmdLook(TEXT("lyud.Scope.Look"),
		TEXT("Puts a telescope where the hero stands, looking where he looks: lyud.Scope.Look [coin|sun]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ULyudTelescope* Scope = ULyudTelescope::Get(World);
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (!Scope || !PC || !PC->PlayerCameraManager)
			{
				return;
			}
			const FString Kind = Args.Num() > 0 ? (TEXT("scope_") + Args[0]) : FString(TEXT("scope"));
			Scope->Begin(PC->PlayerCameraManager->GetCameraLocation() - PC->PlayerCameraManager->GetCameraRotation().Vector() * 60.f,
				PC->PlayerCameraManager->GetCameraRotation().Yaw, Kind, FText::GetEmpty());
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAim(TEXT("lyud.Scope.Aim"),
		TEXT("Aims the telescope: lyud.Scope.Aim <moon|sun|venus|mars|jupiter|saturn> or <altitude> <azimuth>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ULyudTelescope* Scope = ULyudTelescope::Get(World);
			if (!Scope || Args.Num() == 0)
			{
				return;
			}
			if (Args.Num() >= 2)
			{
				Scope->AimAt(FCString::Atod(*Args[0]), FCString::Atod(*Args[1]));
			}
			else if (!Scope->AimAtBody(Args[0]))
			{
				UE_LOG(LogLyudScope, Warning, TEXT("SCOPE no body %s"), *Args[0]);
			}
			UE_LOG(LogLyudScope, Display, TEXT("SCOPE %s"), *Scope->Report());
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdPower(TEXT("lyud.Scope.Power"), TEXT("The eyepiece: 0 (x3) .. 3 (x400)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudTelescope* Scope = ULyudTelescope::Get(World); Scope && Args.Num() > 0)
			{
				Scope->SetEyepiece(FCString::Atoi(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdFocus(TEXT("lyud.Scope.Focus"), TEXT("The focus knob 0..1 (0.85: infinity)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudTelescope* Scope = ULyudTelescope::Get(World); Scope && Args.Num() > 0)
			{
				Scope->SetFocus(FCString::Atof(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdEnd(TEXT("lyud.Scope.End"), TEXT("Steps back from the telescope."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudTelescope* Scope = ULyudTelescope::Get(World))
			{
				Scope->End();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdLog(TEXT("lyud.Scope.Log"), TEXT("Prints the observation log; clear empties it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ULyudSkyLog* Log = ULyudSkyLog::Get(World);
			if (!Log)
			{
				return;
			}
			if (Args.Num() > 0 && Args[0] == TEXT("clear"))
			{
				Log->Reset();
			}
			UE_LOG(LogLyudScope, Display, TEXT("SCOPE log: %d of %d seen, Jupiter's moons on %d nights"), Log->NumSeen(),
				static_cast<int32>(LyudScope::ESight::Num), Log->JupiterNights);
			for (const FLyudSkySight& S : Log->Sights)
			{
				UE_LOG(LogLyudScope, Display, TEXT("SCOPE   %s  %s"), *S.When.ToString(TEXT("%Y-%m-%d %H:%M")), *S.Sight.ToString());
			}
		}));

	// --- developer mode (docs/dev_mode.md): a telescope anywhere, and aimed at what is up
	LyudDev::FAutoAction GDevLook(TEXT("scope.look"), TEXT("scope"), TEXT("dev.action.scope.look"),
		[](UWorld* World)
		{
			ULyudTelescope* Scope = ULyudTelescope::Get(World);
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (!Scope || !PC || !PC->PlayerCameraManager)
			{
				return;
			}
			if (Scope->IsLooking())
			{
				Scope->End();
				return;
			}
			Scope->Begin(PC->PlayerCameraManager->GetCameraLocation() - PC->PlayerCameraManager->GetCameraRotation().Vector() * 60.f,
				PC->PlayerCameraManager->GetCameraRotation().Yaw, TEXT("scope"), FText::GetEmpty());
		},
		[](UWorld* World) { const ULyudTelescope* Scope = ULyudTelescope::Get(World); return Scope && Scope->IsLooking(); });

	LyudDev::FAutoAction GDevBest(TEXT("scope.best"), TEXT("scope"), TEXT("dev.action.scope.best"),
		[](UWorld* World)
		{
			ULyudTelescope* Scope = ULyudTelescope::Get(World);
			if (!Scope || !Scope->IsLooking())
			{
				return;
			}
			for (const TCHAR* Body : { TEXT("jupiter"), TEXT("saturn"), TEXT("venus"), TEXT("mars"), TEXT("moon") })
			{
				if (Scope->AimAtBody(Body))
				{
					Scope->SetFocus(static_cast<float>(LyudScope::InfinityKnob));
					break;
				}
			}
		});
}

// ------------------------------------------------------------------ the log

ULyudSkyLog* ULyudSkyLog::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudSkyLog>() : nullptr;
}

void ULyudSkyLog::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("SkyLog"), this);
		// this subsystem outlives the level: a new game starts with an empty log
		Saves->OnNewGame.AddWeakLambda(this, [this]() { Reset(); });
	}
}

bool ULyudSkyLog::HasSeen(FName Sight) const
{
	for (int32 I = 0; I < static_cast<int32>(LyudScope::ESight::Num); ++I)
	{
		if (Sight == FName(UTF8_TO_TCHAR(LyudScope::SightId(static_cast<LyudScope::ESight>(I)))))
		{
			return (SeenBits >> I) & 1;
		}
	}
	return false;
}

int32 ULyudSkyLog::NumSeen() const
{
	return LyudScopeImpl::ToModel(*this).Count();
}

void ULyudSkyLog::Reset()
{
	SeenBits = 0;
	JupiterNights = 0;
	LastJupiterNight = -1000000;
	Sights.Reset();
}

// ------------------------------------------------------------------ the telescope

ULyudTelescope* ULyudTelescope::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudTelescope>() : nullptr;
}

void ULyudTelescope::Deinitialize()
{
	End();
	Super::Deinitialize();
}

ALyudSky* ULyudTelescope::FindSky()
{
	if (!Sky.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			TActorIterator<ALyudSky> It(World);
			Sky = It ? *It : nullptr;
		}
	}
	return Sky.Get();
}

FVector ULyudTelescope::Direction(double AltDeg, double AzDeg) const
{
	// without a sky: north is +Y, east is -X (ALyudSky's default frame)
	const FVector Flat = Sky.IsValid() ? Sky->BearingDirection(AzDeg)
		: FVector(-FMath::Sin(FMath::DegreesToRadians(AzDeg)), FMath::Cos(FMath::DegreesToRadians(AzDeg)), 0.0);
	return (Flat * FMath::Cos(FMath::DegreesToRadians(AltDeg)) + FVector::UpVector * FMath::Sin(FMath::DegreesToRadians(AltDeg))).GetSafeNormal();
}

void ULyudTelescope::MakeBrushes()
{
	if (!MaskTexture)
	{
		// black with a round hole 0.96 of the side across: a soft edge, a little darkening toward it inside
		const int32 N = 1024;
		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(N * N);
		for (int32 Y = 0; Y < N; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				const float D = FMath::Sqrt(FMath::Square(X + 0.5f - N * 0.5f) + FMath::Square(Y + 0.5f - N * 0.5f)) / (N * 0.5f * 0.96f);
				const float Edge = FMath::SmoothStep(0.985f, 1.005f, D);
				const float Vignette = 0.5f * FMath::Pow(FMath::Clamp(D, 0.f, 1.f), 7.f);
				Pixels[Y * N + X] = FColor(0, 0, 0, static_cast<uint8>(FMath::RoundToInt(255.f * FMath::Clamp(FMath::Max(Edge, Vignette), 0.f, 1.f))));
			}
		}
		MaskTexture = LyudScopeImpl::MakeTexture(N, Pixels, TEXT("T_LyudEyepieceMask"));
	}
	if (!DotTexture)
	{
		// a soft point of light
		const int32 N = 64;
		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(N * N);
		for (int32 Y = 0; Y < N; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				const float D = FMath::Sqrt(FMath::Square(X + 0.5f - N * 0.5f) + FMath::Square(Y + 0.5f - N * 0.5f)) / (N * 0.5f);
				const float A = FMath::Clamp(1.f - D, 0.f, 1.f);
				Pixels[Y * N + X] = FColor(255, 255, 255, static_cast<uint8>(FMath::RoundToInt(255.f * FMath::Clamp(A * A * (1.6f - 0.6f * A) * 1.25f, 0.f, 1.f))));
			}
		}
		DotTexture = LyudScopeImpl::MakeTexture(N, Pixels, TEXT("T_LyudEyepieceDot"));
	}
	if (View.IsValid())
	{
		View->Mask.SetResourceObject(MaskTexture);
		View->Mask.ImageSize = FVector2D(1024.0, 1024.0);
		View->Mask.DrawAs = ESlateBrushDrawType::Image;
		View->Dot.SetResourceObject(DotTexture);
		View->Dot.ImageSize = FVector2D(64.0, 64.0);
		View->Dot.DrawAs = ESlateBrushDrawType::Image;
		for (int32 I = 0; I < 5; ++I)
		{
			View->Body[I].SetResourceObject(Pictures[I]);
			View->Body[I].ImageSize = FVector2D(LyudScopeImpl::PictureSize, LyudScopeImpl::PictureSize);
			View->Body[I].DrawAs = ESlateBrushDrawType::Image;
		}
	}
}

bool ULyudTelescope::Begin(const FVector& At, float FacingYaw, const FString& Kind, const FText& Caption)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!World || !PC || !PC->GetPawn() || bLooking)
	{
		return false;
	}
	FindSky();
	bFilter = Kind == TEXT("scope_sun");
	MaxEyepiece = Kind == TEXT("scope_coin") ? 2 : LyudScope::NumEyepieces - 1;
	EyepieceIndex = 0;
	Knob = FMath::FRandRange(0.3f, 0.55f);     // whoever looked last left it somewhere else
	Mount = At;
	WhiteOut = -1.f;
	WatchSight = -1;
	WatchHeld = 0.0;
	bWatchCounted = false;
	FollowBody = -1;
	BirdHeld = -1;
	BirdHeldFor = 0.f;
	TraceClock = 0.f;
	BirdClock = 0.f;
	AimVAlt = AimVAz = 0.0;

	// the way it looks at first: the instrument's facing, or the nearest way round that is not a wall
	const FVector North = Direction(0.0, 0.0), East = Direction(0.0, 90.0);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(LyudScopeBegin), false, PC->GetPawn());
	float BestYaw = FacingYaw;
	for (const float Turn : { 0.f, 45.f, -45.f, 90.f, -90.f, 135.f, -135.f, 180.f })
	{
		const FVector Dir = FRotator(4.f, FacingYaw + Turn, 0.f).Vector();
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, At + Dir * 30.f, At + Dir * 4000.f, ECC_Visibility, Query))
		{
			BestYaw = FacingYaw + Turn;
			break;
		}
	}
	const FVector Flat = FRotator(0.f, BestYaw, 0.f).Vector();
	AimAz = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Flat, East), FVector::DotProduct(Flat, North)));
	AimAlt = bFilter && Sky.IsValid() ? Sky->SunAltitude() : 3.0;
	if (bFilter && Sky.IsValid() && Sky->SunAltitude() > 0.f)
	{
		AimAz = Sky->SunAzimuth();     // the sun telescope is left on the sun
	}

	// the level's camera (what the exposure was set up with): the highest unbound volume that sets it, else the defaults
	{
		BaseFstop = 4.f;
		BaseIso = 100.f;
		float FstopPrio = -1e30f, IsoPrio = -1e30f;
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			if (!It->bUnbound || !It->bEnabled)
			{
				continue;
			}
			if (It->Settings.bOverride_DepthOfFieldFstop && It->Priority > FstopPrio)
			{
				FstopPrio = It->Priority;
				BaseFstop = It->Settings.DepthOfFieldFstop;
			}
			if (It->Settings.bOverride_CameraISO && It->Priority > IsoPrio)
			{
				IsoPrio = It->Priority;
				BaseIso = It->Settings.CameraISO;
			}
		}
	}

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Spawn.ObjectFlags |= RF_Transient;
	Camera = World->SpawnActor<ACameraActor>(At, Direction(AimAlt, AimAz).Rotation(), Spawn);
	if (!Camera)
	{
		return false;
	}
	if (UCameraComponent* Lens = Camera->GetCameraComponent())
	{
		Lens->bConstrainAspectRatio = false;
		Lens->PostProcessBlendWeight = 1.f;
	}
	FieldNow = static_cast<float>(LyudScope::Eyepiece(0).FieldDeg) * 2.2f;     // the first moments: the view narrows onto the field
	bLooking = true;
	BeginFrame = GFrameCounter;
	BeganAt = World->GetTimeSeconds();
	UpdateCamera(0.f);
	PC->SetViewTargetWithBlend(Camera, 0.3f, VTBlend_Cubic);
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	LockedPC = PC;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, FText::GetEmpty());
		UI->SetExternalScreen(this);
	}
	if (Sky.IsValid())
	{
		// the dome's star texture would be blobs at this power: the field stars are drawn by the eyepiece instead
		SavedStars = Sky->StarBrightness;
		Sky->StarBrightness = 0.f;
		// the member only seeds the material instance when it is made: the dome reads the parameter
		if (UMaterialInstanceDynamic* Mid = Sky->GetSkyMID())
		{
			Mid->SetScalarParameterValue(TEXT("StarBrightness"), 0.f);
		}
	}

	View = MakeShared<FLyudEyepieceView>();
	View->Keys = LyudText::Get(TEXT("scope.keys"));
	View->Caption = Caption;
	SayLeft = Caption.IsEmpty() ? 0.f : 9.f;
	MakeBrushes();
	if (Viewport)
	{
		TSharedPtr<FLyudEyepieceView> Shown = View;
		Widget = SNew(SOverlay)
			.Visibility(EVisibility::HitTestInvisible)
			+ SOverlay::Slot()
			[
				SNew(LyudScopeImpl::SEyepiece, Shown)
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 64.f))
			[
				SNew(SBox).WidthOverride(900.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(LyudUI::Font(20)).ColorAndOpacity(LyudUI::Bright).AutoWrapText(true).Justification(ETextJustify::Center)
						.ShadowOffset(FVector2D(1.0, 1.0)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
						.Text_Lambda([Shown]() { return Shown->Caption; })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 6.f, 0.f, 0.f))
					[
						SNew(STextBlock).Font(LyudUI::Font(14)).ColorAndOpacity(LyudUI::Dim).Justification(ETextJustify::Center)
						.Text_Lambda([Shown]() { return Shown->Hint; })
					]
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 36.f, 26.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(STextBlock).Font(LyudUI::Font(22, "Bold")).ColorAndOpacity(LyudUI::Text).Text_Lambda([Shown]() { return Shown->Power; })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(FMargin(0.f, 4.f, 0.f, 0.f))
				[
					SNew(STextBlock).Font(LyudUI::Font(12)).ColorAndOpacity(LyudUI::Faint).Text_Lambda([Shown]() { return Shown->Keys; })
				]
			];
		Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), 58);
	}
	UE_LOG(LogLyudScope, Display, TEXT("SCOPE begin %s at %s facing %.0f -> az %.1f"), *Kind, *At.ToCompactString(), FacingYaw, AimAz);
	return true;
}

void ULyudTelescope::End()
{
	if (!bLooking)
	{
		return;
	}
	bLooking = false;
	UWorld* World = GetWorld();
	if (APlayerController* PC = LockedPC.Get())
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		if (APawn* Pawn = PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(Pawn, 0.f);
		}
	}
	LockedPC.Reset();
	if (Camera)
	{
		Camera->Destroy();
		Camera = nullptr;
	}
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetExternalScreen(nullptr);
	}
	if (Sky.IsValid() && SavedStars >= 0.f)
	{
		Sky->StarBrightness = SavedStars;
		if (UMaterialInstanceDynamic* Mid = Sky->GetSkyMID())
		{
			Mid->SetScalarParameterValue(TEXT("StarBrightness"), SavedStars);
		}
	}
	SavedStars = -1.f;
	if (Widget.IsValid())
	{
		if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
		}
		Widget.Reset();
	}
	View.Reset();
	UE_LOG(LogLyudScope, Display, TEXT("SCOPE end"));
}

void ULyudTelescope::AimAt(double AltDeg, double AzDeg)
{
	AimAlt = FMath::Clamp(AltDeg, -14.0, 82.0);
	AimAz = AzDeg;
	AimVAlt = AimVAz = 0.0;
	FollowBody = -1;
}

bool ULyudTelescope::AimAtBody(const FString& Body)
{
	ALyudSky* S = FindSky();
	if (!S)
	{
		return false;
	}
	if (Body == TEXT("sun"))
	{
		AimAt(S->SunAltitude(), S->SunAzimuth());
		return true;
	}
	const LyudScope::FSky Now = LyudScope::Sky(LyudEphemeris::DayNumber(S->ShownUtc()), S->LatitudeDeg, S->LongitudeDeg);
	if (Body == TEXT("moon"))
	{
		const FVector Dir = S->MoonDirection();
		const FVector North = Direction(0.0, 0.0), East = Direction(0.0, 90.0);
		AimAt(S->MoonAltitude(), FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Dir, East), FVector::DotProduct(Dir, North))));
		return S->MoonAltitude() > 0.f;
	}
	for (int32 I = 1; I < static_cast<int32>(LyudScope::EBody::Num); ++I)
	{
		if (Body == UTF8_TO_TCHAR(LyudScope::BodyId(static_cast<LyudScope::EBody>(I))))
		{
			AimAt(Now.Bodies[I].AltDeg, Now.Bodies[I].AzDeg);
			return Now.Bodies[I].AltDeg > 2.0;
		}
	}
	return false;
}

void ULyudTelescope::SetEyepiece(int32 Index)
{
	EyepieceIndex = FMath::Clamp(Index, 0, MaxEyepiece);
}

void ULyudTelescope::SetFocus(float InKnob)
{
	Knob = FMath::Clamp(InKnob, 0.f, 1.f);
}

FString ULyudTelescope::Report() const
{
	return FString::Printf(TEXT("looking=%d alt=%.2f az=%.2f x%d field=%.2f knob=%.3f focus=%.0fm blocked=%d (%.0f m) filter=%d watch=%d held=%.1f"),
		bLooking ? 1 : 0, AimAlt, AimAz, LyudScope::Eyepiece(EyepieceIndex).Power, FieldNow, Knob, LyudScope::FocusDistanceM(Knob),
		bBlocked ? 1 : 0, BlockedDistance / 100.f, bFilter ? 1 : 0, WatchSight, WatchHeld);
}

void ULyudTelescope::Say(const FText& Text, float Seconds)
{
	if (View.IsValid())
	{
		View->Caption = Text;
		SayLeft = Seconds;
	}
}

void ULyudTelescope::UpdateCamera(float DeltaTime)
{
	UWorld* World = GetWorld();
	UCameraComponent* Lens = Camera ? Camera->GetCameraComponent() : nullptr;
	if (!World || !Lens)
	{
		return;
	}
	// an eyepiece change is a short zoom
	const float Field = static_cast<float>(LyudScope::Eyepiece(EyepieceIndex).FieldDeg);
	FieldNow = DeltaTime > 0.f ? FMath::Exp(FMath::FInterpTo(FMath::Loge(FieldNow), FMath::Loge(Field), DeltaTime, 9.f)) : FieldNow;
	if (FMath::Abs(FieldNow - Field) < Field * 0.003f)
	{
		FieldNow = Field;
	}
	const FVector Dir = Direction(AimAlt, AimAz);
	// the camera sits at the objective's end; never through a wall
	FVector At = Mount + Dir * LyudScopeImpl::MountForward;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(LyudScopeCamera), false, LockedPC.IsValid() ? LockedPC->GetPawn() : nullptr);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Mount, At, ECC_Visibility, Query))
	{
		At = Hit.ImpactPoint - Dir * 6.f;
	}
	Camera->SetActorLocationAndRotation(At, Dir.Rotation());

	// the field fills the eyepiece's circle: RadiusShare of the screen's height is tan(field / 2)
	FVector2D Size(1920.0, 1080.0);
	if (UGameViewportClient* Viewport = World->GetGameViewport())
	{
		Viewport->GetViewportSize(Size);
	}
	const float Aspect = Size.Y > 1.0 ? static_cast<float>(Size.X / Size.Y) : 16.f / 9.f;
	const float RadiusShare = View.IsValid() ? View->RadiusShare : 0.46f;
	const float TanHalfField = FMath::Tan(FMath::DegreesToRadians(FieldNow * 0.5f));
	const float TanHalfV = TanHalfField / (2.f * RadiusShare);
	const float FovV = 2.f * FMath::RadiansToDegrees(FMath::Atan(TanHalfV));
	const float FovH = 2.f * FMath::RadiansToDegrees(FMath::Atan(TanHalfV * Aspect));
	const ULocalPlayer* Player = LockedPC.IsValid() ? LockedPC->GetLocalPlayer() : nullptr;
	const bool bVertical = Player && Player->AspectRatioAxisConstraint == AspectRatio_MaintainYFOV;
	FovNow = FovH;
	Lens->SetFieldOfView(bVertical ? FovV : FovH);
	if (View.IsValid())
	{
		View->TanHalfField = TanHalfField;
		View->BlurShare = static_cast<float>(LyudScope::Blur(Knob, 0.0, FieldNow));
	}

	// the focus: the camera's depth of field with the telescope's aperture (the blur's angle is aperture x the dioptres off)
	FPostProcessSettings& Post = Lens->PostProcessSettings;
	const bool bDof = LyudScopeImpl::CVarDof.GetValueOnGameThread() != 0;
	const float SensorMm = 24.89f;
	const float FocalMm = SensorMm * 0.5f / FMath::Max(FMath::Tan(FMath::DegreesToRadians(FovH * 0.5f)), 1e-5f);
	const double Dioptre = LyudScope::FocusDioptre(Knob);
	Post.bOverride_DepthOfFieldFocalDistance = bDof;
	Post.bOverride_DepthOfFieldFstop = bDof;
	Post.bOverride_DepthOfFieldSensorWidth = bDof;
	Post.bOverride_DepthOfFieldMinFstop = bDof;
	Post.DepthOfFieldSensorWidth = SensorMm;
	Post.DepthOfFieldFstop = FMath::Clamp(FocalMm / static_cast<float>(LyudScope::ApertureM * 1000.0), 1.f, 32.f);
	Post.DepthOfFieldMinFstop = 1.f;
	// the level's exposure is manual with the physical camera on: the f-number above would change the brightness too
	// (f/1 at x3 is +4 EV, the night sky turns to day). The ISO takes it back: N^2 / ISO stays the level's.
	Post.bOverride_CameraISO = bDof;
	Post.CameraISO = BaseIso * FMath::Square(Post.DepthOfFieldFstop / FMath::Max(BaseFstop, 0.5f));
	// past infinity nothing is sharp: the same blur as focusing that near
	Post.DepthOfFieldFocalDistance = static_cast<float>(100.0 / FMath::Max(FMath::Abs(Dioptre), 1e-6));
}

void ULyudTelescope::UpdateTextures()
{
	ALyudSky* S = Sky.Get();
	if (!S || !View.IsValid())
	{
		return;
	}
	const LyudScope::FSky Now = LyudScope::Sky(LyudEphemeris::DayNumber(S->ShownUtc()), S->LatitudeDeg, S->LongitudeDeg);
	for (int32 I = 1; I < static_cast<int32>(LyudScope::EBody::Num); ++I)
	{
		const LyudScope::EBody Body = static_cast<LyudScope::EBody>(I);
		// only what is up, and only when its look has changed (a planet's picture changes over minutes or weeks)
		const uint32 Key = LyudScope::DrawKey(Body, Now.Bodies[I]);
		if (Now.Bodies[I].AltDeg < 0.0 || (Pictures[I] && PictureKeys[I] == Key))
		{
			continue;
		}
		LyudScope::FImage Image;
		LyudScope::Draw(Body, Now.Bodies[I], LyudScopeImpl::PictureSize, Image);
		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(LyudScopeImpl::PictureSize * LyudScopeImpl::PictureSize);
		for (int32 P = 0; P < Pixels.Num(); ++P)
		{
			Pixels[P] = FColor(Image.Rgba[P * 4], Image.Rgba[P * 4 + 1], Image.Rgba[P * 4 + 2], Image.Rgba[P * 4 + 3]);
		}
		if (!Pictures[I])
		{
			Pictures[I] = LyudScopeImpl::MakeTexture(LyudScopeImpl::PictureSize, Pixels, *FString::Printf(TEXT("T_LyudScopeBody%d"), I));
			View->Body[I].SetResourceObject(Pictures[I]);
			View->Body[I].ImageSize = FVector2D(LyudScopeImpl::PictureSize, LyudScopeImpl::PictureSize);
			View->Body[I].DrawAs = ESlateBrushDrawType::Image;
		}
		else
		{
			LyudScopeImpl::FillTexture(Pictures[I], Pixels);
		}
		PictureKeys[I] = Key;
	}
}

void ULyudTelescope::Noted(int32 SightIndex, bool bFirst, bool bNewNight)
{
	const LyudScope::ESight Sight = static_cast<LyudScope::ESight>(SightIndex);
	const FString Id = UTF8_TO_TCHAR(LyudScope::SightId(Sight));
	UWorld* World = GetWorld();
	ULyudSkyLog* Log = ULyudSkyLog::Get(this);
	if (!World || !Log || Id.IsEmpty())
	{
		return;
	}
	FLyudSkySight Entry;
	Entry.Sight = FName(*Id);
	Entry.When = UGameCalendar::NowIn(World);
	Log->Sights.Add(Entry);
	UE_LOG(LogLyudScope, Display, TEXT("SCOPE seen %s first=%d night=%d (%d sights, %d Jupiter nights)"), *Id, bFirst ? 1 : 0, bNewNight ? 1 : 0,
		Log->NumSeen(), Log->JupiterNights);
	ULyudQuests* Quests = ULyudQuests::Get(this);
	TArray<FString> Args;
	if (Sight == LyudScope::ESight::JupiterMoons && Sky.IsValid())
	{
		const LyudScope::FSky Now = LyudScope::Sky(LyudEphemeris::DayNumber(Sky->ShownUtc()), Sky->LatitudeDeg, Sky->LongitudeDeg);
		Args.Add(UTF8_TO_TCHAR(LyudScope::MoonsDiagram(Now.Of(LyudScope::EBody::Jupiter)).c_str()));
		Args.Add(FString::FromInt(Log->JupiterNights));
	}
	if (bFirst)
	{
		if (Quests)
		{
			Quests->AddNote(TEXT("journal.sky.") + Id, Args);
		}
		if (ULyudCityThings* Things = ULyudCityThings::Get(this))
		{
			Things->GiveStamp(TEXT("sky/") + Id);
		}
		ULyudQuests::Notify(this, TEXT("observe"), Id);
	}
	else if (bNewNight && Quests)
	{
		Quests->AddNote(TEXT("journal.sky.jupiter_night"), Args);
	}
	if (bNewNight)
	{
		ULyudQuests::Notify(this, TEXT("observe"), TEXT("jupiter_night"));
		if (Log->JupiterNights >= 4)
		{
			// the fourth night's drawing: the stamp the quest "Four moons of Jupiter" is told of at the market
			if (ULyudCityThings* Things = ULyudCityThings::Get(this))
			{
				Things->GiveStamp(TEXT("sky/jupiter_four"));
			}
		}
	}
	if (bFirst || bNewNight)
	{
		Say(LyudText::Get((bFirst ? TEXT("scope.seen.") : TEXT("scope.again.")) + Id), 8.f);
		if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
		{
			Sound->PlayEffect(TEXT("diary_page"), Mount);
		}
	}
}

void ULyudTelescope::UpdateSkyView()
{
	ALyudSky* S = Sky.Get();
	UWorld* World = GetWorld();
	if (!S || !World || !View.IsValid())
	{
		return;
	}
	namespace Sc = LyudScope;
	Sc::FSky Now = Sc::Sky(LyudEphemeris::DayNumber(S->ShownUtc()), S->LatitudeDeg, S->LongitudeDeg);
	// the sun and the moon are the sky actor's own (the same the dome draws)
	Now.SunAltDeg = S->SunAltitude();
	Now.SunAzDeg = S->SunAzimuth();
	{
		const FVector Dir = S->MoonDirection();
		const FVector North = Direction(0.0, 0.0), East = Direction(0.0, 90.0);
		Sc::SetMoon(Now, S->MoonAltitude(), FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Dir, East), FVector::DotProduct(Dir, North))),
			S->MoonSizeNowDeg(), S->MoonLitFraction());
	}
	// the tube is nudged after the body it was left on, as the sky turns
	if (FollowBody >= 0)
	{
		const Sc::FBody& B = Now.Bodies[FollowBody];
		AimAz += LyudScopeImpl::Wrap180(B.AzDeg - FollowAz);
		AimAlt += B.AltDeg - FollowAlt;
		FollowAz = B.AzDeg;
		FollowAlt = B.AltDeg;
	}

	Sc::FLook Look;
	Look.Aim.AltDeg = AimAlt;
	Look.Aim.AzDeg = AimAz;
	Look.Eyepiece = EyepieceIndex;
	Look.Knob = Knob;
	Look.bFilter = bFilter;
	Look.bBlocked = bBlocked;
	Look.SunSizeDeg = S->SunAngularDiameterDeg;
	float Cover = 0.f;
	if (const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(this))
	{
		Cover = Weather->GetState().CloudCover;
	}
	Look.CloudCover = Cover;
	if (TActorIterator<ALyudRareSky> It(World); It)
	{
		const FLyudRareSkyNow Rare = It->GetNow();
		Look.Aurora = Rare.Aurora;
		Look.Noctilucent = Rare.Noctilucent;
	}

	// the sun without a filter: white, a word, and the hero steps back
	if (WhiteOut < 0.f && Sc::SunDanger(Now, Look))
	{
		WhiteOut = 0.f;
		Say(LyudText::Get(TEXT("scope.sun")), 6.f);
		if (ULyudQuests* Quests = ULyudQuests::Get(this); Quests && !Quests->HasFlag(TEXT("scope.sun_warned")))
		{
			Quests->SetFlag(TEXT("scope.sun_warned"));
			Quests->AddNote(TEXT("journal.sky.sun"));
		}
	}

	const Sc::FTarget Target = Sc::Target(Now, Look);
	const double Field = FieldNow;
	const double Gap = Sc::CloudGap(Cover, Now.Day, AimAlt, AimAz);
	View->Dots.Reset();
	View->Pictures.Reset();
	View->Filter = bFilter ? 1.f : 0.f;
	if (!bFilter)
	{
		// the field stars (none through a wall: the middle's trace stands for the field)
		if (!bBlocked && Gap > 0.05)
		{
			static Sc::FStar Stars[LyudScopeImpl::MaxStars];
			const double Limit = Sc::LimitMagnitude(EyepieceIndex, Now.SunAltDeg);
			const int32 Num = Sc::Stars(Now, AimAlt, AimAz, Field, Limit, Stars, LyudScopeImpl::MaxStars);
			View->Dots.Reserve(Num + 4);
			for (int32 I = 0; I < Num; ++I)
			{
				FLyudEyepieceView::FDot Dot;
				Dot.At = FVector2f(Stars[I].X, Stars[I].Y);
				const float Over = static_cast<float>(Limit) - Stars[I].Magnitude;
				Dot.Size = FMath::Clamp(2.6f + 1.5f * FMath::Max(Over - 0.5f, 0.f), 2.6f, 11.f);
				Dot.Color = FMath::Lerp(FLinearColor(0.78f, 0.86f, 1.f), FLinearColor(1.f, 0.8f, 0.6f), Stars[I].Warmth);
				Dot.Color.A = FMath::Clamp(0.4f + 0.35f * Over, 0.f, 1.f) * static_cast<float>(Gap);
				View->Dots.Add(Dot);
			}
		}
		// the planets where they stand
		for (int32 I = 1; I < static_cast<int32>(Sc::EBody::Num); ++I)
		{
			const Sc::EBody Body = static_cast<Sc::EBody>(I);
			const Sc::FBody& B = Now.Bodies[I];
			const double Vis = Sc::Visibility(Now, Body) * Sc::CloudGap(Cover, Now.Day, B.AltDeg, B.AzDeg);
			double X = 0.0, Y = 0.0;
			if (Vis < 0.02 || bBlocked || !Sc::Project(AimAlt, AimAz, B.AltDeg, B.AzDeg, X, Y)
				|| X * X + Y * Y > FMath::Square(View->TanHalfField * 1.3f))
			{
				continue;
			}
			FLyudEyepieceView::FPicture Picture;
			Picture.At = FVector2f(static_cast<float>(X), static_cast<float>(Y));
			Picture.DiskTan = static_cast<float>(FMath::Tan(FMath::DegreesToRadians(B.DiameterArcsec / 3600.0)));
			Picture.PictureOverDisk = Body == Sc::EBody::Saturn ? 2.45f : 1.15f;
			const float North = static_cast<float>(FMath::DegreesToRadians(B.ParallacticDeg));
			// Venus and Mars are drawn with the sun on the right: turned to the bright limb (from north through east)
			Picture.Turn = (Body == Sc::EBody::Venus || Body == Sc::EBody::Mars) ? North + static_cast<float>(FMath::DegreesToRadians(B.BrightLimbDeg + 90.0)) : North;
			Picture.Brush = I;
			Picture.Alpha = static_cast<float>(FMath::Clamp(Vis, 0.0, 1.0));
			Picture.DotSize = FMath::Clamp(5.f - 1.3f * static_cast<float>(B.Magnitude), 3.5f, 12.f);
			Picture.Color = Body == Sc::EBody::Mars ? FLinearColor(1.f, 0.62f, 0.4f) : (Body == Sc::EBody::Saturn ? FLinearColor(1.f, 0.9f, 0.68f) : FLinearColor(1.f, 0.96f, 0.86f));
			View->Pictures.Add(Picture);
			// Jupiter's moons: X toward west (the right, north up), Y toward north, in Jupiter's radii
			if (Body == Sc::EBody::Jupiter && EyepieceIndex >= 1)
			{
				const float MoonLight = static_cast<float>(FMath::Clamp((-5.0 - Now.SunAltDeg) / 3.0, 0.0, 1.0)) * Picture.Alpha;
				const float Radius = Picture.DiskTan * 0.5f;
				for (int32 M = 0; M < 4 && MoonLight > 0.02f; ++M)
				{
					if (!B.bMoonSeen[M])
					{
						continue;
					}
					const float Mx = static_cast<float>(B.MoonX[M]) * Radius, My = static_cast<float>(B.MoonY[M]) * Radius;
					FLyudEyepieceView::FDot Dot;
					Dot.At = Picture.At + FVector2f(Mx * FMath::Cos(North) - My * FMath::Sin(North), Mx * FMath::Sin(North) + My * FMath::Cos(North));
					Dot.Size = M == 2 ? 4.6f : 4.f;     // Ganymede is the brightest
					Dot.Color = FLinearColor(1.f, 0.97f, 0.88f, MoonLight);
					View->Dots.Add(Dot);
				}
			}
		}
	}

	// which body the tube follows from now on: the one near the middle while the mouse rests
	if (Target.Body >= 1 && Target.Off < 0.45 && FMath::Abs(AimVAlt) + FMath::Abs(AimVAz) < Field * 0.02)
	{
		if (FollowBody != Target.Body)
		{
			FollowBody = Target.Body;
			FollowAlt = Now.Bodies[FollowBody].AltDeg;
			FollowAz = Now.Bodies[FollowBody].AzDeg;
		}
	}
	else if (Target.Body != FollowBody || Target.Off > 0.5)
	{
		FollowBody = -1;
	}

	// a held look counts
	Sc::FWatch Watch;
	Watch.Sight = WatchSight < 0 ? Sc::ESight::Num : static_cast<Sc::ESight>(WatchSight);
	Watch.Held = WatchHeld;
	Watch.bCounted = bWatchCounted;
	const bool bCounted = FieldNow <= LyudScope::Eyepiece(EyepieceIndex).FieldDeg * 1.02 && Sc::StepWatch(Watch, Target, World->GetDeltaSeconds());
	WatchSight = Watch.Sight == Sc::ESight::Num ? -1 : static_cast<int32>(Watch.Sight);
	WatchHeld = Watch.Held;
	bWatchCounted = Watch.bCounted;
	if (bCounted)
	{
		if (ULyudSkyLog* Log = ULyudSkyLog::Get(this))
		{
			Sc::FLog Model = LyudScopeImpl::ToModel(*Log);
			Sc::FNote Note;
			const ALyudSky* SkyNow = S;
			const double LocalDay = Now.Day + SkyNow->UtcOffsetHours / 24.0;
			if (Sc::Record(Model, Watch.Sight, LocalDay, Note))
			{
				Log->SeenBits = static_cast<int32>(Model.Seen);
				Log->JupiterNights = Model.JupiterNights;
				Log->LastJupiterNight = Model.LastJupiterNight;
				if (Note.Also != Sc::ESight::Num && Note.bAlsoFirst)
				{
					Noted(static_cast<int32>(Note.Also), true, false);
				}
				Noted(static_cast<int32>(Note.Sight), Note.bFirst, Note.bNewNight);
			}
		}
	}

	// the hint line: why what is in the field does not count yet
	static const TCHAR* Hints[5] = { TEXT(""), TEXT("scope.hint.focus"), TEXT("scope.hint.power"), TEXT("scope.hint.bright"), TEXT("scope.hint.cloud") };
	const int32 Hint = Target.Sight == Sc::ESight::Num || Target.Hint == 2 ? FMath::Clamp(Target.Hint, 0, 4) : 0;
	const bool bPowerLeft = EyepieceIndex < MaxEyepiece;
	View->Hint = Hint == 0 || (Hint == 2 && !bPowerLeft) ? FText::GetEmpty() : LyudText::Get(Hints[Hint]);
	View->Power = FText::FromString(FString::Printf(TEXT("×%d"), Sc::Eyepiece(EyepieceIndex).Power));
}

void ULyudTelescope::UpdateBirds(float DeltaTime)
{
#if LYUD_SCOPE_HAS_BIRDS
	const UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this);
	ULyudBirdBook* Book = ULyudBirdBook::Get(this);
	UWorld* World = GetWorld();
	if (!Life || !Book || !World || !Camera || bFilter)
	{
		return;
	}
	BirdHeldFor += DeltaTime;
	if ((BirdClock -= DeltaTime) > 0.f)
	{
		return;
	}
	BirdClock = 0.2f;
	const FVector Eye = Camera->GetActorLocation();
	const FVector Fwd = Camera->GetActorForwardVector();
	// the bird nearest the middle: within a third of the field, big enough in it and in focus
	int32 Best = -1;
	float BestCos = FMath::Cos(FMath::DegreesToRadians(FMath::Min(FieldNow * 0.3f, 2.5f)));
	FBirdLifeInfo BestInfo;
	for (const int32 Id : Life->FindBirdsNear(Eye + Fwd * 30000.f, 45000.f))
	{
		FBirdLifeInfo Info;
		if (!Life->GetBird(Id, Info) || Life->IsBirdAway(Id))
		{
			continue;
		}
		const FVector To = Info.Location - Eye;
		const float D = To.Size();
		if (D < 300.f)
		{
			continue;
		}
		const float C = FVector::DotProduct(To / D, Fwd);
		const float AcrossDeg = FMath::RadiansToDegrees(25.f / D);     // a bird some 25 cm long
		if (C > BestCos && AcrossDeg > FieldNow * 0.012f && LyudScope::Blur(Knob, D / 100.0, FieldNow) < LyudScope::SharpBlur * 1.5)
		{
			BestCos = C;
			Best = Id;
			BestInfo = Info;
		}
	}
	if (Best != BirdHeld)
	{
		BirdHeld = Best;
		BirdHeldFor = 0.f;
		return;
	}
	if (BirdHeld >= 0 && BirdHeldFor > 1.5f)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(LyudScopeBird), false, LockedPC.IsValid() ? LockedPC->GetPawn() : nullptr);
		if (BestInfo.Actor)
		{
			Query.AddIgnoredActor(BestInfo.Actor);
		}
		if (!World->LineTraceSingleByChannel(Hit, Eye, BestInfo.Location + FVector(0.f, 0.f, 5.f), ECC_Visibility, Query)
			&& Book->NoteSeen(this, BestInfo.SpeciesId, BestInfo.bFemale, TEXT("telescope"), BestInfo.Location))
		{
			Say(LyudText::Get(TEXT("ui.tame.seen"), { FStringFormatArg(ULyudBirdBook::SpeciesName(BestInfo.SpeciesId).ToString()) }), 5.f);
		}
		BirdHeldFor = -1e6f;
	}
#endif
}

void ULyudTelescope::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = LockedPC.Get();
	if (!bLooking)
	{
		return;
	}
	if (!World || !PC || !PC->GetPawn() || !Camera || !View.IsValid())
	{
		End();
		return;
	}
	// E or Esc: step back (not the press that began the look)
	if (GFrameCounter != BeginFrame && (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Escape)))
	{
		End();
		return;
	}
	View->Open = FMath::Clamp(static_cast<float>(World->GetTimeSeconds() - BeganAt) / 0.3f, 0.f, 1.f);

	if (WhiteOut >= 0.f)
	{
		// blinded: the view whites out, the tube is let go, and after a moment he steps back
		WhiteOut += DeltaTime;
		View->White = FMath::Clamp(WhiteOut / 0.12f, 0.f, 1.f);
		if (WhiteOut > 1.6f)
		{
			End();
			return;
		}
		UpdateCamera(DeltaTime);
		return;
	}

	// the mouse moves the tube; W / S (or the arrows) change the eyepiece; the wheel and A / D turn the focus
	float MouseX = 0.f, MouseY = 0.f;
	PC->GetInputMouseDelta(MouseX, MouseY);
	LyudScope::FAim Aim;
	Aim.AltDeg = AimAlt;
	Aim.AzDeg = AimAz;
	Aim.VAlt = AimVAlt;
	Aim.VAz = AimVAz;
	LyudScope::FMount Limits;
	// the mouse's delta comes in the look input's units: 12 of them make one "count" of the model
	LyudScope::StepAim(Aim, MouseX * 12.0, MouseY * 12.0, FieldNow, DeltaTime, Limits);
	if (FMath::Abs(MouseX) + FMath::Abs(MouseY) > 0.f)
	{
		FollowBody = -1;
	}
	AimAlt = Aim.AltDeg;
	AimAz = Aim.AzDeg;
	AimVAlt = Aim.VAlt;
	AimVAz = Aim.VAz;
	if (PC->WasInputKeyJustPressed(EKeys::W) || PC->WasInputKeyJustPressed(EKeys::Up))
	{
		SetEyepiece(EyepieceIndex + 1);
	}
	if (PC->WasInputKeyJustPressed(EKeys::S) || PC->WasInputKeyJustPressed(EKeys::Down))
	{
		SetEyepiece(EyepieceIndex - 1);
	}
	float Turn = 0.f;
	Turn += PC->WasInputKeyJustPressed(EKeys::MouseScrollUp) ? 0.012f : 0.f;
	Turn -= PC->WasInputKeyJustPressed(EKeys::MouseScrollDown) ? 0.012f : 0.f;
	Turn += (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) ? 0.16f * DeltaTime : 0.f;
	Turn -= (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left)) ? 0.16f * DeltaTime : 0.f;
	if (Turn != 0.f)
	{
		SetFocus(Knob + Turn);
	}

	// what stands in the way of the middle of the view, a few times a second
	if ((TraceClock -= DeltaTime) <= 0.f)
	{
		TraceClock = 0.1f;
		const FVector Eye = Camera->GetActorLocation();
		const FVector Dir = Direction(AimAlt, AimAz);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(LyudScopeLook), false, PC->GetPawn());
		FHitResult Hit;
		bBlocked = World->LineTraceSingleByChannel(Hit, Eye, Eye + Dir * LyudScopeImpl::TraceLength, ECC_Visibility, Query);
		BlockedDistance = bBlocked ? Hit.Distance : 0.f;
		UpdateTextures();
	}

	UpdateSkyView();
	UpdateCamera(DeltaTime);
	UpdateBirds(DeltaTime);

	if (SayLeft > 0.f && (SayLeft -= DeltaTime) <= 0.f)
	{
		View->Caption = FText::GetEmpty();
	}
	if (LyudScopeImpl::CVarDebug.GetValueOnGameThread() != 0 && (DebugClock -= DeltaTime) <= 0.f)
	{
		DebugClock = 1.f;
		UE_LOG(LogLyudScope, Display, TEXT("SCOPE %s dots=%d pictures=%d"), *Report(), View->Dots.Num(), View->Pictures.Num());
	}
}
