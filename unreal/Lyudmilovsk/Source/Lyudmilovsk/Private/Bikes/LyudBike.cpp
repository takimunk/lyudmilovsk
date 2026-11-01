#include "Bikes/LyudBike.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
	FVector BikeJsonVec(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FVector& Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, A) && A->Num() >= 3)
		{
			return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		}
		return Default;
	}

	UStaticMesh* BikeMesh(const TCHAR* Name)
	{
		// unreal/scripts/import_bikes.py puts them here (Interchange's own folder as a fallback)
		for (const TCHAR* Dir : { TEXT("/Game/Bikes/Meshes"), TEXT("/Game/Bikes/Meshes/bike_kit/StaticMeshes"), TEXT("/Game/Bikes/Meshes/bike_kit") })
		{
			if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), Dir, Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return M;
			}
		}
		return nullptr;
	}

	USoundBase* BikeSound(const TCHAR* Name)
	{
		return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/Bikes/S_%s.S_%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** A value through keys (time, value), eased between neighbours. */
	FVector BikeKeys(float T, std::initializer_list<TPair<float, FVector>> Keys)
	{
		const TPair<float, FVector>* Prev = nullptr;
		for (const TPair<float, FVector>& K : Keys)
		{
			if (T <= K.Key)
			{
				if (!Prev)
				{
					return K.Value;
				}
				const float A = FMath::SmoothStep(0.f, 1.f, (T - Prev->Key) / FMath::Max(1e-4f, K.Key - Prev->Key));
				return FMath::Lerp(Prev->Value, K.Value, A);
			}
			Prev = &K;
		}
		return Prev ? Prev->Value : FVector::ZeroVector;
	}

	float BikeKeysF(float T, std::initializer_list<TPair<float, float>> Keys)
	{
		const TPair<float, float>* Prev = nullptr;
		for (const TPair<float, float>& K : Keys)
		{
			if (T <= K.Key)
			{
				if (!Prev)
				{
					return K.Value;
				}
				const float A = FMath::SmoothStep(0.f, 1.f, (T - Prev->Key) / FMath::Max(1e-4f, K.Key - Prev->Key));
				return FMath::Lerp(Prev->Value, K.Value, A);
			}
			Prev = &K;
		}
		return Prev ? Prev->Value : 0.f;
	}

	/** The gear the cranks turn the rear wheel by (42 x 17: a calm town gear), wheel turns per crank turn. */
	constexpr float GearRatio = 42.f / 17.f;
}

const FLyudBikeGeom& FLyudBikeGeom::Get()
{
	static FLyudBikeGeom G = []()
	{
		FLyudBikeGeom Out;
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Bikes/bike.json"))))
		{
			UE_LOG(LogTemp, Warning, TEXT("Bikes: Content/Bikes/bike.json missing (blender/bikes/build_bike.py); built-in measures"));
			return Out;
		}
		TSharedPtr<FJsonObject> J;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Bikes: bike.json unreadable"));
			return Out;
		}
		Out.bFromFile = true;
		Out.AxleF = BikeJsonVec(J, TEXT("axle_front"), Out.AxleF);
		Out.AxleR = BikeJsonVec(J, TEXT("axle_rear"), Out.AxleR);
		Out.BB = BikeJsonVec(J, TEXT("bb"), Out.BB);
		Out.WheelRadius = J->GetNumberField(TEXT("wheel_radius"));
		Out.CrankLength = J->GetNumberField(TEXT("crank_length"));
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (J->TryGetObjectField(TEXT("pedal_l"), O))
		{
			Out.PedalY[0] = (*O)->GetNumberField(TEXT("y"));
		}
		if (J->TryGetObjectField(TEXT("pedal_r"), O))
		{
			Out.PedalY[1] = (*O)->GetNumberField(TEXT("y"));
		}
		if (J->TryGetObjectField(TEXT("steer_axis"), O))
		{
			Out.SteerPoint = BikeJsonVec(*O, TEXT("point"), Out.SteerPoint);
			Out.SteerDir = BikeJsonVec(*O, TEXT("dir"), Out.SteerDir).GetSafeNormal();
		}
		Out.SaddleTop = BikeJsonVec(J, TEXT("saddle_top"), Out.SaddleTop);
		if (J->TryGetObjectField(TEXT("hoods"), O))
		{
			Out.Hoods[0] = BikeJsonVec(*O, TEXT("l"), Out.Hoods[0]);
			Out.Hoods[1] = BikeJsonVec(*O, TEXT("r"), Out.Hoods[1]);
			Out.HoodAxis[0] = BikeJsonVec(*O, TEXT("grip_axis_l"), Out.HoodAxis[0]).GetSafeNormal();
			Out.HoodAxis[1] = BikeJsonVec(*O, TEXT("grip_axis_r"), Out.HoodAxis[1]).GetSafeNormal();
		}
		J->TryGetNumberField(TEXT("hood_radius"), Out.HoodRadius);
		if (J->TryGetObjectField(TEXT("bar_tops"), O))
		{
			Out.BarTops[0] = BikeJsonVec(*O, TEXT("l"), Out.BarTops[0]);
			Out.BarTops[1] = BikeJsonVec(*O, TEXT("r"), Out.BarTops[1]);
			(*O)->TryGetNumberField(TEXT("radius"), Out.BarRadius);
		}
		if (J->TryGetObjectField(TEXT("stand"), O))
		{
			Out.StandHinge = BikeJsonVec(*O, TEXT("hinge"), Out.StandHinge);
			Out.StandAxis = BikeJsonVec(*O, TEXT("axis"), Out.StandAxis).GetSafeNormal();
			(*O)->TryGetNumberField(TEXT("up_angle_deg"), Out.StandUpDeg);
			(*O)->TryGetNumberField(TEXT("park_roll_deg"), Out.ParkRoll);
		}
		if (J->TryGetObjectField(TEXT("parts"), O))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*O)->Values)
			{
				Out.Pivots.Add(FName(*P.Key), BikeJsonVec(P.Value->AsObject(), TEXT("pivot"), FVector::ZeroVector));
			}
		}
		if (J->TryGetObjectField(TEXT("chain"), O))
		{
			(*O)->TryGetNumberField(TEXT("links"), Out.ChainLinks);
		}
		if (J->TryGetObjectField(TEXT("bounds"), O))
		{
			Out.Bounds = FBox(BikeJsonVec(*O, TEXT("min"), Out.Bounds.Min), BikeJsonVec(*O, TEXT("max"), Out.Bounds.Max));
		}
		return Out;
	}();
	return G;
}

ALyudBike::ALyudBike()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
	Lean = CreateDefaultSubobject<USceneComponent>(TEXT("Lean"));
	Lean->SetupAttachment(Root);
	auto Part = [&](const TCHAR* Name, USceneComponent* Parent)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->CanCharacterStepUpOn = ECB_No;
		// small and moving: no part in Lumen's scene or the distance fields (docs/benchmark.md, the perf checklist)
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		C->SetCachedMaxDrawDistance(9000.f);
		return C;
	};
	Frame = Part(TEXT("Frame"), Lean);
	SteerPart = Part(TEXT("Steer"), Lean);
	WheelF = Part(TEXT("WheelF"), SteerPart);
	WheelR = Part(TEXT("WheelR"), Lean);
	CrankPart = Part(TEXT("Crank"), Lean);
	PedalL = Part(TEXT("PedalL"), CrankPart);
	PedalR = Part(TEXT("PedalR"), CrankPart);
	Chain = Part(TEXT("Chain"), Lean);
	Stand = Part(TEXT("Stand"), Lean);
	for (UStaticMeshComponent* C : { WheelF.Get(), WheelR.Get(), CrankPart.Get(), PedalL.Get(), PedalR.Get(), Chain.Get(), Stand.Get() })
	{
		C->SetCachedMaxDrawDistance(6000.f);
	}
	Hit = CreateDefaultSubobject<UBoxComponent>(TEXT("Hit"));
	Hit->SetupAttachment(Lean);
	Hit->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hit->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hit->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Hit->SetGenerateOverlapEvents(false);
	Hit->SetCanEverAffectNavigation(false);
}

void ALyudBike::Setup(FName Id, const FLinearColor& InPaint)
{
	BikeId = Id;
	Paint = InPaint;
	const FLyudBikeGeom& G = Geom();
	struct FPartDef { UStaticMeshComponent* C; const TCHAR* Mesh; };
	const FPartDef Parts[] = {
		{ Frame, TEXT("SM_Bike_Frame") }, { SteerPart, TEXT("SM_Bike_Steer") }, { WheelF, TEXT("SM_Bike_WheelF") }, { WheelR, TEXT("SM_Bike_WheelR") },
		{ CrankPart, TEXT("SM_Bike_Crank") }, { PedalL, TEXT("SM_Bike_PedalL") }, { PedalR, TEXT("SM_Bike_PedalR") }, { Chain, TEXT("SM_Bike_Chain") },
		{ Stand, TEXT("SM_Bike_Stand") } };
	int32 Missing = 0;
	for (const FPartDef& P : Parts)
	{
		UStaticMesh* M = BikeMesh(P.Mesh);
		Missing += M ? 0 : 1;
		P.C->SetStaticMesh(M);
	}
	if (Missing)
	{
		UE_LOG(LogTemp, Warning, TEXT("Bikes: %d of the bike's meshes missing (unreal/scripts/import_bikes.sh)"), Missing);
	}
	// each part hangs where its pivot is: the front wheel on the steering, the pedals on the cranks
	const FVector SteerPivot = G.Pivot(TEXT("SM_Bike_Steer"));
	Frame->SetRelativeLocation(G.Pivot(TEXT("SM_Bike_Frame")));
	SteerPart->SetRelativeLocation(SteerPivot);
	WheelF->SetRelativeLocation(G.AxleF - SteerPivot);
	WheelR->SetRelativeLocation(G.AxleR);
	CrankPart->SetRelativeLocation(G.BB);
	PedalR->SetRelativeLocation(FVector(G.CrankLength, G.PedalY[1] - G.BB.Y, 0.f));
	PedalL->SetRelativeLocation(FVector(-G.CrankLength, G.PedalY[0] - G.BB.Y, 0.f));
	Chain->SetRelativeLocation(G.Pivot(TEXT("SM_Bike_Chain")));
	Stand->SetRelativeLocation(G.StandHinge);
	Hit->SetRelativeLocation(G.Bounds.GetCenter());
	Hit->SetBoxExtent(G.Bounds.GetExtent());

	// the paint: the frame's own material with this bike's colour (glTF's material instance takes BaseColorFactor)
	for (UStaticMeshComponent* C : { Frame.Get(), SteerPart.Get(), Stand.Get() })
	{
		for (int32 I = 0; C && C->GetStaticMesh() && I < C->GetNumMaterials(); I++)
		{
			const FName Slot = C->GetStaticMesh()->GetStaticMaterials().IsValidIndex(I) ? C->GetStaticMesh()->GetStaticMaterials()[I].MaterialSlotName : NAME_None;
			if (Slot.ToString().Contains(TEXT("Paint")))
			{
				if (!PaintMID)
				{
					PaintMID = UMaterialInstanceDynamic::Create(C->GetMaterial(I), this);
					PaintMID->SetVectorParameterValue(TEXT("BaseColorFactor"), Paint);
					PaintMID->SetVectorParameterValue(TEXT("BaseColor"), Paint);
				}
				C->SetMaterial(I, PaintMID);
			}
		}
	}
	if (Chain->GetStaticMesh() && Chain->GetNumMaterials() > 0 && Chain->GetMaterial(0))
	{
		ChainMID = UMaterialInstanceDynamic::Create(Chain->GetMaterial(0), this);
		Chain->SetMaterial(0, ChainMID);
	}
	bBuilt = true;
	PoseParts();
}

void ALyudBike::Park(const FVector& Ground, float Yaw, float InLean)
{
	ParkedAt = Ground;
	ParkedYaw = Yaw;
	ParkedLean = InLean;
	Speed = 0.f;
	Steer = InLean == 0.f ? -12.f : 0.f;      // on its stand the bars fall a little to the stand's side
	Place(Ground, Yaw, 0.f, InLean != 0.f ? InLean : -Geom().ParkRoll, InLean != 0.f ? 0.f : 1.f);
	Hush();
}

void ALyudBike::Place(const FVector& Ground, float Yaw, float Pitch, float RollDeg, float StandDown)
{
	Root->SetWorldLocationAndRotation(Ground, FRotator(0.f, Yaw, 0.f));
	// nose up: about -Y; leaning right (its right side down): about -X
	Lean->SetRelativeRotation(FQuat(FVector::YAxisVector, -FMath::DegreesToRadians(Pitch)) * FQuat(FVector::XAxisVector, -FMath::DegreesToRadians(RollDeg)));
	StandDownNow = FMath::Clamp(StandDown, 0.f, 1.f);
	PoseParts();
}

void ALyudBike::PoseParts()
{
	const FLyudBikeGeom& G = Geom();
	SteerPart->SetRelativeRotation(FQuat(G.SteerDir, FMath::DegreesToRadians(Steer)));
	// rolling forward a wheel turns about +Y; the cranks too, the pedals back again so they stay level
	WheelF->SetRelativeRotation(FQuat(FVector::YAxisVector, WheelAngle[0]));
	WheelR->SetRelativeRotation(FQuat(FVector::YAxisVector, WheelAngle[1]));
	const float C = FMath::DegreesToRadians(Crank);
	CrankPart->SetRelativeRotation(FQuat(FVector::YAxisVector, C));
	PedalL->SetRelativeRotation(FQuat(FVector::YAxisVector, -C));
	PedalR->SetRelativeRotation(FQuat(FVector::YAxisVector, -C));
	Stand->SetRelativeRotation(FQuat(G.StandAxis, FMath::DegreesToRadians(G.StandUpDeg * (1.f - StandDownNow))));
	if (ChainMID)
	{
		ChainMID->SetScalarParameterValue(TEXT("Scroll"), ChainScroll);
	}
}

void ALyudBike::Roll(float DeltaSeconds, float InSpeed, float SteerDeg, bool bPedal, float CrankTo, bool bLoose, bool bBraking)
{
	const FLyudBikeGeom& G = Geom();
	Speed = InSpeed;
	Steer = SteerDeg;
	const float Turn = Speed * DeltaSeconds / FMath::Max(1.f, G.WheelRadius);       // rad
	WheelAngle[0] = FMath::Fmod(WheelAngle[0] + Turn, 2.f * PI);
	WheelAngle[1] = FMath::Fmod(WheelAngle[1] + Turn, 2.f * PI);
	if (bPedal)
	{
		const float Step = FMath::RadiansToDegrees(Turn) / GearRatio;
		Crank = FMath::Fmod(Crank + Step, 360.f);
		// the chain runs round with the chainring (42 teeth a turn, one link a tooth)
		ChainScroll = FMath::Fmod(ChainScroll + Step / 360.f * 42.f, 1000.f);
	}
	else if (CrankTo >= 0.f)
	{
		const float D = FMath::FindDeltaAngleDegrees(Crank, CrankTo);
		Crank += FMath::Clamp(D, -200.f * DeltaSeconds, 200.f * DeltaSeconds);
	}
	PoseParts();

	// sounds: the tyres on the ground, the freewheel's pawls ticking when the cranks stand, the chain when they turn
	if (!TyreSound)
	{
		MakeSounds();
	}
	const float Mps = FMath::Abs(Speed) / 100.f;
	if (TyreSound)
	{
		if (bLoose != bLooseNow || !TyreSound->IsPlaying())
		{
			bLooseNow = bLoose;
			TyreSound->SetSound(bLoose ? TyreGravel : TyreAsphalt);
			if (Mps > 0.2f)
			{
				TyreSound->Play(FMath::FRand() * 2.f);
			}
		}
		TyreSound->SetVolumeMultiplier(FMath::Clamp(Mps / 7.f, 0.f, 1.f) * (bLoose ? 0.9f : 0.6f) + 0.001f);
		TyreSound->SetPitchMultiplier(0.8f + FMath::Clamp(Mps / 12.f, 0.f, 0.6f));
	}
	if (FreeSound)
	{
		const bool bTick = !bPedal && Mps > 0.4f;
		if (bTick && !FreeSound->IsPlaying())
		{
			FreeSound->Play();
		}
		else if (!bTick && FreeSound->IsPlaying())
		{
			FreeSound->FadeOut(0.15f, 0.f);
		}
		// the loop was made at a wheel turning 1.5 times a second
		FreeSound->SetPitchMultiplier(FMath::Clamp(Mps / (2.f * PI * G.WheelRadius / 100.f) / 1.5f, 0.35f, 2.5f));
		FreeSound->SetVolumeMultiplier(0.45f);
	}
	if (ChainSound)
	{
		const bool bRun = bPedal && Mps > 0.4f;
		if (bRun && !ChainSound->IsPlaying())
		{
			ChainSound->Play(FMath::FRand());
		}
		else if (!bRun && ChainSound->IsPlaying())
		{
			ChainSound->FadeOut(0.2f, 0.f);
		}
		ChainSound->SetPitchMultiplier(FMath::Clamp(0.7f + Mps / 12.f, 0.7f, 1.4f));
		ChainSound->SetVolumeMultiplier(FMath::Clamp(Mps / 6.f, 0.1f, 0.6f));
	}
	BrakeCooldown -= DeltaSeconds;
	if (bBraking && Mps > 2.f && BrakeCooldown <= 0.f && BrakeSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BrakeSound, WheelR->GetComponentLocation(), FMath::Clamp(Mps / 8.f, 0.3f, 0.8f), FMath::FRandRange(0.94f, 1.06f), 0.f, Atten);
		BrakeCooldown = 2.5f;
	}
}

void ALyudBike::MakeSounds()
{
	TyreAsphalt = BikeSound(TEXT("tyre_asphalt_01"));
	TyreGravel = BikeSound(TEXT("tyre_gravel_01"));
	BrakeSound = BikeSound(TEXT("brake_01"));
	StandSound = BikeSound(TEXT("stand_01"));
	// a bike is heard close by only: full within 3 m, gone by ~35 m
	Atten = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& A = Atten->Attenuation;
	A.bAttenuate = true;
	A.bSpatialize = true;
	A.AttenuationShape = EAttenuationShape::Sphere;
	A.AttenuationShapeExtents = FVector(300.f, 0.f, 0.f);
	A.FalloffDistance = 3200.f;
	A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	auto Loop = [&](USoundBase* Sound, const TCHAR* Name, USceneComponent* At)
	{
		if (!Sound)
		{
			return (UAudioComponent*)nullptr;
		}
		UAudioComponent* C = NewObject<UAudioComponent>(this, Name);
		C->SetFlags(RF_DuplicateTransient);
		C->SetSound(Sound);
		C->bAutoActivate = false;
		C->bOverrideAttenuation = false;
		C->AttenuationSettings = Atten;
		C->SetupAttachment(At);
		C->RegisterComponent();
		return C;
	};
	TyreSound = Loop(TyreAsphalt, TEXT("TyreSound"), WheelR);
	FreeSound = Loop(BikeSound(TEXT("freewheel_01")), TEXT("FreeSound"), WheelR);
	ChainSound = Loop(BikeSound(TEXT("chain_01")), TEXT("ChainSound"), CrankPart);
	if (!TyreSound)
	{
		UE_LOG(LogTemp, Warning, TEXT("Bikes: sounds missing (/Game/Audio/Bikes: tools/bikesim/bike_sounds.py, import_bikes.sh)"));
	}
}

void ALyudBike::Hush()
{
	for (UAudioComponent* A : { TyreSound.Get(), FreeSound.Get(), ChainSound.Get() })
	{
		if (A && A->IsPlaying())
		{
			A->Stop();
		}
	}
}

FTransform ALyudBike::GroundFrame() const
{
	return Lean->GetComponentTransform();
}

void ALyudBike::RiderPose(const USkeletalMeshComponent* Mesh, const FLyudBikeRide& Ride, FBikeRiderPose& Out) const
{
	Out = FBikeRiderPose();
	if (!Mesh || Ride.Act == ELyudBikeAct::None)
	{
		return;
	}
	const FLyudBikeGeom& G = Geom();
	const FTransform B = Lean->GetComponentTransform();
	const FTransform M = Mesh->GetComponentTransform();
	// the bike's local cm -> the body's component space
	auto P = [&](const FVector& Local) { return M.InverseTransformPosition(B.TransformPosition(Local)); };
	auto W = [&](const FVector& World) { return M.InverseTransformPosition(World); };
	auto V = [&](const FVector& Local) { return M.InverseTransformVectorNoScale(B.TransformVectorNoScale(Local)).GetSafeNormal(); };
	Out.Fwd = V(FVector::XAxisVector);
	Out.Up = V(FVector::ZAxisVector);
	Out.Left = V(-FVector::YAxisVector);
	Out.Weight = FMath::Clamp(Ride.Weight, 0.f, 1.f);
	const float S = Ride.Scale;

	// the hands on the bar's tops either side of the stem (as the bars are turned now): palms on top, fingers round the front
	const FTransform SteerW = SteerPart->GetComponentTransform();
	const FVector SteerPivot = G.Pivot(TEXT("SM_Bike_Steer"));
	const FVector BarAxis = M.InverseTransformVectorNoScale(SteerW.TransformVectorNoScale(FVector::YAxisVector)).GetSafeNormal();
	const FVector BarFwd = M.InverseTransformVectorNoScale(SteerW.TransformVectorNoScale(FVector::XAxisVector)).GetSafeNormal();
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FBikeRiderPose::FHand& H = Out.Hands[Side];
		H.Grip = W(SteerW.TransformPosition(G.BarTops[Side] - SteerPivot));
		H.Axis = BarAxis;
		H.Radius = G.BarRadius;
		H.PalmNormal = (-Out.Up - BarFwd * 0.25f).GetSafeNormal();
		H.FingerDir = (BarFwd - Out.Up * 0.25f).GetSafeNormal();
		H.Weight = 1.f;
	}

	// the feet on the pedals: the ball over the spindle, the ankle dropping the heel at the top of the stroke and the toes at the bottom
	auto OnPedal = [&](int32 Side, FBikeRiderPose::FFoot& F)
	{
		const UStaticMeshComponent* Pedal = Side == 0 ? PedalL.Get() : PedalR.Get();
		F.Ball = W(Pedal->GetComponentLocation()) + Out.Up * 1.6f;
		const float A = FMath::DegreesToRadians(Crank + (Side == 0 ? 180.f : 0.f));
		const float Down = 12.f + 10.f * FMath::Sin(A);       // degrees the toes point down (crank forward = 0: level going down)
		F.Toes = FQuat(Out.Left, -FMath::DegreesToRadians(Down)).RotateVector(Out.Fwd);
		F.Pole = (Out.Fwd + Out.Up * 0.15f + Out.Left * (Side == 0 ? 0.12f : -0.12f)).GetSafeNormal();
		F.Weight = 1.f;
	};
	const FVector RidePelvis = G.SaddleTop + FVector(2.5f, 0.f, 8.5f * S);
	switch (Ride.Act)
	{
	case ELyudBikeAct::Lead:
		// walking beside it: the legs are the walk's own, the body a little forward to the bars
		Out.PelvisWeight = 0.f;
		Out.TorsoPitch = 8.f;
		break;
	case ELyudBikeAct::Ride:
	{
		const float Rock = FMath::Sin(FMath::DegreesToRadians(Crank)) * 0.8f;
		Out.Pelvis = P(RidePelvis + FVector(Ride.bSprint ? 8.f : 0.f, Rock, Ride.bSprint ? 7.f : 0.f));
		Out.PelvisWeight = 1.f;
		Out.TorsoPitch = Ride.bSprint ? 34.f : 16.f;
		Out.bAutoLean = true;
		Out.HeadUp = Ride.HeadUp;
		Out.TorsoRoll = -Rock * 2.f;
		OnPedal(0, Out.Feet[0]);
		OnPedal(1, Out.Feet[1]);
		break;
	}
	case ELyudBikeAct::Mount:
	case ELyudBikeAct::Dismount:
	{
		// getting on from the bike's left (its -Y): weight on the left foot, the right leg swung up behind and over the saddle's back,
		// onto the saddle, the right foot finds its pedal, then the left lifts onto its own. Getting off is the same backwards.
		const float T = Ride.Act == ELyudBikeAct::Mount ? Ride.Phase : 1.f - Ride.Phase;
		const float Hip = 92.f * S;
		const float SX = G.SaddleTop.X, SZ = G.SaddleTop.Z;
		const FVector StandAt(SX + 16.f, -38.f * S, Hip);
		Out.Pelvis = P(BikeKeys(T, {
			{ 0.f, StandAt }, { 0.3f, StandAt + FVector(-3.f, 7.f, 2.f) }, { 0.56f, FVector(SX + 7.f, -9.f, FMath::Max(SZ + 14.f, Hip - 4.f)) },
			{ 0.76f, RidePelvis }, { 1.f, RidePelvis } }));
		Out.PelvisWeight = 1.f;
		Out.TorsoPitch = BikeKeysF(T, { { 0.f, 10.f }, { 0.3f, 32.f }, { 0.6f, 26.f }, { 0.8f, 16.f } });
		Out.bAutoLean = T > 0.7f;
		Out.HeadUp = 0.7f;
		Out.TorsoRoll = BikeKeysF(T, { { 0.15f, 0.f }, { 0.35f, 8.f }, { 0.6f, 3.f }, { 0.75f, 0.f } });
		FBikeRiderPose::FFoot PedalFoot[2];
		OnPedal(0, PedalFoot[0]);
		OnPedal(1, PedalFoot[1]);
		// the left foot stands on the ground until the hips are on the saddle
		FBikeRiderPose::FFoot& L = Out.Feet[0];
		const FVector LGround(SX + 28.f, -27.f * S, 0.f);
		const float LUp = FMath::SmoothStep(0.62f, 0.86f, T);
		const FVector LArc = FMath::Lerp(P(LGround), PedalFoot[0].Ball, LUp) + Out.Up * (14.f * FMath::Sin(PI * LUp));
		L.Ball = LArc;
		L.Toes = LUp < 0.5f ? Out.Fwd : PedalFoot[0].Toes;
		L.Pole = PedalFoot[0].Pole;
		L.Weight = 1.f;
		// the right leg: up behind, over the back of the saddle, down the right side onto its pedal
		FBikeRiderPose::FFoot& R = Out.Feet[1];
		const FVector RPath = BikeKeys(T, {
			{ 0.f, FVector(SX + 12.f, -52.f * S, 0.f) }, { 0.14f, FVector(SX - 4.f, -54.f * S, 10.f) }, { 0.3f, FVector(SX - 26.f, -40.f, SZ - 12.f) },
			{ 0.44f, FVector(SX - 32.f, 0.f, SZ + 16.f) }, { 0.58f, FVector(SX - 8.f, 32.f, SZ - 30.f) } });
		const float ROn = FMath::SmoothStep(0.58f, 0.72f, T);
		R.Ball = FMath::Lerp(P(RPath), PedalFoot[1].Ball, ROn);
		R.Toes = ROn > 0.5f ? PedalFoot[1].Toes : (T < 0.12f ? Out.Fwd : (Out.Fwd - Out.Up * 0.6f).GetSafeNormal());
		R.Pole = (Out.Fwd + Out.Up * 0.3f).GetSafeNormal();
		R.Weight = 1.f;
		break;
	}
	default:
		break;
	}
}

void ALyudBike::OnGameSaving_Implementation()
{
	// a bike someone rides is saved standing where it is now
	if (Holder.IsValid() && Holder->IsA<APawn>())
	{
		ParkedAt = Root->GetComponentLocation();
		ParkedYaw = Root->GetComponentRotation().Yaw;
		ParkedLean = 0.f;
		bMoved = true;
	}
}

void ALyudBike::OnGameLoaded_Implementation()
{
	if (bMoved && !Holder.IsValid())
	{
		Park(ParkedAt, ParkedYaw, ParkedLean);
	}
}
