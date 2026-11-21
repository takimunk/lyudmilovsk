#include "Photo/LyudPhoto.h"

#include "Animals/AnimalLife.h"
#include "Audio/LyudSoundscape.h"
#include "Birds/BirdBook.h"
#include "Birds/BirdLife.h"
#include "Camera/PlayerCameraManager.h"
#include "City/LyudCityThings.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Festival/LyudFestival.h"
#include "Fishing/LyudFishing.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "People/PeopleLife.h"
#include "Photo/PhotoContestModel.h"
#include "Photo/PhotoModel.h"
#include "Plants/GameCalendar.h"
#include "Rendering/DrawElements.h"
#include "Sky/LyudRareSky.h"
#include "Sky/LyudSky.h"
#include "Sky/LyudSunset.h"
#include "Sea/LyudSea.h"
#include "Styling/CoreStyle.h"
#include "TextureResource.h"
#include "UI/LyudDiaryStyle.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "UI/LyudUIText.h"
#include "Weather/LyudWeather.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

const FName ULyudPhoto::CameraId(TEXT("camera"));
const FName ULyudPhoto::FilmId(TEXT("photo_film"));

namespace LyudPhotoDetail
{
	/** The print: a 3:2 frame, small enough to capture and keep by the hundred. */
	constexpr int32 PrintW = 720, PrintH = 480;
	/** The view's angle with the camera at the eye (the frame lines sit inside it), degrees. */
	constexpr float RaisedFov = 48.f;
	constexpr float RaiseSpeed = 4.f;
	/** How long the finder stays dark after the shutter, seconds. */
	constexpr float ShutterSeconds = 0.35f;
	/** Subjects further than this are dots on a 50 mm frame, cm. */
	constexpr float BirdReach = 2500.f, AnimalReach = 3000.f, PersonReach = 1500.f;
	/** People this near know they are being photographed. */
	constexpr float PoseReach = 800.f;
	constexpr int32 AlbumPerPage = 4;

	const TCHAR* RegimeTag(ELyudWeatherRegime Regime)
	{
		switch (Regime)
		{
		case ELyudWeatherRegime::High: return TEXT("high");
		case ELyudWeatherRegime::WarmFront: return TEXT("warm_front");
		case ELyudWeatherRegime::WarmSector: return TEXT("warm_sector");
		case ELyudWeatherRegime::ColdFront: return TEXT("cold_front");
		case ELyudWeatherRegime::Showery: return TEXT("showery");
		case ELyudWeatherRegime::Occluded: return TEXT("occluded");
		case ELyudWeatherRegime::ArcticHigh: return TEXT("arctic_high");
		default: return TEXT("thundery_low");
		}
	}

	std::string Utf8(const FString& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text));
	}

	FString Wide(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	AHeroCharacter* HeroOf(const UWorld* World, APlayerController** OutPC = nullptr)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (OutPC)
		{
			*OutPC = PC;
		}
		return PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	}

	/** The finder: the world dimmed round the bright frame lines, the frame's corners, the dark of the shutter. */
	class SLyudViewfinder : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SLyudViewfinder) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, ULyudPhoto* InOwner)
		{
			Owner = InOwner;
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(16, 16); }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
			int32 Layer, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			const ULyudPhoto* Photo = Owner.Get();
			if (!Photo || Photo->RaiseAmount() <= 0.02f)
			{
				return Layer;
			}
			const float K = FMath::SmoothStep(0.f, 1.f, Photo->RaiseAmount());
			const FVector2f Size(Geo.GetLocalSize());
			const float FrameH = Size.Y * ULyudPhoto::FrameHeightShare;
			const float FrameW = FMath::Min(FrameH * 1.5f, Size.X * 0.94f);
			const FVector2f A((Size.X - FrameW) * 0.5f, (Size.Y - FrameH) * 0.5f);
			const FVector2f B(A.X + FrameW, A.Y + FrameH);
			const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
			const FLinearColor Dim(0.f, 0.f, 0.f, 0.62f * K);
			auto Box = [&](const FVector2f& At, const FVector2f& BoxSize, const FLinearColor& Color, int32 OnLayer)
			{
				if (BoxSize.X > 0.f && BoxSize.Y > 0.f)
				{
					FSlateDrawElement::MakeBox(Out, OnLayer, Geo.ToPaintGeometry(BoxSize, FSlateLayoutTransform(At)), White, ESlateDrawEffect::None, Color);
				}
			};
			// the finder's own body round the window
			Box(FVector2f(0.f, 0.f), FVector2f(Size.X, A.Y), Dim, Layer);
			Box(FVector2f(0.f, B.Y), FVector2f(Size.X, Size.Y - B.Y), Dim, Layer);
			Box(FVector2f(0.f, A.Y), FVector2f(A.X, FrameH), Dim, Layer);
			Box(FVector2f(B.X, A.Y), FVector2f(Size.X - B.X, FrameH), Dim, Layer);
			// the bright frame lines: four corners and the rangefinder's patch in the middle
			const FLinearColor Line(1.f, 0.96f, 0.78f, 0.9f * K);
			const float Arm = FrameH * 0.09f;
			const float Thick = 2.f;
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				const bool bRight = (Corner & 1) != 0, bLow = (Corner & 2) != 0;
				const FVector2f P(bRight ? B.X : A.X, bLow ? B.Y : A.Y);
				Box(FVector2f(bRight ? P.X - Arm : P.X, bLow ? P.Y - Thick : P.Y), FVector2f(Arm, Thick), Line, Layer + 1);
				Box(FVector2f(bRight ? P.X - Thick : P.X, bLow ? P.Y - Arm : P.Y), FVector2f(Thick, Arm), Line, Layer + 1);
			}
			const FVector2f Mid = Size * 0.5f;
			const float Patch = FrameH * 0.05f;
			Box(FVector2f(Mid.X - Patch, Mid.Y - Patch * 0.6f), FVector2f(Patch * 2.f, Patch * 1.2f), FLinearColor(1.f, 0.9f, 0.55f, 0.16f * K), Layer + 1);
			// the slow shutter: the window goes dark and opens again
			if (Photo->ShutterDark() > 0.f)
			{
				Box(A, FVector2f(FrameW, FrameH), FLinearColor(0.f, 0.f, 0.f, FMath::Clamp(Photo->ShutterDark() * 1.6f, 0.f, 1.f)), Layer + 2);
			}
			return Layer + 3;
		}

	private:
		TWeakObjectPtr<ULyudPhoto> Owner;
	};

	/** One page of the album: its prints' brushes live as long as the page is on the screen. */
	class SLyudAlbumPage : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SLyudAlbumPage) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, const TSharedRef<SWidget>& Content, TArray<TSharedPtr<FSlateBrush>> InBrushes)
		{
			Brushes = MoveTemp(InBrushes);
			ChildSlot[Content];
		}

	private:
		TArray<TSharedPtr<FSlateBrush>> Brushes;
	};

	FAutoConsoleCommandWithWorldAndArgs GPhotoRaise(TEXT("lyud.Photo.Raise"), TEXT("0 | 1: the camera down or at the eye."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudPhoto* Photo = ULyudPhoto::Get(World))
			{
				Photo->SetRaised(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPhotoShoot(TEXT("lyud.Photo.Shoot"), TEXT("Fires the shutter (the camera must be at the eye)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudPhoto* Photo = ULyudPhoto::Get(World))
			{
				const bool bShot = Photo->Shoot();
				UE_LOG(LogTemp, Display, TEXT("PHOTO shoot %s, %d frames left"), bShot ? TEXT("ok") : TEXT("refused"), Photo->FramesLeft());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPhotoDevelop(TEXT("lyud.Photo.Develop"), TEXT("Every film becomes prints now."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudPhoto* Photo = ULyudPhoto::Get(World))
			{
				Photo->DevelopAllNow();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPhotoAlbum(TEXT("lyud.Photo.Album"), TEXT("Opens or closes the album."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudPhoto* Photo = ULyudPhoto::Get(World))
			{
				Photo->ToggleAlbum();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPhotoState(TEXT("lyud.Photo.State"), TEXT("Logs the films, the prints and the stamps."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudPhoto* Photo = ULyudPhoto::Get(World))
			{
				const PhotoModel::FState& S = Photo->State();
				UE_LOG(LogTemp, Display, TEXT("PHOTO state: %d rolls, %d frames left, %d exposed, %d at the studio, %d prints, %d stamps"),
					(int32)S.Rolls().size(), S.FramesLeft(), S.Exposed(), S.AtStudio(), (int32)S.Album().size(), (int32)S.Stamps().size());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPhotoContest(TEXT("lyud.Photo.Contest"), TEXT("The photo club's contest box: the result, or the album to pick a print from."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudPhoto* Photo = ULyudPhoto::Get(World))
			{
				Photo->UseContest();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPhotoWall(TEXT("lyud.Photo.Wall"), TEXT("The photo club's wall of winners."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudPhoto* Photo = ULyudPhoto::Get(World))
			{
				Photo->ShowWall();
			}
		}));

	// developer mode (docs/dev_mode.md)
	LyudDev::FAutoAction GDevGive(TEXT("photo.give"), TEXT("photo"), TEXT("dev.action.photo.give"),
		[](UWorld* W)
		{
			if (ULyudInventory* Inv = ULyudInventory::Get(W))
			{
				if (Inv->Count(ULyudPhoto::CameraId) == 0)
				{
					Inv->Add(ULyudPhoto::CameraId);
				}
				Inv->Add(ULyudPhoto::FilmId, 3);
				Inv->Earn(200);
			}
		});
	LyudDev::FAutoAction GDevDevelop(TEXT("photo.develop"), TEXT("photo"), TEXT("dev.action.photo.develop"),
		[](UWorld* W)
		{
			if (ULyudPhoto* Photo = ULyudPhoto::Get(W))
			{
				Photo->DevelopAllNow();
			}
		});
	LyudDev::FAutoAction GDevContest(TEXT("photo.contest"), TEXT("photo"), TEXT("dev.action.photo.contest"),
		[](UWorld* W)
		{
			if (ULyudPhoto* Photo = ULyudPhoto::Get(W))
			{
				Photo->UseContest();
			}
		});
	LyudDev::FAutoAction GDevWall(TEXT("photo.wall"), TEXT("photo"), TEXT("dev.action.photo.wall"),
		[](UWorld* W)
		{
			if (ULyudPhoto* Photo = ULyudPhoto::Get(W))
			{
				Photo->ShowWall();
			}
		});
	LyudDev::FAutoAction GDevAlbum(TEXT("photo.album"), TEXT("photo"), TEXT("dev.action.photo.album"),
		[](UWorld* W)
		{
			if (ULyudPhoto* Photo = ULyudPhoto::Get(W))
			{
				Photo->ToggleAlbum();
			}
		});
}

ULyudPhoto::ULyudPhoto()
{
}

ULyudPhoto::~ULyudPhoto()
{
}

ULyudPhoto* ULyudPhoto::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine && WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudPhoto>() : nullptr;
}

bool ULyudPhoto::IsUp(const UObject* WorldContext)
{
	const ULyudPhoto* Photo = Get(WorldContext);
	return Photo && (Photo->bRaised || Photo->Raise > 0.f);
}

void ULyudPhoto::DropCamera()
{
	bRaised = false;
	if (Raise > 0.f)
	{
		Raise = 0.f;
		Dark = 0.f;
		APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->UnlockFOV();
		}
		NormalFov = 0.f;
	}
}

PhotoModel::FState& ULyudPhoto::State()
{
	if (!Model.IsValid())
	{
		Model = MakePimpl<PhotoModel::FState>();
	}
	return *Model;
}

PhotoContest::FState& ULyudPhoto::Contest()
{
	if (!ContestModel.IsValid())
	{
		ContestModel = MakePimpl<PhotoContest::FState>();
	}
	return *ContestModel;
}

void ULyudPhoto::Today(int32& Year, int32& Month, int32& Day) const
{
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	Year = Now.GetYear();
	Month = Now.GetMonth();
	Day = Now.GetDay();
}

bool ULyudPhoto::ContestNews(const UObject* WorldContext, int32 Year, int32 Month, int32& OutPlace, FString& OutShotPlace)
{
	ULyudPhoto* Photo = Get(WorldContext);
	const PhotoContest::FEntry* Entry = Photo ? Photo->Contest().Find(PhotoContest::ContestOf(Year, Month)) : nullptr;
	if (!Entry)
	{
		return false;
	}
	OutPlace = PhotoContest::PlaceOf(Entry->Contest, Entry->Score);
	OutShotPlace = LyudPhotoDetail::Wide(Entry->Place);
	return true;
}

void ULyudPhoto::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	State();
	// the ring rests at infinity
	FocusIndex = PhotoModel::FocusStops() - 1;
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("Photo"), this);
		Saves->OnNewGame.AddUObject(this, &ULyudPhoto::StartOver);
	}
	if (ULyudCityThings* Things = ULyudCityThings::Get(&InWorld))
	{
		// the studio's counter and the darkrooms are city acts that hand over to us
		Things->OnOpen.AddWeakLambda(this, [this](FName Screen, int32)
		{
			if (Screen == TEXT("photo_lab"))
			{
				UseStudio();
			}
			else if (Screen == TEXT("photo_darkroom"))
			{
				StartDarkroom();
			}
			else if (Screen == TEXT("photo_contest"))
			{
				UseContest();
			}
			else if (Screen == TEXT("photo_wall"))
			{
				ShowWall();
			}
		});
	}
}

void ULyudPhoto::Deinitialize()
{
	if (GEngine && GEngine->GameViewport)
	{
		for (const TSharedPtr<SWidget>& W : { Finder, Album })
		{
			if (W.IsValid())
			{
				GEngine->GameViewport->RemoveViewportWidgetContent(W.ToSharedRef());
			}
		}
	}
	Finder.Reset();
	Album.Reset();
	PagePrints.Reset();
	if (Capture)
	{
		Capture->DestroyComponent();
		Capture = nullptr;
	}
	Super::Deinitialize();
}

void ULyudPhoto::StartOver()
{
	State() = PhotoModel::FState();
	Contest() = PhotoContest::FState();
	// a new life gets a folder of its own; the old pictures stay on the disk with their save
	GameId.Reset();
	bRaised = false;
	Bath = -1;
}

void ULyudPhoto::OnGameSaving_Implementation()
{
	Saved = LyudPhotoDetail::Wide(State().Save());
	SavedContest = LyudPhotoDetail::Wide(Contest().Save());
}

void ULyudPhoto::OnGameLoaded_Implementation()
{
	PhotoModel::FState Loaded;
	if (!Saved.IsEmpty() && Loaded.Restore(LyudPhotoDetail::Utf8(Saved)))
	{
		State() = Loaded;
	}
	else
	{
		State() = PhotoModel::FState();
	}
	PhotoContest::FState Entered;
	if (SavedContest.IsEmpty() || !Entered.Restore(LyudPhotoDetail::Utf8(SavedContest)))
	{
		Entered = PhotoContest::FState();
	}
	Contest() = Entered;
}

double ULyudPhoto::NowHours() const
{
	const UWorld* World = GetWorld();
	const UGameCalendar* Calendar = World ? World->GetSubsystem<UGameCalendar>() : nullptr;
	const FDateTime Start = Calendar ? Calendar->StartDate : FDateTime(2026, 10, 20, 16, 0, 0);
	return (UGameCalendar::NowIn(World) - Start.GetDate()).GetTotalHours();
}

FString ULyudPhoto::PhotoDir() const
{
	return FPaths::ProjectSavedDir() / TEXT("Photos") / (GameId.IsEmpty() ? FString(TEXT("game")) : GameId);
}

FString ULyudPhoto::PhotoFile(int32 Roll, int32 Frame) const
{
	return PhotoDir() / FString::Printf(TEXT("film%03d_%02d.jpg"), Roll, Frame);
}

float ULyudPhoto::FocusDistance() const
{
	return PhotoModel::FocusStop(FocusIndex);
}

int32 ULyudPhoto::FramesLeft() const
{
	return Model.IsValid() ? Model->FramesLeft() : 0;
}

bool ULyudPhoto::HasFilm() const
{
	return Model.IsValid() && Model->HasFilm();
}

float ULyudPhoto::DarkroomIdeal() const
{
	return Bath >= 0 ? PhotoModel::BathIdeal(Bath) : 0.f;
}

void ULyudPhoto::Say(const TCHAR* Key, const TArray<FStringFormatArg>& Args)
{
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->Toast(LyudText::Get(Key, Args));
	}
	UE_LOG(LogTemp, Display, TEXT("PHOTO %s"), Key);
}

void ULyudPhoto::LockHero(bool bLock)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || bLocked == bLock)
	{
		return;
	}
	bLocked = bLock;
	PC->SetIgnoreLookInput(bLock);
	PC->SetIgnoreMoveInput(bLock);
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		// the other screens and prompts stand back while ours is up
		UI->SetExternalScreen(bLock ? this : nullptr);
	}
}

void ULyudPhoto::EnsureOverlay()
{
	if (Finder.IsValid() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	TWeakObjectPtr<ULyudPhoto> Self(this);
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 15);
	Finder = SNew(SOverlay)
		.Visibility_Lambda([Self] { return Self.IsValid() && (Self->RaiseAmount() > 0.02f || Self->InDarkroom()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		+ SOverlay::Slot()
		[
			SNew(LyudPhotoDetail::SLyudViewfinder, this)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0, 0, 0, 34))
		[
			// the focus ring and the frame counter, read off the top of the camera
			SNew(STextBlock)
			.Font(Font)
			.ColorAndOpacity(FLinearColor(1.f, 0.96f, 0.8f))
			.ShadowOffset(FVector2D(1, 1))
			.ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f))
			.Visibility_Lambda([Self] { return Self.IsValid() && Self->RaiseAmount() > 0.5f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.Text_Lambda([Self]
			{
				if (!Self.IsValid())
				{
					return FText::GetEmpty();
				}
				const float Focus = Self->FocusDistance();
				const FText Ring = Focus > 1.e6f
					? LyudText::Get(TEXT("ui.photo.focus.inf"))
					: LyudText::Get(TEXT("ui.photo.focus.m"), { FStringFormatArg(Focus < 10.f ? FString::Printf(TEXT("%.1f"), Focus) : FString::Printf(TEXT("%.0f"), Focus)) });
				if (!Self->HasFilm())
				{
					return LyudText::Get(TEXT("ui.photo.finder.no_film"), { FStringFormatArg(Ring.ToString()) });
				}
				return LyudText::Get(TEXT("ui.photo.finder"), { FStringFormatArg(Ring.ToString()),
					FStringFormatArg(PhotoModel::FramesPerFilm - Self->FramesLeft()), FStringFormatArg(PhotoModel::FramesPerFilm) });
			})
		]
		+ SOverlay::Slot()
		[
			// the darkroom: the red lamp over everything, the bath and its seconds
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.16f, 0.f, 0.f, 0.86f))
			.HAlign(HAlign_Center).VAlign(VAlign_Center)
			.Visibility_Lambda([Self] { return Self.IsValid() && Self->InDarkroom() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 26)).ColorAndOpacity(FLinearColor(1.f, 0.42f, 0.3f))
					.Text_Lambda([Self]
					{
						return Self.IsValid() && Self->InDarkroom()
							? LyudText::Get(FString::Printf(TEXT("ui.photo.dark.bath.%d"), Self->DarkroomBath())) : FText::GetEmpty();
					})
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 14)
				[
					SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Mono", 40)).ColorAndOpacity(FLinearColor(1.f, 0.55f, 0.42f))
					.Text_Lambda([Self]
					{
						return Self.IsValid() && Self->InDarkroom()
							? LyudText::Get(TEXT("ui.photo.dark.seconds"), { FStringFormatArg(FString::Printf(TEXT("%.1f"), Self->DarkroomSeconds())),
								FStringFormatArg(FString::Printf(TEXT("%.0f"), Self->DarkroomIdeal())) })
							: FText::GetEmpty();
					})
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(Font).ColorAndOpacity(FLinearColor(1.f, 0.42f, 0.3f, 0.85f)).Text(LyudText::Get(TEXT("ui.photo.dark.hint")))
				]
			]
		];
	GEngine->GameViewport->AddViewportWidgetContent(Finder.ToSharedRef(), 41);
}

void ULyudPhoto::SetRaised(bool bUp)
{
	if (bUp == bRaised)
	{
		return;
	}
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (bUp && !(Inv && Inv->Count(CameraId) > 0))
	{
		return;
	}
	const APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (bUp && Player && Player->IsInputKeyDown(EKeys::RightMouseButton))
	{
		// the binoculars are at the eyes: both would turn the view's angle and each take the other's for the normal one
		return;
	}
	bRaised = bUp;
	if (!bUp)
	{
		return;
	}
	EnsureOverlay();
	// a fresh film goes in by itself when the camera is empty
	if (Inv && !State().HasFilm() && Inv->Count(FilmId) > 0 && Inv->Remove(FilmId) && State().LoadFilm())
	{
		Say(TEXT("ui.photo.loaded"));
	}
}

float ULyudPhoto::CaptureFov(const APlayerController& PC) const
{
	// the frame lines take a part of the raised view's width: the film sees that part
	int32 W = 16, H = 9;
	PC.GetViewportSize(W, H);
	const float FrameW = FMath::Min(float(H) * FrameHeightShare * 1.5f, float(W) * 0.94f);
	const float Share = W > 0 ? FrameW / float(W) : 0.6f;
	const float Half = FMath::DegreesToRadians(LyudPhotoDetail::RaisedFov * 0.5f);
	return FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(Half) * Share));
}

void ULyudPhoto::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = nullptr;
	AHeroCharacter* Hero = LyudPhotoDetail::HeroOf(World, &PC);
	if (!PC)
	{
		return;
	}

	if (Album.IsValid())
	{
		if (PC->WasInputKeyJustPressed(EKeys::P) || PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::Q))
		{
			CloseAlbum();
		}
		else
		{
			const int32 Step = (PC->WasInputKeyJustPressed(EKeys::D) || PC->WasInputKeyJustPressed(EKeys::Right) ? 1 : 0)
				- (PC->WasInputKeyJustPressed(EKeys::A) || PC->WasInputKeyJustPressed(EKeys::Left) ? 1 : 0);
			if (Step != 0)
			{
				// ShowAlbumPage holds the page to what there is
				AlbumPage = FMath::Max(0, AlbumPage + Step);
				ShowAlbumPage();
			}
			else if (AlbumMode == 1)
			{
				const FKey Digits[LyudPhotoDetail::AlbumPerPage] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
				for (int32 I = 0; I < LyudPhotoDetail::AlbumPerPage; ++I)
				{
					if (PC->WasInputKeyJustPressed(Digits[I]))
					{
						PickPrint(I);
						return;
					}
				}
			}
		}
		return;
	}

	if (Bath >= 0)
	{
		// the bath runs; E takes the film out (not the very press that began it)
		BathSeconds += DeltaTime;
		if (BathSeconds > 0.4f && PC->WasInputKeyJustPressed(EKeys::E))
		{
			BathTimes[Bath] = BathSeconds;
			BathSeconds = 0.f;
			if (++Bath >= PhotoModel::Baths)
			{
				FinishDarkroom();
			}
		}
		return;
	}

	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	const bool bFree = Hero && !(UI && UI->IsOpen()) && !UGameplayStatics::IsGamePaused(World);
	if (!bFree)
	{
		bRaised = false;
	}
	else
	{
		if (PC->WasInputKeyJustPressed(EKeys::V))
		{
			SetRaised(!bRaised);
		}
		if (!bRaised && Raise <= 0.f && PC->WasInputKeyJustPressed(EKeys::P))
		{
			ToggleAlbum();
			return;
		}
	}

	const float Before = Raise;
	Raise = FMath::FInterpConstantTo(Raise, bRaised ? 1.f : 0.f, DeltaTime, LyudPhotoDetail::RaiseSpeed);
	if (Raise <= 0.f && Before <= 0.f)
	{
		// the camera is down: nothing more to do
		return;
	}
	APlayerCameraManager* Cam = PC->PlayerCameraManager;
	if (Cam)
	{
		if (Raise > 0.f)
		{
			if (Before <= 0.f || NormalFov <= 0.f)
			{
				Cam->UnlockFOV();
				NormalFov = Cam->GetFOVAngle();
			}
			Cam->SetFOV(FMath::Lerp(NormalFov, LyudPhotoDetail::RaisedFov, FMath::SmoothStep(0.f, 1.f, Raise)));
		}
		else
		{
			Cam->UnlockFOV();
			NormalFov = 0.f;
		}
	}
	Dark = FMath::Max(0.f, Dark - DeltaTime / LyudPhotoDetail::ShutterSeconds);
	if (!bRaised || Raise < 0.9f)
	{
		return;
	}
	const int32 Ring = (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp) ? 1 : 0) - (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown) ? 1 : 0);
	if (Ring != 0)
	{
		FocusIndex = FMath::Clamp(FocusIndex + Ring, 0, PhotoModel::FocusStops() - 1);
	}
	if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		Shoot();
	}
	else if (PC->WasInputKeyJustPressed(EKeys::R))
	{
		Rewind();
	}
}

bool ULyudPhoto::Rewind()
{
	if (!State().Rewind())
	{
		Say(TEXT("ui.photo.rewind.nothing"));
		return false;
	}
	Say(TEXT("ui.photo.rewound"), { FStringFormatArg(State().Exposed()) });
	// the next film, if the pocket has one
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (Inv && Inv->Count(FilmId) > 0 && Inv->Remove(FilmId))
	{
		State().LoadFilm();
	}
	return true;
}

bool ULyudPhoto::Shoot()
{
	UWorld* World = GetWorld();
	APlayerController* PC = nullptr;
	AHeroCharacter* Hero = LyudPhotoDetail::HeroOf(World, &PC);
	if (!Hero || !PC || !PC->PlayerCameraManager || !bRaised || Raise < 0.9f || Dark > 0.f)
	{
		return false;
	}
	PhotoModel::FState& S = State();
	if (!S.HasFilm())
	{
		Say(TEXT("ui.photo.no_film"));
		return false;
	}
	if (S.FramesLeft() <= 0)
	{
		Say(TEXT("ui.photo.film_end"));
		return false;
	}
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FRotator Look = PC->PlayerCameraManager->GetCameraRotation();
	PhotoModel::FShot Shot;
	Shot.Hours = NowHours();
	LookThrough(*Hero, Eye, Look, Shot);
	const float Sharp = Shot.Sharp;
	const int32 Subjects = (int32)Shot.Subjects.size();
	const int32 Frame = S.Shoot(MoveTemp(Shot));
	if (Frame <= 0 || S.Rolls().empty())
	{
		return false;
	}
	if (GameId.IsEmpty())
	{
		GameId = FGuid::NewGuid().ToString(EGuidFormats::Short);
	}
	TakePicture(Eye, Look, Sharp, PhotoFile(S.Rolls().back().Id, Frame));
	Dark = 1.f;
	if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
	{
		Sound->PlayEffect(TEXT("camera_flash"), Eye, 0.8f);
	}
	UE_LOG(LogTemp, Display, TEXT("PHOTO frame %d: sharp %.2f, %d subjects, %d left"), Frame, Sharp, Subjects, S.FramesLeft());
	if (S.FramesLeft() <= 0)
	{
		Say(TEXT("ui.photo.film_end"));
	}
	return true;
}

void ULyudPhoto::LookThrough(const AHeroCharacter& Hero, const FVector& Eye, const FRotator& Look, PhotoModel::FShot& Shot)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World->GetFirstPlayerController();
	const float HalfH = FMath::DegreesToRadians((PC ? CaptureFov(*PC) : 30.f) * 0.5f);
	const float HalfV = FMath::Atan(FMath::Tan(HalfH) / 1.5f);
	const float FocusM = FocusDistance();
	const float HeroSpeed = float(Hero.GetVelocity().Size()) / 100.f;
	const FVector Forward = Look.Vector();

	auto InFrame = [&](const FVector& Point, float& OutDist)
	{
		const FVector Local = Look.UnrotateVector(Point - Eye);
		OutDist = float(Local.Size());
		return Local.X > 10.0 && FMath::Abs(FMath::Atan2(Local.Y, Local.X)) < HalfH && FMath::Abs(FMath::Atan2(Local.Z, Local.X)) < HalfV;
	};
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PhotoSight), true, &Hero);
	auto Seen = [&](const FVector& Point)
	{
		FHitResult Hit;
		return !World->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Params) || FVector::DistSquared(Hit.ImpactPoint, Point) < FMath::Square(80.0);
	};
	float MainSharp = -1.f, MainDist = 1.e9f;
	auto Add = [&](const FString& Tag, float Sharp, float Dist, bool bMain)
	{
		for (PhotoModel::FSubject& Old : Shot.Subjects)
		{
			if (Old.Tag == LyudPhotoDetail::Utf8(Tag))
			{
				Old.Sharp = FMath::Max(Old.Sharp, Sharp);
				return;
			}
		}
		PhotoModel::FSubject Subject;
		Subject.Tag = LyudPhotoDetail::Utf8(Tag);
		Subject.Sharp = Sharp;
		Shot.Subjects.push_back(Subject);
		if (bMain && Dist < MainDist)
		{
			MainDist = Dist;
			MainSharp = Sharp;
		}
	};
	float Dist = 0.f;

	// birds
	if (const UBirdLifeSubsystem* Birds = UBirdLifeSubsystem::Get(this))
	{
		int32 Looked = 0;
		for (const int32 Id : Birds->FindBirdsNear(Eye, LyudPhotoDetail::BirdReach))
		{
			FBirdLifeInfo Info;
			if (++Looked > 60)
			{
				break;
			}
			if (Birds->GetBird(Id, Info) && InFrame(Info.Location, Dist) && Seen(Info.Location))
			{
				Add(TEXT("bird:") + Info.SpeciesId.ToString(), PhotoModel::Sharpness(FocusM, Dist / 100.f, HeroSpeed, float(Info.Velocity.Size()) / 100.f), Dist, true);
			}
		}
	}
	// Maru
	if (const UAnimalLifeSubsystem* Animals = UAnimalLifeSubsystem::Get(this))
	{
		for (const int32 Id : Animals->FindAnimalsNear(Eye, LyudPhotoDetail::AnimalReach))
		{
			FAnimalInfo Info;
			const FVector Body = FVector(0, 0, 30);
			if (Animals->GetAnimal(Id, Info) && Info.Breed == TEXT("Maru") && InFrame(Info.Location + Body, Dist) && Seen(Info.Location + Body))
			{
				Add(TEXT("maru"), PhotoModel::Sharpness(FocusM, Dist / 100.f, HeroSpeed, float(Info.Velocity.Size()) / 100.f), Dist, true);
			}
		}
	}
	// people: in the frame they are subjects; near, they notice the lens and answer it each in their own way
	if (UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this))
	{
		const int32 Num = People->NumPeople();
		for (int32 Id = 0; Id < Num; ++Id)
		{
			int32 Activity = -1;
			bool bDoing = false;
			EPersonPose Pose = EPersonPose();
			FVector Where = FVector::ZeroVector;
			if (!People->GetActivityState(Id, Activity, bDoing, Pose, Where) || FVector::DistSquared(Where, Eye) > FMath::Square(LyudPhotoDetail::PersonReach))
			{
				continue;
			}
			const FVector Chest = Where + FVector(0, 0, 110);
			FPersonInfo Info;
			if (!InFrame(Chest, Dist) || !Seen(Chest) || !People->GetPerson(Id, Info))
			{
				continue;
			}
			const float PersonSharp = PhotoModel::Sharpness(FocusM, Dist / 100.f, HeroSpeed, 0.f);
			Add(TEXT("person:") + Info.Key, PersonSharp, Dist, true);
			if (bDoing)
			{
				// busy with something: "people at work" for the club's contest
				Add(TEXT("work"), PersonSharp, Dist, false);
			}
			if (Dist < LyudPhotoDetail::PoseReach)
			{
				const float Warmth = People->GetTrait(Id, TEXT("warmth")), Nerve = People->GetTrait(Id, TEXT("nerve")), Curiosity = People->GetTrait(Id, TEXT("curiosity"));
				// the warm wave, the shy fold their arms, the curious strike a pose, the rest nod
				const FName Manner = Warmth > 0.62f ? TEXT("wave") : (Nerve < 0.38f ? TEXT("fold_arms") : (Curiosity > 0.6f ? TEXT("hands_hips") : TEXT("nod")));
				People->ReactManner(Id, Manner, 3.f);
			}
		}
	}
	// a fish just landed, still in the hands
	const float SceneSharp = PhotoModel::Sharpness(FocusM, 1.e9f, HeroSpeed, 0.f);
	if (const ULyudFishingState* Fishing = ULyudFishingState::Get(this))
	{
		if (Fishing->Journal.Num() > 0 && ULyudFishing::HasHand(this)
			&& (UGameCalendar::NowIn(World) - Fishing->Journal.Last().When).GetTotalMinutes() < 4.0)
		{
			Add(TEXT("fish"), PhotoModel::Sharpness(FocusM, 0.9f, HeroSpeed, 0.f), 90.f, true);
		}
	}
	// the sky: the sun's height for the light of the frame, the sunset of this weather when the lens looks toward the
	// sun, the aurora when it looks up at it
	TActorIterator<ALyudSky> SkyIt(World);
	const ALyudSky* Sky = SkyIt ? *SkyIt : nullptr;
	if (Sky)
	{
		Add(FString::Printf(TEXT("sun:%d"), FMath::RoundToInt(Sky->SunAltitude())), 1.f, 1.e9f, false);
	}
	// the sea: the lens looks at open water (the sight line reaches the sea's level with nothing in the way)
	if (const ALyudSea* SeaActor = ALyudSea::Get(this))
	{
		FVector Down = Forward;
		Down.Z = FMath::Min(Down.Z, -0.05);
		Down.Normalize();
		const double Fall = Eye.Z - double(SeaActor->SeaLevel());
		if (Look.Pitch < 12.f && Fall > 0.0 && Fall / -Down.Z < 200000.0)
		{
			const FVector OnWater = Eye + Down * (Fall / -Down.Z);
			FHitResult Hit;
			if (!World->LineTraceSingleByChannel(Hit, Eye, OnWater + FVector(0, 0, 30), ECC_Visibility, Params))
			{
				Add(TEXT("sea"), SceneSharp, 1.e9f, false);
			}
		}
	}
	if (const ULyudSunset* Sunset = ULyudSunset::Get(this))
	{
		const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(this);
		if (Sky && Weather && Sunset->GetLook().Weight > 0.35f && Sky->SunAltitude() < 12.f
			&& FVector::DotProduct(Forward, Sky->SunDirection().GetSafeNormal()) > 0.8)
		{
			Add(FString(TEXT("sunset:")) + LyudPhotoDetail::RegimeTag(Weather->GetState().Regime), SceneSharp, 1.e9f, false);
		}
	}
	TActorIterator<ALyudRareSky> RareIt(World);
	if (RareIt && RareIt->GetNow().Aurora > 0.15f && Look.Pitch > 8.f)
	{
		Add(TEXT("aurora"), SceneSharp, 1.e9f, false);
	}
	// a festival's show: its running part, taken at its venue
	const FVector2D OnMap = ULyudQuests::Frame().FromWorld(Hero.GetActorLocation());
	if (const ULyudFestivalSubsystem* Festivals = ULyudFestivalSubsystem::Get(this))
	{
		for (const FLyudFestivalNow& F : Festivals->ActiveFestivals())
		{
			if (F.Phase.IsNone())
			{
				continue;
			}
			bool bThere = true;
			for (const FLyudMapPlace& P : ULyudQuests::Places())
			{
				if (P.Id == F.Venue)
				{
					bThere = FVector2D::Distance(P.At, OnMap) < FMath::Max(double(P.Radius), 150.0);
				}
			}
			if (bThere)
			{
				Add(TEXT("festival:") + F.Id.ToString(), SceneSharp, 1.e9f, false);
			}
		}
	}
	// the place: every map place the hero stands in is in the picture; the nearest names the print
	double Best = FMath::Square(400.0);
	for (const FLyudMapPlace& P : ULyudQuests::Places())
	{
		if (P.Kind == TEXT("land"))
		{
			continue;
		}
		const double D = FVector2D::DistSquared(P.At, OnMap);
		if (D < FMath::Square(FMath::Max(double(P.Radius), 80.0)))
		{
			Add(TEXT("place:") + P.Id.ToString(), SceneSharp, 1.e9f, false);
		}
		if (D < Best)
		{
			Best = D;
			Shot.Place = LyudPhotoDetail::Utf8(P.Id.ToString());
		}
	}
	Shot.Sharp = MainSharp >= 0.f ? MainSharp : SceneSharp;
}

void ULyudPhoto::TakePicture(const FVector& Eye, const FRotator& Look, float Sharp, const FString& File)
{
	constexpr int32 PrintW = LyudPhotoDetail::PrintW, PrintH = LyudPhotoDetail::PrintH;
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!World || !PC)
	{
		return;
	}
	if (!Target)
	{
		Target = NewObject<UTextureRenderTarget2D>(this);
		Target->ClearColor = FLinearColor::Black;
		Target->InitCustomFormat(PrintW, PrintH, PF_B8G8R8A8, false);
		Target->UpdateResourceImmediate(true);
	}
	if (!Capture)
	{
		// made at the first shot, used only at a shot: it never captures by itself
		Capture = NewObject<USceneCaptureComponent2D>(World->GetWorldSettings());
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->bAlwaysPersistRenderingState = true;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->TextureTarget = Target;
		Capture->RegisterComponentWithWorld(World);
	}
	Capture->HiddenActors.Reset();
	if (AActor* Pawn = PC->GetPawn())
	{
		Capture->HiddenActors.Add(Pawn);
	}
	Capture->FOVAngle = CaptureFov(*PC);
	Capture->SetWorldLocationAndRotation(Eye, Look);
	// every shot is a cut: the kept render state still holds the last shot's exposure and temporal history, taken
	// minutes ago somewhere else (the flag clears itself after the capture)
	Capture->bCameraCutThisFrame = true;
	Capture->CaptureScene();

	FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
	TArray<FColor> Pixels;
	if (!Resource || !Resource->ReadPixels(Pixels) || Pixels.Num() != PrintW * PrintH)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHOTO the frame could not be read back"));
		return;
	}
	// black-and-white film: luma, a little more contrast
	TArray<uint8> Grey, Line;
	Grey.SetNumUninitialized(Pixels.Num());
	for (int32 I = 0; I < Pixels.Num(); ++I)
	{
		const FColor& C = Pixels[I];
		const float Y = (0.299f * C.R + 0.587f * C.G + 0.114f * C.B) / 255.f;
		Grey[I] = uint8(FMath::Clamp(FMath::SmoothStep(0.f, 1.f, Y) * 0.7f + Y * 0.3f, 0.f, 1.f) * 255.f);
	}
	// an unsharp frame is unsharp on the print: a box blur, wider the worse the focus or the steadier the hand was not
	const int32 Radius = FMath::RoundToInt((1.f - FMath::Clamp(Sharp, 0.f, 1.f)) * 7.f);
	if (Radius > 0)
	{
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const bool bRows = Pass == 0;
			const int32 Len = bRows ? PrintW : PrintH, Lines = bRows ? PrintH : PrintW, Step = bRows ? 1 : PrintW;
			Line.SetNumUninitialized(Len);
			for (int32 L = 0; L < Lines; ++L)
			{
				uint8* Row = Grey.GetData() + (bRows ? L * PrintW : L);
				int32 Sum = 0;
				for (int32 K = -Radius; K <= Radius; ++K)
				{
					Sum += Row[FMath::Clamp(K, 0, Len - 1) * Step];
				}
				for (int32 X = 0; X < Len; ++X)
				{
					Line[X] = uint8(Sum / (2 * Radius + 1));
					Sum += Row[FMath::Min(X + Radius + 1, Len - 1) * Step] - Row[FMath::Max(X - Radius, 0) * Step];
				}
				for (int32 X = 0; X < Len; ++X)
				{
					Row[X * Step] = Line[X];
				}
			}
		}
	}
	// the film's grain, the same for the same frame
	uint32 Seed = GetTypeHash(File);
	for (int32 I = 0; I < Pixels.Num(); ++I)
	{
		Seed = Seed * 1664525u + 1013904223u;
		const int32 V = FMath::Clamp(int32(Grey[I]) + int32((Seed >> 24) & 15) - 7, 0, 255);
		Pixels[I] = FColor(uint8(V), uint8(V), uint8(V), 255);
	}
	IImageWrapperModule& Wrappers = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> Jpeg = Wrappers.CreateImageWrapper(EImageFormat::JPEG);
	if (Jpeg.IsValid() && Jpeg->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), PrintW, PrintH, ERGBFormat::BGRA, 8))
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
		const TArray64<uint8>& Bytes = Jpeg->GetCompressed(86);
		if (!FFileHelper::SaveArrayToFile(Bytes, *File))
		{
			UE_LOG(LogTemp, Warning, TEXT("PHOTO could not write %s"), *File);
		}
	}
}

void ULyudPhoto::HandOut(const PhotoModel::FDeveloped& Got)
{
	UWorld* World = GetWorld();
	if (ULyudCityThings* Things = ULyudCityThings::Get(World))
	{
		for (const std::string& Stamp : Got.Stamps)
		{
			// false for a subject the town keeps no stamp of: the print is in the album all the same
			Things->GiveStamp(LyudPhotoDetail::Wide(Stamp));
		}
	}
	const AHeroCharacter* Hero = LyudPhotoDetail::HeroOf(World);
	if (ULyudBirdBook* Book = ULyudBirdBook::Get(this))
	{
		for (const std::string& Species : Got.Birds)
		{
			// a sharp photograph is as good as a look through the binoculars
			Book->NoteSeen(this, FName(*LyudPhotoDetail::Wide(Species)), false, TEXT("camera"), Hero ? Hero->GetActorLocation() : FVector::ZeroVector);
		}
	}
	TellPlaces();
	UE_LOG(LogTemp, Display, TEXT("PHOTO developed %d films: %d prints, %d stamps, %d birds, %d places"),
		(int32)Got.Rolls.size(), Got.Prints, (int32)Got.Stamps.size(), (int32)Got.Birds.size(), (int32)Got.Places.size());
}

void ULyudPhoto::TellPlaces()
{
	// every place there is a print of, each time: a quest that began after the film was developed still gets its views
	std::set<std::string> Told;
	for (const PhotoModel::FShot* Shot : State().Album())
	{
		if (!Shot->Place.empty())
		{
			Told.insert(Shot->Place);
		}
		for (const PhotoModel::FSubject& Subject : Shot->Subjects)
		{
			if (Subject.Tag.compare(0, 6, "place:") == 0 && Subject.Tag.size() > 6)
			{
				Told.insert(Subject.Tag.substr(6));
			}
			else if (Subject.Sharp >= PhotoModel::SharpEnough && Subject.Tag == "maru")
			{
				// a sharp print of Maru or of a resident is a quest's "photo maru" / "photo <resident key>"
				Told.insert(Subject.Tag);
			}
			else if (Subject.Sharp >= PhotoModel::SharpEnough && Subject.Tag.compare(0, 7, "person:") == 0 && Subject.Tag.size() > 7)
			{
				Told.insert(Subject.Tag.substr(7));
			}
		}
	}
	for (const std::string& Place : Told)
	{
		ULyudQuests::Notify(this, TEXT("photo"), LyudPhotoDetail::Wide(Place));
	}
}

void ULyudPhoto::UseStudio()
{
	PhotoModel::FState& S = State();
	const double Now = NowHours();
	const PhotoModel::FDeveloped Got = S.Collect(Now);
	if (!Got.Rolls.empty())
	{
		HandOut(Got);
		Say(TEXT("ui.photo.studio.prints"), { FStringFormatArg(Got.Prints) });
	}
	const int32 Films = S.Exposed();
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (Films > 0 && Inv)
	{
		const int32 Cost = Films * PhotoModel::StudioPrice;
		if (Inv->Spend(Cost))
		{
			S.HandIn(Now);
			Say(TEXT("ui.photo.studio.taken"), { FStringFormatArg(Films), FStringFormatArg(Cost) });
		}
		else
		{
			Say(TEXT("ui.photo.studio.no_money"), { FStringFormatArg(Cost) });
		}
	}
	else if (Got.Rolls.empty())
	{
		Say(S.AtStudio() > 0 ? TEXT("ui.photo.studio.wait") : TEXT("ui.photo.studio.nothing"));
	}
}

void ULyudPhoto::StartDarkroom()
{
	if (Bath >= 0)
	{
		return;
	}
	if (State().Exposed() <= 0)
	{
		Say(TEXT("ui.photo.dark.nothing"));
		return;
	}
	DropCamera();
	// the panel lives in the finder's overlay
	EnsureOverlay();
	Bath = 0;
	BathSeconds = 0.f;
	LockHero(true);
}

void ULyudPhoto::FinishDarkroom()
{
	Bath = -1;
	LockHero(false);
	const float Quality = PhotoModel::DarkroomQuality(BathTimes);
	const PhotoModel::FDeveloped Got = State().DevelopSelf(Quality);
	if (Got.Rolls.empty())
	{
		return;
	}
	HandOut(Got);
	if (Quality < PhotoModel::RuinedBelow)
	{
		Say(TEXT("ui.photo.dark.ruined"));
	}
	else
	{
		Say(Quality < 0.6f ? TEXT("ui.photo.dark.pale") : TEXT("ui.photo.dark.done"), { FStringFormatArg(Got.Prints) });
	}
}

void ULyudPhoto::DevelopAllNow()
{
	PhotoModel::FState& S = State();
	const double Now = NowHours();
	S.Rewind();
	S.HandIn(Now);
	const PhotoModel::FDeveloped Got = S.Collect(Now + 72.0);
	HandOut(Got);
	Say(TEXT("ui.photo.studio.prints"), { FStringFormatArg(Got.Prints) });
}

void ULyudPhoto::ToggleAlbum()
{
	if (Album.IsValid())
	{
		CloseAlbum();
		return;
	}
	OpenPages(0);
	TellPlaces();
}

void ULyudPhoto::OpenPages(int32 Mode)
{
	DropCamera();
	AlbumMode = Mode;
	// it opens on the newest prints (ShowAlbumPage holds the page to what there is)
	AlbumPage = MAX_int32 / 2;
	LockHero(true);
	ShowAlbumPage();
}

void ULyudPhoto::ShowWall()
{
	if (!Album.IsValid() && Bath < 0)
	{
		OpenPages(2);
	}
}

void ULyudPhoto::UseContest()
{
	if (Album.IsValid() || Bath >= 0)
	{
		return;
	}
	int32 Year = 0, Month = 0, Day = 0;
	Today(Year, Month, Day);
	PhotoContest::FState& C = Contest();
	if (const PhotoContest::FEntry* Judged = C.Untold(Year, Month, Day))
	{
		// the jury has sat: the place and the prize
		const int32 Place = PhotoContest::PlaceOf(Judged->Contest, Judged->Score);
		const int32 Prize = PhotoContest::PrizeOf(Place);
		const FText Was = LyudText::Get(FString(TEXT("ui.photo.contest.theme.")) + UTF8_TO_TCHAR(PhotoContest::ThemeId(PhotoContest::ThemeOf(PhotoContest::MonthOf(Judged->Contest)))));
		const int32 Of = PhotoContest::Rivals + 1;
		C.Told(Judged->Contest);
		if (Prize > 0)
		{
			if (ULyudInventory* Inv = ULyudInventory::Get(this))
			{
				Inv->Earn(Prize);
			}
			Say(Place == 1 ? TEXT("ui.photo.contest.won") : TEXT("ui.photo.contest.prize"),
				{ FStringFormatArg(Was.ToString()), FStringFormatArg(Place), FStringFormatArg(Prize) });
		}
		else
		{
			Say(TEXT("ui.photo.contest.placed"), { FStringFormatArg(Was.ToString()), FStringFormatArg(Place), FStringFormatArg(Of) });
		}
		ULyudQuests::Notify(this, TEXT("do"), Prize > 0 ? TEXT("photo_contest_prize") : TEXT("photo_contest_result"));
		return;
	}
	const FText Theme = LyudText::Get(FString(TEXT("ui.photo.contest.theme.")) + UTF8_TO_TCHAR(PhotoContest::ThemeId(PhotoContest::ThemeOf(Month))));
	if (C.CanEnter(Year, Month, Day))
	{
		if (State().Album().empty())
		{
			Say(TEXT("ui.photo.contest.no_prints"), { FStringFormatArg(Theme.ToString()), FStringFormatArg(PhotoContest::LastEntryDay) });
			return;
		}
		OpenPages(1);
	}
	else if (C.Find(PhotoContest::ContestOf(Year, Month)))
	{
		Say(TEXT("ui.photo.contest.wait"), { FStringFormatArg(PhotoContest::JudgeDay) });
	}
	else
	{
		const FText Next = LyudText::Get(FString(TEXT("ui.photo.contest.theme.")) + UTF8_TO_TCHAR(PhotoContest::ThemeId(PhotoContest::ThemeOf(Month % 12 + 1))));
		Say(TEXT("ui.photo.contest.closed"), { FStringFormatArg(Next.ToString()) });
	}
}

void ULyudPhoto::PickPrint(int32 Index)
{
	const std::vector<const PhotoModel::FShot*> Shots = State().Album();
	const int32 At = AlbumPage * LyudPhotoDetail::AlbumPerPage + Index;
	if (AlbumMode != 1 || At < 0 || At >= (int32)Shots.size())
	{
		return;
	}
	int32 Year = 0, Month = 0, Day = 0;
	Today(Year, Month, Day);
	const bool bEntered = Contest().Enter(Year, Month, Day, *Shots[(size_t)At]);
	CloseAlbum();
	if (bEntered)
	{
		Say(TEXT("ui.photo.contest.entered"), { FStringFormatArg(PhotoContest::JudgeDay) });
		ULyudQuests::Notify(this, TEXT("do"), TEXT("photo_contest_enter"));
	}
}

void ULyudPhoto::CloseAlbum()
{
	if (Album.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Album.ToSharedRef());
	}
	Album.Reset();
	PagePrints.Reset();
	LockHero(false);
}

void ULyudPhoto::ShowAlbumPage()
{
	constexpr int32 AlbumPerPage = LyudPhotoDetail::AlbumPerPage;
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}
	if (Album.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Album.ToSharedRef());
		Album.Reset();
	}
	PagePrints.Reset();
	const PhotoModel::FState& S = State();
	std::vector<const PhotoModel::FShot*> Shots = S.Album();
	// the wall: only the prints that took a prize, each with its place
	std::vector<const PhotoContest::FEntry*> Hung;
	if (AlbumMode == 2)
	{
		int32 Year = 0, Month = 0, Day = 0;
		Today(Year, Month, Day);
		std::vector<const PhotoModel::FShot*> OnWall;
		for (const PhotoContest::FEntry* Entry : Contest().Wall(Year, Month, Day))
		{
			for (const PhotoModel::FShot* Print : Shots)
			{
				if (Print->Roll == Entry->Roll && Print->Frame == Entry->Frame)
				{
					OnWall.push_back(Print);
					Hung.push_back(Entry);
				}
			}
		}
		Shots = OnWall;
	}
	const int32 Num = (int32)Shots.size();
	const int32 Pages = FMath::Max(1, (Num + AlbumPerPage - 1) / AlbumPerPage);
	AlbumPage = FMath::Clamp(AlbumPage, 0, Pages - 1);
	const UWorld* World = GetWorld();
	const UGameCalendar* Calendar = World ? World->GetSubsystem<UGameCalendar>() : nullptr;
	const FDateTime Day0 = (Calendar ? Calendar->StartDate : FDateTime(2026, 10, 20, 16, 0, 0)).GetDate();

	TArray<TSharedPtr<FSlateBrush>> Brushes;
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	TSharedPtr<SHorizontalBox> Row;
	for (int32 I = AlbumPage * AlbumPerPage; I < FMath::Min(Num, (AlbumPage + 1) * AlbumPerPage); ++I)
	{
		const PhotoModel::FShot& Shot = *Shots[(size_t)I];
		float Quality = 1.f;
		for (const PhotoModel::FRoll& Roll : S.Rolls())
		{
			if (Roll.Id == Shot.Roll)
			{
				Quality = Roll.Quality;
			}
		}
		// the print itself, from this game's folder (a grey card when the file is gone)
		TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
		Brush->ImageSize = FVector2D(396, 264);
		Brush->DrawAs = ESlateBrushDrawType::Image;
		UTexture2D* Print = FImageUtils::ImportFileAsTexture2D(PhotoFile(Shot.Roll, Shot.Frame));
		if (Print)
		{
			PagePrints.Add(Print);
			Brush->SetResourceObject(Print);
		}
		Brushes.Add(Brush);
		// a hurried darkroom prints grey and flat
		const float Tone = FMath::Lerp(0.5f, 1.f, FMath::Clamp(Quality, 0.f, 1.f));
		TSharedPtr<SWidget> Picture;
		if (Print)
		{
			Picture = SNew(SImage).Image(Brush.Get()).ColorAndOpacity(FLinearColor(Tone, Tone, Tone, 1.f));
		}
		else
		{
			Picture = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.35f, 0.35f, 0.35f));
		}
		const FDateTime When = Day0 + FTimespan::FromHours(Shot.Hours);
		const FText Place = Shot.Place.empty() ? LyudText::Get(TEXT("ui.photo.album.nowhere")) : ULyudQuests::PlaceName(FName(*LyudPhotoDetail::Wide(Shot.Place)));
		FText Caption = LyudText::Get(TEXT("ui.photo.album.caption"), { FStringFormatArg(LyudUIText::Date(When).ToString()), FStringFormatArg(Place.ToString()) });
		if (AlbumMode == 1)
		{
			// the number to press for this print
			Caption = LyudText::Get(TEXT("ui.photo.contest.pick.caption"), { FStringFormatArg(I - AlbumPage * AlbumPerPage + 1), FStringFormatArg(Caption.ToString()) });
		}
		else if (AlbumMode == 2)
		{
			const PhotoContest::FEntry& Entry = *Hung[(size_t)I];
			const FText HungTheme = LyudText::Get(FString(TEXT("ui.photo.contest.theme.")) + UTF8_TO_TCHAR(PhotoContest::ThemeId(PhotoContest::ThemeOf(PhotoContest::MonthOf(Entry.Contest)))));
			Caption = LyudText::Get(TEXT("ui.photo.contest.wall.caption"), { FStringFormatArg(PhotoContest::PlaceOf(Entry.Contest, Entry.Score)),
				FStringFormatArg(HungTheme.ToString()), FStringFormatArg(Place.ToString()) });
		}
		if (!Row.IsValid() || (I - AlbumPage * AlbumPerPage) % 2 == 0)
		{
			Rows->AddSlot().AutoHeight().Padding(0, 6)[SAssignNew(Row, SHorizontalBox)];
		}
		Row->AddSlot().AutoWidth().Padding(14, 0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				// the white border of the paper
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.97f, 0.96f, 0.93f)).Padding(FMargin(8.f))
				[
					SNew(SBox).WidthOverride(396).HeightOverride(264)
					[
						Picture.ToSharedRef()
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4, 4, 0, 0)
			[
				SNew(STextBlock).Font(LyudDiary::Hand(20.f)).ColorAndOpacity(LyudDiary::Ink)
				.Text(Caption)
			]
		];
	}
	if (Num == 0)
	{
		Rows->AddSlot().AutoHeight().Padding(0, 120).HAlign(HAlign_Center)
		[
			SNew(STextBlock).Font(LyudDiary::Hand(26.f)).ColorAndOpacity(LyudDiary::Pencil)
			.Text(LyudText::Get(AlbumMode == 2 ? TEXT("ui.photo.contest.wall.empty") : TEXT("ui.photo.album.empty")))
		];
	}

	FText Title = LyudText::Get(TEXT("ui.photo.album.title"));
	if (AlbumMode == 1)
	{
		int32 Year = 0, Month = 0, Day = 0;
		Today(Year, Month, Day);
		Title = LyudText::Get(TEXT("ui.photo.contest.pick.title"), { FStringFormatArg((LyudText::Get(FString(TEXT("ui.photo.contest.theme.")) + UTF8_TO_TCHAR(PhotoContest::ThemeId(PhotoContest::ThemeOf(Month))))).ToString()) });
	}
	else if (AlbumMode == 2)
	{
		Title = LyudText::Get(TEXT("ui.photo.contest.wall.title"));
	}
	const TSharedRef<SWidget> Page = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f))
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(940).HeightOverride(790)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(LyudDiary::Paper).Padding(FMargin(26, 16))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(LyudDiary::Hand(34.f, true)).ColorAndOpacity(LyudDiary::Ink).Text(Title)
					]
					+ SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Center)
					[
						Rows
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1)
						[
							SNew(STextBlock).Font(LyudDiary::Hand(18.f)).ColorAndOpacity(LyudDiary::Pencil).Text(LyudText::Get(AlbumMode == 1 ? TEXT("ui.photo.contest.pick.keys") : TEXT("ui.photo.album.keys")))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock).Font(LyudDiary::Hand(18.f)).ColorAndOpacity(LyudDiary::Pencil)
							.Text(LyudText::Get(TEXT("ui.photo.album.page"), { FStringFormatArg(AlbumPage + 1), FStringFormatArg(Pages) }))
						]
					]
				]
			]
		];
	Album = SNew(LyudPhotoDetail::SLyudAlbumPage, Page, MoveTemp(Brushes));
	GEngine->GameViewport->AddViewportWidgetContent(Album.ToSharedRef(), 67);
}
