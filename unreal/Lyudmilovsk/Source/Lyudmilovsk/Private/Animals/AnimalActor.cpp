#include "Animals/AnimalActor.h"

#include "Animals/AnimalAnimInstance.h"
#include "Animals/AnimalCollar.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* AllClips[] = { TEXT("Idle"), TEXT("Walk"), TEXT("Trot"), TEXT("Run"), TEXT("Stalk"), TEXT("Watch"), TEXT("Sniff"), TEXT("Eat"),
		TEXT("Sit"), TEXT("Rest"), TEXT("Sleep"), TEXT("Groom"), TEXT("Crouch"), TEXT("Pounce"), TEXT("Hiss"), TEXT("Climb"), TEXT("Perch"),
		TEXT("Rub"), TEXT("Bark"), TEXT("PlayBow"), TEXT("Play"), TEXT("Beg") };

	/** Content/AnimalsData/animal_bodies.json (written by unreal/scripts/import_animals.py): per breed its coats, eye
	 *  textures and the stride speeds of its locomotion clips. */
	struct FBreedData
	{
		TArray<FString> Coats;
		int32 Eyes = 1;
		TMap<FName, float> Speeds;
		/** A collar (blender/animals/collar.py), in the breed's frame (cm). */
		bool bCollar = false;
		FAnimalCollarSpec Collar;
	};

	const TMap<FName, FBreedData>& Breeds()
	{
		static TMap<FName, FBreedData> Table;
		static bool bLoaded = false;
		if (bLoaded)
		{
			return Table;
		}
		bLoaded = true;
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("AnimalsData/animal_bodies.json");
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Animals: no %s"), *Path);
			return Table;
		}
		for (const auto& KV : Root->Values)
		{
			const TSharedPtr<FJsonObject>* O = nullptr;
			if (!KV.Value->TryGetObject(O))
			{
				continue;
			}
			FBreedData D;
			const TArray<TSharedPtr<FJsonValue>>* Coats = nullptr;
			if ((*O)->TryGetArrayField(TEXT("coats"), Coats))
			{
				for (const TSharedPtr<FJsonValue>& C : *Coats)
				{
					D.Coats.Add(C->AsString());
				}
			}
			(*O)->TryGetNumberField(TEXT("eyes"), D.Eyes);
			const TSharedPtr<FJsonObject>* Sp = nullptr;
			if ((*O)->TryGetObjectField(TEXT("speeds_cms"), Sp))
			{
				for (const auto& S : (*Sp)->Values)
				{
					D.Speeds.Add(FName(*S.Key), (float)S.Value->AsNumber());
				}
			}
			const TSharedPtr<FJsonObject>* Co = nullptr;
			if ((*O)->TryGetObjectField(TEXT("collar"), Co))
			{
				auto Vec = [&](const TCHAR* Field, FVector& Out)
				{
					const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
					if ((*Co)->TryGetArrayField(Field, A) && A->Num() == 3)
					{
						Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
					}
				};
				FString Bone;
				(*Co)->TryGetStringField(TEXT("bone"), Bone);
				D.Collar.Bone = FName(*Bone);
				Vec(TEXT("hang"), D.Collar.Hang);
				Vec(TEXT("chest_center"), D.Collar.ChestCenter);
				Vec(TEXT("forward"), D.Collar.Forward);
				double V = 0.0;
				if ((*Co)->TryGetNumberField(TEXT("chest_radius_cm"), V)) { D.Collar.ChestRadius = (float)V; }
				if ((*Co)->TryGetNumberField(TEXT("com_cm"), V)) { D.Collar.TagLength = (float)V; }
				D.bCollar = true;
			}
			Table.Add(FName(*KV.Key), D);
		}
		return Table;
	}

	FString Root(FName Breed) { return FString::Printf(TEXT("/Game/Animals/%s"), *Breed.ToString()); }

	bool IsMovingActivity(EAnimalActivity A)
	{
		switch (A)
		{
		case EAnimalActivity::Walk: case EAnimalActivity::Trot: case EAnimalActivity::Run: case EAnimalActivity::Sniff:
		case EAnimalActivity::Approach: case EAnimalActivity::Stalk: case EAnimalActivity::Chase: case EAnimalActivity::Flee:
		case EAnimalActivity::Play: case EAnimalActivity::Follow: case EAnimalActivity::Rub:
			return true;
		default:
			return false;
		}
	}
}

static void SpawnAnimalCmd(const TArray<FString>& Args, UWorld* World)
{
	if (!World || Args.Num() < 1)
	{
		return;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	FVector Loc;
	FRotator Rot;
	PC->GetPlayerViewPoint(Loc, Rot);
	FHitResult Hit;
	const FVector Ahead = Loc + FRotator(0.f, Rot.Yaw, 0.f).Vector() * 250.f;
	FVector At = Ahead;
	if (World->LineTraceSingleByChannel(Hit, Ahead + FVector(0, 0, 300), Ahead - FVector(0, 0, 3000), ECC_Visibility))
	{
		At = Hit.ImpactPoint;
	}
	float Yaw = Rot.Yaw + 180.f;
	if (Args.Num() >= 6)
	{
		At = FVector(FCString::Atof(*Args[3]), FCString::Atof(*Args[4]), FCString::Atof(*Args[5]));
		Yaw = Args.Num() >= 7 ? FCString::Atof(*Args[6]) : Yaw;
	}
	const FTransform T(FRotator(0.f, Yaw, 0.f), At);
	AAnimalActor* A = World->SpawnActorDeferred<AAnimalActor>(AAnimalActor::StaticClass(), T);
	if (!A)
	{
		return;
	}
	A->Breed = FName(*Args[0]);
	A->Coat = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
	if (Args.Num() > 2)
	{
		if (const UEnum* E = StaticEnum<EAnimalActivity>())
		{
			const int64 V = E->GetValueByNameString(Args[2]);
			if (V != INDEX_NONE)
			{
				A->InitialActivity = (EAnimalActivity)V;
			}
		}
	}
	A->FinishSpawning(T);
}

static FAutoConsoleCommandWithWorldAndArgs GAnimalSpawnCmd(TEXT("Animal.Spawn"),
	TEXT("Animal.Spawn <Cat|Laika|Shepherd|Mutt> [coat] [activity] [x y z [yaw]]: an animal body 2.5 m in front of the camera, or there (testing)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SpawnAnimalCmd));

AAnimalActor::AAnimalActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetAnimInstanceClass(UAnimalAnimInstance::StaticClass());
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Body->bCastDynamicShadow = true;
	Body->SetCanEverAffectNavigation(false);
	// Scene 01's lights: the club's on channel 0, the outdoor sun and moon on channel 1 only
	Body->LightingChannels.bChannel0 = true;
	Body->LightingChannels.bChannel1 = true;
}

void AAnimalActor::BeginPlay()
{
	Super::BeginPlay();
	Rng.Initialize(Seed ? Seed : GetUniqueID());
	WagPhase = Rng.FRand() * 6.28f;
	if (SetBreed(Breed, Coat))
	{
		SetActivity(InitialActivity, 0.f);
	}
}

bool AAnimalActor::SetBreed(FName InBreed, int32 InCoat)
{
	const FString R = Root(InBreed);
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s/SK_%s.SK_%s"), *R, *InBreed.ToString(), *InBreed.ToString()));
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Animal: no mesh for %s"), *InBreed.ToString());
		return false;
	}
	Breed = InBreed;
	Coat = InCoat;
	Body->SetSkinnedAssetAndUpdate(Mesh);
	Body->SetAnimInstanceClass(UAnimalAnimInstance::StaticClass());
	Body->SetRelativeScale3D(FVector(Size));
	Clips.Reset();
	for (const TCHAR* C : AllClips)
	{
		if (UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s/Anims/A_%s_%s.A_%s_%s"), *R, *InBreed.ToString(), C, *InBreed.ToString(), C)))
		{
			Clips.Add(C, Seq);
		}
	}
	ClipSpeeds.Reset();
	if (const FBreedData* D = Breeds().Find(InBreed))
	{
		ClipSpeeds = D->Speeds;
	}
	ApplyCoat();
	UpdateCollar();
	PlayingClip = NAME_None;
	UpdateClip(0.f);
	return true;
}

void AAnimalActor::UpdateCollar()
{
	const FBreedData* D = Breeds().Find(Breed);
	if (!D || !D->bCollar)
	{
		if (Collar)
		{
			Collar->DestroyComponent();
			Collar = nullptr;
		}
		return;
	}
	if (!Collar)
	{
		Collar = NewObject<UAnimalCollarComponent>(this, TEXT("Collar"));
		Collar->RegisterComponent();
	}
	Collar->Setup(Body, D->Collar, Root(Breed) / FString::Printf(TEXT("SM_%s_Collar.SM_%s_Collar"), *Breed.ToString(), *Breed.ToString()),
		Root(Breed) / FString::Printf(TEXT("SM_%s_Tag.SM_%s_Tag"), *Breed.ToString(), *Breed.ToString()));
}

void AAnimalActor::ApplyCoat()
{
	const FBreedData* D = Breeds().Find(Breed);
	if (!D || D->Coats.Num() == 0)
	{
		return;
	}
	const FString& CoatName = D->Coats[FMath::Clamp(Coat, 0, D->Coats.Num() - 1)];
	const FString R = Root(Breed);
	const FString B = Breed.ToString();
	UMaterialInterface* BodyM = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/MI_%s_%s_Body.MI_%s_%s_Body"), *R, *B, *CoatName, *B, *CoatName));
	UMaterialInterface* FurM = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/MI_%s_%s_Fur.MI_%s_%s_Fur"), *R, *B, *CoatName, *B, *CoatName));
	const int32 EyeI = FMath::Clamp(Coat, 0, FMath::Max(D->Eyes, 1) - 1);
	UMaterialInterface* EyeM = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/MI_%s_Eye_%d.MI_%s_Eye_%d"), *R, *B, EyeI, *B, EyeI));
	const TArray<FName> Slots = Body->GetMaterialSlotNames();
	for (int32 i = 0; i < Slots.Num(); i++)
	{
		const FString S = Slots[i].ToString();
		UMaterialInterface* M = S.Contains(TEXT("Fur")) ? FurM : (S.Contains(TEXT("Eye")) ? EyeM : BodyM);
		if (M)
		{
			Body->SetMaterial(i, M);
		}
	}
}

UAnimalAnimInstance* AAnimalActor::Anim() const
{
	return Body ? Cast<UAnimalAnimInstance>(Body->GetAnimInstance()) : nullptr;
}

UAnimSequence* AAnimalActor::Clip(FName Name) const
{
	const TObjectPtr<UAnimSequence>* S = Clips.Find(Name);
	return S ? S->Get() : nullptr;
}

void AAnimalActor::SetActivity(EAnimalActivity Activity, float SpeedCms)
{
	const bool bChanged = Activity != Act;
	Act = Activity;
	Speed = SpeedCms;
	if (bChanged && Activity == EAnimalActivity::Pounce)
	{
		PlayingClip = NAME_None;      // restart the one-shot
	}
	UpdateClip(bChanged ? 0.3f : 0.25f);
}

FName AAnimalActor::ClipFor(EAnimalActivity A) const
{
	const bool bCat = IsCat();
	switch (A)
	{
	case EAnimalActivity::Sleep: return TEXT("Sleep");
	case EAnimalActivity::Rest: return TEXT("Rest");
	case EAnimalActivity::Sit: return TEXT("Sit");
	case EAnimalActivity::Groom: return TEXT("Groom");
	case EAnimalActivity::Eat: return TEXT("Eat");
	case EAnimalActivity::Watch: return TEXT("Watch");
	case EAnimalActivity::Greet: return TEXT("Sniff");
	case EAnimalActivity::Beg: return bCat ? TEXT("Sit") : TEXT("Beg");
	case EAnimalActivity::PlayBow: return bCat ? TEXT("Crouch") : TEXT("PlayBow");
	case EAnimalActivity::Crouch: return bCat ? TEXT("Crouch") : TEXT("PlayBow");
	case EAnimalActivity::Pounce: return bCat ? TEXT("Pounce") : TEXT("Play");
	case EAnimalActivity::Hiss: return bCat ? TEXT("Hiss") : TEXT("Bark");
	case EAnimalActivity::Bark: return bCat ? TEXT("Watch") : TEXT("Bark");
	case EAnimalActivity::Climb: return bCat ? TEXT("Climb") : TEXT("Idle");
	case EAnimalActivity::Perch: return bCat ? TEXT("Perch") : TEXT("Rest");
	default: return TEXT("Idle");
	}
}

void AAnimalActor::UpdateClip(float BlendTime)
{
	UAnimalAnimInstance* A = Anim();
	if (!A)
	{
		return;
	}
	FName Want;
	float Rate = 1.f;
	bool bLoop = true;
	const float S = Speed / FMath::Max(Size, 0.3f);    // a kitten's stride is shorter
	if (IsMovingActivity(Act) && Speed > 8.f)
	{
		// the gait by speed (hysteresis round the switch points), at a rate that keeps the feet planted
		const float* W = ClipSpeeds.Find(TEXT("Walk"));
		const float* T = ClipSpeeds.Find(TEXT("Trot"));
		const float* R = ClipSpeeds.Find(TEXT("Run"));
		const float* St = ClipSpeeds.Find(TEXT("Stalk"));
		const float Ws = W ? *W : 60.f, Ts = T ? *T : 150.f, Rs = R ? *R : 400.f;
		if (Act == EAnimalActivity::Stalk && St && Clip(TEXT("Stalk")))
		{
			Want = TEXT("Stalk");
			Rate = S / *St;
		}
		else if (Act == EAnimalActivity::Rub && Clip(TEXT("Rub")))
		{
			Want = TEXT("Rub");
			Rate = FMath::Clamp(S / (Ws * 0.5f), 0.5f, 1.5f);
		}
		else
		{
			const float Up1 = (Ws + Ts) * 0.5f, Up2 = (Ts + Rs) * 0.5f;
			const bool bWas = PlayingClip == TEXT("Trot") || PlayingClip == TEXT("Run");
			if (S > Up2 * (PlayingClip == TEXT("Run") ? 0.9f : 1.05f))
			{
				Want = TEXT("Run");
				Rate = S / Rs;
			}
			else if (S > Up1 * (bWas ? 0.9f : 1.05f))
			{
				Want = TEXT("Trot");
				Rate = S / Ts;
			}
			else
			{
				Want = TEXT("Walk");
				Rate = S / Ws;
			}
		}
		Rate = FMath::Clamp(Rate, 0.35f, 2.2f);
	}
	else
	{
		Want = ClipFor(Act);
		bLoop = Act != EAnimalActivity::Pounce;
		// a still play act for a dog bounces on the spot; stopped mid-walk: stands
		if (IsMovingActivity(Act))
		{
			Want = Act == EAnimalActivity::Play && !IsCat() ? TEXT("Play") : (Act == EAnimalActivity::Sniff ? TEXT("Sniff") : TEXT("Idle"));
		}
	}
	UAnimSequence* Seq = Clip(Want);
	if (!Seq)
	{
		Want = TEXT("Idle");
		Seq = Clip(Want);
		if (!Seq)
		{
			return;
		}
	}
	if (Want != PlayingClip)
	{
		A->Play(Want, Seq, Rate, bLoop, BlendTime, !bLoop);
		PlayingClip = Want;
	}
	else
	{
		A->SetRate(Want, Rate);
	}
}

void AAnimalActor::SetLookTarget(FVector WorldPoint, bool bActive)
{
	bLookActive = bActive;
	LookWorld = WorldPoint;
}

void AAnimalActor::SetMood(float InExcite, float InFear, float InFriendly)
{
	Excite = InExcite;
	Fear = InFear;
	Friendly = InFriendly;
}

void AAnimalActor::Vocalize(FName Sound)
{
	// the mouth opens for the call (a bark is short and wide, a meow longer, a hiss held)
	if (Sound == TEXT("Bark"))
	{
		JawPulse = 1.f;
		JawHold = 0.12f;
	}
	else if (Sound == TEXT("BarkHappy"))
	{
		// the glad call is two or three quick barks (audio dog_bark_happy): the mouth snaps open with each
		JawPulse = 1.f;
		JawHold = 0.1f;
		JawTrain = 2;
		JawTrainTimer = 0.32f;
	}
	else if (Sound == TEXT("Meow") || Sound == TEXT("Whine"))
	{
		JawPulse = 0.55f;
		JawHold = 0.45f;
	}
	else if (Sound == TEXT("Hiss") || Sound == TEXT("Growl"))
	{
		JawPulse = Sound == TEXT("Hiss") ? 0.9f : 0.25f;
		JawHold = 1.2f;
	}
}

void AAnimalActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UAnimalAnimInstance* A = Anim();
	if (!A)
	{
		return;
	}
	if (Act == EAnimalActivity::Pounce && A->LeadProgress() >= 1.f)
	{
		SetActivity(EAnimalActivity::Groom);
	}
	FAnimalOverlay& O = A->Over;
	// the head: it eases onto the target and off it
	LookWeight = FMath::FInterpTo(LookWeight, bLookActive ? 1.f : 0.f, DeltaSeconds, 4.f);
	O.LookWeight = LookWeight;
	if (LookWeight > 1e-3f)
	{
		O.Look = Body->GetComponentTransform().InverseTransformPosition(LookWorld);
	}
	// the mouth
	if (JawTrain > 0 && (JawTrainTimer -= DeltaSeconds) <= 0.f)
	{
		JawPulse = 0.9f + 0.1f * Rng.FRand();
		JawHold = 0.1f;
		JawTrainTimer = 0.28f + 0.1f * Rng.FRand();
		JawTrain--;
	}
	if (JawHold > 0.f)
	{
		JawHold -= DeltaSeconds;
	}
	else
	{
		JawPulse = FMath::FInterpTo(JawPulse, 0.f, DeltaSeconds, 10.f);
	}
	O.Jaw = JawPulse + (Act == EAnimalActivity::Hiss ? 0.6f : 0.f);
	// a dog pants after a run (and in play), a cat doesn't
	const bool bExerting = Act == EAnimalActivity::Run || Act == EAnimalActivity::Chase || Act == EAnimalActivity::Play || Act == EAnimalActivity::Flee;
	Pant = FMath::FInterpTo(Pant, (!IsCat() && (bExerting || (Excite > 0.5f && Act != EAnimalActivity::Sleep))) ? 1.f : 0.f, DeltaSeconds, bExerting ? 1.f : 0.08f);
	O.Pant = Pant * 0.8f;
	// the tail: a dog wags when friendly and excited (fast and wide in play, slow and low when calm); a cat's lashes when
	// it's worked up and flicks when it watches
	float Amp, Hz;
	if (IsCat())
	{
		Amp = Excite * 9.f + (Act == EAnimalActivity::Watch || Act == EAnimalActivity::Crouch ? 3.f : 0.f);
		Hz = 0.6f + Excite * 1.2f;
	}
	else
	{
		const bool bWag = Act != EAnimalActivity::Sleep && Act != EAnimalActivity::Flee;
		Amp = bWag ? (Friendly * 4.f + Excite * 10.f) * (1.f - Fear) : 0.f;
		Hz = 1.2f + Excite * 2.6f;
	}
	WagPhase += DeltaSeconds * Hz * 6.2832f;
	O.WagDeg = FMath::FInterpTo(O.WagDeg, Amp, DeltaSeconds, 3.f);
	O.WagPhase = WagPhase;
	O.EarsBackDeg = FMath::FInterpTo(O.EarsBackDeg, Fear * 55.f - (Act == EAnimalActivity::Watch ? 8.f : 0.f), DeltaSeconds, 5.f);
	if (IsMovingActivity(Act))
	{
		UpdateClip(0.25f);
	}
}
