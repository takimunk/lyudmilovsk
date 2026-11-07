#include "Fishing/LyudRowBoat.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FishingPlaces.h"
#include "Fishing/LyudFishing.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "Plants/GameCalendar.h"
#include "Sea/LyudSea.h"
#include "Sea/LyudSwim.h"
#include "StaticMeshAttributes.h"

// A 3.8 m wooden rowing boat like the ones the Baltic clubs hire out: a round-bilged hull, a transom, three thwarts, two oars in
// rowlocks. Sizes in cm; the hull's frame: X to the bow, Y to starboard... (UE's left-handed: +Y right), Z up, the keel at 0.
namespace
{
	constexpr float BoatHalfLen = 190.f;
	constexpr float BoatHalfBeam = 70.f;
	/** The keel this far under the water. */
	constexpr float BoatDraft = 16.f;
	constexpr float BoatTopSpeed = 220.f;
	constexpr int32 BoatStations = 17;
	constexpr int32 BoatAround = 11;

	/** The hull's skin at station I (stern 0 .. bow N-1) and around-point J (port gunwale 0 .. keel .. starboard gunwale). */
	FVector3f BoatSkin(int32 I, int32 J)
	{
		const float U = -1.f + 2.f * float(I) / float(BoatStations - 1);
		const float X = U * BoatHalfLen;
		// a fine bow, a broad transom at the stern
		const float W = U > 0.f ? BoatHalfBeam * FMath::Pow(FMath::Max(0.f, 1.f - FMath::Pow(U, 1.7f)), 0.65f) : BoatHalfBeam * (1.f - 0.42f * U * U);
		// the sheer rises to the bow, a little to the stern; the keel too
		const float Top = U > 0.f ? 50.f + 16.f * U * U : 48.f + 6.f * U * U;
		const float Bottom = 7.f * U * U * (U > 0.f ? 1.6f : 0.6f);
		const float A = PI * float(J) / float(BoatAround - 1);
		const float C = FMath::Cos(A), S = FMath::Sin(A);
		const float Y = -W * FMath::Sign(C) * FMath::Pow(FMath::Abs(C), 0.7f);
		const float Z = Bottom + (Top - Bottom) * (1.f - FMath::Pow(S, 0.55f));
		return FVector3f(X, Y, Z);
	}

	/** The hull: its outer skin and, a plank's thickness in, the inner one facing in; the transom closing the stern. */
	UStaticMesh* BoatHullMesh(UObject* Outer)
	{
		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
		VUV.SetNumChannels(1);
		const FPolygonGroupID Out = MD.CreatePolygonGroup();
		const FPolygonGroupID In = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[Out] = TEXT("Outside");
		Attr.GetPolygonGroupMaterialSlotNames()[In] = TEXT("Inside");
		auto Inst = [&](const FVector3f& P, const FVector3f& N, const FVector2f& UV)
		{
			const FVertexID V = MD.CreateVertex();
			VPos[V] = P;
			const FVertexInstanceID I = MD.CreateVertexInstance(V);
			VNrm[I] = N;
			const FVector3f T = FMath::Abs(N.X) < 0.9f ? FVector3f::CrossProduct(N, FVector3f(1.f, 0.f, 0.f)).GetSafeNormal() : FVector3f(0.f, 1.f, 0.f);
			VTan[I] = T;
			VSign[I] = 1.f;
			VUV.Set(I, 0, UV);
			return I;
		};
		// the skin's normals from the neighbours (outward: away from the hull's middle line)
		auto Normal = [](int32 I, int32 J)
		{
			const FVector3f DI = BoatSkin(FMath::Min(I + 1, BoatStations - 1), J) - BoatSkin(FMath::Max(I - 1, 0), J);
			const FVector3f DJ = BoatSkin(I, FMath::Min(J + 1, BoatAround - 1)) - BoatSkin(I, FMath::Max(J - 1, 0));
			FVector3f N = FVector3f::CrossProduct(DI, DJ).GetSafeNormal();
			const FVector3f P = BoatSkin(I, J);
			if (FVector3f::DotProduct(N, FVector3f(0.f, P.Y, P.Z - 40.f)) < 0.f) N = -N;
			return N;
		};
		TArray<FVertexInstanceID> O, Ii;
		O.SetNum(BoatStations * BoatAround);
		Ii.SetNum(BoatStations * BoatAround);
		for (int32 I = 0; I < BoatStations; I++)
		{
			for (int32 J = 0; J < BoatAround; J++)
			{
				const FVector3f P = BoatSkin(I, J), N = Normal(I, J);
				const FVector2f UV(float(I) / (BoatStations - 1), float(J) / (BoatAround - 1));
				O[I * BoatAround + J] = Inst(P, N, UV);
				Ii[I * BoatAround + J] = Inst(P - N * 1.8f, -N, UV);
			}
		}
		for (int32 I = 0; I + 1 < BoatStations; I++)
		{
			for (int32 J = 0; J + 1 < BoatAround; J++)
			{
				const int32 A = I * BoatAround + J, B = A + 1, C = A + BoatAround, D = C + 1;
				// outside faces out, inside faces in: opposite windings; which is which checked against the normal
				const FVector3f PA = BoatSkin(I, J), PB = BoatSkin(I, J + 1), PC = BoatSkin(I + 1, J);
				const bool bFlip = FVector3f::DotProduct(FVector3f::CrossProduct(PC - PA, PB - PA), Normal(I, J)) < 0.f;
				if (!bFlip)
				{
					MD.CreateTriangle(Out, { O[A], O[C], O[B] });
					MD.CreateTriangle(Out, { O[B], O[C], O[D] });
					MD.CreateTriangle(In, { Ii[A], Ii[B], Ii[C] });
					MD.CreateTriangle(In, { Ii[B], Ii[D], Ii[C] });
				}
				else
				{
					MD.CreateTriangle(Out, { O[A], O[B], O[C] });
					MD.CreateTriangle(Out, { O[B], O[D], O[C] });
					MD.CreateTriangle(In, { Ii[A], Ii[C], Ii[B] });
					MD.CreateTriangle(In, { Ii[B], Ii[C], Ii[D] });
				}
			}
		}
		// the transom: a fan over station 0, both faces
		{
			FVector3f Mid(0.f);
			for (int32 J = 0; J < BoatAround; J++) Mid += BoatSkin(0, J);
			Mid /= float(BoatAround);
			const FVector3f Back(-1.f, 0.f, 0.f);
			const FVertexInstanceID MO = Inst(Mid, Back, FVector2f(0.5f, 0.5f)), MI = Inst(Mid + FVector3f(1.8f, 0.f, 0.f), -Back, FVector2f(0.5f, 0.5f));
			for (int32 J = 0; J + 1 < BoatAround; J++)
			{
				const FVertexInstanceID A = Inst(BoatSkin(0, J), Back, FVector2f(0.f, 0.f)), B = Inst(BoatSkin(0, J + 1), Back, FVector2f(1.f, 0.f));
				const FVertexInstanceID A2 = Inst(BoatSkin(0, J) + FVector3f(1.8f, 0.f, 0.f), -Back, FVector2f(0.f, 0.f));
				const FVertexInstanceID B2 = Inst(BoatSkin(0, J + 1) + FVector3f(1.8f, 0.f, 0.f), -Back, FVector2f(1.f, 0.f));
				const FVector3f PA = BoatSkin(0, J), PB = BoatSkin(0, J + 1);
				const bool bFace = FVector3f::DotProduct(FVector3f::CrossProduct(PB - PA, Mid - PA), Back) > 0.f;
				if (bFace)
				{
					MD.CreateTriangle(Out, { A, B, MO });
					MD.CreateTriangle(In, { A2, MI, B2 });
				}
				else
				{
					MD.CreateTriangle(Out, { A, MO, B });
					MD.CreateTriangle(In, { A2, B2, MI });
				}
			}
		}
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, TEXT("SM_RowBoatHull"), RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Outside")));
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Inside")));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		return Mesh;
	}

	void BoatNoLumen(UPrimitiveComponent* C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetMobility(EComponentMobility::Movable);
		// it moves every frame: out of the Lumen scene and distance fields (the perf checklist)
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		C->SetCanEverAffectNavigation(false);
	}

	int32 BoatDay(const UWorld* World)
	{
		return int32(UGameCalendar::NowIn(World).GetTicks() / ETimespan::TicksPerDay);
	}
}

ALyudRowBoat::ALyudRowBoat()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

ALyudRowBoat* ALyudRowBoat::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ALyudRowBoat> It(World);
	return It ? *It : nullptr;
}

bool ALyudRowBoat::IsHeroAboard(const UObject* WorldContext)
{
	const ALyudRowBoat* B = Get(WorldContext);
	return B && B->IsAboard();
}

UMaterialInstanceDynamic* ALyudRowBoat::Paint(const FLinearColor& Tint)
{
	if (UMaterialInstanceDynamic** Found = Paints.Find(Tint))
	{
		return *Found;
	}
	if (!BasicMat)
	{
		BasicMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (!BasicMat)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(BasicMat, this);
	M->SetVectorParameterValue(TEXT("Color"), Tint);
	Mids.Add(M);
	Paints.Add(Tint, M);
	return M;
}

UStaticMeshComponent* ALyudRowBoat::Piece(const TCHAR* Name, UStaticMesh* Mesh, USceneComponent* Parent, const FLinearColor& Tint)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, Name, RF_Transient);
	C->SetupAttachment(Parent);
	BoatNoLumen(C);
	C->SetStaticMesh(Mesh);
	if (UMaterialInstanceDynamic* M = Paint(Tint))
	{
		C->SetMaterial(0, M);
	}
	C->RegisterComponent();
	return C;
}

void ALyudRowBoat::Build()
{
	// engine shapes (always in memory) for the wood; the hull lofted here
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	const FLinearColor Paintwork(0.62f, 0.7f, 0.74f), Wood(0.5f, 0.35f, 0.2f), Dark(0.16f, 0.12f, 0.09f), Red(0.6f, 0.08f, 0.05f), Iron(0.2f, 0.2f, 0.22f);
	Hull = NewObject<USceneComponent>(this, TEXT("Hull"), RF_Transient);
	Hull->SetupAttachment(Root);
	Hull->SetMobility(EComponentMobility::Movable);
	Hull->RegisterComponent();
	UStaticMeshComponent* Skin = Piece(TEXT("HullSkin"), BoatHullMesh(this), Hull, Paintwork);
	if (UMaterialInstanceDynamic* M = Paint(Wood)) Skin->SetMaterial(1, M);
	if (!Cube || !Cyl)
	{
		return;
	}
	// three thwarts (the middle one the rower's), the floorboards, the gunwale's rubbing strakes
	auto Box = [&](const TCHAR* Name, const FVector& At, const FVector& Size, const FLinearColor& Tint)
	{
		UStaticMeshComponent* C = Piece(Name, Cube, Hull, Tint);
		C->SetRelativeLocation(At);
		C->SetRelativeScale3D(Size / 100.f);
		return C;
	};
	Box(TEXT("ThwartBow"), FVector(95.f, 0.f, 40.f), FVector(22.f, 2.f * BoatHalfBeam * 0.72f, 3.f), Wood);
	Box(TEXT("ThwartMid"), FVector(-18.f, 0.f, 36.f), FVector(26.f, 2.f * BoatHalfBeam * 0.92f, 3.f), Wood);
	Box(TEXT("ThwartStern"), FVector(-150.f, 0.f, 38.f), FVector(30.f, 2.f * BoatHalfBeam * 0.82f, 3.f), Wood);
	Box(TEXT("Floor"), FVector(-15.f, 0.f, 9.f), FVector(250.f, 56.f, 2.f), Dark);
	for (int32 Side = 0; Side < 2; Side++)
	{
		const float S = Side ? 1.f : -1.f;
		Box(*FString::Printf(TEXT("Strake%d"), Side), FVector(-10.f, S * BoatHalfBeam * 0.97f, 49.f), FVector(300.f, 3.f, 4.f), Dark);
		// the rowlock on the gunwale, the oar in it
		Box(*FString::Printf(TEXT("Rowlock%d"), Side), FVector(-5.f, S * BoatHalfBeam * 0.98f, 54.f), FVector(4.f, 4.f, 8.f), Iron);
		USceneComponent* P = NewObject<USceneComponent>(this, *FString::Printf(TEXT("OarPivot%d"), Side), RF_Transient);
		P->SetupAttachment(Hull);
		P->SetMobility(EComponentMobility::Movable);
		P->SetRelativeLocation(FVector(-5.f, S * BoatHalfBeam * 0.98f, 57.f));
		P->RegisterComponent();
		OarPivot.Add(P);
		// the loom inboard, the shaft out over the water, the blade (painted red at its tip)
		UStaticMeshComponent* Shaft = Piece(*FString::Printf(TEXT("OarShaft%d"), Side), Cyl, P, Wood);
		Shaft->SetRelativeLocationAndRotation(FVector(0.f, S * 55.f, 0.f), FRotator(0.f, 0.f, 90.f));
		Shaft->SetRelativeScale3D(FVector(0.035f, 0.035f, 2.3f));
		UStaticMeshComponent* Blade = Piece(*FString::Printf(TEXT("OarBlade%d"), Side), Cube, P, Red);
		Blade->SetRelativeLocation(FVector(0.f, S * 150.f, 0.f));
		Blade->SetRelativeScale3D(FVector(0.13f, 0.5f, 0.012f));
	}
	Box(TEXT("Stem"), FVector(BoatHalfLen - 4.f, 0.f, 36.f), FVector(6.f, 4.f, 60.f), Dark);
}

void ALyudRowBoat::BeginPlay()
{
	Super::BeginPlay();
	Build();
	Pos = FVector2D(GetActorLocation());
	Yaw = GetActorRotation().Yaw;
	if (Mooring.IsZero())
	{
		Mooring = Pos;
		Landing = LyudFishPlaces::Get().Boat.Landing;
	}
}

bool ALyudRowBoat::WaterFor(const FVector& Where) const
{
	const ALyudSea* Sea = ALyudSea::Get(this);
	const LyudFishPlaces::FBoatDef& D = LyudFishPlaces::Get().Boat;
	return Sea && Sea->DepthAt(Where) > D.MinDepth && FVector2D::Distance(FVector2D(Where), Mooring) < D.Range;
}

bool ALyudRowBoat::NearMooring() const
{
	return FVector2D::Distance(Pos, Mooring) < 1200.f;
}

void ALyudRowBoat::SetMooring(const FVector2D& InMooring, const FVector2D& InLanding)
{
	Mooring = InMooring;
	Landing = InLanding;
}

void ALyudRowBoat::Store(ULyudFishingState& State) const
{
	State.bBoatSaved = true;
	State.BoatPos = FVector(Pos, 0.f);
	State.BoatYaw = Yaw;
	State.BoatPaidDay = PaidDay;
	State.bBoatAboard = bAboard;
	State.BoatMooring = FVector(Mooring, 0.f);
	State.BoatLanding = FVector(Landing, 0.f);
}

void ALyudRowBoat::Restore(const ULyudFishingState& State)
{
	if (!State.bBoatSaved)
	{
		return;
	}
	Pos = FVector2D(State.BoatPos);
	Yaw = State.BoatYaw;
	PaidDay = State.BoatPaidDay;
	Speed = Turn = 0.f;
	SetActorLocationAndRotation(FVector(Pos, GetActorLocation().Z), FRotator(0.f, Yaw, 0.f));
	// saved in it: back on the thwart (the hire is paid: PaidDay is today's)
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (State.bBoatAboard && Hero && !bAboard)
	{
		Board(Hero, false);
	}
	else if (!State.bBoatAboard && bAboard && Hero && Hero->IsSeated())
	{
		Hero->StandUp();
	}
}

bool ALyudRowBoat::Blocked(const FVector2D& From, const FVector2D& To, float AtYaw) const
{
	// the hull's box above the water line (the bed under it is the depth test's), swept from here to there
	const float Z = GetActorLocation().Z + 50.f;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BoatSweep), false, this);
	if (const AActor* R = Rower.Get()) Q.AddIgnoredActor(R);
	FHitResult Hit;
	if (!GetWorld()->SweepSingleByChannel(Hit, FVector(From, Z), FVector(To, Z), FRotator(0.f, AtYaw, 0.f).Quaternion(), ECC_Visibility,
		FCollisionShape::MakeBox(FVector(BoatHalfLen, BoatHalfBeam, 20.f)), Q))
	{
		return false;
	}
	// already touching it (a teleport, something left on the mooring): moving or turning away from it is free
	if (Hit.bStartPenetrating)
	{
		const FVector2D Move = To - From;
		return Move.SizeSquared() > 1.f && FVector2D::DotProduct(Move, FVector2D(Hit.Normal)) < 0.f;
	}
	return true;
}

bool ALyudRowBoat::Board(AHeroCharacter* Hero, bool bCheckReach)
{
	if (!Hero || bAboard || Hero->IsSeated() || Hero->IsOnBike())
	{
		return false;
	}
	if (const ULyudSwimComponent* Swim = Hero->FindComponentByClass<ULyudSwimComponent>(); Swim && Swim->IsSwimming())
	{
		return false;
	}
	if (bCheckReach && FVector::Dist2D(Hero->GetActorLocation(), GetActorLocation()) > 280.f)
	{
		return false;
	}
	// the rent, once a day, when it is taken from the mooring
	const int32 Day = BoatDay(GetWorld());
	if (PaidDay != Day && NearMooring())
	{
		ULyudInventory* Inv = ULyudInventory::Get(this);
		const int32 Fee = LyudFishPlaces::Get().Boat.Fee;
		if (!Inv || !Inv->Spend(Fee))
		{
			Say(TEXT("no_money"));
			return false;
		}
		PaidDay = Day;
	}
	Rower = Hero;
	bAboard = true;
	Speed = Turn = 0.f;
	// on the middle thwart facing the bow, the feet on the floorboards
	Hero->SitOn(Hull, FTransform(FRotator::ZeroRotator, FVector(-5.f, 0.f, 10.f)), 26.f);
	TWeakObjectPtr<ALyudRowBoat> WeakThis(this);
	Hero->SeatMove = [WeakThis](const FVector2D& V)
	{
		ALyudRowBoat* B = WeakThis.Get();
		const APlayerController* PC = B ? UGameplayStatics::GetPlayerController(B, 0) : nullptr;
		if (B && !(PC && PC->IsMoveInputIgnored()))
		{
			B->Input = V;
			B->InputFrame = GFrameCounter;
		}
	};
	Hero->SeatLeave = [WeakThis]() { ALyudRowBoat* B = WeakThis.Get(); return !B || B->Leave(); };
	Hero->AddTickPrerequisiteActor(this);
	HintLeft = 8.f;
	UE_LOG(LogTemp, Display, TEXT("FISHING boat boarded at %s"), *GetActorLocation().ToCompactString());
	return true;
}

void ALyudRowBoat::Say(FName What)
{
	Note = What;
	NoteLeft = 3.f;
	// the same word again shows again
	PromptKey = ~0ull;
}

bool ALyudRowBoat::Leave()
{
	if (!bAboard)
	{
		return true;
	}
	// at the pier's steps, or where one can stand (a beach's shallows); not out at sea
	const ALyudSea* Sea = ALyudSea::Get(this);
	const float Depth = Sea ? Sea->DepthAt(GetActorLocation()) : 0.f;
	if (!NearMooring() && Depth > 110.f)
	{
		Say(TEXT("too_deep"));
		return false;
	}
	bLeaving = true;
	return true;
}

void ALyudRowBoat::Home()
{
	Pos = Mooring;
	Yaw = LyudFishPlaces::Get().Boat.Yaw;
	Speed = Turn = 0.f;
}

void ALyudRowBoat::Ride(float DeltaTime)
{
	// the keys held this frame (Enhanced Input only calls while one is down)
	const FVector2D In = InputFrame + 1 >= GFrameCounter ? Input : FVector2D::ZeroVector;
	const float Target = In.Y >= 0.f ? In.Y * BoatTopSpeed : In.Y * BoatTopSpeed * 0.45f;
	Speed += (Target - Speed) * (1.f - FMath::Exp(-(FMath::Abs(In.Y) > 0.1f ? 0.9f : 0.35f) * DeltaTime));
	// one oar pulled turns it even at rest; faster under way
	const float TurnGoal = In.X * 26.f * (0.55f + 0.45f * FMath::Min(1.f, FMath::Abs(Speed) / 150.f));
	Turn += (TurnGoal - Turn) * (1.f - FMath::Exp(-2.f * DeltaTime));
	// turning against a pile: it stays as it was
	const float NewYaw = FRotator::NormalizeAxis(Yaw + Turn * DeltaTime);
	if (FMath::Abs(Turn) > 0.5f && Blocked(Pos, Pos + FVector2D(0.1f, 0.f), NewYaw))
	{
		Turn = 0.f;
	}
	else
	{
		Yaw = NewYaw;
	}
	const FVector2D Fwd(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
	const FVector2D Next = Pos + Fwd * Speed * DeltaTime;
	// the end going first needs water under it (the pier's piles, a beach: it stops, bumped back a little)
	const FVector2D Lead = Next + Fwd * (Speed >= 0.f ? BoatHalfLen : -BoatHalfLen);
	if (WaterFor(FVector(Lead, 0.f)) && (FMath::Abs(Speed) < 1.f || !Blocked(Pos, Next, Yaw)))
	{
		Pos = Next;
	}
	else
	{
		Speed *= -0.15f;
	}
	Stroke = FMath::Clamp(FMath::Abs(In.Y) + 0.6f * FMath::Abs(In.X), 0.f, 1.f);
	Input = In;
}

void ALyudRowBoat::Oars(float DeltaTime)
{
	if (OarPivot.Num() != 2)
	{
		return;
	}
	// a stroke a second and a bit while rowing: the blades sweep aft in the water, come forward over it
	if (Stroke > 0.05f) Phase = FMath::Fmod(Phase + DeltaTime * 2.f * PI * 0.75f, 2.f * PI);
	for (int32 Side = 0; Side < 2; Side++)
	{
		const float S = Side ? 1.f : -1.f;
		// turning: the outer oar pulls, the inner one rests
		const float Amp = FMath::Clamp(Stroke * (1.f + S * -Input.X * 0.8f) * (Input.Y < -0.1f ? -1.f : 1.f), -1.f, 1.f);
		const float Sweep = 32.f * Amp * FMath::Sin(Phase);
		const float Dip = Stroke > 0.05f ? 9.f * FMath::Cos(Phase) - 4.f : -6.f;
		if (OarPivot[Side]) OarPivot[Side]->SetRelativeRotation(FRotator(0.f, S * Sweep, S * -Dip));
	}
}

void ALyudRowBoat::Prompt(AHeroCharacter* Hero)
{
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!UI)
	{
		return;
	}
	// what it says: a word for a moment (no money, too deep), else E close by, else the oars' keys a while after boarding
	const ULyudSwimComponent* Swim = Hero ? Hero->FindComponentByClass<ULyudSwimComponent>() : nullptr;
	const bool bNear = !bAboard && Hero && !Hero->IsSeated() && !(Swim && Swim->IsSwimming())
		&& FVector::Dist2D(Hero->GetActorLocation(), GetActorLocation()) < 280.f;
	const bool bRent = bNear && NearMooring() && PaidDay != BoatDay(GetWorld());
	uint64 Key = 0;
	if (NoteLeft > 0.f) Key = 1;
	else if (bNear) Key = bRent ? 2 : 3;
	else if (bAboard && HintLeft > 0.f) Key = 4;
	if (Key == PromptKey)
	{
		return;
	}
	PromptKey = Key;
	const int32 Fee = LyudFishPlaces::Get().Boat.Fee;
	switch (Key)
	{
	case 1: UI->SetPrompt(this, LyudText::Get(*FString::Printf(TEXT("ui.boat.%s"), *Note.ToString()), { FStringFormatArg(Fee) })); break;
	case 2: UI->SetPrompt(this, LyudText::Get(TEXT("ui.boat.rent"), { FStringFormatArg(Fee) })); break;
	case 3: UI->SetPrompt(this, LyudText::Get(TEXT("ui.boat.board"))); break;
	case 4: UI->SetPrompt(this, LyudText::Get(TEXT("ui.boat.rowing"))); break;
	default: UI->SetPrompt(this, FText::GetEmpty()); break;
	}
}

void ALyudRowBoat::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	const ALyudSea* Sea = ALyudSea::Get(this);
	if (!Sea)
	{
		return;
	}
	HintLeft = FMath::Max(0.f, HintLeft - DeltaTime);
	NoteLeft = FMath::Max(0.f, NoteLeft - DeltaTime);
	// out of the boat (Space where it may, or a script stood the hero up)
	if (bAboard && (!Rower.IsValid() || !Rower->IsSeated()))
	{
		bAboard = false;
		if (AHeroCharacter* R = Rower.Get())
		{
			R->RemoveTickPrerequisiteActor(this);
			if (NearMooring())
			{
				// up the pier's steps onto the deck (found by a trace down), the boat tied up again
				const FVector2D L = Landing;
				FVector At(L, GetActorLocation().Z + 120.f);
				FHitResult Hit;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(BoatLanding), false, this);
				Q.AddIgnoredActor(R);
				if (World->LineTraceSingleByChannel(Hit, FVector(L, At.Z + 800.f), FVector(L, At.Z - 600.f), ECC_Visibility, Q))
				{
					At.Z = Hit.ImpactPoint.Z;
				}
				R->SetActorLocation(At + FVector(0.f, 0.f, R->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
				Home();
			}
		}
		Rower.Reset();
		bLeaving = false;
		UE_LOG(LogTemp, Display, TEXT("FISHING boat left at %s"), *GetActorLocation().ToCompactString());
	}
	// far from the hero and nobody in it: lies still (no sampling of the sea)
	const bool bNearHero = Hero && FVector::Dist2D(Hero->GetActorLocation(), GetActorLocation()) < 15000.f;
	if (!bAboard && !bNearHero)
	{
		if (PromptKey != 0) Prompt(Hero);
		// left somewhere else: the boatman brings it back to the pier by the next day
		if (PaidDay != BoatDay(World) && !NearMooring()) Home();
		return;
	}
	if (bAboard)
	{
		Ride(DeltaTime);
	}
	else
	{
		// moored or left: it drifts to a stop
		Speed *= FMath::Exp(-1.5f * DeltaTime);
		Turn *= FMath::Exp(-1.5f * DeltaTime);
		Stroke = 0.f;
	}
	Oars(DeltaTime);
	// on the waves: the surface under its ends and sides gives its height, pitch and roll
	const FRotator R0(0.f, Yaw, 0.f);
	const FVector C(Pos, 0.f);
	const FVector F = R0.RotateVector(FVector(BoatHalfLen * 0.8f, 0.f, 0.f)), S = R0.RotateVector(FVector(0.f, BoatHalfBeam, 0.f));
	const float ZB = Sea->SurfaceZ(C + F), ZS = Sea->SurfaceZ(C - F), ZR = Sea->SurfaceZ(C + S), ZL = Sea->SurfaceZ(C - S);
	// never under the bed: at low water it sits on the sand (the ground is the sea's level less the depth there)
	const float Ground = Sea->SeaLevel() - Sea->DepthAt(C);
	const float Z = FMath::Max(0.25f * (ZB + ZS + ZR + ZL) - BoatDraft + (bAboard ? -3.f : 0.f), Ground + 2.f);
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(ZB - ZS, 2.f * BoatHalfLen * 0.8f)) * 0.7f;
	const float Roll = FMath::RadiansToDegrees(FMath::Atan2(ZR - ZL, 2.f * BoatHalfBeam)) * 0.6f - Turn * 0.08f;
	SetActorLocationAndRotation(FVector(Pos, Z), FRotator(Pitch, Yaw, -Roll));
	// E close by gets in (read, not consumed: the key stays everyone's)
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	if (!bAboard && PC && !PC->IsMoveInputIgnored() && PC->WasInputKeyJustPressed(EKeys::E))
	{
		Board(Hero);
	}
	Prompt(Hero);
}

static FAutoConsoleCommandWithWorldAndArgs GFishBoat(TEXT("lyud.Fish.Boat"),
	TEXT("lyud.Fish.Boat [board|leave|home]: into the rowing boat at the pier (wherever the hero is), out of it, or the boat back to its mooring"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ALyudRowBoat* B = ALyudRowBoat::Get(World);
		AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		if (!B || !Hero)
		{
			UE_LOG(LogTemp, Warning, TEXT("FISHING boat: none here"));
			return;
		}
		const FString Cmd = Args.Num() > 0 ? Args[0] : FString(TEXT("board"));
		if (Cmd == TEXT("leave"))
		{
			if (B->Leave()) Hero->StandUp();
		}
		else if (Cmd == TEXT("home"))
		{
			B->Home();
		}
		else
		{
			if (ULyudInventory* Inv = ULyudInventory::Get(World)) Inv->Earn(LyudFishPlaces::Get().Boat.Fee);
			B->Board(Hero, false);
		}
		UE_LOG(LogTemp, Display, TEXT("FISHING boat %s aboard %d at %s"), *Cmd, B->IsAboard() ? 1 : 0, *B->GetActorLocation().ToCompactString());
	}));
