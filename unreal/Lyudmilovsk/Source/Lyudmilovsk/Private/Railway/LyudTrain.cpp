#include "Railway/LyudTrain.h"

#include "Birds/BirdActor.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RailMaterials.h"
#include "Railway/LyudTrainWake.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sky/LyudSky.h"
#include "Sound/SoundBase.h"

namespace
{
// the kit's frame is Blender's (Y left): Unreal's Y is its -Y
FVector KitToUE(double X, double Y, double Z)
{
	return FVector(X * 100.0, -Y * 100.0, Z * 100.0);
}

USoundBase* RailSound(const TCHAR* Name)
{
	return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/Railway/S_%s.S_%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

constexpr double JointM = 25.0;       // rail lengths: a joint every 25 m
constexpr double BogieX = 5.6;        // bogie centres from a car's middle, m
constexpr double AxleX = 1.05;        // axles from a bogie's centre
}

ALyudTrain::ALyudTrain()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
	Tags.Add(TEXT("NoGrass"));
	Tags.Add(TEXT("NoTree"));
}

void ALyudTrain::Setup(ULyudRailwaySubsystem* InRailway, int32 InIndex)
{
	Railway = InRailway;
	Index = InIndex;
	// the kit's sockets (blender/railway/rail_kit.py writes Content/Railway/kit.json)
	FString Text;
	TSharedPtr<FJsonObject> Kit;
	if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Railway/kit.json"))))
	{
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Kit);
	}
	const TSharedPtr<FJsonObject> Car = Kit.IsValid() ? Kit->GetObjectField(TEXT("car")) : nullptr;
	if (Car.IsValid())
	{
		CarLength = Car->GetNumberField(TEXT("length"));
	}
	TSharedPtr<FJsonObject> Built;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Railway->BuiltJson), Built);
	if (Built.IsValid())
	{
		NumCars = int32(Built->GetObjectField(TEXT("train"))->GetNumberField(TEXT("cars")));
	}
	for (int32 C = 0; C < NumCars; C++)
	{
		BuildCar(C);
	}
	// seats and doors from the kit, per car (car 1 runs turned round: its seats mirror)
	if (Car.IsValid())
	{
		for (int32 C = 0; C < NumCars; C++)
		{
			const double Turn = C % 2 ? -1.0 : 1.0;
			for (const TSharedPtr<FJsonValue>& V : Car->GetArrayField(TEXT("seats")))
			{
				const TSharedPtr<FJsonObject> S = V->AsObject();
				const FVector P = KitToUE(S->GetNumberField(TEXT("x")), S->GetNumberField(TEXT("y")), S->GetNumberField(TEXT("z")));
				const FVector Face = KitToUE(0.0, S->GetNumberField(TEXT("face_y")), 0.0).GetSafeNormal();
				FSeat Seat;
				Seat.Car = C;
				Seat.Local = FTransform(FRotationMatrix::MakeFromXZ(Face * Turn, FVector::UpVector).Rotator(), P * FVector(Turn, Turn, 1.0));
				Seats.Add(Seat);
			}
		}
	}
	Wake = NewObject<ULyudTrainWake>(this, TEXT("Wake"));
	Wake->SetupAttachment(RootComponent);
	Wake->RegisterComponent();
	AddInstanceComponent(Wake);
	auto Loop = [&](const TCHAR* Sound, const TCHAR* Name)
	{
		UAudioComponent* A = NewObject<UAudioComponent>(this, Name);
		A->SetSound(RailSound(Sound));
		A->bAutoActivate = false;
		A->SetupAttachment(CarRoots.Num() ? CarRoots[0].Get() : RootComponent.Get());
		A->RegisterComponent();
		A->SetRelativeLocation(FVector(BogieX * 100.0, 0.0, 60.0));
		AddInstanceComponent(A);
		return A;
	};
	Motor = Loop(TEXT("motor_01"), TEXT("Motor"));
	Wheels = Loop(TEXT("roll_01"), TEXT("Wheels"));
	for (const TCHAR* N : { TEXT("joint_01"), TEXT("joint_02"), TEXT("joint_03"), TEXT("horn_01"), TEXT("door_01"), TEXT("door_02") })
	{
		Sounds.Add(N, RailSound(N));
	}
	SetActorHiddenInGame(true);
}

void ALyudTrain::BuildCar(int32 C)
{
	USceneComponent* Root = NewObject<USceneComponent>(this, *FString::Printf(TEXT("Car%d"), C));
	Root->SetMobility(EComponentMobility::Movable);
	Root->SetupAttachment(RootComponent);
	Root->RegisterComponent();
	AddInstanceComponent(Root);
	CarRoots.Add(Root);
	FCar Car;
	Car.Root = Root;
	Cars.Add(Car);
	// car 1 runs turned round (its cab at the train's other end)
	const FRotator Turn(0.0, C % 2 ? 180.0 : 0.0, 0.0);
	auto Piece = [&](const TCHAR* Name, const FVector& At, const FRotator& R, bool bShadow = true) -> UStaticMeshComponent*
	{
		UStaticMesh* M = LyudRailMat::KitMesh(Name);
		if (!M)
		{
			return nullptr;
		}
		UStaticMeshComponent* S = NewObject<UStaticMeshComponent>(this);
		S->SetMobility(EComponentMobility::Movable);
		S->SetStaticMesh(M);
		S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		S->SetCastShadow(bShadow);
		S->LightingChannels.bChannel0 = true;
		S->LightingChannels.bChannel1 = true;
		S->SetupAttachment(Root);
		S->RegisterComponent();
		S->SetRelativeLocationAndRotation(At, R);
		AddInstanceComponent(S);
		return S;
	};
	UStaticMeshComponent* Body = Piece(TEXT("SM_RL_Car"), FVector::ZeroVector, Turn);
	if (!Body)
	{
		// the kit isn't imported: a plain box the train's size so the line still works
		UStaticMeshComponent* B = NewObject<UStaticMeshComponent>(this);
		B->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		B->SetupAttachment(Root);
		B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		B->RegisterComponent();
		B->SetRelativeLocation(FVector(0, 0, 250.0));
		B->SetRelativeScale3D(FVector(CarLength, 2.8, 2.9));
		AddInstanceComponent(B);
		Bodies.Add(B);
		return;
	}
	Bodies.Add(Body);
	UStaticMeshComponent* Glass = Piece(TEXT("SM_RL_CarGlass"), FVector::ZeroVector, Turn, false);
	UStaticMeshComponent* Trim = Piece(TEXT("SM_RL_CarTrim"), FVector::ZeroVector, Turn);
	Piece(TEXT("SM_RL_Underframe"), FVector::ZeroVector, Turn);
	UStaticMeshComponent* Inside = Piece(TEXT("SM_RL_CarInterior"), FVector::ZeroVector, Turn, false);
	for (const double Bx : { -BogieX, BogieX })
	{
		Piece(TEXT("SM_RL_Bogie"), FVector(Bx * 100.0, 0.0, 0.0), FRotator::ZeroRotator);
	}
	if (C == 0)
	{
		Piece(TEXT("SM_RL_Pantograph"), FVector(280.0, 0.0, 395.0), FRotator::ZeroRotator);
	}
	if (C + 1 < NumCars)
	{
		Piece(TEXT("SM_RL_Gangway"), FVector(-(CarLength + CarGap) * 50.0, 0.0, 0.0), FRotator::ZeroRotator, false);
	}
	// glass, the lamps and the lit signs on the game's own materials (driven by day and night)
	auto Swap = [&](UStaticMeshComponent* S, const TCHAR* Contains, UMaterialInterface* M)
	{
		if (!S || !M)
		{
			return;
		}
		const TArray<FName> Names = S->GetMaterialSlotNames();
		for (int32 I = 0; I < Names.Num(); I++)
		{
			if (Names[I].ToString().Contains(Contains))
			{
				S->SetMaterial(I, M);
			}
		}
	};
	Swap(Glass, TEXT("glass"), LyudRailMat::Get(this, TEXT("glass"), Mats));
	Swap(Trim, TEXT("glass"), LyudRailMat::Get(this, TEXT("glass"), Mats));
	UMaterialInstanceDynamic* Lamp = LyudRailMat::Glow(this, FLinearColor(1.f, 0.78f, 0.5f), 2.f);
	UMaterialInstanceDynamic* Head = LyudRailMat::Glow(this, FLinearColor(1.f, 0.95f, 0.82f), 0.f);
	UMaterialInstanceDynamic* Tail = LyudRailMat::Glow(this, FLinearColor(1.f, 0.05f, 0.02f), 0.f);
	UMaterialInstanceDynamic* Sign = LyudRailMat::Glow(this, FLinearColor(0.98f, 0.95f, 0.85f), 1.5f);
	Swap(Inside, TEXT("RL_lamp"), Lamp);
	Swap(Trim, TEXT("RL_head"), Head);
	Swap(Trim, TEXT("RL_tail"), Tail);
	Swap(Trim, TEXT("RL_sign"), Sign);
	FCarMats CM;
	CM.Lamp = Lamp;
	CM.Head = Head;
	CM.Tail = Tail;
	CM.Sign = Sign;
	CarMats.Add(CM);
	KeepMats.Append({ Lamp, Head, Tail, Sign });   // the struct above isn't seen by the GC
	// warm light in the car (three lamps' worth), the headlamp's beam at the cab end
	for (const double Lx : { -5.0, 0.0, 5.0 })
	{
		UPointLightComponent* P = NewObject<UPointLightComponent>(this);
		P->SetupAttachment(Root);
		P->SetMobility(EComponentMobility::Movable);
		P->SetLightColor(FLinearColor(1.f, 0.78f, 0.52f));
		P->SetIntensityUnits(ELightUnits::Lumens);
		P->SetIntensity(0.f);
		P->SetAttenuationRadius(650.f);
		P->SetSourceRadius(20.f);
		P->SetCastShadows(false);
		P->LightingChannels.bChannel0 = true;
		P->LightingChannels.bChannel1 = true;
		P->RegisterComponent();
		P->SetRelativeLocation(FVector(Lx * 100.0, 0.0, 330.0));
		AddInstanceComponent(P);
		CabinLights.Add(P);
	}
	USpotLightComponent* Spot = NewObject<USpotLightComponent>(this);
	Spot->SetupAttachment(Root);
	Spot->SetMobility(EComponentMobility::Movable);
	Spot->SetLightColor(FLinearColor(1.f, 0.93f, 0.8f));
	Spot->SetIntensityUnits(ELightUnits::Lumens);
	Spot->SetIntensity(0.f);
	Spot->SetAttenuationRadius(9000.f);
	Spot->SetOuterConeAngle(14.f);
	Spot->SetInnerConeAngle(6.f);
	Spot->SetCastShadows(false);
	Spot->LightingChannels.bChannel1 = true;
	Spot->RegisterComponent();
	// the cab end: +X for car 0, -X for car 1 (turned)
	const double CabX = (C % 2 ? -1.0 : 1.0) * (CarLength * 50.0 + 15.0);
	Spot->SetRelativeLocationAndRotation(FVector(CabX, 0.0, 160.0), FRotator(-3.0, C % 2 ? 180.0 : 0.0, 0.0));
	AddInstanceComponent(Spot);
	HeadLights.Add(Spot);
	// door leaves: two per doorway, both sides (the kit's leaf faces Blender +Y = world -Y)
	const double Hw = 1.40 - 0.03;
	for (const double Dx : { -4.55, 4.55 })
	{
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			FDoor D;
			D.Car = C;
			D.X = Dx * (C % 2 ? -1.0 : 1.0);
			D.Side = Side;
			for (int32 E = -1; E <= 1; E += 2)
			{
				UStaticMeshComponent* Leaf = Piece(TEXT("SM_RL_DoorLeaf"), FVector((D.X + E * 0.30) * 100.0, Side * Hw * 100.0, 115.0),
					FRotator(0.0, Side > 0 ? 180.0 : 0.0, 0.0), false);
				(E < 0 ? D.LeafA : D.LeafB) = Leaf;
			}
			Doors.Add(D);
		}
	}
	// collision: the floor, the walls between the doorways, the ends, the roof (so the player can ride inside)
	auto Box = [&](const FVector& Centre, const FVector& Half)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(this);
		B->SetupAttachment(Root);
		B->SetBoxExtent(Half);
		B->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		B->SetCanEverAffectNavigation(false);
		B->RegisterComponent();
		B->SetRelativeLocation(Centre);
		AddInstanceComponent(B);
		Floors.Add(B);
	};
	const double HalfL = CarLength * 50.0;
	Box(FVector(0.0, 0.0, 105.0), FVector(HalfL, 137.0, 10.0));
	Box(FVector(0.0, 0.0, 420.0), FVector(HalfL, 140.0, 20.0));
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		// wall pieces round the doorways at -4.55 and +4.55 (1.2 m wide)
		const double Edges[] = { -HalfL, -515.0, -395.0, 395.0, 515.0, HalfL };
		for (int32 K = 0; K < 6; K += 2)
		{
			Box(FVector((Edges[K] + Edges[K + 1]) * 0.5, Side * 137.0, 260.0), FVector((Edges[K + 1] - Edges[K]) * 0.5, 4.0, 140.0));
		}
	}
	for (int32 E = -1; E <= 1; E += 2)
	{
		Box(FVector(E * (HalfL - 4.0), 0.0, 260.0), FVector(4.0, 140.0, 140.0));
	}
}

FTransform ALyudTrain::CarFrame(double S) const
{
	// a car's middle at S along the line, pitched to the rail heads under its two bogies
	const FVector A = Railway->LineToWorld(S - BogieX, 0.0, 0.0), B = Railway->LineToWorld(S + BogieX, 0.0, 0.0);
	const FVector Mid = (A + B) * 0.5;
	return FTransform(FRotationMatrix::MakeFromXZ(B - A, FVector::UpVector).Rotator(), Mid);
}

void ALyudTrain::Place(const FLyudTrainInfo& InInfo, float DeltaSeconds)
{
	const FLyudTrainInfo Prev = Info;
	Info = InInfo;
	const bool bAboard = Rider.IsValid();
	const bool bShow = Info.bVisible || bAboard;
	if (bShow != bWasVisible)
	{
		SetActorHiddenInGame(!bShow);
		for (UBoxComponent* B : Floors)
		{
			B->SetCollisionEnabled(bShow ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		}
		bWasVisible = bShow;
	}
	// the cars along the line (car 0 leads outbound)
	TArray<FTransform> Old;
	for (int32 C = 0; C < Cars.Num(); C++)
	{
		Old.Add(Cars[C].Root->GetComponentTransform());
		const double Off = (double(Cars.Num() - 1) * 0.5 - C) * (CarLength + CarGap);
		Cars[C].S = Info.S + Off;
		const FTransform T = CarFrame(Cars[C].S);
		Cars[C].Root->SetWorldTransform(T);
	}
	if (!bShow)
	{
		LastS = Info.S;
		return;
	}
	CarryPlayer(Old);
	UpdateDoors(DeltaSeconds);
	UpdateLights();
	UpdateSound(DeltaSeconds, Prev);
	// the wheels in the water (wake and spray)
	if (Wake)
	{
		TArray<FVector> Pts;
		const int32 Lead = Info.Dir >= 0 ? 0 : Cars.Num() - 1;
		for (int32 K = 0; K < Cars.Num(); K++)
		{
			const int32 C = Info.Dir >= 0 ? K : Cars.Num() - 1 - K;
			for (const double Bx : { BogieX, -BogieX })
			{
				for (const double Ax : { AxleX, -AxleX })
				{
					Pts.Add(Railway->LineToWorld(Cars[C].S + Info.Dir * (Bx + Ax), 0.0, 0.0));
				}
			}
		}
		(void)Lead;
		Wake->Feed(Pts, Railway->LineDir(Info.S) * Info.Speed * 100.0, DeltaSeconds);
	}
	UpdateGulls(DeltaSeconds);
	LastS = Info.S;
}

void ALyudTrain::CarryPlayer(const TArray<FTransform>& Old)
{
	// the player standing in a car goes with it (CharacterMovement doesn't ride a moved component's floor reliably)
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pawn)
	{
		return;
	}
	const FVector P = Pawn->GetActorLocation();
	int32 InCar = -1;
	for (int32 C = 0; C < Cars.Num() && C < Old.Num(); C++)
	{
		const FVector L = Old[C].InverseTransformPosition(P);
		if (FMath::Abs(L.X) < CarLength * 50.0 - 5.0 && FMath::Abs(L.Y) < 135.0 && L.Z > 100.0 && L.Z < 400.0)
		{
			InCar = C;
			break;
		}
	}
	if (InCar < 0)
	{
		Rider.Reset();
		return;
	}
	Rider = Pawn;
	const FTransform& Now = Cars[InCar].Root->GetComponentTransform();
	const FVector Local = Old[InCar].InverseTransformPosition(P);
	const FVector NewP = Now.TransformPosition(Local);
	if (!NewP.Equals(P, 0.01))
	{
		Pawn->SetActorLocation(NewP, false, nullptr, ETeleportType::None);
	}
}

void ALyudTrain::UpdateDoors(float DeltaSeconds)
{
	const float Target = Info.bDoorsOpen ? 1.f : 0.f;
	const float Was = DoorOpen;
	DoorOpen = FMath::FInterpConstantTo(DoorOpen, Target, DeltaSeconds, 1.f / 1.6f);
	if (Was == DoorOpen && !bDoorsInit)
	{
		return;
	}
	bDoorsInit = false;
	if ((Was == 0.f && DoorOpen > 0.f) || (Was == 1.f && DoorOpen < 1.f))
	{
		const bool bOpening = DoorOpen > Was;
		if (USoundBase* S = Sounds.FindRef(bOpening ? TEXT("door_01") : TEXT("door_02")))
		{
			for (int32 C = 0; C < Cars.Num(); C++)
			{
				UGameplayStatics::PlaySoundAtLocation(this, S, Cars[C].Root->GetComponentLocation() + FVector(0, -100.0, 200.0), 0.7f);
			}
		}
	}
	const float E = DoorOpen * DoorOpen * (3.f - 2.f * DoorOpen);
	// the halts' platforms are on the line's south side (world -Y): only those leaves open
	for (const FDoor& D : Doors)
	{
		const float Open = D.Side < 0 ? E : 0.f;
		const double Hw = 1.40 - 0.03;
		if (UStaticMeshComponent* A = D.LeafA.Get())
		{
			A->SetRelativeLocation(FVector((D.X - 0.30 - 0.58 * Open) * 100.0, D.Side * Hw * 100.0, 115.0));
		}
		if (UStaticMeshComponent* B = D.LeafB.Get())
		{
			B->SetRelativeLocation(FVector((D.X + 0.30 + 0.58 * Open) * 100.0, D.Side * Hw * 100.0, 115.0));
		}
	}
	// the doorway's collision: the side wall boxes leave the openings free already; shut leaves block nothing yet
}

void ALyudTrain::UpdateLights()
{
	if (!Sky.IsValid())
	{
		TActorIterator<ALyudSky> It(GetWorld());
		if (It)
		{
			Sky = *It;
		}
	}
	const ALyudSky* S = Cast<ALyudSky>(Sky.Get());
	const float Alt = S ? S->SunAltitude() : 20.f;
	const float Dark = FMath::Clamp((4.f - Alt) / 8.f, 0.f, 1.f);       // 0 day .. 1 night
	const float Dusk = FMath::Clamp((12.f - Alt) / 14.f, 0.f, 1.f);
	for (UPointLightComponent* L : CabinLights)
	{
		L->SetIntensity(150.f + 520.f * Dusk + 600.f * Dark);
	}
	const int32 LeadCar = Info.Dir >= 0 ? 0 : Cars.Num() - 1;
	for (int32 C = 0; C < CarMats.Num(); C++)
	{
		const bool bLead = C == LeadCar;
		const FCarMats& M = CarMats[C];
		if (M.Lamp)
		{
			// at night the lamps must read through the windows from out on the water
			M.Lamp->SetScalarParameterValue(TEXT("Intensity"), 1.f + 3.f * Dusk + 9.f * Dark);
		}
		if (M.Head)
		{
			M.Head->SetScalarParameterValue(TEXT("Intensity"), bLead ? 8.f + 12.f * Dark : 0.f);
		}
		if (M.Tail)
		{
			M.Tail->SetScalarParameterValue(TEXT("Intensity"), bLead ? 0.f : 18.f);
		}
		if (M.Sign)
		{
			M.Sign->SetScalarParameterValue(TEXT("Intensity"), 1.f + 5.f * Dusk);
		}
		if (HeadLights.IsValidIndex(C))
		{
			HeadLights[C]->SetIntensity(bLead ? 5000.f * Dusk : 0.f);
		}
	}
}

void ALyudTrain::UpdateSound(float DeltaSeconds, const FLyudTrainInfo& Prev)
{
	const float Speed = FMath::Abs(Info.Speed);
	if (Motor)
	{
		if (!Motor->IsPlaying() && Info.bInService)
		{
			Motor->Play();
		}
		Motor->SetPitchMultiplier(0.45f + Speed / 16.f * 1.25f);
		Motor->SetVolumeMultiplier(Info.bInService ? 0.12f + 0.5f * FMath::Min(1.f, Speed / 8.f) : 0.f);
	}
	if (Wheels)
	{
		if (!Wheels->IsPlaying() && Info.bInService)
		{
			Wheels->Play();
		}
		Wheels->SetPitchMultiplier(0.7f + Speed / 16.f * 0.5f);
		Wheels->SetVolumeMultiplier(FMath::Min(1.f, Speed / 10.f) * 0.8f);
	}
	// ta-tan: each bogie over a rail joint
	if (Speed > 0.5f)
	{
		static const TCHAR* Joints[] = { TEXT("joint_01"), TEXT("joint_02"), TEXT("joint_03") };
		for (int32 C = 0; C < Cars.Num(); C++)
		{
			for (const double Bx : { -BogieX, BogieX })
			{
				const double Now = Cars[C].S + Bx, Before = Now - (Info.S - LastS);
				if (FMath::FloorToDouble(Now / JointM) != FMath::FloorToDouble(Before / JointM))
				{
					if (USoundBase* S = Sounds.FindRef(Joints[(C * 2 + (Bx > 0 ? 1 : 0)) % 3]))
					{
						UGameplayStatics::PlaySoundAtLocation(this, S, Railway->LineToWorld(Now, 0.0, 0.3), FMath::Clamp(Speed / 12.f, 0.25f, 1.f),
							0.9f + Speed / 60.f);
					}
				}
			}
		}
	}
	// the horn: coming out of the tunnel, and leaving a halt
	const bool bAppear = Info.bVisible && !Prev.bVisible && Info.Dir > 0 && Info.S < 400.0;
	const bool bLeave = Prev.Stop >= 0 && Info.Stop < 0;
	if ((bAppear || bLeave) && Sounds.FindRef(TEXT("horn_01")))
	{
		const int32 Lead = Info.Dir >= 0 ? 0 : Cars.Num() - 1;
		UGameplayStatics::PlaySoundAtLocation(this, Sounds.FindRef(TEXT("horn_01")), Cars[Lead].Root->GetComponentLocation() + FVector(0, 0, 350.0), 1.f);
	}
}

void ALyudTrain::UpdateGulls(float DeltaSeconds)
{
	// a few gulls follow the train over the sea by day, calling, as gulls follow a ferry; near the camera only
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector CamP = Cam ? Cam->GetCameraLocation() : FVector::ZeroVector;
	const int32 Tail = Info.Dir >= 0 ? Cars.Num() - 1 : 0;
	const FVector Rear = Cars[Tail].Root->GetComponentLocation();
	const ALyudSky* S = Cast<ALyudSky>(Sky.Get());
	const bool bDay = !S || S->SunAltitude() > -2.f;
	const bool bWant = bDay && Info.bInService && FMath::Abs(Info.Speed) > 2.f && Wake && Wake->GetWetness() > 0.05f
		&& FVector::Dist(Rear, CamP) < 60000.f;
	if (!bWant)
	{
		GullAway = FMath::Min(GullAway + DeltaSeconds, 30.f);
	}
	else
	{
		GullAway = 0.f;
	}
	if (bWant && Gulls.Num() == 0)
	{
		static const FName Species[] = { TEXT("HerringGull"), TEXT("BlackHeadedGull"), TEXT("HerringGull"), TEXT("CommonGull"), TEXT("BlackHeadedGull") };
		for (int32 I = 0; I < 5; I++)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			P.bDeferConstruction = true;
			const FVector At = Rear + FVector(FMath::FRandRange(-3000.f, 3000.f), FMath::FRandRange(-2500.f, 2500.f), FMath::FRandRange(1500.f, 2500.f));
			ABirdActor* B = GetWorld()->SpawnActor<ABirdActor>(ABirdActor::StaticClass(), FTransform(At), P);
			if (B)
			{
				B->Species = Species[I];
				B->InitialState = EBirdState::Glide;
				B->Seed = 7000 + I;
				B->FinishSpawning(FTransform(At));
				FGull G;
				G.Bird = B;
				G.Phase = I * 1.3f;
				G.Vel = FVector::ZeroVector;
				G.NextCall = FMath::FRandRange(2.f, 9.f);
				G.Offset = FVector(FMath::FRandRange(-900.f, 300.f), FMath::FRandRange(-1400.f, 1400.f), FMath::FRandRange(500.f, 1500.f));
				Gulls.Add(G);
			}
		}
	}
	if (Gulls.Num() == 0)
	{
		return;
	}
	const FVector Fwd = Railway->LineDir(Info.S) * (Info.Dir >= 0 ? 1.0 : -1.0);
	const FVector TrainV = Railway->LineDir(Info.S) * Info.Speed * 100.0;
	const float T = GetWorld()->GetTimeSeconds();
	bool bAllGone = true;
	for (int32 I = 0; I < Gulls.Num(); I++)
	{
		FGull& G = Gulls[I];
		ABirdActor* B = G.Bird.Get();
		if (!B)
		{
			continue;
		}
		FVector Target;
		if (GullAway > 0.f)
		{
			// they peel off and climb away, then go
			Target = B->GetActorLocation() + FVector(-Fwd.Y, Fwd.X, 0.0) * (I % 2 ? 4000.0 : -4000.0) + FVector(0, 0, 2500.0);
		}
		else
		{
			// hanging over the last car and its wake: slow figure-eights round a spot behind and above it
			const FVector Side(-Fwd.Y, Fwd.X, 0.0);
			Target = Rear - Fwd * (800.0 - G.Offset.X) + Side * G.Offset.Y + FVector(0, 0, G.Offset.Z)
				+ Side * 600.0 * FMath::Sin(T * 0.35f + G.Phase) + Fwd * 400.0 * FMath::Sin(T * 0.7f + G.Phase * 2.f)
				+ FVector(0, 0, 200.0 * FMath::Sin(T * 0.5f + G.Phase));
		}
		const FVector P = B->GetActorLocation();
		const FVector Want = TrainV * (GullAway > 0.f ? 0.3 : 1.0) + (Target - P) * 0.6;
		G.Vel = FMath::VInterpTo(G.Vel, Want.GetClampedToMaxSize(2600.0), DeltaSeconds, 1.5f);
		const FVector NewP = P + G.Vel * DeltaSeconds;
		const FVector Rel = G.Vel - TrainV * 0.0;
		const float Bank = FMath::Clamp(float(FVector::DotProduct((Want - G.Vel), FVector(-Rel.Y, Rel.X, 0.0).GetSafeNormal())) * 0.02f, -35.f, 35.f);
		B->SetActorLocationAndRotation(NewP, FRotator(FMath::Clamp(float(-G.Vel.Z * 0.02f), -20.f, 20.f), G.Vel.Rotation().Yaw, Bank));
		B->SetVelocity(G.Vel);
		// glide most of the time, a few wingbeats now and then
		const bool bFlap = FMath::Frac(T * 0.08f + G.Phase * 0.2f) < 0.22f || G.Vel.Z > 120.f;
		if (bFlap != G.bFlap)
		{
			B->SetState(bFlap ? EBirdState::Fly : EBirdState::Glide, 0.4f);
			G.bFlap = bFlap;
		}
		G.NextCall -= DeltaSeconds;
		if (G.NextCall <= 0.f && GullAway <= 0.f)
		{
			G.NextCall = FMath::FRandRange(4.f, 14.f);
			const FString Sp = B->Species.ToString();
			const TCHAR* Call = Sp == TEXT("HerringGull") ? TEXT("kyow") : (Sp == TEXT("BlackHeadedGull") ? TEXT("kree") : TEXT("kee_ya"));
			const FString Path = FString::Printf(TEXT("/Game/Birds/%s/Sounds/S_%s_%s.S_%s_%s"), *Sp, *Sp, Call, *Sp, Call);
			if (USoundBase* Snd = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				GullSounds.AddUnique(Snd);
				UGameplayStatics::PlaySoundAtLocation(this, Snd, NewP, 1.f, FMath::FRandRange(0.92f, 1.08f));
			}
		}
		if (GullAway < 20.f)
		{
			bAllGone = false;
		}
	}
	if (bAllGone)
	{
		for (FGull& G : Gulls)
		{
			if (ABirdActor* B = G.Bird.Get())
			{
				B->Destroy();
			}
		}
		Gulls.Reset();
	}
}

void ALyudTrain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

bool ALyudTrain::GetSeat(int32 Seat, FTransform& Out) const
{
	if (!Seats.IsValidIndex(Seat) || !Cars.IsValidIndex(Seats[Seat].Car) || !Cars[Seats[Seat].Car].Root)
	{
		return false;
	}
	Out = Seats[Seat].Local * Cars[Seats[Seat].Car].Root->GetComponentTransform();
	return true;
}

int32 ALyudTrain::GetSeatCar(int32 Seat) const
{
	return Seats.IsValidIndex(Seat) ? Seats[Seat].Car : -1;
}

FVector ALyudTrain::GetDoorPoint(int32 Door) const
{
	if (!Doors.IsValidIndex(Door) || !Cars.IsValidIndex(Doors[Door].Car))
	{
		return FVector::ZeroVector;
	}
	const FDoor& D = Doors[Door];
	return Cars[D.Car].Root->GetComponentTransform().TransformPosition(FVector(D.X * 100.0, D.Side * 145.0, 115.0));
}

bool ALyudTrain::IsInside(const FVector& Location) const
{
	for (const FCar& C : Cars)
	{
		const FVector L = C.Root->GetComponentTransform().InverseTransformPosition(Location);
		if (FMath::Abs(L.X) < CarLength * 50.0 && FMath::Abs(L.Y) < 140.0 && L.Z > 90.0 && L.Z < 420.0)
		{
			return true;
		}
	}
	return false;
}
