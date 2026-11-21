#include "Animals/LyudPets.h"

#include "Animals/AnimalLife.h"
#include "AnimalSim.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "Plants/GameCalendar.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudPets, Log, All);

using namespace LyudAnimals;

namespace LyudPetsImpl
{
	/** Leashes and hedgehogs are drawn this near the camera (cm). */
	constexpr double DrawCm = 7000.0;
	/** The cable: its radius (cm), the collar over the dog's feet and ahead of its middle (cm, for a dog of size 1). */
	constexpr double LeashRadius = 0.45;
	constexpr double CollarUp = 40.0;
	constexpr double CollarAhead = 24.0;
	/** The owner's hand when their body is not drawn (cm over the feet). */
	constexpr double HandUp = 85.0;

	V3 ToSim(const FVector& V) { return V3((float)(V.X / 100.0), (float)(V.Y / 100.0), (float)(V.Z / 100.0)); }
	FVector ToUE(const V3& V) { return FVector(V.X, V.Y, V.Z) * 100.0; }

	bool ReadV3(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, V3& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!O || !O->TryGetArrayField(Field, Arr) || Arr->Num() < 3)
		{
			return false;
		}
		Out = V3((float)(*Arr)[0]->AsNumber(), (float)(*Arr)[1]->AsNumber(), (float)(*Arr)[2]->AsNumber());
		return true;
	}

	int32 DayOf(const UWorld* World)
	{
		return int32(UGameCalendar::NowIn(World).GetTicks() / ETimespan::TicksPerDay);
	}
}

ULyudPets* ULyudPets::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudPets>() : nullptr;
}

void ULyudPets::Deinitialize()
{
	Leashes.Reset();
	Hogs.Reset();
	Holder = nullptr;
	Super::Deinitialize();
}

UStaticMeshComponent* ULyudPets::NewPart(UStaticMesh* Mesh)
{
	if (!IsValid(Holder))
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Holder = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		if (!Holder)
		{
			return nullptr;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"), RF_Transient);
		Root->SetMobility(EComponentMobility::Movable);
		Holder->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Holder, NAME_None, RF_Transient);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->SetAffectDistanceFieldLighting(false);
	C->bAffectDynamicIndirectLighting = false;
	C->bVisibleInRayTracing = false;
	C->SetVisibility(false);
	C->SetupAttachment(Holder->GetRootComponent());
	C->RegisterComponent();
	return C;
}

bool ULyudPets::Load()
{
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim || !People || !People->IsRunning())
	{
		return false;
	}
	bTried = true;
	const FString Path = FPaths::ProjectContentDir() / TEXT("AnimalsData/pets.json");
	FString Text;
	TSharedPtr<FJsonObject> Doc;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Doc) || !Doc)
	{
		UE_LOG(LogLyudPets, Warning, TEXT("PETS no %s (python3 tools/town/town_pets.py)"), *Path);
		return false;
	}
	auto ReadActs = [&](const TCHAR* Field, TArray<int32>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Ids = nullptr;
		if (Doc->TryGetArrayField(Field, Ids))
		{
			for (const TSharedPtr<FJsonValue>& V : *Ids)
			{
				const int32 Act = People->FindActivity(V->AsString());
				if (Act >= 0)
				{
					Out.Add(Act);
				}
			}
		}
	};
	ReadActs(TEXT("walk"), WalkActs);
	ReadActs(TEXT("feed"), FeedActs);
	Lady = People->FindPerson(Doc->GetStringField(TEXT("lady")));
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Doc->TryGetArrayField(TEXT("dogs"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			V3 Door;
			const int32 Person = O ? People->FindPerson(O->GetStringField(TEXT("owner"))) : -1;
			if (Person < 0 || WalkActs.IsEmpty() || !LyudPetsImpl::ReadV3(O, TEXT("door"), Door))
			{
				continue;
			}
			const int Id = Sim->AddAnimal(EKind::Dog, O->GetIntegerField(TEXT("breed")), O->GetIntegerField(TEXT("coat")), Door);
			if (!Sim->AdoptDog(Id, Person, Door))
			{
				continue;
			}
			FAnimal& A = Sim->Animals[Id];
			A.bFemale = O->GetIntegerField(TEXT("female")) != 0;
			A.NameKey = TCHAR_TO_UTF8(*O->GetStringField(TEXT("name")));
			A.Friendly = FMath::Max(A.Friendly, 0.6f);
			FDog D;
			D.Animal = Id;
			D.Person = Person;
			Dogs.Add(D);
		}
	}
	if (Doc->TryGetArrayField(TEXT("cats"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			V3 Door, Sill;
			if (!LyudPetsImpl::ReadV3(O, TEXT("door"), Door) || !LyudPetsImpl::ReadV3(O, TEXT("sill"), Sill))
			{
				continue;
			}
			const int32 Person = People->FindPerson(O->GetStringField(TEXT("owner")));
			const int Id = Sim->AddAnimal(EKind::Cat, 0, O->GetIntegerField(TEXT("coat")), Door);
			if (!Sim->AdoptCat(Id, Person, Sill, (float)O->GetNumberField(TEXT("yaw")), Door))
			{
				continue;
			}
			FAnimal& A = Sim->Animals[Id];
			A.bFemale = O->GetIntegerField(TEXT("female")) != 0;
			A.NameKey = TCHAR_TO_UTF8(*O->GetStringField(TEXT("name")));
			FCat C;
			C.Animal = Id;
			C.Person = Person;
			Cats.Add(C);
		}
	}
	if (Doc->TryGetArrayField(TEXT("hedgehogs"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			V3 At;
			if (LyudPetsImpl::ReadV3(O, TEXT("pos"), At))
			{
				Sim->AddHedgehog(At, (float)O->GetNumberField(TEXT("range")));
			}
		}
	}
	// the lady fills the feeders now, not the clock (without her in the town they stay the clock's)
	Sim->bFeedByPerson = Lady >= 0 && !FeedActs.IsEmpty();
	HogMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Characters/Humans/Props/SM_Prop_hedgehog.SM_Prop_hedgehog"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	HogBall = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Characters/Humans/Props/SM_Prop_hedgehog_ball.SM_Prop_hedgehog_ball"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		LeashMat = UMaterialInstanceDynamic::Create(Base, this);
		LeashMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.035f, 0.022f, 0.015f));
	}
	UE_LOG(LogLyudPets, Display, TEXT("PETS %d dogs, %d cats, %d hedgehogs, the lady %d (walks %d, rounds %d, hedgehog meshes %d)"), Dogs.Num(), Cats.Num(),
		(int32)Sim->Hedgehogs.size(), Lady, WalkActs.Num(), FeedActs.Num(), (HogMesh ? 1 : 0) + (HogBall ? 1 : 0));
	return true;
}

void ULyudPets::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	if (!bLoaded)
	{
		// once the two lives run (a failed read is not tried again)
		if (bTried)
		{
			return;
		}
		bLoaded = Load();
		if (!bLoaded)
		{
			return;
		}
	}
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim)
	{
		return;
	}
	const APlayerController* PC = World->GetFirstPlayerController();
	const FVector Camera = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const double Now = World->GetTimeSeconds();
	TickDogs(DeltaTime, Camera);
	TickHedgehogs(Camera);
	if (Now >= NextSlow)
	{
		NextSlow = Now + 1.0;
		// (the console's look at them forces the month only: they still wait for dusk)
		Sim->Month = bHedgehogsNow ? 7 : UGameCalendar::NowIn(World).GetMonth();
		TickLady();
		if (const APawn* Pawn = PC ? PC->GetPawn() : nullptr)
		{
			TickRemarks(Pawn->GetActorLocation());
		}
	}
}

void ULyudPets::TickDogs(float DeltaTime, const FVector& Camera)
{
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim || !People)
	{
		return;
	}
	int32 Drawn = 0;
	for (FDog& D : Dogs)
	{
		int32 Act = -1;
		bool bDoing = false;
		EPersonPose Pose = EPersonPose::Hidden;
		FVector At = FVector::ZeroVector;
		const bool bOut = People->GetActivityState(D.Person, Act, bDoing, Pose, At) && WalkActs.Contains(Act) && Pose != EPersonPose::Hidden
			&& Pose != EPersonPose::Sleep;
		if (!bOut)
		{
			if (D.bOut)
			{
				D.bOut = false;
				Sim->OwnerWalk(D.Animal, false, V3(), V3(), V3());
			}
			continue;
		}
		const AActor* Body = People->GetShownBody(D.Person);
		// the person's place is their body's middle when one is drawn; the sim wants the feet (the ground under them)
		FVector Feet = At;
		FVector Hand = At + FVector(0.0, 0.0, LyudPetsImpl::HandUp);
		if (Body)
		{
			if (const USkeletalMeshComponent* Mesh = Body->FindComponentByClass<USkeletalMeshComponent>())
			{
				if (Mesh->GetBoneIndex(TEXT("hand_l")) != INDEX_NONE)
				{
					Hand = Mesh->GetSocketLocation(TEXT("hand_l"));
				}
			}
		}
		const FVector Vel = D.bOut && DeltaTime > 1e-4f ? (At - D.LastAt) / DeltaTime : FVector::ZeroVector;
		D.LastAt = At;
		D.bOut = true;
		// a jump of the clock or a teleport is not a speed
		const FVector SafeVel = Vel.SizeSquared() < FMath::Square(600.0) ? Vel : FVector::ZeroVector;
		Sim->OwnerWalk(D.Animal, true, LyudPetsImpl::ToSim(Feet), LyudPetsImpl::ToSim(SafeVel), LyudPetsImpl::ToSim(Hand));

		// the leash: one stretched cylinder from the hand to the collar, near the camera only
		const FAnimal& A = Sim->Animals[D.Animal];
		const FVector DogAt = LyudPetsImpl::ToUE(Sim->RenderPos(A));
		if (!Body || !Cylinder || FVector::DistSquared(DogAt, Camera) > FMath::Square(LyudPetsImpl::DrawCm))
		{
			continue;
		}
		if (!Leashes.IsValidIndex(Drawn))
		{
			UStaticMeshComponent* C = NewPart(Cylinder);
			if (!C)
			{
				continue;
			}
			if (LeashMat)
			{
				C->SetMaterial(0, LeashMat);
			}
			Leashes.Add(C);
		}
		const FVector Collar = DogAt + FVector(FMath::Cos(A.Yaw), FMath::Sin(A.Yaw), 0.0) * (LyudPetsImpl::CollarAhead * A.Size)
			+ FVector(0.0, 0.0, LyudPetsImpl::CollarUp * A.Size);
		const FVector Along = Collar - Hand;
		const double Len = Along.Size();
		UStaticMeshComponent* C = Leashes[Drawn++];
		if (Len < 1.0)
		{
			C->SetVisibility(false);
			continue;
		}
		// the engine's cylinder is 100 cm tall along Z with a radius of 50
		C->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(Along / Len).ToQuat(), (Hand + Collar) * 0.5,
			FVector(LyudPetsImpl::LeashRadius / 50.0, LyudPetsImpl::LeashRadius / 50.0, Len / 100.0)));
		C->SetVisibility(true);
	}
	for (int32 I = Drawn; I < Leashes.Num(); I++)
	{
		if (Leashes[I] && Leashes[I]->IsVisible())
		{
			Leashes[I]->SetVisibility(false);
		}
	}
}

void ULyudPets::TickLady()
{
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim || !People || Lady < 0 || FeedActs.IsEmpty())
	{
		return;
	}
	int32 Act = -1;
	bool bDoing = false;
	EPersonPose Pose = EPersonPose::Hidden;
	FVector At = FVector::ZeroVector;
	if (!People->GetActivityState(Lady, Act, bDoing, Pose, At) || !FeedActs.Contains(Act) || !bDoing)
	{
		return;
	}
	// at a feeder: the bowls go down (a full feeder is left alone), and she calls the cats
	std::vector<int> Near;
	Sim->FeedersNear(LyudPetsImpl::ToSim(At), 4.f, Near);
	for (const int Place : Near)
	{
		if (Sim->Places[Place].FoodLeft < Sim->Places[Place].Capacity - 0.5f && LyudAnimals::FWorld::FeedingTime(Sim->Hour))
		{
			Sim->PutFood(Place);
			const double Now = GetWorld()->GetTimeSeconds();
			if (Now >= NextCall)
			{
				NextCall = Now + 20.0;
				if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
				{
					if (AActor* Body = People->GetShownBody(Lady))
					{
						Talk->SayEvent(Body, TEXT("cat_call"), Lady);
					}
				}
			}
		}
	}
}

void ULyudPets::TickHedgehogs(const FVector& Camera)
{
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	const FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim || !HogMesh)
	{
		return;
	}
	const int32 N = (int32)Sim->Hedgehogs.size();
	while (Hogs.Num() < N)
	{
		Hogs.Add(NewPart(HogMesh));
	}
	for (int32 I = 0; I < N; I++)
	{
		UStaticMeshComponent* C = Hogs[I];
		if (!C)
		{
			continue;
		}
		const FHedgehog& H = Sim->Hedgehogs[I];
		const FVector At = LyudPetsImpl::ToUE(H.Pos);
		const bool bShow = H.State != EHog::Away && FVector::DistSquared(At, Camera) < FMath::Square(LyudPetsImpl::DrawCm);
		if (bShow != C->IsVisible())
		{
			C->SetVisibility(bShow);
		}
		if (!bShow)
		{
			continue;
		}
		UStaticMesh* Want = H.Curl > 0.5f && HogBall ? HogBall.Get() : HogMesh.Get();
		if (C->GetStaticMesh() != Want)
		{
			C->SetStaticMesh(Want);
		}
		// the shuffle: a little waddle from side to side with the steps
		const float Waddle = H.State == EHog::Shuffle ? FMath::Sin(H.Phase) * 5.f : 0.f;
		C->SetWorldLocationAndRotation(At, FRotator(0.f, FMath::RadiansToDegrees(H.Yaw), Waddle));
	}
}

void ULyudPets::TickRemarks(const FVector& HeroAt)
{
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this);
	const FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim || !People || !Talk)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// an owner talks to the dog now and then (heard when the hero is near)
	for (FDog& D : Dogs)
	{
		if (!D.bOut || Now < D.NextTalk || FVector::DistSquared(D.LastAt, HeroAt) > FMath::Square(1400.0))
		{
			continue;
		}
		const FLeash* L = Sim->LeashOf(D.Animal);
		if (L && (L->Sniff > 0.f || L->Greet > 0.f))
		{
			if (AActor* Body = People->GetShownBody(D.Person))
			{
				D.NextTalk = Now + (Talk->SayEvent(Body, TEXT("pet_talk"), D.Person) ? 45.0 : 8.0);
			}
		}
	}
	// under a window with the flat's cat on the sill, its owner close by: a word about it
	if (Now >= NextSill)
	{
		NextSill = Now + 6.0;
		for (const FCat& C : Cats)
		{
			const FAnimal& A = Sim->Animals[C.Animal];
			if (A.Pet != EPet::Sill || C.Person < 0 || FVector::DistSquared2D(LyudPetsImpl::ToUE(A.Pos), HeroAt) > FMath::Square(600.0))
			{
				continue;
			}
			AActor* Body = People->GetShownBody(C.Person);
			if (Body && FVector::DistSquared(Body->GetActorLocation(), HeroAt) < FMath::Square(800.0) && Talk->SayEvent(Body, TEXT("cat_sill"), C.Person))
			{
				NextSill = Now + 180.0;
				break;
			}
		}
	}
	// a hedgehog near the hero and somebody of the town near both
	if (Now >= NextHog)
	{
		NextHog = Now + 8.0;
		for (const FHedgehog& H : Sim->Hedgehogs)
		{
			if (H.State == EHog::Away || FVector::DistSquared(LyudPetsImpl::ToUE(H.Pos), HeroAt) > FMath::Square(900.0))
			{
				continue;
			}
			AActor* Speaker = nullptr;
			const int32 Person = Talk->NearestSpeaker(Speaker);
			if (Speaker && FVector::DistSquared(Speaker->GetActorLocation(), HeroAt) < FMath::Square(700.0) && Talk->SayEvent(Speaker, TEXT("hedgehog"), Person))
			{
				NextHog = Now + 240.0;
			}
			break;
		}
	}
}

int32 ULyudPets::OwnerOf(int32 AnimalId) const
{
	for (const FDog& D : Dogs)
	{
		if (D.Animal == AnimalId)
		{
			return D.Person;
		}
	}
	for (const FCat& C : Cats)
	{
		if (C.Animal == AnimalId)
		{
			return C.Person;
		}
	}
	return -1;
}

bool ULyudPets::NeedsAsking(int32 AnimalId) const
{
	const UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	const FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim || AnimalId < 0 || AnimalId >= (int32)Sim->Animals.size())
	{
		return false;
	}
	const FAnimal& A = Sim->Animals[AnimalId];
	return A.Pet == EPet::Leash && A.Sniffed <= 0.f;
}

bool ULyudPets::AskToStroke(int32 AnimalId)
{
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	FWorld* Sim = Life ? Life->GetSim() : nullptr;
	FDog* Dog = Dogs.FindByPredicate([AnimalId](const FDog& D) { return D.Animal == AnimalId; });
	if (!Sim || !People || !Dog || !NeedsAsking(AnimalId))
	{
		return false;
	}
	// most owners say yes; one in four not today (the same answer all day), and nobody while the dogs are nose to nose
	const int32 Day = LyudPetsImpl::DayOf(GetWorld());
	const FLeash* L = Sim->LeashOf(AnimalId);
	const bool bBusy = L && L->Greet > 0.f;
	const bool bNo = bBusy || Dog->AskedDay == Day || (GetTypeHash(FIntPoint(Dog->Person, Day)) % 4u) == 0u;
	if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
	{
		if (AActor* Body = People->GetShownBody(Dog->Person))
		{
			Talk->SayEvent(Body, bNo ? TEXT("pet_stroke_no") : TEXT("pet_stroke_yes"), Dog->Person);
		}
	}
	if (bNo)
	{
		Dog->AskedDay = bBusy ? Dog->AskedDay : Day;
		return false;
	}
	Sim->Animals[AnimalId].Trust = FMath::Max(Sim->Animals[AnimalId].Trust, 0.4f);
	return Sim->AllowStroke(AnimalId);
}

bool ULyudPets::SendOut(int32 Index)
{
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	return People && Dogs.IsValidIndex(Index) && People->Order(Dogs[Index].Person, TEXT("walk_dog"));
}

FString ULyudPets::Report() const
{
	const UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	const FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim || !bLoaded)
	{
		return TEXT("pets: not started");
	}
	int32 Out = 0, Sill = 0, Door = 0, Hog = 0;
	for (const FDog& D : Dogs)
	{
		Out += D.bOut ? 1 : 0;
	}
	for (const FCat& C : Cats)
	{
		Sill += Sim->Animals[C.Animal].Pet == EPet::Sill ? 1 : 0;
		Door += Sim->Animals[C.Animal].Pet == EPet::Door ? 1 : 0;
	}
	for (const FHedgehog& H : Sim->Hedgehogs)
	{
		Hog += H.State != EHog::Away ? 1 : 0;
	}
	return FString::Printf(TEXT("pets: %d dogs (%d out on the leash), %d cats (%d on a sill, %d at a door), %d of %d hedgehogs about, feeders by the lady %d"),
		Dogs.Num(), Out, Cats.Num(), Sill, Door, Hog, (int32)Sim->Hedgehogs.size(), Sim->bFeedByPerson ? 1 : 0);
}

static FAutoConsoleCommandWithWorldAndArgs GPetsReport(TEXT("lyud.Pets.Report"), TEXT("Animals with people: who is out, on a sill, at a door; the hedgehogs."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (const ULyudPets* P = ULyudPets::Get(World))
		{
			UE_LOG(LogLyudPets, Display, TEXT("%s"), *P->Report());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GPetsWalk(TEXT("lyud.Pets.Walk"), TEXT("lyud.Pets.Walk [n]: the n-th dog's owner goes out with it now."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudPets* P = ULyudPets::Get(World))
		{
			const int32 N = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
			UE_LOG(LogLyudPets, Display, TEXT("PETS walk %d: %d"), N, P->SendOut(N) ? 1 : 0);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GPetsHogs(TEXT("lyud.Pets.Hedgehogs"), TEXT("The hedgehogs' month is forced to July (they still wait for dusk: lyud.Birds.Time 21)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (ULyudPets* P = ULyudPets::Get(World))
		{
			P->bHedgehogsNow = true;
		}
	}));
