#include "Birds/BirdBook.h"

#include "BirdTame.h"
#include "Birds/BirdLife.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "Styling/SlateBrush.h"
#include "UI/LyudQuests.h"
#include "UObject/StrongObjectPtr.h"

ULyudBirdBook* ULyudBirdBook::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudBirdBook>() : nullptr;
}

void ULyudBirdBook::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("BirdBook"), this);
		Saves->OnAfterLoad.AddWeakLambda(this, [this](ULyudSaveGame*) { OnChanged.Broadcast(); });
	}
}

void ULyudBirdBook::Reset()
{
	Seen.Reset();
	Friends.Reset();
	OnChanged.Broadcast();
}

FDateTime ULyudBirdBook::Now(const UObject* WorldContext) const
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World && GetGameInstance())
	{
		World = GetGameInstance()->GetWorld();
	}
	return UGameCalendar::NowIn(World);
}

FName ULyudBirdBook::PlaceNear(const FVector& Where) const
{
	// the nearest place the hero has found (within 400 m): "by the club", "on the beach"
	const ULyudQuests* Q = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULyudQuests>() : nullptr;
	const FVector2D At = ULyudQuests::Frame().FromWorld(Where);
	FName Best;
	double BestD = 400.0 * 400.0;
	for (const FLyudMapPlace& P : ULyudQuests::Places())
	{
		const double D = FVector2D::DistSquared(P.At, At);
		if (Q && (Q->IsFound(P.Id) || P.bKnown) && P.Kind != TEXT("land") && D < BestD)
		{
			BestD = D;
			Best = P.Id;
		}
	}
	return Best;
}

bool ULyudBirdBook::NoteSeen(const UObject* WorldContext, FName Species, bool bFemale, FName How, const FVector& Where)
{
	if (Species.IsNone())
	{
		return false;
	}
	const uint8 Sex = bFemale ? 2 : 1;
	for (FLyudBirdSeen& S : Seen)
	{
		if (S.Species == Species)
		{
			if (!(S.Sexes & Sex))
			{
				S.Sexes |= Sex;
				OnChanged.Broadcast();
			}
			return false;
		}
	}
	FLyudBirdSeen& S = Seen.AddDefaulted_GetRef();
	S.Species = Species;
	S.When = Now(WorldContext);
	S.Place = PlaceNear(Where);
	S.How = How;
	S.Sexes = Sex;
	UE_LOG(LogTemp, Log, TEXT("BirdBook: met %s (%s)"), *Species.ToString(), *How.ToString());
	const FString NameKey = FString(TEXT("@bird.")) + Snake(Species.ToString()) + TEXT(".name");
	if (ULyudQuests* Q = ULyudQuests::Get(WorldContext))
	{
		Q->AddNote(How == TEXT("binoculars") ? TEXT("journal.bird.seen_binoculars") : TEXT("journal.bird.seen"), { NameKey });
	}
	ULyudQuests::Notify(WorldContext, TEXT("bird_seen"), Species.ToString());
	OnChanged.Broadcast();
	return true;
}

bool ULyudBirdBook::NoteTamed(const UObject* WorldContext, int32 BirdId, FName Species, bool bFemale, const FVector& Where)
{
	if (BirdId < 0 || Species.IsNone())
	{
		return false;
	}
	NoteSeen(WorldContext, Species, bFemale, TEXT("hand"), Where);
	const FDateTime T = Now(WorldContext);
	for (FLyudBirdFriend& F : Friends)
	{
		if (F.BirdId == BirdId)
		{
			F.Visits++;
			F.LastVisit = T;
			OnChanged.Broadcast();
			return false;
		}
	}
	FLyudBirdFriend& F = Friends.AddDefaulted_GetRef();
	F.BirdId = BirdId;
	F.Species = Species;
	F.bFemale = bFemale;
	F.Tamed = T;
	F.LastVisit = T;
	F.Visits = 1;
	F.Place = PlaceNear(Where);
	UE_LOG(LogTemp, Log, TEXT("BirdBook: bird %d (%s %s) is a friend"), BirdId, *Species.ToString(), bFemale ? TEXT("female") : TEXT("male"));
	const FString NameKey = FString(TEXT("@bird.")) + Snake(Species.ToString()) + TEXT(".name");
	if (ULyudQuests* Q = ULyudQuests::Get(WorldContext))
	{
		Q->AddNote(bFemale ? TEXT("journal.bird.tamed_f") : TEXT("journal.bird.tamed_m"), { NameKey });
	}
	ULyudQuests::Notify(WorldContext, TEXT("bird_tamed"), Species.ToString());
	OnChanged.Broadcast();
	return true;
}

const FLyudBirdSeen* ULyudBirdBook::FindSeen(FName Species) const
{
	return Seen.FindByPredicate([Species](const FLyudBirdSeen& S) { return S.Species == Species; });
}

const FLyudBirdFriend* ULyudBirdBook::FindFriend(int32 BirdId) const
{
	return Friends.FindByPredicate([BirdId](const FLyudBirdFriend& F) { return F.BirdId == BirdId; });
}

TArray<const FLyudBirdFriend*> ULyudBirdBook::FriendsOf(FName Species) const
{
	TArray<const FLyudBirdFriend*> Out;
	for (const FLyudBirdFriend& F : Friends)
	{
		if (F.Species == Species)
		{
			Out.Add(&F);
		}
	}
	return Out;
}

FString ULyudBirdBook::Snake(const FString& Camel)
{
	FString Out;
	for (int32 I = 0; I < Camel.Len(); ++I)
	{
		const TCHAR C = Camel[I];
		if (FChar::IsUpper(C) && I > 0)
		{
			Out.AppendChar(TEXT('_'));
		}
		Out.AppendChar(FChar::ToLower(C));
	}
	return Out;
}

FText ULyudBirdBook::SpeciesName(FName Species)
{
	return LyudText::Get(FString::Printf(TEXT("bird.%s.name"), *Snake(Species.ToString())));
}

const FSlateBrush* ULyudBirdBook::Drawing(FName Species, bool bFemale)
{
	// the female's own drawing where the species has one, else the species' (the male's)
	struct FDrawing { TSharedPtr<FSlateBrush> Brush; TStrongObjectPtr<UTexture2D> Tex; };
	static TMap<FString, FDrawing> Cache;
	const FString Base = Species.IsNone() ? FString(TEXT("Unknown")) : Species.ToString();
	for (const FString& Name : { bFemale ? Base + TEXT("_f") : Base, Base })
	{
		if (FDrawing* D = Cache.Find(Name))
		{
			if (D->Brush)
			{
				return D->Brush.Get();
			}
			continue;
		}
		FDrawing& D = Cache.Add(Name);
		const FString Asset = TEXT("T_Bird_") + Name;
		if (UTexture2D* T = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/UI/Diary/Birds/%s.%s"), *Asset, *Asset), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			D.Tex.Reset(T);
			D.Brush = MakeShared<FSlateBrush>();
			D.Brush->SetResourceObject(T);
			D.Brush->ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY());
			return D.Brush.Get();
		}
	}
	return nullptr;
}

// --- dev commands ---
static FAutoConsoleCommandWithWorldAndArgs GBirdsBook(TEXT("Birds.Book"), TEXT("Birds.Book: logs the hero's bird book (BIRDBOOK lines)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		const ULyudBirdBook* B = ULyudBirdBook::Get(World);
		if (!B)
		{
			return;
		}
		for (const FLyudBirdSeen& S : B->Seen)
		{
			UE_LOG(LogTemp, Display, TEXT("BIRDBOOK seen %s %s %s by %s sexes %d"), *S.Species.ToString(), *S.When.ToString(), *S.Place.ToString(), *S.How.ToString(), S.Sexes);
		}
		for (const FLyudBirdFriend& F : B->Friends)
		{
			UE_LOG(LogTemp, Display, TEXT("BIRDBOOK friend %d %s %s %s visits %d"), F.BirdId, *F.Species.ToString(), F.bFemale ? TEXT("f") : TEXT("m"), *F.Tamed.ToString(), F.Visits);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GBirdsMeet(TEXT("Birds.Meet"), TEXT("Birds.Meet <Species> [tame [female]]: writes a species (or a bird of it nearest the hero, as a friend) into the bird book"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudBirdBook* B = ULyudBirdBook::Get(World);
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!B || !Pawn || Args.Num() < 1)
		{
			return;
		}
		const FName Species(*Args[0]);
		const bool bTame = Args.Num() > 1 && Args[1] == TEXT("tame");
		if (!bTame)
		{
			B->NoteSeen(World, Species, Args.Num() > 2 && Args[2] == TEXT("female"), TEXT("binoculars"), Pawn->GetActorLocation());
			return;
		}
		// a real bird of that species, the nearest to the hero, so it can be called
		const UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(World);
		if (!Life)
		{
			return;
		}
		for (const float R : { 5000.f, 30000.f, 400000.f })
		{
			for (const int32 Id : Life->FindBirdsNear(Pawn->GetActorLocation(), R))
			{
				FBirdLifeInfo Info;
				if (Life->GetBird(Id, Info) && Info.SpeciesId == Species && !Life->IsBirdAway(Id) && !B->FindFriend(Id))
				{
					B->NoteTamed(World, Id, Species, Info.bFemale, Info.Location);
					UE_LOG(LogTemp, Display, TEXT("BIRDBOOK tamed %d %s at %.0f m"), Id, *Species.ToString(), FVector::Dist(Info.Location, Pawn->GetActorLocation()) / 100.f);
					return;
				}
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("BIRDBOOK no %s to tame"), *Species.ToString());
	}));

static FAutoConsoleCommandWithWorldAndArgs GBirdsCall(TEXT("Birds.Call"), TEXT("Birds.Call [n]: calls the n-th friend of the bird book (0 first), as the diary's \"call\" does"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const ULyudBirdBook* B = ULyudBirdBook::Get(World);
		const int32 N = Args.Num() ? FCString::Atoi(*Args[0]) : 0;
		UBirdTameSubsystem* Tame = UBirdTameSubsystem::Get(World);
		if (B && Tame && B->Friends.IsValidIndex(N))
		{
			const bool bOk = Tame->Summon(B->Friends[N].BirdId);
			UE_LOG(LogTemp, Display, TEXT("BIRDBOOK call %d (%s): %s"), B->Friends[N].BirdId, *B->Friends[N].Species.ToString(), bOk ? TEXT("coming") : TEXT("can't"));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GBirdsForget(TEXT("Birds.Forget"), TEXT("Birds.Forget: an empty bird book"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (ULyudBirdBook* B = ULyudBirdBook::Get(World))
		{
			B->Reset();
		}
	}));
