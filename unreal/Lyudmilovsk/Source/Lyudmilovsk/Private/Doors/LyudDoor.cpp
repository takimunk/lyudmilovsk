#include "Doors/LyudDoor.h"

#include "CharacterStandard.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Between a leaf and the lining or the other leaf, cm. */
	constexpr float DoorGap = 0.3f;

	/** Ray (Origin, unit Dir) against an axis-aligned box: entry distance in OutT (0 when Origin is inside). */
	bool RayBox(const FBox& Box, const FVector& Origin, const FVector& Dir, float MaxT, float& OutT)
	{
		float T0 = 0.f;
		float T1 = MaxT;
		for (int32 A = 0; A < 3; ++A)
		{
			if (FMath::Abs(Dir[A]) < 1e-6f)
			{
				if (Origin[A] < Box.Min[A] || Origin[A] > Box.Max[A])
				{
					return false;
				}
				continue;
			}
			float Near = (Box.Min[A] - Origin[A]) / Dir[A];
			float Far = (Box.Max[A] - Origin[A]) / Dir[A];
			if (Near > Far)
			{
				Swap(Near, Far);
			}
			T0 = FMath::Max(T0, Near);
			T1 = FMath::Min(T1, Far);
			if (T0 > T1)
			{
				return false;
			}
		}
		OutT = T0;
		return true;
	}

	float BodyRadius(const AActor* Actor)
	{
		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Pawn->GetRootComponent()))
			{
				return Capsule->GetScaledCapsuleRadius();
			}
		}
		return 35.f;
	}
}

// ------------------------------------------------------------------ ALyudDoorBase

ALyudDoorBase::ALyudDoorBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ALyudDoorBase::BeginPlay()
{
	Super::BeginPlay();
	PrepareLeaves();
	bOpen = bStartOpen;
	bOpenedForPassage = false;
	Openness = bOpen ? 1.f : 0.f;
	ApplyOpenness(Openness);
	bLeavesBlock = false;
	SetLeavesBlockPawns(true);
	if (ULyudDoorSubsystem* Doors = ULyudDoorSubsystem::Get(this))
	{
		Doors->RegisterDoor(this);
	}
}

void ALyudDoorBase::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ULyudDoorSubsystem* Doors = ULyudDoorSubsystem::Get(this))
	{
		Doors->UnregisterDoor(this);
	}
	Super::EndPlay(Reason);
}

void ALyudDoorBase::OnGameLoaded_Implementation()
{
	Openness = bOpen ? 1.f : 0.f;
	PassageHolders.Reset();
	bWaitingToClose = bOpen && bOpenedForPassage;
	ApplyOpenness(Openness);
	SetLeavesBlockPawns(true);
	UpdateTick();
}

bool ALyudDoorBase::Open(AActor* By)
{
	return SetOpen(true, By, false);
}

bool ALyudDoorBase::Close(AActor* By)
{
	return SetOpen(false, By, false);
}

bool ALyudDoorBase::Toggle(AActor* By)
{
	return SetOpen(!bOpen, By, false);
}

void ALyudDoorBase::SetLocked(bool bInLocked)
{
	bLocked = bInLocked;
}

bool ALyudDoorBase::SetOpen(bool bInOpen, AActor* By, bool bForPassage)
{
	if (bInOpen == bOpen)
	{
		return false;
	}
	if (bInOpen && bLocked)
	{
		Play(LockedSound);
		UE_LOG(LogTemp, Log, TEXT("Door %s is locked"), *GetName());
		return false;
	}
	bOpen = bInOpen;
	bOpenedForPassage = bInOpen && bForPassage;
	bWaitingToClose = false;
	if (bOpen)
	{
		// the leaves let everyone through while they move: they never shove a body
		SetLeavesBlockPawns(false);
		Play(OpenSound);
		OnOpening.Broadcast(this, By);
	}
	else
	{
		Play(CloseSound);
		OnClosing.Broadcast(this, By);
	}
	UpdateTick();
	return true;
}

bool ALyudDoorBase::RequestPassage(AActor* Agent)
{
	if (!CanNpcsPass())
	{
		return false;
	}
	if (Agent && !PassageHolders.Contains(Agent))
	{
		PassageHolders.Add(Agent);
	}
	bWaitingToClose = false;
	if (!bOpen)
	{
		SetOpen(true, Agent, true);
	}
	UpdateTick();
	return IsPassable();
}

void ALyudDoorBase::ReleasePassage(AActor* Agent)
{
	PassageHolders.Remove(Agent);
	PassageHolders.RemoveAll([](const TWeakObjectPtr<AActor>& A) { return !A.IsValid(); });
	if (PassageHolders.IsEmpty() && bOpen && bOpenedForPassage && bCloseBehindNpcs)
	{
		bWaitingToClose = true;
		UpdateTick();
	}
}

FVector ALyudDoorBase::GetPassageCenter() const
{
	const FBox Box = GetDoorwayLocalBox();
	return GetActorTransform().TransformPosition(FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z));
}

FVector ALyudDoorBase::GetPassageNormal() const
{
	return GetActorForwardVector().GetSafeNormal2D();
}

float ALyudDoorBase::GetClearWidth() const
{
	return GetDoorwayLocalBox().GetSize().Y * GetActorScale3D().Y;
}

float ALyudDoorBase::GetClearHeight() const
{
	return GetDoorwayLocalBox().GetSize().Z * GetActorScale3D().Z;
}

FVector ALyudDoorBase::GetWaitPoint(const FVector& From, float Radius) const
{
	const FTransform& T = GetActorTransform();
	const FVector Local = T.InverseTransformPosition(From);
	const FBox Door = GetDoorwayLocalBox();
	const FBox Sweep = GetSweepLocalBox();
	const float Margin = Radius + 15.f;
	// the swing side waits beyond the open leaf, the other side just short of the doorway
	const float X = Local.X > Door.GetCenter().X ? FMath::Max(Sweep.Max.X, Door.Max.X) + Margin : FMath::Min(Sweep.Min.X, Door.Min.X) - Margin;
	return T.TransformPosition(FVector(X, Door.GetCenter().Y, Local.Z));
}

bool ALyudDoorBase::CrossesSegment(const FVector& From, const FVector& To, float& OutAlpha) const
{
	const FTransform& T = GetActorTransform();
	const FVector A = T.InverseTransformPosition(From);
	const FVector B = T.InverseTransformPosition(To);
	const float Len = FVector::Dist(A, B);
	if (Len < 1.f)
	{
		return false;
	}
	FBox Box = GetDoorwayLocalBox();
	// bodies move at capsule height: let the segment run anywhere from the floor to above the head
	Box.Min.Z -= 100.f;
	float Hit = 0.f;
	if (!RayBox(Box, A, (B - A) / Len, Len, Hit))
	{
		return false;
	}
	OutAlpha = Hit / Len;
	return true;
}

bool ALyudDoorBase::IsInSweep(const FVector& P, float Radius) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(P);
	const FBox Box = GetSweepLocalBox();
	return FBox(Box.Min - FVector(Radius, Radius, 120.f), Box.Max + FVector(Radius, Radius, 20.f)).IsInside(Local);
}

bool ALyudDoorBase::PlayerCanUse(const APawn* Pawn, const FVector& Eye, const FVector& Dir, float& OutDistance) const
{
	if (!bPlayerCanUse)
	{
		return false;
	}
	bool bHit = false;
	float Best = InteractRange;
	// the doorway itself (closed leaf or the open hole) ...
	const FTransform& T = GetActorTransform();
	const FVector LocalEye = T.InverseTransformPosition(Eye);
	const FVector LocalDir = T.InverseTransformVector(Dir).GetSafeNormal();
	float Hit = 0.f;
	FBox Door = GetDoorwayLocalBox();
	Door = FBox(Door.Min - FVector(8.f, 0.f, 0.f), Door.Max + FVector(8.f, 0.f, 0.f));
	if (RayBox(Door, LocalEye, LocalDir, InteractRange, Hit))
	{
		// local distances are in actor units; doors are not scaled in practice
		Best = Hit;
		bHit = true;
	}
	// ... or the leaves where they are now (an open leaf against the wall)
	TArray<UPrimitiveComponent*> Leaves;
	GetLeafPrimitives(Leaves);
	for (const UPrimitiveComponent* Leaf : Leaves)
	{
		if (Leaf && Leaf->IsVisible() && RayBox(Leaf->Bounds.GetBox(), Eye, Dir, Best, Hit))
		{
			Best = Hit;
			bHit = true;
		}
	}
	OutDistance = Best;
	return bHit;
}

void ALyudDoorBase::SetLeavesBlockPawns(bool bBlock)
{
	if (bLeavesBlock == bBlock)
	{
		return;
	}
	bLeavesBlock = bBlock;
	TArray<UPrimitiveComponent*> Leaves;
	GetLeafPrimitives(Leaves);
	for (UPrimitiveComponent* Leaf : Leaves)
	{
		if (Leaf && Leaf->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			Leaf->SetCollisionResponseToChannel(ECC_Pawn, bBlock ? ECR_Block : ECR_Ignore);
		}
	}
}

bool ALyudDoorBase::SomeoneInSweep() const
{
	const ULyudDoorSubsystem* Doors = ULyudDoorSubsystem::Get(this);
	if (!Doors)
	{
		return false;
	}
	TArray<TPair<FVector, float>> Bodies;
	Doors->GetBodies(Bodies);
	for (const TPair<FVector, float>& B : Bodies)
	{
		if (IsInSweep(B.Key, B.Value))
		{
			return true;
		}
	}
	return false;
}

void ALyudDoorBase::Play(USoundBase* Sound) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetPassageCenter() + FVector(0.f, 0.f, 100.f));
	}
}

void ALyudDoorBase::UpdateTick()
{
	const float Target = bOpen ? 1.f : 0.f;
	SetActorTickEnabled(!FMath::IsNearlyEqual(Openness, Target) || bWaitingToClose);
}

void ALyudDoorBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bWaitingToClose)
	{
		PassageHolders.RemoveAll([](const TWeakObjectPtr<AActor>& A) { return !A.IsValid(); });
		if (!PassageHolders.IsEmpty() || !bOpen)
		{
			bWaitingToClose = false;
		}
		else if (IsPassable() && !SomeoneInSweep())
		{
			SetOpen(false, nullptr, false);
		}
	}
	const float Target = bOpen ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(Openness, Target))
	{
		// a closing door waits for whoever stands where its leaves sweep
		if (!bOpen && SomeoneInSweep())
		{
			return;
		}
		SetLeavesBlockPawns(false);
		Openness = FMath::FInterpConstantTo(Openness, Target, DeltaSeconds, SwingSpeed / 90.f);
		if (FMath::IsNearlyEqual(Openness, Target, 1e-4f))
		{
			Openness = Target;
		}
		ApplyOpenness(Openness);
		if (Openness == Target)
		{
			SetLeavesBlockPawns(true);
			if (bOpen)
			{
				OnOpened.Broadcast(this, nullptr);
			}
			else
			{
				Play(ShutSound);
				OnClosed.Broadcast(this, nullptr);
			}
		}
	}
	UpdateTick();
}

// ------------------------------------------------------------------ ALyudDoor

ALyudDoor::ALyudDoor()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Frame = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Frame"));
	Frame->SetupAttachment(Root);
	Frame->SetStaticMesh(Cube.Object);
	Frame->SetCollisionProfileName(TEXT("BlockAll"));

	UStaticMesh* CubeMesh = Cube.Object;
	auto MakeLeaf = [this, CubeMesh](const TCHAR* HingeName, const TCHAR* LeafName, const TCHAR* HandlesName,
		TObjectPtr<USceneComponent>& OutHinge, TObjectPtr<UStaticMeshComponent>& OutLeaf, TObjectPtr<UInstancedStaticMeshComponent>& OutHandles)
	{
		OutHinge = CreateDefaultSubobject<USceneComponent>(HingeName);
		OutHinge->SetupAttachment(Root);
		OutHinge->SetMobility(EComponentMobility::Movable);
		OutLeaf = CreateDefaultSubobject<UStaticMeshComponent>(LeafName);
		OutLeaf->SetupAttachment(OutHinge);
		OutLeaf->SetMobility(EComponentMobility::Movable);
		OutLeaf->SetStaticMesh(CubeMesh);
		OutLeaf->SetCollisionProfileName(TEXT("BlockAll"));
		// a navmesh runs through doorways: agents open the door when they get there
		OutLeaf->SetCanEverAffectNavigation(false);
		OutHandles = CreateDefaultSubobject<UInstancedStaticMeshComponent>(HandlesName);
		OutHandles->SetupAttachment(OutHinge);
		OutHandles->SetMobility(EComponentMobility::Movable);
		OutHandles->SetStaticMesh(CubeMesh);
		OutHandles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OutHandles->SetCanEverAffectNavigation(false);
	};
	MakeLeaf(TEXT("HingeA"), TEXT("LeafA"), TEXT("HandlesA"), HingeA, LeafA, HandlesA);
	MakeLeaf(TEXT("HingeB"), TEXT("LeafB"), TEXT("HandlesB"), HingeB, LeafB, HandlesB);
}

int32 ALyudDoor::NumLeaves() const
{
	switch (Size)
	{
	case ELyudDoorSize::Double: return 2;
	case ELyudDoorSize::Custom:
	case ELyudDoorSize::FitHole: return bDoubleLeaf ? 2 : 1;
	default: return 1;
	}
}

FVector2D ALyudDoor::HoleForClear(float InClearWidth, float InClearHeight, bool bTwoLeaves)
{
	using namespace LyudmilovskCharacter;
	return FVector2D(InClearWidth + 2.f * DoorLiningThickness + (bTwoLeaves ? 2.f : 1.f) * DoorLeafThickness, InClearHeight + DoorLiningThickness);
}

FVector2D ALyudDoor::ClearForHole(float InHoleWidth, float InHoleHeight, bool bTwoLeaves)
{
	using namespace LyudmilovskCharacter;
	return FVector2D(InHoleWidth - 2.f * DoorLiningThickness - (bTwoLeaves ? 2.f : 1.f) * DoorLeafThickness, InHoleHeight - DoorLiningThickness);
}

void ALyudDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ALyudDoor::BeginPlay()
{
	// a loaded level does not run OnConstruction again outside the editor: the sizes (not saved) and the dynamic
	// materials are made here; the frame and leaves were laid out and saved by the editor
	ComputeSizes();
	ApplyLooks();
	Super::BeginPlay();
}

void ALyudDoor::ComputeSizes()
{
	using namespace LyudmilovskCharacter;
	const int32 N = NumLeaves();
	const float L = bBuildFrame ? DoorLiningThickness : 0.f;
	const float T = DoorLeafThickness;
	float Cw = StandardDoorClearWidth;
	float Ch = StandardDoorClearHeight;
	bBelowStandard = false;
	switch (Size)
	{
	case ELyudDoorSize::Wide: Cw = WideDoorClearWidth; break;
	case ELyudDoorSize::Double: Cw = DoubleDoorClearWidth; Ch = DoubleDoorClearHeight; break;
	case ELyudDoorSize::Custom:
		Cw = FMath::Max(ClearWidth, MinDoorClearWidth);
		Ch = FMath::Max(ClearHeight, MinDoorClearHeight);
		break;
	case ELyudDoorSize::FitHole:
		Cw = HoleWidth - 2.f * L - N * T;
		Ch = HoleHeight - L;
		bBelowStandard = Cw < MinDoorClearWidth - 0.01f || Ch < MinDoorClearHeight - 0.01f;
		if (bBelowStandard)
		{
			UE_LOG(LogTemp, Warning, TEXT("Door %s: hole %.0f x %.0f gives %.0f x %.0f clear, below the character standard %.0f x %.0f"),
				*GetName(), HoleWidth, HoleHeight, Cw, Ch, MinDoorClearWidth, MinDoorClearHeight);
		}
		break;
	default: break;
	}
	if (Size != ELyudDoorSize::FitHole)
	{
		HoleWidth = Cw + 2.f * L + N * T;
		HoleHeight = Ch + L;
	}
	ActualClearWidth = Cw;
	ActualClearHeight = Ch;
	InnerW = HoleWidth - 2.f * L;
	InnerH = HoleHeight - L;
	LeafW = (InnerW - (N + 1) * DoorGap) / N;
}

void ALyudDoor::Rebuild()
{
	using namespace LyudmilovskCharacter;
	ComputeSizes();
	const int32 N = NumLeaves();
	const float L = bBuildFrame ? DoorLiningThickness : 0.f;
	const float T = DoorLeafThickness;
	const float Gap = DoorGap;
	const float FloorGap = 1.f;
	const float LeafH = InnerH - FloorGap - Gap;
	const float Half = WallThickness * 0.5f;
	auto Cube = [](const FVector& Center, const FVector& Extent) { return FTransform(FQuat::Identity, Center, Extent / 100.f); };

	Frame->ClearInstances();
	if (bBuildFrame)
	{
		const float Cs = CasingWidth;
		Frame->AddInstance(Cube(FVector(0.f, InnerW * 0.5f + L * 0.5f, InnerH * 0.5f), FVector(WallThickness, L, InnerH)));
		Frame->AddInstance(Cube(FVector(0.f, -InnerW * 0.5f - L * 0.5f, InnerH * 0.5f), FVector(WallThickness, L, InnerH)));
		Frame->AddInstance(Cube(FVector(0.f, 0.f, InnerH + L * 0.5f), FVector(WallThickness, HoleWidth, L)));
		for (const float Face : { Half + 1.f, -Half - 1.f })
		{
			for (const float Side : { 1.f, -1.f })
			{
				Frame->AddInstance(Cube(FVector(Face, Side * (HoleWidth + Cs) * 0.5f, (HoleHeight + Cs) * 0.5f), FVector(2.f, Cs, HoleHeight + Cs)));
			}
			Frame->AddInstance(Cube(FVector(Face, 0.f, HoleHeight + Cs * 0.5f), FVector(2.f, HoleWidth + 2.f * Cs, Cs)));
		}
	}

	// leaf A on the hinge side asked for; leaf B (double doors) on the other jamb
	const float HingeYSign = Hinge == ELyudDoorHinge::Left ? 1.f : -1.f;
	FBox MeshBox(FVector(-50.f), FVector(50.f));
	if (LeafMesh)
	{
		MeshBox = LeafMesh->GetBoundingBox();
	}
	auto Lay = [&](USceneComponent* HingeC, UStaticMeshComponent* Leaf, UInstancedStaticMeshComponent* Handles, float YSign, bool bUsed)
	{
		HingeC->SetVisibility(bUsed, true);
		Leaf->SetCollisionEnabled(bUsed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Handles->ClearInstances();
		if (!bUsed)
		{
			return;
		}
		// the hinge on the swing face's edge of the lining; the leaf runs from it across the opening (-YSign)
		HingeC->SetRelativeLocation(FVector(Half, YSign * InnerW * 0.5f, 0.f));
		const float S = -YSign;
		const FVector Box(T, LeafW, LeafH);
		const FVector Center(-T * 0.5f, S * (Gap + LeafW * 0.5f), FloorGap + LeafH * 0.5f);
		if (LeafMesh)
		{
			Leaf->SetStaticMesh(LeafMesh);
		}
		const FVector Scale = Box / MeshBox.GetSize().ComponentMax(FVector(0.01f));
		Leaf->SetRelativeTransform(FTransform(FQuat::Identity, Center - Scale * MeshBox.GetCenter(), Scale));
		// a lever handle on both faces near the free edge
		const float Y = S * (Gap + LeafW - 7.f);
		for (const float Face : { 1.f, -1.f })
		{
			const float X0 = Face > 0.f ? 0.f : -T;
			Handles->AddInstance(Cube(FVector(X0 + Face * 0.6f, Y, 100.f), FVector(1.2f, 5.f, 16.f)));
			Handles->AddInstance(Cube(FVector(X0 + Face * 3.f, Y, 104.f), FVector(4.f, 2.f, 2.f)));
			Handles->AddInstance(Cube(FVector(X0 + Face * 5.5f, Y + YSign * 5.f, 104.f), FVector(2.f, 12.f, 2.2f)));
		}
	};
	if (N == 2)
	{
		Lay(HingeA, LeafA, HandlesA, 1.f, true);
		Lay(HingeB, LeafB, HandlesB, -1.f, true);
	}
	else
	{
		Lay(HingeA, LeafA, HandlesA, HingeYSign, true);
		Lay(HingeB, LeafB, HandlesB, -HingeYSign, false);
	}
	ApplyLooks();
	ApplyOpenness(Openness);
}

void ALyudDoor::ApplyLooks()
{
	UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	auto Pick = [this, Basic](UMaterialInterface* Given, const FLinearColor& Color) -> UMaterialInterface*
	{
		if (Given || !Basic)
		{
			return Given;
		}
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Basic, this);
		Mid->SetVectorParameterValue(TEXT("Color"), Color);
		return Mid;
	};
	UMaterialInterface* Leaf = Pick(LeafMaterial, LeafColor);
	UMaterialInterface* FrameM = Pick(FrameMaterial, FrameColor);
	UMaterialInterface* Metal = Pick(HandleMaterial, FLinearColor(0.55f, 0.5f, 0.4f));
	// a supplied mesh keeps its own materials unless LeafMaterial says otherwise
	if (!LeafMesh || LeafMaterial)
	{
		LeafA->SetMaterial(0, Leaf);
		LeafB->SetMaterial(0, Leaf);
	}
	Frame->SetMaterial(0, FrameM);
	HandlesA->SetMaterial(0, Metal);
	HandlesB->SetMaterial(0, Metal);
}

void ALyudDoor::ApplyOpenness(float InOpenness)
{
	const float Angle = OpenAngle * InOpenness;
	// a hinge at +Y has its leaf along -Y: a positive yaw swings it out to +X; the -Y hinge mirrors it
	const bool bDouble = NumLeaves() == 2;
	const float SignA = bDouble ? 1.f : (Hinge == ELyudDoorHinge::Left ? 1.f : -1.f);
	HingeA->SetRelativeRotation(FRotator(0.f, SignA * Angle, 0.f));
	HingeB->SetRelativeRotation(FRotator(0.f, -SignA * Angle, 0.f));
}

void ALyudDoor::GetLeafPrimitives(TArray<UPrimitiveComponent*>& Out) const
{
	Out.Add(LeafA);
	if (NumLeaves() == 2)
	{
		Out.Add(LeafB);
	}
}

FBox ALyudDoor::GetDoorwayLocalBox() const
{
	// the clear opening: the lining's inside less the open leaves standing across it at their hinges
	const float Half = WallThickness * 0.5f;
	const float T = LyudmilovskCharacter::DoorLeafThickness;
	float Y0 = -InnerW * 0.5f;
	float Y1 = InnerW * 0.5f;
	if (NumLeaves() == 2)
	{
		Y0 += T;
		Y1 -= T;
	}
	else if (Hinge == ELyudDoorHinge::Left)
	{
		Y1 -= T;
	}
	else
	{
		Y0 += T;
	}
	return FBox(FVector(-Half, Y0, 0.f), FVector(Half, Y1, InnerH));
}

FBox ALyudDoor::GetSweepLocalBox() const
{
	const float Half = WallThickness * 0.5f;
	return FBox(FVector(-Half, -InnerW * 0.5f - 10.f, 0.f), FVector(Half + LeafW + 5.f, InnerW * 0.5f + 10.f, InnerH));
}

// ------------------------------------------------------------------ ULyudDoorSubsystem

ULyudDoorSubsystem* ULyudDoorSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudDoorSubsystem>() : nullptr;
}

void ULyudDoorSubsystem::RegisterDoor(ALyudDoorBase* Door)
{
	Doors.AddUnique(Door);
}

void ULyudDoorSubsystem::UnregisterDoor(ALyudDoorBase* Door)
{
	Doors.Remove(Door);
}

void ULyudDoorSubsystem::RegisterAgent(AActor* Agent, float Radius)
{
	if (!Agent)
	{
		return;
	}
	for (FAgent& A : Agents)
	{
		if (A.Actor == Agent)
		{
			A.bRegistered = true;
			A.Radius = Radius;
			return;
		}
	}
	FAgent& A = Agents.AddDefaulted_GetRef();
	A.Actor = Agent;
	A.Radius = Radius;
	A.bRegistered = true;
	A.LastPos = Agent->GetActorLocation();
}

void ULyudDoorSubsystem::UnregisterAgent(AActor* Agent)
{
	for (int32 i = Agents.Num() - 1; i >= 0; --i)
	{
		if (Agents[i].Actor == Agent)
		{
			for (const TWeakObjectPtr<ALyudDoorBase>& D : Agents[i].Holding)
			{
				if (D.IsValid())
				{
					D->ReleasePassage(Agent);
				}
			}
			Agents.RemoveAt(i);
		}
	}
}

ALyudDoorBase* ULyudDoorSubsystem::FindDoorOnSegment(const FVector& From, const FVector& To) const
{
	ALyudDoorBase* Best = nullptr;
	float BestAlpha = 2.f;
	for (const TWeakObjectPtr<ALyudDoorBase>& D : Doors)
	{
		float Alpha = 0.f;
		if (D.IsValid() && D->CrossesSegment(From, To, Alpha) && Alpha < BestAlpha)
		{
			BestAlpha = Alpha;
			Best = D.Get();
		}
	}
	return Best;
}

ALyudDoorBase* ULyudDoorSubsystem::DoorInView() const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return nullptr;
	}
	FVector Eye;
	FRotator View;
	PC->GetPlayerViewPoint(Eye, View);
	const FVector Dir = View.Vector();
	ALyudDoorBase* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<ALyudDoorBase>& D : Doors)
	{
		float Distance = 0.f;
		if (D.IsValid() && D->PlayerCanUse(Pawn, Eye, Dir, Distance) && Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = D.Get();
		}
	}
	return Best;
}

bool ULyudDoorSubsystem::PlayerInteract()
{
	ALyudDoorBase* Door = DoorInView();
	if (!Door)
	{
		return false;
	}
	Door->Toggle(UGameplayStatics::GetPlayerPawn(this, 0));
	return true;
}

void ULyudDoorSubsystem::GetBodies(TArray<TPair<FVector, float>>& Out) const
{
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Out.Add({ Player->GetActorLocation(), BodyRadius(Player) });
	}
	for (const FAgent& A : Agents)
	{
		if (A.Actor.IsValid())
		{
			Out.Add({ A.Actor->GetActorLocation(), A.Radius });
		}
	}
}

void ULyudDoorSubsystem::Tick(float DeltaTime)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		// polled, never consumed: E belongs to several things at once
		if (PC->WasInputKeyJustPressed(EKeys::E))
		{
			PlayerInteract();
		}
	}
	AgentClock += DeltaTime;
	if (AgentClock >= 0.1f)
	{
		UpdateAgents(AgentClock);
		AgentClock = 0.f;
	}
}

void ULyudDoorSubsystem::UpdateAgents(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// pawns nobody plays are agents by themselves; others must register
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		if (Pawn->IsPlayerControlled() || Agents.ContainsByPredicate([Pawn](const FAgent& A) { return A.Actor == Pawn; }))
		{
			continue;
		}
		FAgent& A = Agents.AddDefaulted_GetRef();
		A.Actor = Pawn;
		A.Radius = BodyRadius(Pawn);
		A.LastPos = Pawn->GetActorLocation();
	}
	for (int32 i = Agents.Num() - 1; i >= 0; --i)
	{
		FAgent& A = Agents[i];
		AActor* Actor = A.Actor.Get();
		const APawn* Pawn = Cast<APawn>(Actor);
		if (!Actor || (!A.bRegistered && Pawn && Pawn->IsPlayerControlled()))
		{
			for (const TWeakObjectPtr<ALyudDoorBase>& D : A.Holding)
			{
				if (D.IsValid())
				{
					D->ReleasePassage(Actor);
				}
			}
			Agents.RemoveAt(i);
			continue;
		}
		const FVector Pos = Actor->GetActorLocation();
		A.Velocity = (Pos - A.LastPos) / FMath::Max(DeltaTime, 1e-3f);
		A.LastPos = Pos;
		for (const TWeakObjectPtr<ALyudDoorBase>& DoorPtr : Doors)
		{
			ALyudDoorBase* Door = DoorPtr.Get();
			if (!Door)
			{
				continue;
			}
			const bool bHolding = A.Holding.Contains(Door);
			bool bWants = false;
			if (Door->CanNpcsPass())
			{
				const FTransform& T = Door->GetActorTransform();
				const FVector Local = T.InverseTransformPosition(Pos);
				const FVector LocalVel = T.InverseTransformVector(A.Velocity);
				const FBox Way = Door->GetDoorwayLocalBox();
				const FBox Approach(Way.Min - FVector(160.f, 25.f, 150.f), Way.Max + FVector(160.f, 25.f, 50.f));
				if (Approach.IsInside(Local))
				{
					const bool bInDoorway = FBox(Way.Min - FVector(A.Radius + 25.f, 0.f, 150.f), Way.Max + FVector(A.Radius + 25.f, 0.f, 50.f)).IsInside(Local);
					const float ToPlane = Way.GetCenter().X - Local.X;
					const bool bComing = LocalVel.X * ToPlane > 0.f && FMath::Abs(LocalVel.X) > 10.f;
					bWants = bInDoorway || bComing;
				}
			}
			if (bWants && !bHolding)
			{
				A.Holding.Add(Door);
				Door->RequestPassage(Actor);
			}
			else if (!bWants && bHolding)
			{
				A.Holding.Remove(Door);
				Door->ReleasePassage(Actor);
			}
		}
		A.Holding.RemoveAll([](const TWeakObjectPtr<ALyudDoorBase>& D) { return !D.IsValid(); });
	}
}

// ------------------------------------------------------------------ console

namespace
{
	TArray<ALyudDoorBase*> DoorsMatching(UWorld* World, const FString& Part)
	{
		TArray<ALyudDoorBase*> Out;
		if (const ULyudDoorSubsystem* S = ULyudDoorSubsystem::Get(World))
		{
			for (const TWeakObjectPtr<ALyudDoorBase>& D : S->GetDoors())
			{
				if (D.IsValid() && (Part.IsEmpty() || D->GetName().Contains(Part) || D->GetActorNameOrLabel().Contains(Part)))
				{
					Out.Add(D.Get());
				}
			}
		}
		return Out;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GDoorList(TEXT("lyud.Door.List"), TEXT("lyud.Door.List [name part]: every door, open or not, its clear size"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		for (ALyudDoorBase* D : DoorsMatching(World, Args.IsValidIndex(0) ? Args[0] : FString()))
		{
			UE_LOG(LogTemp, Warning, TEXT("DOOR %s open %d openness %.2f locked %d clear %.0f x %.0f at %s"), *D->GetName(), D->IsOpen(),
				D->GetOpenness(), D->bLocked, D->GetClearWidth(), D->GetClearHeight(), *D->GetPassageCenter().ToString());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GDoorToggle(TEXT("lyud.Door.Toggle"), TEXT("lyud.Door.Toggle <name part>: open or close those doors"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		for (ALyudDoorBase* D : DoorsMatching(World, Args.IsValidIndex(0) ? Args[0] : FString()))
		{
			D->Toggle(nullptr);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GDoorLock(TEXT("lyud.Door.Lock"), TEXT("lyud.Door.Lock <name part> [1|0]: lock or unlock those doors"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		for (ALyudDoorBase* D : DoorsMatching(World, Args.IsValidIndex(0) ? Args[0] : FString()))
		{
			D->SetLocked(!Args.IsValidIndex(1) || FCString::Atoi(*Args[1]) != 0);
		}
	}));
