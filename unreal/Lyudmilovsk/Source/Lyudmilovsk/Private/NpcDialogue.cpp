#include "NpcDialogue.h"
#include "UI/LyudQuests.h"

#include "BoothOwner.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HeroSeat.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Sky/LyudSky.h"
#include "Components/AudioComponent.h"
#include "People/LyudBodyAnim.h"
#include "Sound/SoundBase.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Brushes/SlateColorBrush.h"

// ---------------------------------------------------------------------------------------------------
// The dialogue file

namespace
{
	// "hero.owner_first.2" -> line.hero.owner_first.2, said by the player
	FNpcLine ParseLine(FString S)
	{
		S.TrimStartAndEndInline();
		FNpcLine Line;
		Line.bHero = S.StartsWith(TEXT("hero."));
		Line.Key = TEXT("line.") + S;
		return Line;
	}

	TArray<FNpcLine> ParseExchange(const FString& S)
	{
		TArray<FString> Parts;
		S.ParseIntoArray(Parts, TEXT("|"));
		TArray<FNpcLine> Out;
		for (const FString& P : Parts)
		{
			Out.Add(ParseLine(P));
		}
		return Out;
	}
}

void UNpcDialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Reload();
	// TalkCounts goes into every save under "NpcDialogue"
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("NpcDialogue"), this);
	}
}

void UNpcDialogueSubsystem::Reload()
{
	Characters.Reset();
	const FString Path = FPaths::ProjectContentDir() / TEXT("Dialogue/npc_dialogue.txt");
	TArray<FString> FileLines;
	if (!FFileHelper::LoadFileToStringArray(FileLines, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("NpcDialogue: no %s"), *Path);
		return;
	}
	FNpcLines* Current = nullptr;
	for (FString Raw : FileLines)
	{
		Raw.TrimStartAndEndInline();
		if (Raw.IsEmpty() || Raw.StartsWith(TEXT("#")))
		{
			continue;
		}
		if (Raw.StartsWith(TEXT("[")) && Raw.EndsWith(TEXT("]")))
		{
			const FString Id = Raw.Mid(1, Raw.Len() - 2).TrimStartAndEnd();
			Current = &Characters.Add(FName(Id));
			Current->NameKey = FString::Printf(TEXT("npc.%s.name"), *Id.ToLower());
			Current->RoleKey = FString::Printf(TEXT("npc.%s.role"), *Id.ToLower());
			continue;
		}
		FString Key, Value;
		if (!Current || !Raw.Split(TEXT("="), &Key, &Value))
		{
			continue;
		}
		Key.TrimStartAndEndInline();
		Value.TrimStartAndEndInline();
		if (Key == TEXT("first"))
		{
			Current->First.Append(ParseExchange(Value));
		}
		else if (Key == TEXT("again"))
		{
			Current->Again.Add(ParseExchange(Value));
		}
		else if (Key.StartsWith(TEXT("sun.")))
		{
			Current->Sun.Add(FName(Key.Mid(4)), ParseExchange(Value));
		}
		else if (Key.StartsWith(TEXT("quest.")) || Key.StartsWith(TEXT("aside.")))
		{
			const bool bAside = Key.StartsWith(TEXT("aside."));
			// quest.<quest>.<stage>[,<stage>...] = the talk while the quest is on one of these stages; ....again = later talks there
			TArray<FString> Parts;
			Key.ParseIntoArray(Parts, TEXT("."));
			if (Parts.Num() < 3)
			{
				UE_LOG(LogTemp, Warning, TEXT("NpcDialogue: '%s' needs quest.<quest>.<stage>"), *Key);
				continue;
			}
			const FName Quest(*Parts[1]);
			TArray<FString> StageNames;
			Parts[2].ParseIntoArray(StageNames, TEXT(","));
			TArray<FName> Stages;
			for (FString& StageName : StageNames)
			{
				StageName.TrimStartAndEndInline();
				Stages.Add(FName(*StageName));
			}
			// ....if.<cond>[+<cond>] = a variant of the talk for when the conditions hold (QuestTalkHolds)
			TArray<FString> When;
			int32 Tail = 3;
			if (Parts.Num() > 4 && Parts[3] == TEXT("if"))
			{
				// a condition may hold dots itself (flag:remark.<id>): everything up to a last ".again"
				int32 CondEnd = Parts.Num();
				if (Parts.Last() == TEXT("again"))
				{
					--CondEnd;
				}
				FString Conds = Parts[4];
				for (int32 PI = 5; PI < CondEnd; ++PI)
				{
					Conds += TEXT(".") + Parts[PI];
				}
				Conds.ParseIntoArray(When, TEXT("+"));
				Tail = CondEnd;
			}
			FNpcQuestTalk* Talk = Current->QuestTalks.FindByPredicate([&Quest, &Stages, &When, bAside](const FNpcQuestTalk& T) { return T.Quest == Quest && T.Stages == Stages && T.When == When && T.bAside == bAside; });
			if (!Talk)
			{
				Talk = &Current->QuestTalks.AddDefaulted_GetRef();
				Talk->Quest = Quest;
				Talk->Stages = Stages;
				Talk->When = When;
				Talk->bAside = bAside;
			}
			if (Parts.Num() > Tail && Parts[Tail] == TEXT("again"))
			{
				Talk->Again.Add(ParseExchange(Value));
			}
			else
			{
				Talk->Lines = ParseExchange(Value);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("NpcDialogue: %d characters from %s"), Characters.Num(), *Path);
}

static TAutoConsoleVariable<int32> CVarOvercast(TEXT("lyud.Overcast"), 0,
	TEXT("1: the sky counts as overcast for what people say about the sun (until there is weather)"));

FName UNpcDialogueSubsystem::NpcSunState(const UWorld* World)
{
	TActorIterator<ALyudSky> Sky(const_cast<UWorld*>(World));
	if (!Sky)
	{
		return NAME_None;
	}
	const float Altitude = Sky->SunAltitude();
	if (Altitude < -4.f)
	{
		return TEXT("night");
	}
	if (CVarOvercast.GetValueOnGameThread() > 0)
	{
		return TEXT("cloudy");
	}
	return Altitude < 10.f ? TEXT("low") : TEXT("day");
}

/** Do a quest talk's conditions hold now (npc_dialogue.txt "quest.<quest>.<stages>.if.<cond>+<cond>")? */
static bool QuestTalkHolds(const UObject* WorldContext, const ULyudQuests& Quests, const TArray<FString>& When, FName SunState)
{
	for (const FString& Cond : When)
	{
		FString Kind, Arg;
		if (!Cond.Split(TEXT(":"), &Kind, &Arg))
		{
			// night, low, day, cloudy: what the sun is doing
			if (FName(*Cond) != SunState)
			{
				return false;
			}
			continue;
		}
		bool bHolds = false;
		if (Kind == TEXT("have") || Kind == TEXT("no"))
		{
			const ULyudInventory* Pocket = ULyudInventory::Get(WorldContext);
			const bool bHas = Pocket && Pocket->Count(FName(*Arg)) > 0;
			bHolds = (Kind == TEXT("have")) == bHas;
		}
		else if (Kind == TEXT("done"))
		{
			bHolds = Quests.State(FName(*Arg)) == ELyudQuestState::Done;
		}
		else if (Kind == TEXT("on"))
		{
			bHolds = Quests.State(FName(*Arg)) == ELyudQuestState::Active;
		}
		else if (Kind == TEXT("flag"))
		{
			bHolds = Quests.HasFlag(FName(*Arg));
		}
		if (!bHolds)
		{
			return false;
		}
	}
	return true;
}

/** A quest talk's own count in TalkCounts, apart from the everyday talks: "<Id>/<quest>.<first stage>[?<conditions>]". */
static FName QuestTalkCountKey(FName Id, const FNpcQuestTalk& T)
{
	FString CountName = FString::Printf(TEXT("%s/%s%s.%s"), *Id.ToString(), T.bAside ? TEXT("aside.") : TEXT(""), *T.Quest.ToString(), *T.Stages[0].ToString());
	if (T.When.Num() > 0)
	{
		CountName += TEXT("?") + FString::Join(T.When, TEXT("+"));
	}
	return FName(*CountName);
}

TArray<FNpcLine> UNpcDialogueSubsystem::StartTalk(FName Id, FName SunState)
{
	const FNpcLines* Npc = Characters.Find(Id);
	if (!Npc)
	{
		return {};
	}
	// a quest's talk (quest.<quest>.<stage> in npc_dialogue.txt) comes before everything else; which stage the quest is on
	// is read before the talk is reported, because the talk itself may finish that stage
	const FNpcQuestTalk* QuestTalk = nullptr;
	if (const ULyudQuests* Quests = ULyudQuests::Get(this))
	{
		// what is said after a quest is said once (unless it has .again lines): then it stands in nobody's way
		const FName DoneStage(TEXT("done"));
		// a bystander's aside is said through once, and never before their first conversation
		const bool bMet = TalkCounts.FindRef(Id) > 0 || Npc->First.Num() == 0;
		const auto Spent = [this, Id, DoneStage, bMet](const FNpcQuestTalk& T)
		{
			if (T.Stages.Num() == 0)
			{
				return true;
			}
			const int32 Said = TalkCounts.FindRef(QuestTalkCountKey(Id, T));
			if (T.bAside)
			{
				return !bMet || Said > T.Again.Num();
			}
			return T.Again.Num() == 0 && T.Stages.Contains(DoneStage) && Said > 0;
		};
		// the talk of a quest's live stage first, wherever it is written; asides and after-talks second
		for (int32 Pass = 0; Pass < 2 && !QuestTalk; ++Pass)
		{
			for (const FNpcQuestTalk& T : Npc->QuestTalks)
			{
				if ((Pass == 0) != (!T.bAside && !T.Stages.Contains(DoneStage)))
				{
					continue;
				}
				const FName Stage = Quests->TalkStage(T.Quest, Id.ToString());
				if (!Stage.IsNone() && T.Stages.Contains(Stage) && T.Lines.Num() > 0 && !Spent(T) && QuestTalkHolds(this, *Quests, T.When, SunState))
				{
					QuestTalk = &T;
					break;
				}
			}
		}
		// a variant whose conditions hold is said rather than the plain talk of the same quest and stage, wherever it is written
		if (QuestTalk && QuestTalk->When.Num() == 0)
		{
			const FName Stage = Quests->TalkStage(QuestTalk->Quest, Id.ToString());
			for (const FNpcQuestTalk& T : Npc->QuestTalks)
			{
				if (T.Quest == QuestTalk->Quest && T.bAside == QuestTalk->bAside && T.When.Num() > 0 && T.Stages.Contains(Stage) && T.Lines.Num() > 0 && !Spent(T) && QuestTalkHolds(this, *Quests, T.When, SunState))
				{
					QuestTalk = &T;
					break;
				}
			}
		}
	}
	// the everyday talks are counted only when one is said: a quest's talk does not use up the person's first words
	const int32 Held = TalkCounts.FindRef(Id);
	ULyudQuests::Notify(this, TEXT("talk"), Id.ToString());
	if (QuestTalk && QuestTalk->Lines.Num() > 0)
	{
		const FName CountKey = QuestTalkCountKey(Id, *QuestTalk);
		const int32 QuestHeld = TalkCounts.FindOrAdd(CountKey)++;
		if (QuestHeld > 0 && QuestTalk->Again.Num() > 0)
		{
			return QuestTalk->Again[(QuestHeld - 1) % QuestTalk->Again.Num()];
		}
		// what is said after a quest is said once; then the everyday talks go on
		if (QuestHeld == 0 || !QuestTalk->Stages.Contains(FName(TEXT("done"))))
		{
			return QuestTalk->Lines;
		}
	}
	TalkCounts.FindOrAdd(Id) = Held + 1;
	if (Held == 0 && Npc->First.Num() > 0)
	{
		return Npc->First;
	}
	if (Npc->Again.Num() == 0)
	{
		return Npc->First;
	}
	// every third later talk, if they have something to say about what the sun is doing now (quietly, not every time)
	if (Held % 3 == 2)
	{
		if (const TArray<FNpcLine>* SunLines = Npc->Sun.Find(SunState))
		{
			return *SunLines;
		}
	}
	// later talks go through the exchanges in turn (the first talk did not use one)
	const int32 Skip = Npc->First.Num() > 0 ? 1 : 0;
	return Npc->Again[(Held - Skip) % Npc->Again.Num()];
}

// ---------------------------------------------------------------------------------------------------
// What the overlay shows, filled by the director every frame

struct FNpcTalkView
{
	struct FLabel
	{
		FVector2D Pixel;   // viewport pixels
		FText Name;
		FText Role;
		bool bTarget = false;
		float Alpha = 1.f;
	};

	FVector2D ViewportSize = FVector2D(1, 1);
	TArray<FLabel> Labels;

	bool bTalking = false;
	FText Speaker;
	bool bHeroSpeaks = false;
	FText Text;
	FText Hint;
};

namespace
{
	const FLinearColor Warm(1.f, 0.78f, 0.45f);
	const FLinearColor Paper(0.96f, 0.94f, 0.9f);
	const FLinearColor Shade(0.f, 0.f, 0.f, 0.62f);

	const FSlateBrush* Flat()
	{
		static FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	/** Names over the heads: drawn where the director projected them. */
	class SNpcLabels : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SNpcLabels) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, TSharedRef<FNpcTalkView> InView) { View = InView; }

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
			int32 Layer, const FWidgetStyle&, bool) const override
		{
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", 15);
			const FSlateFontInfo RoleFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
			const FSlateFontInfo HintFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
			const FText HintText = LyudText::Get(TEXT("ui.talk.prompt"));
			const FVector2D Scale = Geometry.GetLocalSize() / View->ViewportSize;

			for (const FNpcTalkView::FLabel& L : View->Labels)
			{
				TArray<TTuple<FText, FSlateFontInfo, FLinearColor>> Rows;
				Rows.Emplace(L.Name, NameFont, Warm);
				if (!L.Role.IsEmpty())
				{
					Rows.Emplace(L.Role, RoleFont, Paper * FLinearColor(1, 1, 1, 0.8f));
				}
				if (L.bTarget)
				{
					Rows.Emplace(HintText, HintFont, Paper);
				}
				FVector2D Box(0, 0);
				TArray<FVector2D> Sizes;
				for (const auto& R : Rows)
				{
					Sizes.Add(Measure->Measure(R.Get<0>(), R.Get<1>()));
					Box.X = FMath::Max(Box.X, Sizes.Last().X);
					Box.Y += Sizes.Last().Y;
				}
				const FVector2D Pad(10, 5);
				const FVector2D Anchor = L.Pixel * Scale;
				// the label sits above the head point
				FVector2D TopLeft(Anchor.X - Box.X * 0.5f - Pad.X, Anchor.Y - Box.Y - Pad.Y * 2);
				FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Box + Pad * 2, FSlateLayoutTransform(TopLeft)),
					Flat(), ESlateDrawEffect::None, Shade * FLinearColor(1, 1, 1, L.Alpha * (L.bTarget ? 1.f : 0.7f)));
				float Y = TopLeft.Y + Pad.Y;
				for (int32 i = 0; i < Rows.Num(); ++i)
				{
					const FVector2D At(Anchor.X - Sizes[i].X * 0.5f, Y);
					FSlateDrawElement::MakeText(Out, Layer + 1, Geometry.ToPaintGeometry(Sizes[i], FSlateLayoutTransform(At)),
						Rows[i].Get<0>(), Rows[i].Get<1>(), ESlateDrawEffect::None, Rows[i].Get<2>() * FLinearColor(1, 1, 1, L.Alpha));
					Y += Sizes[i].Y;
				}
			}
			return Layer + 1;
		}

	private:
		TSharedPtr<FNpcTalkView> View;
	};

	TSharedRef<SWidget> MakeOverlay(TSharedRef<FNpcTalkView> View)
	{
		return SNew(SOverlay).Visibility(EVisibility::HitTestInvisible)
			+ SOverlay::Slot()
			[
				SNew(SNpcLabels, View)
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 60)
			[
				SNew(SBox).WidthOverride(780)
				.Visibility_Lambda([View] { return View->bTalking ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SBorder).BorderImage(Flat()).BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.025f, 0.78f))
					.Padding(FMargin(22, 14))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
						[
							SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
							.Text_Lambda([View] { return View->Speaker; })
							.ColorAndOpacity_Lambda([View] { return FSlateColor(View->bHeroSpeaks ? Paper * 0.75f : Warm); })
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 17)).AutoWrapText(true)
							.ColorAndOpacity(Paper)
							.Text_Lambda([View] { return View->Text; })
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 8, 0, 0)
						[
							SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
							.ColorAndOpacity(Paper * FLinearColor(1, 1, 1, 0.55f))
							.Text_Lambda([View] { return View->Hint; })
						]
					]
				]
			];
	}
}

// ---------------------------------------------------------------------------------------------------
// The director

ANpcTalkDirector::ANpcTalkDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the camera and the animations moved
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UNpcDialogueSubsystem* ANpcTalkDirector::Dialogue() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNpcDialogueSubsystem>() : nullptr;
}

void ANpcTalkDirector::BeginPlay()
{
	Super::BeginPlay();
	if (UNpcDialogueSubsystem* D = Dialogue())
	{
		D->Reload();
	}
	FindPeople();

	View = MakeShared<FNpcTalkView>();
	if (GEngine && GEngine->GameViewport)
	{
		Overlay = MakeOverlay(View.ToSharedRef());
		GEngine->GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 5);
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		EnableInput(PC);
		if (InputComponent)
		{
			// several actors listen for E (doors, the hero's seat); none may swallow it
			InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ANpcTalkDirector::Interact).bConsumeInput = false;
		}
	}
}

void ANpcTalkDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Overlay.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::EndPlay(Reason);
}

void ANpcTalkDirector::FindPeople()
{
	const UNpcDialogueSubsystem* D = Dialogue();
	if (!D)
	{
		return;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("NoTalk")))   // background people (the crowd before the invisible wall)
		{
			continue;
		}
		USkeletalMeshComponent* Mesh = nullptr;
		if (const ABoothOwner* Owner = Cast<ABoothOwner>(*It))
		{
			Mesh = Owner->Body;
		}
		else
		{
			Mesh = It->FindComponentByClass<USkeletalMeshComponent>();
		}
		const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		if (!Asset)
		{
			continue;
		}
		// SK_Streamer -> Streamer
		FString Id = Asset->GetName();
		if (!Id.RemoveFromStart(TEXT("SK_")) || !D->Find(FName(Id)))
		{
			continue;
		}
		People.Add({FName(Id), *It, Mesh});
	}
	UE_LOG(LogTemp, Log, TEXT("NpcDialogue: %d people to talk to"), People.Num());
}

void ANpcTalkDirector::AddPerson(FName Id, AActor* Actor, USkeletalMeshComponent* Mesh)
{
	const UNpcDialogueSubsystem* D = Dialogue();
	if (!Actor || !Mesh || !D || !D->Find(Id))
	{
		return;
	}
	// bodies of an earlier run of the people's life are gone: drop them, the list would only grow; the indices of the
	// one being talked to and the one targeted move with it
	if (TalkingIndex == INDEX_NONE)
	{
		const int32 Was = People.Num();
		People.RemoveAll([](const FNpc& N) { return !N.Actor.IsValid(); });
		if (People.Num() != Was)
		{
			TargetIndex = INDEX_NONE;
		}
	}
	for (const FNpc& Npc : People)
	{
		if (Npc.Actor.Get() == Actor)
		{
			return;
		}
	}
	People.Add({ Id, Actor, Mesh });
}

/** Not there now: the people's life has them elsewhere (the mesh or the actor is hidden). */
bool ANpcTalkDirector::IsThere(const FNpc& Npc) const
{
	return Npc.Actor.IsValid() && Npc.Mesh.IsValid() && Npc.Mesh->IsVisible() && !Npc.Actor->IsHidden();
}

FVector ANpcTalkDirector::HeadOf(const FNpc& Npc) const
{
	const USkeletalMeshComponent* Mesh = Npc.Mesh.Get();
	if (!Mesh)
	{
		return FVector::ZeroVector;
	}
	if (Mesh->GetBoneIndex(TEXT("head")) != INDEX_NONE)
	{
		return Mesh->GetBoneLocation(TEXT("head")) + FVector(0, 0, 26.f);
	}
	return Mesh->Bounds.Origin + FVector(0, 0, Mesh->Bounds.BoxExtent.Z + 8.f);
}

bool ANpcTalkDirector::InSight(const FVector& From, const FVector& To, const AActor* Npc) const
{
	// looks through glass (the owner's booth window): skips translucent surfaces
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NpcSight), false);
	Params.AddIgnoredActor(Npc);
	Params.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
	FVector Start = From;
	for (int32 Hop = 0; Hop < 4; ++Hop)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, To, ECC_Visibility, Params))
		{
			return true;
		}
		const UPrimitiveComponent* Comp = Hit.GetComponent();
		const UMaterialInterface* Mat = Comp ? Comp->GetMaterial(FMath::Max<int32>(Hit.ElementIndex, 0)) : nullptr;
		if (!Mat || !IsTranslucentBlendMode(Mat->GetBlendMode()))
		{
			return false;
		}
		Params.AddIgnoredComponent(Comp);
		Start = Hit.ImpactPoint;
	}
	return false;
}

bool ANpcTalkDirector::CanTalkTo(const FNpc& Npc, const FVector& CamLoc, const FVector& CamDir) const
{
	if (!IsThere(Npc))
	{
		return false;
	}
	const FVector To = HeadOf(Npc) - CamLoc;
	if (FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(To.GetSafeNormal(), CamDir))) > TalkAngle)
	{
		return false;
	}
	// the owner talks through his window once he has come to it
	if (const ABoothOwner* Owner = Cast<ABoothOwner>(Npc.Actor.Get()))
	{
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
		return Pawn && Owner->CallZone->Bounds.GetBox().IsInside(Pawn->GetActorLocation()) && Owner->IsAtWindow();
	}
	return To.Size() <= TalkRange;
}

bool ANpcTalkDirector::PlayerBusy() const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pawn || Pawn->IsHidden())   // hidden while the sit-down cutscene and ATON OS have him
	{
		return true;
	}
	// E at the hero's chair means "sit down"
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - SeatsAt > 2.0 || Now < SeatsAt)
	{
		SeatsAt = Now;
		Seats.Reset();
		for (TActorIterator<AHeroSeat> It(GetWorld()); It; ++It)
		{
			Seats.Add(*It);
		}
	}
	for (const TWeakObjectPtr<AHeroSeat>& Seat : Seats)
	{
		if (Seat.IsValid() && Seat->IsPlayerInRange())
		{
			return true;
		}
	}
	return false;
}

void ANpcTalkDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	View->Labels.Reset();
	TargetIndex = INDEX_NONE;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const UNpcDialogueSubsystem* D = Dialogue();
	if (!PC || !PC->PlayerCameraManager || !D || PlayerBusy())
	{
		Close();
		return;
	}
	int32 SizeX = 1, SizeY = 1;
	PC->GetViewportSize(SizeX, SizeY);
	View->ViewportSize = FVector2D(FMath::Max(SizeX, 1), FMath::Max(SizeY, 1));
	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamDir = PC->PlayerCameraManager->GetCameraRotation().Vector();

	// the one in front of you, closest to the middle of the screen
	float BestDot = -1.f;
	for (int32 i = 0; i < People.Num(); ++i)
	{
		if (People[i].Actor.IsValid() && CanTalkTo(People[i], CamLoc, CamDir) && InSight(CamLoc, HeadOf(People[i]), People[i].Actor.Get()))
		{
			const float Dot = FVector::DotProduct((HeadOf(People[i]) - CamLoc).GetSafeNormal(), CamDir);
			if (Dot > BestDot)
			{
				BestDot = Dot;
				TargetIndex = i;
			}
		}
	}

	for (int32 i = 0; i < People.Num(); ++i)
	{
		const FNpc& Npc = People[i];
		if (!IsThere(Npc))
		{
			continue;
		}
		const FVector Head = HeadOf(Npc);
		const float Dist = FVector::Dist(Head, CamLoc);
		const bool bTalking = i == TalkingIndex;
		if ((Dist > LabelRange && !bTalking) || FVector::DotProduct(Head - CamLoc, CamDir) <= 0.f)
		{
			continue;
		}
		if (!bTalking && i != TargetIndex && !InSight(CamLoc, Head, Npc.Actor.Get()))
		{
			continue;
		}
		FVector2D Pixel;
		if (!PC->ProjectWorldLocationToScreen(Head, Pixel, true))
		{
			continue;
		}
		const FNpcLines* Info = D->Find(Npc.Id);
		FNpcTalkView::FLabel& L = View->Labels.AddDefaulted_GetRef();
		L.Pixel = Pixel;
		L.Name = LyudText::Get(Info->NameKey);
		L.Role = LyudText::Get(Info->RoleKey);
		L.bTarget = i == TargetIndex && TalkingIndex == INDEX_NONE;
		L.Alpha = FMath::Clamp((LabelRange - Dist) / 120.f, 0.f, 1.f);
		if (bTalking || L.bTarget)
		{
			L.Alpha = 1.f;
		}
	}

	// walked away mid-sentence
	if (TalkingIndex != INDEX_NONE)
	{
		UpdateLineText();
		const FNpc& Npc = People[TalkingIndex];
		const bool bOwner = Cast<ABoothOwner>(Npc.Actor.Get()) != nullptr;
		const APawn* Pawn = PC->GetPawn();
		const bool bGone = !IsThere(Npc) || (bOwner
			? !(Pawn && Cast<ABoothOwner>(Npc.Actor.Get())->CallZone->Bounds.GetBox().ExpandBy(40.f).IsInside(Pawn->GetActorLocation()))
			: FVector::Dist(HeadOf(Npc), CamLoc) > TalkRange + 120.f);
		if (bGone)
		{
			Close();
		}
	}
}

void ANpcTalkDirector::LogPeople() const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UE_LOG(LogTemp, Log, TEXT("NpcDialogue: player %s, busy %d, target %d"), Pawn ? *Pawn->GetActorLocation().ToString() : TEXT("-"), PlayerBusy(), TargetIndex);
	for (const FNpc& Npc : People)
	{
		const ABoothOwner* Owner = Cast<ABoothOwner>(Npc.Actor.Get());
		UE_LOG(LogTemp, Log, TEXT("NpcDialogue:   %s head %s%s"), *Npc.Id.ToString(), *HeadOf(Npc).ToString(),
			Owner ? *FString::Printf(TEXT(" zone %s in %d at window %d"), *Owner->CallZone->GetComponentLocation().ToString(),
				Pawn && Owner->CallZone->IsOverlappingActor(Pawn), Owner->IsAtWindow()) : TEXT(""));
	}
}

void ANpcTalkDirector::Interact()
{
	if (TalkingIndex != INDEX_NONE)
	{
		if (++LineIndex >= Lines.Num())
		{
			Close();
		}
		else
		{
			ShowLine();
		}
		return;
	}
	if (TargetIndex == INDEX_NONE || PlayerBusy())
	{
		return;
	}
	if (UNpcDialogueSubsystem* D = Dialogue())
	{
		const FName SunState = UNpcDialogueSubsystem::NpcSunState(GetWorld());
		Lines = D->StartTalk(People[TargetIndex].Id, SunState);
		// the later lines' voices load in the background while the first is said
		for (int32 WI = 1; WI < Lines.Num(); ++WI)
		{
			LyudText::WarmVoice(this, Lines[WI].Key);
		}
		if (Lines.Num() > 0)
		{
			TalkingIndex = TargetIndex;
			LineIndex = 0;
			UE_LOG(LogTemp, Log, TEXT("NpcDialogue: talking to %s (%d lines, sun %s)"), *People[TalkingIndex].Id.ToString(), Lines.Num(), *SunState.ToString());
			ShowLine();
		}
	}
}

void ANpcTalkDirector::UpdateLineText()
{
	// every frame, so a language switch shows at once
	const FNpcLines* Info = Dialogue()->Find(People[TalkingIndex].Id);
	const FNpcLine& Line = Lines[LineIndex];
	View->bHeroSpeaks = Line.bHero;
	View->Speaker = LyudText::Get(Line.bHero ? FString(TEXT("ui.talk.you")) : Info->NameKey);
	View->Text = LyudText::Get(Line.Key);
	View->Hint = LyudText::Get(LineIndex + 1 < Lines.Num() ? TEXT("ui.talk.next") : TEXT("ui.talk.end"));
}

void ANpcTalkDirector::ShowLine()
{
	View->bTalking = true;
	UpdateLineText();
	StopVoice();
	UE_LOG(LogTemp, Log, TEXT("NpcDialogue: line %d/%d %s"), LineIndex + 1, Lines.Num(), *Lines[LineIndex].Key);
	if (USoundBase* Sound = LyudText::LoadVoice(Lines[LineIndex].Key))
	{
		Voice = UGameplayStatics::SpawnSound2D(this, Sound);
		// the person's lips move with the line (tools/lipsync.py curves, People/LyudBodyAnim.h)
		if (!Lines[LineIndex].bHero && People.IsValidIndex(TalkingIndex))
		{
			ULyudBodyAnimInstance::SpeakLine(People[TalkingIndex].Mesh.Get(), Lines[LineIndex].Key);
		}
	}
}

void ANpcTalkDirector::StopVoice()
{
	if (People.IsValidIndex(TalkingIndex))
	{
		ULyudBodyAnimInstance::StopLine(People[TalkingIndex].Mesh.Get());
	}
	if (Voice)
	{
		Voice->Stop();
		Voice = nullptr;
	}
}

void ANpcTalkDirector::Close()
{
	StopVoice();
	TalkingIndex = INDEX_NONE;
	Lines.Reset();
	if (View.IsValid())
	{
		View->bTalking = false;
	}
}

// ---------------------------------------------------------------------------------------------------

// "Npc.Talk" in the console presses E for the dialogue (tests without a keyboard)
static FAutoConsoleCommandWithWorld GNpcTalkCommand(TEXT("Npc.Talk"), TEXT("Talk to the person in front of you / next line"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		for (TActorIterator<ANpcTalkDirector> It(World); It; ++It)
		{
			It->Interact();
		}
	}));

static FAutoConsoleCommandWithWorld GNpcWhereCommand(TEXT("Npc.Where"), TEXT("Logs where the people are and who you could talk to"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		for (TActorIterator<ANpcTalkDirector> It(World); It; ++It)
		{
			It->LogPeople();
		}
	}));

void UNpcTalkSpawner::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	// every level start is a new game or a load (which restores the counts on the next tick)
	if (UNpcDialogueSubsystem* Dialogue = World.GetGameInstance() ? World.GetGameInstance()->GetSubsystem<UNpcDialogueSubsystem>() : nullptr)
	{
		Dialogue->ResetTalks();
	}
	if (!TActorIterator<ANpcTalkDirector>(&World))
	{
		World.SpawnActor<ANpcTalkDirector>();
	}
}
