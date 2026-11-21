#include "Look/LyudLook.h"

#include "LookModel.h"
#include "AnimationRuntime.h"
#include "Audio/LyudSoundscape.h"
#include "City/LyudCityThings.h"
#include "City/SLyudCityPage.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "HeroShadowMesh.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "Plants/GameCalendar.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "Weather/LyudWeather.h"
#if __has_include("Dev/LyudDev.h")
#include "Dev/LyudDev.h"
#define LYUD_LOOK_DEV 1
#else
#define LYUD_LOOK_DEV 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudLook, Log, All);

namespace LyudLookImpl
{
	/** The look is compared with the pockets and the calendar this often (s). */
	constexpr float SyncEvery = 0.5f;
	/** A resident adds the word this long after the greeting (s), if the hero is still this near (cm). */
	constexpr float SayAfter = 3.2f;
	constexpr float SayWithin = 700.f;
	/** A roof this low over him (cm) is indoors: nobody speaks of the rain there. */
	constexpr float RoofCm = 1500.f;
	/** Rain lighter than this (mm/h) wets nobody. */
	constexpr float WetRate = 0.15f;
	/** A cut's lines, a misfit's lines (strings/look.json: line.look.cut.<cut>.<n>, line.look.<fit>.<n>). */
	constexpr int CutLines = 3;
	constexpr int FitLines = 2;

	std::string LookStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
	FString LookStr(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	FText CutName(const FString& Cut)
	{
		return LyudText::Get(TEXT("look.cut.") + Cut);
	}

	ELyudEquipSlot SlotOfWear(const LyudLook::FWear& W)
	{
		return W.Slot == "head" ? ELyudEquipSlot::Head : ELyudEquipSlot::Outer;
	}

	bool IsRain(const FLyudWeatherState& W)
	{
		switch (W.Precip)
		{
		case ELyudPrecipType::Drizzle:
		case ELyudPrecipType::Rain:
		case ELyudPrecipType::Showers:
		case ELyudPrecipType::Thunderstorm:
		case ELyudPrecipType::Sleet:
		case ELyudPrecipType::FreezingDrizzle:
		case ELyudPrecipType::FreezingRain:
			return W.PrecipRate >= WetRate;
		default:
			return false;
		}
	}

	/** The hair and the cap only cast the shadow, like the shadow's body they sit on. */
	void ShadowOnly(UStaticMeshComponent* C)
	{
		C->SetRenderInMainPass(false);
		C->SetRenderInDepthPass(false);
		C->SetCastShadow(true);
		C->bCastDynamicShadow = true;
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->bReceivesDecals = false;
		C->SetCanEverAffectNavigation(false);
	}
}

using namespace LyudLookImpl;

ULyudLook::ULyudLook() = default;
ULyudLook::~ULyudLook() = default;

ULyudLook* ULyudLook::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudLook>() : nullptr;
}

void ULyudLook::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	State = MakePimpl<LyudLook::FState>();
	Keep();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("Look"), this);
		Saves->OnNewGame.AddWeakLambda(this, [this]()
		{
			ClosePage(true);
			if (State)
			{
				*State = LyudLook::FState();
				Keep();
			}
			bDiaryCut = bDiaryClothes = false;
			bGrownTold = true;
			SayPerson = -1;
		});
	}
	if (ULyudCityThings* Things = ULyudCityThings::Get(&InWorld))
	{
		OpenHandle = Things->OnOpen.AddUObject(this, &ULyudLook::OnCityOpen);
	}
	bBegun = true;
}

void ULyudLook::Deinitialize()
{
	ClosePage(true);
	if (ULyudCityThings* Things = ULyudCityThings::Get(this))
	{
		Things->OnOpen.Remove(OpenHandle);
	}
	bBegun = false;
	Super::Deinitialize();
}

double ULyudLook::Day() const
{
	return (UGameCalendar::NowIn(GetWorld()) - FDateTime(2000, 1, 1)).GetTotalDays();
}

void ULyudLook::SyncState()
{
	// the save writes and reads Saved whenever it likes: a text that is not ours is a game just loaded
	if (State && Saved != Parsed)
	{
		LyudLook::FState Read;
		if (LyudLook::Load(Read, LookStd(Saved)))
		{
			*State = Read;
		}
		else
		{
			*State = LyudLook::FState();
		}
		Keep();
		// a loaded head that has grown out is not news
		bGrownTold = LyudLook::CutNow(*State, Day()) == "shaggy";
	}
}

void ULyudLook::Keep()
{
	if (State)
	{
		Saved = LookStr(LyudLook::Save(*State));
		Parsed = Saved;
	}
}

FString ULyudLook::CutNow() const
{
	return State ? LookStr(LyudLook::CutNow(*State, Day())) : FString(TEXT("shaggy"));
}

void ULyudLook::Toast(const FText& Text) const
{
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->Toast(Text);
	}
}

void ULyudLook::Stamp(const FString& Piece) const
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(this))
	{
		Things->GiveStamp(TEXT("looks/") + Piece);
	}
}

void ULyudLook::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	if (!Hero || !State)
	{
		ClosePage(true);
		return;
	}
	SyncIn -= DeltaTime;
	if (SyncIn <= 0.f)
	{
		SyncIn = SyncEvery;
		SyncState();
		SyncLook(Hero);
	}
	if (SayPerson >= 0)
	{
		SayIn -= DeltaTime;
		if (SayIn <= 0.f)
		{
			const int32 Person = SayPerson;
			SayPerson = -1;
			UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(World);
			ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(World);
			AActor* Body = Life ? Life->BodyFor(Person) : nullptr;
			if (Body && Talk && FVector::Dist(Body->GetActorLocation(), Hero->GetActorLocation()) < SayWithin && Talk->SayLine(Body, SayKey, Person))
			{
				const double Now = Day();
				if (bSayRemark)
				{
					LyudLook::NoteRemark(*State, Now, Now * 24.0);
				}
				else
				{
					LyudLook::NoteComment(*State, Person, Now * 24.0);
				}
				Keep();
			}
		}
	}
	if (Page)
	{
		TickPage(PC);
	}
}

// --- what is on him -----------------------------------------------------------------------------------------------------

void ULyudLook::SyncLook(AHeroCharacter* Hero)
{
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv)
	{
		return;
	}
	if (LookOf.Get() != Hero)
	{
		// another body (a new game, a respawn): nothing of ours is on it yet
		LookOf = Hero;
		OuterOn = NAME_None;
		CutOn.Reset();
		bCapOn = false;
		OuterSlot = INDEX_NONE;
		OwnCloth = nullptr;
		Cloth = nullptr;
		Hair = nullptr;
		Cap = nullptr;
		if (const USkeletalMeshComponent* Mesh = Hero->GetMesh())
		{
			const TArray<FName> Slots = Mesh->GetMaterialSlotNames();
			for (const FName Slot : Slots)
			{
				if (Slot.ToString().Contains(TEXT("overshirt"), ESearchCase::IgnoreCase))
				{
					OuterSlot = Mesh->GetMaterialIndex(Slot);
					OwnCloth = Mesh->GetMaterial(OuterSlot);
					break;
				}
			}
		}
		if (OuterSlot == INDEX_NONE)
		{
			UE_LOG(LogLyudLook, Warning, TEXT("LOOK the hero's body has no overshirt material slot: the outer clothes will not show"));
		}
	}
	const FName Outer = Inv->InSlot(ELyudEquipSlot::Outer);
	if (Outer != OuterOn)
	{
		ApplyOuter(Hero, Outer);
		const LyudLook::FWear* Bought = Outer.IsNone() ? nullptr : LyudLook::FindWear(LookStd(Outer.ToString()));
		if (Bought && Bought->Price > 0)
		{
			Stamp(Outer.ToString());
		}
	}
	const bool bCap = Inv->IsWorn(TEXT("cap"));
	const std::string Cut = LyudLook::CutNow(*State, Day());
	if (bCap != bCapOn || !CutOn.Equals(UTF8_TO_TCHAR(Cut.c_str())))
	{
		if (bCap && !bCapOn)
		{
			Stamp(TEXT("cap"));
		}
		if (Cut == "shaggy" && !bGrownTold)
		{
			bGrownTold = true;
			Toast(LyudText::Get(TEXT("look.grown")));
		}
		ApplyHead(Hero, LookStr(Cut), bCap);
	}
}

void ULyudLook::ApplyOuter(AHeroCharacter* Hero, FName Item)
{
	OuterOn = Item;
	USkeletalMeshComponent* Mesh = Hero->GetMesh();
	if (!Mesh || OuterSlot == INDEX_NONE || !OwnCloth)
	{
		return;
	}
	const LyudLook::FWear* Wear = Item.IsNone() ? nullptr : LyudLook::FindWear(LookStd(Item.ToString()));
	// the old cloth is dropped (collected), never kept beside the new one
	Cloth = nullptr;
	if (!Wear || Wear->bOwnCloth)
	{
		// his own corduroy overshirt; with nothing in the slot too: the hoodie has no back under it to show
		Mesh->SetMaterial(OuterSlot, OwnCloth);
		return;
	}
	Cloth = UMaterialInstanceDynamic::Create(OwnCloth, this);
	if (!Cloth)
	{
		return;
	}
	// a plain cloth of the piece's colour over the overshirt's own folds; a slick one (the raincoat's rubber) loses the weave
	if (UTexture2D* White = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")))
	{
		Cloth->SetTextureParameterValue(TEXT("BaseColor"), White);
	}
	if (Wear->Rough < 0.4f)
	{
		if (UTexture2D* Flat = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal")))
		{
			Cloth->SetTextureParameterValue(TEXT("Normal"), Flat);
		}
	}
	Cloth->SetVectorParameterValue(TEXT("Tint"), FLinearColor(Wear->Tint[0], Wear->Tint[1], Wear->Tint[2], 1.f));
	Cloth->SetScalarParameterValue(TEXT("Roughness"), Wear->Rough);
	Mesh->SetMaterial(OuterSlot, Cloth);
}

void ULyudLook::ApplyHead(AHeroCharacter* Hero, const FString& Cut, bool bCap)
{
	CutOn = Cut;
	bCapOn = bCap;
	UHeroShadowMesh* Shadow = Hero->GetShadowBody();
	const USkinnedAsset* Asset = Shadow ? Shadow->GetSkinnedAsset() : nullptr;
	if (!Asset)
	{
		// the body is not there yet: asked again at the next look
		CutOn.Reset();
		return;
	}
	static const FName HeadBone(TEXT("head"));
	const int32 Bone = Asset->GetRefSkeleton().FindBoneIndex(HeadBone);
	// the meshes are modelled in the body's own space (blender/hero/build_looks.py): on the head bone they sit where the bind pose has them
	const FTransform OnHead = Bone != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(Asset->GetRefSkeleton(), Bone).Inverse() : FTransform::Identity;
	auto MeshOf = [this](const FName Name) -> UStaticMesh*
	{
		if (const TObjectPtr<UStaticMesh>* Found = Meshes.Find(Name))
		{
			return Found->Get();
		}
		const FString Path = FString::Printf(TEXT("/Game/Characters/Hero/Look/%s.%s"), *Name.ToString(), *Name.ToString());
		UStaticMesh* Loaded = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Loaded)
		{
			UE_LOG(LogLyudLook, Display, TEXT("LOOK %s is not imported: the shadow keeps its own head (blender/hero/build_looks.py, unreal/scripts/import_looks.sh)"), *Path);
		}
		Meshes.Add(Name, Loaded);
		return Loaded;
	};
	auto Put = [&](TObjectPtr<UStaticMeshComponent>& Comp, UStaticMesh* Mesh)
	{
		if (!Mesh)
		{
			if (Comp)
			{
				Comp->DestroyComponent();
				Comp = nullptr;
			}
			return;
		}
		if (!Comp)
		{
			Comp = NewObject<UStaticMeshComponent>(Hero);
			ShadowOnly(Comp);
			Comp->SetupAttachment(Shadow, HeadBone);
			Comp->RegisterComponent();
			Comp->SetRelativeTransform(OnHead);
		}
		Comp->SetStaticMesh(Mesh);
	};
	UStaticMesh* HairMesh = Cut == TEXT("shaggy") ? nullptr : MeshOf(FName(*(TEXT("SM_HeroHair_") + Cut)));
	Put(Hair, HairMesh);
	Put(Cap, bCap ? MeshOf(TEXT("SM_HeroCap")) : nullptr);
	// his own grown-out hair leaves the shadow while a barber's cut is on it
	const TArray<FName> Slots = Shadow->GetMaterialSlotNames();
	for (const FName Slot : Slots)
	{
		if (Slot.ToString().Contains(TEXT("hair"), ESearchCase::IgnoreCase))
		{
			const int32 Index = Shadow->GetMaterialIndex(Slot);
			for (int32 Lod = 0; Lod < Shadow->GetNumLODs(); ++Lod)
			{
				Shadow->ShowMaterialSection(Index, Index, HairMesh == nullptr, Lod);
			}
		}
	}
}

uint8 ULyudLook::FitNow(const AHeroCharacter* Hero) const
{
	const UWorld* World = GetWorld();
	const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(this);
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!World || !Weather || !Inv || !Hero)
	{
		return 0;
	}
	// under a roof nobody minds what he came in
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LyudLookRoof), false, Hero);
	const FVector From = Hero->GetActorLocation();
	if (World->LineTraceSingleByChannel(Hit, From, From + FVector(0.0, 0.0, RoofCm), ECC_Visibility, Params))
	{
		return 0;
	}
	const FLyudWeatherState W = Weather->GetState();
	const FName Outer = Inv->InSlot(ELyudEquipSlot::Outer);
	return static_cast<uint8>(LyudLook::Fit(Outer.IsNone() ? std::string() : LookStd(Outer.ToString()), Inv->IsWorn(TEXT("cap")), W.TempC, IsRain(W)));
}

void ULyudLook::OnGreeted(int32 PersonId)
{
	if (!State || PersonId < 0 || SayPerson >= 0)
	{
		return;
	}
	SyncState();
	const double Now = Day();
	if (LyudLook::WillComment(*State, PersonId, Now, Now * 24.0))
	{
		SayKey = FString::Printf(TEXT("line.look.cut.%s.%d"), *LookStr(State->Cut), LyudLook::CommentLine(PersonId, State->Serial, CutLines));
		bSayRemark = false;
	}
	else
	{
		const AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
		const LyudLook::EFit Fit = static_cast<LyudLook::EFit>(FitNow(Hero));
		if (!LyudLook::WillRemark(*State, Fit, Now, Now * 24.0))
		{
			return;
		}
		SayKey = FString::Printf(TEXT("line.look.%s.%d"), UTF8_TO_TCHAR(LyudLook::FitId(Fit)), LyudLook::CommentLine(PersonId, static_cast<int>(Now), FitLines));
		bSayRemark = true;
	}
	SayPerson = PersonId;
	SayIn = SayAfter;
}

bool ULyudLook::GiveCut(const FString& Cut)
{
	if (!State)
	{
		return false;
	}
	SyncState();
	const double Now = Day();
	const bool bTrim = LyudLook::CutNow(*State, Now) == LookStd(Cut);
	if (!LyudLook::GetCut(*State, LookStd(Cut), Now))
	{
		return false;
	}
	Keep();
	bGrownTold = false;
	SyncIn = 0.f;
	Stamp(Cut);
	Toast(bTrim ? LyudText::Get(TEXT("look.barber.trim")) : LyudText::Get(TEXT("look.barber.done"), { FStringFormatArg(CutName(Cut).ToString()) }));
	if (!bDiaryCut)
	{
		bDiaryCut = true;
		if (ULyudQuests* Quests = ULyudQuests::Get(this))
		{
			Quests->AddNote(TEXT("look.diary.first_cut"), { CutName(Cut).ToString() });
		}
	}
	UE_LOG(LogLyudLook, Display, TEXT("LOOK haircut: %s%s"), *Cut, bTrim ? TEXT(" (a trim)") : TEXT(""));
	return true;
}

// --- the pages ----------------------------------------------------------------------------------------------------------

void ULyudLook::OnCityOpen(FName Screen, int32)
{
	if (Screen == TEXT("barber"))
	{
		OpenPage(EPage::Barber);
	}
	else if (Screen == TEXT("clothes"))
	{
		ULyudQuests::Notify(this, TEXT("do"), TEXT("ps_clothes"));
		OpenPage(EPage::Clothes);
	}
	else if (Screen == TEXT("fitting"))
	{
		ULyudQuests::Notify(this, TEXT("do"), TEXT("ps_fitting_room"));
		OpenPage(EPage::Fitting);
	}
}

void ULyudLook::OpenPage(EPage Which)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Viewport || !State || !Inv || Which == EPage::None)
	{
		return;
	}
	SyncState();
	FString Body;
	FText Title, Keys;
	PageItems.Reset();
	int32 Line = 0;
	if (Which == EPage::Barber)
	{
		const std::string Now = LyudLook::CutNow(*State, Day());
		for (const std::string& Cut : LyudLook::Offered())
		{
			PageItems.Add(FName(*LookStr(Cut)));
			Body += LyudText::Get(Cut == Now ? TEXT("look.barber.line_now") : TEXT("look.barber.line"),
				{ FStringFormatArg(++Line), FStringFormatArg(CutName(LookStr(Cut)).ToString()) }).ToString() + TEXT("\n");
		}
		Title = LyudText::Get(TEXT("look.barber.title"));
		Keys = LyudText::Get(TEXT("look.barber.keys"));
	}
	else
	{
		for (const LyudLook::FWear& W : LyudLook::Clothes())
		{
			const FName Item(*LookStr(W.Item));
			const bool bHave = Inv->Count(Item) > 0;
			const FString Name = ULyudInventory::ItemName(Item).ToString();
			if (Which == EPage::Clothes)
			{
				if (W.Price <= 0)
				{
					continue;
				}
				PageItems.Add(Item);
				Body += (bHave ? LyudText::Get(TEXT("look.rail.line_have"), { FStringFormatArg(++Line), FStringFormatArg(Name) })
					: LyudText::Get(TEXT("look.rail.line"), { FStringFormatArg(++Line), FStringFormatArg(Name), FStringFormatArg(W.Price) })).ToString() + TEXT("\n");
			}
			else if (bHave)
			{
				PageItems.Add(Item);
				Body += LyudText::Get(Inv->IsWorn(Item) ? TEXT("look.fit.line_on") : TEXT("look.fit.line"), { FStringFormatArg(++Line), FStringFormatArg(Name) }).ToString() + TEXT("\n");
			}
		}
		if (Which == EPage::Fitting && PageItems.Num() == 0)
		{
			Body = LyudText::Get(TEXT("look.fit.none")).ToString();
		}
		Title = LyudText::Get(Which == EPage::Clothes ? TEXT("look.rail.title") : TEXT("look.fit.title"));
		Keys = LyudText::Get(Which == EPage::Clothes ? TEXT("look.rail.keys") : TEXT("look.fit.keys"));
	}
	if (!Page)
	{
		Page = SNew(SLyudCityPage);
		Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 61);
	}
	Page->SetScreen(false);
	Page->SetPage(Title, FText::FromString(Body.TrimEnd()), Keys);
	PageKind = Which;
	PageFrame = GFrameCounter;
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (PC && !MoveBlockedBy.IsValid())
	{
		PC->SetIgnoreMoveInput(true);
		MoveBlockedBy = PC;
	}
}

void ULyudLook::ClosePage(bool bChosen)
{
	const EPage Was = PageKind;
	if (Page)
	{
		UWorld* World = GetWorld();
		if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Page.ToSharedRef());
		}
		Page.Reset();
	}
	if (APlayerController* PC = MoveBlockedBy.Get())
	{
		PC->SetIgnoreMoveInput(false);
	}
	MoveBlockedBy.Reset();
	PageKind = EPage::None;
	PageItems.Reset();
	if (Was == EPage::Barber && !bChosen && State)
	{
		// the chair is paid for: "any way you like" is a trim of what he has, or short back and sides over a grown-out head
		const std::string Now = LyudLook::CutNow(*State, Day());
		AtChair(LookStr(Now == "shaggy" ? std::string("short") : Now));
	}
}

void ULyudLook::TickPage(APlayerController* PC)
{
	UWorld* World = GetWorld();
	const ULyudInventoryUI* UI = World ? ULyudInventoryUI::Get(World) : nullptr;
	if (!World || !PC || (UI && UI->IsOpen()))
	{
		ClosePage(false);
		return;
	}
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if ((Menu && Menu->IsOpen()) || GFrameCounter == PageFrame)
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		ClosePage(false);
		return;
	}
	static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	const int32 Num = FMath::Min<int32>(PageItems.Num(), UE_ARRAY_COUNT(Digits));
	for (int32 I = 0; I < Num; ++I)
	{
		if (!PC->WasInputKeyJustPressed(Digits[I]))
		{
			continue;
		}
		const EPage Kind = PageKind;
		if (Kind == EPage::Barber)
		{
			const FName Cut = PageItems[I];
			ClosePage(true);
			AtChair(Cut.ToString());
		}
		else if (Kind == EPage::Clothes)
		{
			Buy(I);
		}
		else
		{
			PutOn(I);
		}
		return;
	}
}

void ULyudLook::AtChair(const FString& Cut)
{
	if (!GiveCut(Cut))
	{
		return;
	}
	ULyudQuests::Notify(this, TEXT("do"), TEXT("haircut"));
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (ULyudSoundscapeSubsystem* Sound = Pawn ? ULyudSoundscapeSubsystem::Get(this) : nullptr)
	{
		Sound->PlayEffect(TEXT("scissors"), Pawn->GetActorLocation(), 0.9f);
	}
}

void ULyudLook::Buy(int32 Index)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	const LyudLook::FWear* Wear = PageItems.IsValidIndex(Index) ? LyudLook::FindWear(LookStd(PageItems[Index].ToString())) : nullptr;
	if (!Inv || !Wear)
	{
		return;
	}
	const FName Item = PageItems[Index];
	const FString Name = ULyudInventory::ItemName(Item).ToString();
	if (Inv->Count(Item) > 0)
	{
		// his already: the rail's mirror will do to put it on
		Inv->Wear(Item);
		Toast(LyudText::Get(TEXT("look.fit.on"), { FStringFormatArg(Name) }));
	}
	else if (!Inv->Spend(Wear->Price))
	{
		Toast(LyudText::Get(TEXT("look.rail.no_money"), { FStringFormatArg(Wear->Price) }));
		return;
	}
	else
	{
		Inv->Add(Item, 1);
		Inv->Wear(Item);
		Stamp(Item.ToString());
		Toast(LyudText::Get(TEXT("look.rail.bought"), { FStringFormatArg(Name) }));
		if (!bDiaryClothes)
		{
			bDiaryClothes = true;
			if (ULyudQuests* Quests = ULyudQuests::Get(this))
			{
				Quests->AddNote(TEXT("look.diary.first_clothes"), { Name });
			}
		}
		UE_LOG(LogLyudLook, Display, TEXT("LOOK bought %s for %d"), *Item.ToString(), Wear->Price);
	}
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (ULyudSoundscapeSubsystem* Sound = Pawn ? ULyudSoundscapeSubsystem::Get(this) : nullptr)
	{
		Sound->PlayEffect(TEXT("hanger_clink"), Pawn->GetActorLocation(), 0.8f);
	}
	SyncIn = 0.f;
	OpenPage(EPage::Clothes);
}

void ULyudLook::PutOn(int32 Index)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	const LyudLook::FWear* Wear = PageItems.IsValidIndex(Index) ? LyudLook::FindWear(LookStd(PageItems[Index].ToString())) : nullptr;
	if (!Inv || !Wear)
	{
		return;
	}
	const FName Item = PageItems[Index];
	const FString Name = ULyudInventory::ItemName(Item).ToString();
	const ELyudEquipSlot Slot = SlotOfWear(*Wear);
	if (!Inv->IsWorn(Item))
	{
		Inv->Wear(Item);
		Toast(LyudText::Get(TEXT("look.fit.on"), { FStringFormatArg(Name) }));
	}
	else if (Slot == ELyudEquipSlot::Head)
	{
		Inv->TakeOff(Slot);
		Toast(LyudText::Get(TEXT("look.fit.off"), { FStringFormatArg(Name) }));
	}
	else if (!Wear->bOwnCloth && Inv->Count(TEXT("overshirt")) > 0)
	{
		// an outer piece comes off into his own overshirt: the hoodie alone has no back to show
		Inv->Wear(TEXT("overshirt"));
		Toast(LyudText::Get(TEXT("look.fit.off"), { FStringFormatArg(Name) }));
	}
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (ULyudSoundscapeSubsystem* Sound = Pawn ? ULyudSoundscapeSubsystem::Get(this) : nullptr)
	{
		Sound->PlayEffect(TEXT("hanger_clink"), Pawn->GetActorLocation(), 0.8f);
	}
	SyncIn = 0.f;
	OpenPage(EPage::Fitting);
}

// --- console and developer mode -----------------------------------------------------------------------------------------

FString ULyudLook::Report() const
{
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const double Now = Day();
	const LyudLook::EFit Fit = static_cast<LyudLook::EFit>(FitNow(Hero));
	return FString::Printf(TEXT("cut %s (the barber's %s, %.1f days ago, haircut #%d, %d have spoken), outer %s%s, cap %s, hair mesh %s, fit out of doors: %s"),
		*CutNow(), State ? *LookStr(State->Cut) : TEXT("?"), State ? Now - State->CutDay : 0.0, State ? State->Serial : 0, State ? static_cast<int32>(State->Told.size()) : 0,
		Inv ? *Inv->InSlot(ELyudEquipSlot::Outer).ToString() : TEXT("?"), OuterSlot == INDEX_NONE ? TEXT(" (no overshirt slot)") : (Cloth ? TEXT(" (cloth changed)") : TEXT("")),
		bCapOn ? (Cap ? TEXT("on") : TEXT("on (no mesh)")) : TEXT("off"), Hair ? TEXT("on") : TEXT("none"),
		Fit == LyudLook::EFit::Fine ? TEXT("fine") : UTF8_TO_TCHAR(LyudLook::FitId(Fit)));
}

void ULyudLook::DevOpen(const FString& Screen)
{
	OpenPage(Screen == TEXT("barber") ? EPage::Barber : Screen == TEXT("fitting") ? EPage::Fitting : EPage::Clothes);
}

void ULyudLook::DevNextCut()
{
	if (!State)
	{
		return;
	}
	const std::vector<std::string> All = LyudLook::Offered();
	const std::string Now = LyudLook::CutNow(*State, Day());
	size_t Next = 0;
	for (size_t I = 0; I < All.size(); ++I)
	{
		if (All[I] == Now)
		{
			Next = (I + 1) % All.size();
		}
	}
	GiveCut(LookStr(All[Next]));
}

void ULyudLook::DevClothes()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	for (const LyudLook::FWear& W : LyudLook::Clothes())
	{
		const FName Item(*LookStr(W.Item));
		if (Inv && Inv->Count(Item) <= 0)
		{
			Inv->Add(Item, 1);
		}
	}
}

void ULyudLook::DevNextOuter()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv)
	{
		return;
	}
	TArray<FName> Held;
	for (const LyudLook::FWear& W : LyudLook::Clothes())
	{
		const FName Item(*LookStr(W.Item));
		if (W.Slot == "outer" && Inv->Count(Item) > 0)
		{
			Held.Add(Item);
		}
	}
	if (Held.Num() > 0)
	{
		const int32 At = Held.IndexOfByKey(Inv->InSlot(ELyudEquipSlot::Outer));
		Inv->Wear(Held[(At + 1) % Held.Num()]);
		SyncIn = 0.f;
	}
}

bool ULyudLook::DevWear(const FString& Item)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv || !LyudLook::FindWear(LookStd(Item)))
	{
		return false;
	}
	const FName Id(*Item);
	if (Inv->Count(Id) <= 0)
	{
		Inv->Add(Id, 1);
	}
	SyncIn = 0.f;
	return Inv->Wear(Id);
}

namespace LyudLookConsole
{
	ULyudLook* Of(UWorld* World) { return World ? World->GetSubsystem<ULyudLook>() : nullptr; }

	FAutoConsoleCommandWithWorldAndArgs GInfo(TEXT("lyud.Look.Info"), TEXT("The hero's haircut, what he wears and how it suits the weather, into the log."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (const ULyudLook* L = Of(World)) { UE_LOG(LogLyudLook, Display, TEXT("LOOK %s"), *L->Report()); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GCut(TEXT("lyud.Look.Cut"), TEXT("A haircut now: crop | short | parted (none: the next of the list)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudLook* L = Of(World))
			{
				if (Args.Num() == 0) { L->DevNextCut(); }
				else if (!L->GiveCut(Args[0])) { UE_LOG(LogLyudLook, Display, TEXT("LOOK no such cut: %s"), *Args[0]); }
				UE_LOG(LogLyudLook, Display, TEXT("LOOK %s"), *L->Report());
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GWear(TEXT("lyud.Look.Wear"), TEXT("Holds and puts on a piece: overshirt | wind_jacket | raincoat | winter_coat | cap."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudLook* L = Of(World))
			{
				const bool bOk = Args.Num() > 0 && L->DevWear(Args[0]);
				UE_LOG(LogLyudLook, Display, TEXT("LOOK wear %s: %s"), Args.Num() > 0 ? *Args[0] : TEXT("?"), bOk ? TEXT("on") : TEXT("no"));
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GOpen(TEXT("lyud.Look.Open"), TEXT("A page wherever he stands: barber | clothes | fitting."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudLook* L = Of(World)) { L->DevOpen(Args.Num() > 0 ? Args[0] : FString(TEXT("clothes"))); }
		}));
}

#if LYUD_LOOK_DEV
namespace LyudLookDev
{
	using namespace LyudDev;
	FAutoAction GCutAction(TEXT("look.cut"), TEXT("look"), TEXT("dev.action.look.cut"),
		[](UWorld* W) { if (ULyudLook* L = LyudLookConsole::Of(W)) { L->DevNextCut(); } });
	FAutoAction GClothesAction(TEXT("look.clothes"), TEXT("look"), TEXT("dev.action.look.clothes"),
		[](UWorld* W) { if (ULyudLook* L = LyudLookConsole::Of(W)) { L->DevClothes(); } });
	FAutoAction GOuterAction(TEXT("look.outer"), TEXT("look"), TEXT("dev.action.look.outer"),
		[](UWorld* W) { if (ULyudLook* L = LyudLookConsole::Of(W)) { L->DevNextOuter(); } });
}
#endif
