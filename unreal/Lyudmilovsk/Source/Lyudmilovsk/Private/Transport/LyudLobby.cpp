// The station lobbies (Dima, 2026-10-26: every vestibule gets its own signature feature and is made cosy): what each hall
// has besides the common furniture, and the universal vending machine. blender/transport/lobby_props.py makes the meshes
// (SM_TR_Lobby_*); blender/transport/lobby_layout.py puts the records into vestibules.json ("furniture" items Vending,
// CoffeeBar, Stove, AmberCase, Aquarium; "lobby": {"centre": chandelier | pendulum | beacon}); this file builds them and moves
// what moves. docs/transport.md "Lobbies".

#include "Transport/LyudCableway.h"

#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Plants/GameCalendar.h"
#include "TransportKit.h"
#include "VendingMachine.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

using namespace LyudTransportKit;

namespace
{
	FVector JsonVec3(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		FVector V = FVector::ZeroVector;
		if (O.IsValid() && O->TryGetArrayField(Key, A))
		{
			for (int32 I = 0; I < A->Num() && I < 3; I++)
			{
				V[I] = (*A)[I]->AsNumber();
			}
		}
		return V;
	}

	/** The machine's paint by what it sells. */
	FLinearColor SkinOf(const FString& Stock)
	{
		if (Stock == TEXT("snacks")) return FLinearColor(0.75f, 0.42f, 0.08f);
		if (Stock == TEXT("drinks")) return FLinearColor(0.08f, 0.32f, 0.55f);
		if (Stock == TEXT("press")) return FLinearColor(0.45f, 0.1f, 0.15f);
		if (Stock == TEXT("hot")) return FLinearColor(0.28f, 0.14f, 0.07f);
		if (Stock == TEXT("cold")) return FLinearColor(0.55f, 0.72f, 0.78f);
		return FLinearColor(0.5f, 0.5f, 0.5f);
	}

	UMaterialInstanceDynamic* Paint(UObject* Outer, const FLinearColor& Color, float Rough, float Metal)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Transport/Materials/M_TR_Paint.M_TR_Paint"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Base)
		{
			Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetScalarParameterValue(TEXT("Roughness"), Rough);
		M->SetScalarParameterValue(TEXT("Metallic"), Metal);
		return M;
	}

	UMaterialInstanceDynamic* Glow(UObject* Outer, const FLinearColor& Color, float Intensity)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Transport/Materials/M_TR_Glow.M_TR_Glow"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Base)
		{
			Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		return M;
	}

	int32 SlotIndex(const UStaticMeshComponent* C, const TCHAR* Name)
	{
		return C ? C->GetMaterialSlotNames().IndexOfByKey(FName(Name)) : INDEX_NONE;
	}
}

void ALyudCableway::LobbyMaterials(UStaticMeshComponent* C, const FLinearColor& Skin)
{
	KitMaterials(C);
	const TArray<FName> Slots = C->GetMaterialSlotNames();
	for (int32 I = 0; I < Slots.Num(); I++)
	{
		const FString N = Slots[I].ToString();
		if (N == TEXT("TR_Skin")) C->SetMaterial(I, Paint(this, Skin, 0.35f, 0.f));
		else if (N == TEXT("TR_Brass")) C->SetMaterial(I, Paint(this, FLinearColor(0.7f, 0.45f, 0.14f), 0.3f, 0.9f));
		else if (N == TEXT("TR_Wood")) C->SetMaterial(I, Paint(this, FLinearColor(0.2f, 0.1f, 0.045f), 0.65f, 0.f));
		else if (N == TEXT("TR_Glow_Amber")) C->SetMaterial(I, Glow(this, FLinearColor(1.f, 0.5f, 0.08f), 3.f));
		else if (N == TEXT("TR_Glow_Fire")) C->SetMaterial(I, Glow(this, FLinearColor(1.f, 0.28f, 0.04f), 6.f));
		else if (N == TEXT("TR_Glow_Aqua")) C->SetMaterial(I, Glow(this, FLinearColor(0.25f, 0.65f, 1.f), 1.4f));
		else if (N == TEXT("TR_Fish")) C->SetMaterial(I, Paint(this, FLinearColor(0.5f, 0.55f, 0.5f), 0.3f, 0.3f));
	}
}

UStaticMeshComponent* ALyudCableway::LobbyMesh(const TCHAR* Name, const FTransform& At, bool bCollide, USceneComponent* Parent, const FLinearColor& Skin)
{
	UStaticMesh* M = LoadMesh(FString(TEXT("SM_TR_Lobby_")) + Name);
	if (!M)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(M);
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetupAttachment(Parent ? Parent : static_cast<USceneComponent*>(RootComponent));
	C->RegisterComponent();
	if (Parent)
	{
		C->SetRelativeTransform(At);
	}
	else
	{
		C->SetWorldTransform(At);
	}
	LobbyMaterials(C, Skin);
	AddInstanceComponent(C);
	return C;
}

namespace
{
	UPointLightComponent* AddLight(AActor* Owner, USceneComponent* Parent, const FVector& World, float Intensity, float Radius, const FLinearColor& Color)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(Owner);
		L->SetupAttachment(Parent);
		L->RegisterComponent();
		L->SetWorldLocation(World);
		L->SetIntensity(Intensity);
		L->SetAttenuationRadius(Radius);
		L->SetLightColor(Color);
		L->SetCastShadows(false);
		Owner->AddInstanceComponent(L);
		return L;
	}
}

bool ALyudCableway::LobbyItem(int32 S, const FString& Item, const TSharedPtr<FJsonObject>& O, const FTransform& At)
{
	UWorld* World = GetWorld();
	const FLinearColor Warm(1.f, 0.78f, 0.52f);
	auto Anim = [this, S, &At](ELobbyAnim Kind) -> FLobbyAnim&
	{
		FLobbyAnim& A = LobbyAnims.AddDefaulted_GetRef();
		A.Kind = Kind;
		A.Station = S;
		A.Frame = At;
		return A;
	};
	auto Steam = [this, World, &Anim, &At](const FVector& Local, int32 N, float Rise)
	{
		UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UMaterialInterface* Glass = Mat(TEXT("glass"));
		for (int32 I = 0; I < N; I++)
		{
			UStaticMeshComponent* P = NewObject<UStaticMeshComponent>(this);
			P->SetMobility(EComponentMobility::Movable);
			P->SetStaticMesh(Ball);
			P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			P->SetCastShadow(false);
			P->SetupAttachment(RootComponent);
			P->RegisterComponent();
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Glass, this);
			M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.95f, 0.95f));
			P->SetMaterial(0, M);
			AddInstanceComponent(P);
			FLobbyAnim& A = Anim(ELobbyAnim::Steam);
			A.A = P;
			A.Mat = M;
			A.Base = Local;
			A.P0 = (float)I / N;
			A.P1 = Rise;
			A.P2 = I * 2.4f;
			A.Speed = 0.35f;
			// in its place and unseen until UpdateLobbies takes it up: a component left at the actor's origin
			// (the world's origin, in the club's hall) stood there as a grey metre-wide ball
			P->SetWorldLocationAndRotation(At.TransformPosition(Local), At.GetRotation());
			P->SetWorldScale3D(FVector(0.03f));
			M->SetScalarParameterValue(TEXT("Opacity"), 0.f);
		}
	};
	if (Item == TEXT("Vending"))
	{
		FString Stock = TEXT("drinks");
		O->TryGetStringField(TEXT("stock"), Stock);
		AVendingMachine* M = World->SpawnActorDeferred<AVendingMachine>(AVendingMachine::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!M)
		{
			return true;
		}
		M->Preset = FName(*Stock);
		M->FinishSpawning(At);
		M->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		if (UStaticMesh* Mesh = LoadMesh(TEXT("SM_TR_Lobby_Vending")))
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(M);
			C->SetMobility(EComponentMobility::Movable);
			C->SetStaticMesh(Mesh);
			C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			C->SetupAttachment(M->GetRootComponent());
			C->RegisterComponent();
			C->SetRelativeTransform(FTransform::Identity);
			LobbyMaterials(C, SkinOf(Stock));
			M->AddInstanceComponent(C);
		}
		LobbyPoints.Add({ S, TEXT("vending"), At.TransformPosition(FVector(30.f, 10.f, 70.f)), At.TransformPosition(FVector(230.f, 20.f, 0.f)) });
		// the machine's own light on the floor and the goods
		AddLight(this, RootComponent, At.TransformPosition(FVector(40.f, 0.f, 120.f)), 500.f, 280.f, FLinearColor(1.f, 0.92f, 0.8f));
		return true;
	}
	if (Item == TEXT("CoffeeBar") || Item == TEXT("Stove") || Item == TEXT("AmberCase") || Item == TEXT("Aquarium"))
	{
		LobbyPoints.Add({ S, TEXT("piece"), At.TransformPosition(FVector(0.f, 0.f, 110.f)), At.TransformPosition(FVector(Item == TEXT("CoffeeBar") ? 340.f : 260.f, 0.f, 0.f)) });
	}
	if (Item == TEXT("CoffeeBar"))
	{
		LobbyMesh(TEXT("CoffeeBar"), At, true);
		for (const float Y : { -70.f, 0.f, 70.f })
		{
			AddLight(this, RootComponent, At.TransformPosition(FVector(20.f, Y, 190.f)), 1100.f, 420.f, Warm);
		}
		Steam(FVector(-10.f, -70.f, 150.f), 3, 45.f);
		Steam(FVector(5.f, 62.f, 118.f), 2, 30.f);
		return true;
	}
	if (Item == TEXT("Stove"))
	{
		UStaticMeshComponent* C = LobbyMesh(TEXT("Stove"), At, true);
		FLobbyAnim& A = Anim(ELobbyAnim::Fire);
		A.Light = AddLight(this, RootComponent, At.TransformPosition(FVector(55.f, -35.f, 70.f)), 1800.f, 650.f, FLinearColor(1.f, 0.45f, 0.18f));
		A.Base.X = A.Light->Intensity;
		if (C)
		{
			A.Mat = Cast<UMaterialInstanceDynamic>(C->GetMaterial(FMath::Max(0, SlotIndex(C, TEXT("TR_Glow_Fire")))));
		}
		Steam(FVector(20.f, 95.f, 112.f), 3, 55.f);
		return true;
	}
	if (Item == TEXT("AmberCase"))
	{
		LobbyMesh(TEXT("AmberCase"), At, true);
		FLobbyAnim& A = Anim(ELobbyAnim::Pulse);
		A.Light = AddLight(this, RootComponent, At.TransformPosition(FVector(-15.f, 0.f, 150.f)), 1400.f, 500.f, FLinearColor(1.f, 0.55f, 0.15f));
		A.Base.X = A.Light->Intensity;
		A.P0 = S * 1.3f;
		return true;
	}
	if (Item == TEXT("Aquarium"))
	{
		LobbyMesh(TEXT("Aquarium"), At, true);
		FLobbyAnim& L = Anim(ELobbyAnim::Pulse);
		L.Light = AddLight(this, RootComponent, At.TransformPosition(FVector(10.f, 0.f, 150.f)), 900.f, 450.f, FLinearColor(0.45f, 0.75f, 1.f));
		L.Base.X = L.Light->Intensity;
		L.P0 = 1.1f;
		L.Speed = 1.7f;
		// perch in three tints: silver-green, bronze, red-finned
		const FLinearColor Tints[] = { FLinearColor(0.35f, 0.5f, 0.4f), FLinearColor(0.55f, 0.4f, 0.2f), FLinearColor(0.6f, 0.62f, 0.65f) };
		for (int32 I = 0; I < 11; I++)
		{
			UStaticMeshComponent* F = LobbyMesh(TEXT("Fish"), At, false);
			if (!F)
			{
				break;
			}
			const int32 Slot = SlotIndex(F, TEXT("TR_Fish"));
			UMaterialInstanceDynamic* M = Paint(this, Tints[I % 3], 0.3f, 0.3f);
			if (Slot != INDEX_NONE)
			{
				F->SetMaterial(Slot, M);
			}
			F->SetCastShadow(false);
			FLobbyAnim& A = Anim(ELobbyAnim::Fish);
			A.A = F;
			A.P0 = I * 1.7f;
			A.P1 = 0.7f + 0.12f * (I % 5);
			A.P2 = 0.8f + 0.18f * (I % 4);
			A.Far = 3500.f;
		}
		UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UMaterialInterface* Glass = Mat(TEXT("glass"));
		for (int32 I = 0; I < 6; I++)
		{
			UStaticMeshComponent* P = NewObject<UStaticMeshComponent>(this);
			P->SetMobility(EComponentMobility::Movable);
			P->SetStaticMesh(Ball);
			P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			P->SetCastShadow(false);
			P->SetupAttachment(RootComponent);
			P->RegisterComponent();
			P->SetMaterial(0, Glass);
			P->SetWorldScale3D(FVector(0.012f + 0.004f * (I % 3)));
			AddInstanceComponent(P);
			FLobbyAnim& A = Anim(ELobbyAnim::Bubble);
			A.A = P;
			A.P0 = I / 6.f;
			A.P1 = -20.f + I * 7.f;
			A.Far = 3500.f;
			P->SetWorldLocation(At.TransformPosition(FVector(-6.f, A.P1, 100.f + 70.f * A.P0)));   // in the tank, not at the world's origin
		}
		return true;
	}
	return false;
}

void ALyudCableway::LobbyCentre(int32 S, const TSharedPtr<FJsonObject>& V, const FTransform& C)
{
	const TSharedPtr<FJsonObject>* Lobby = nullptr;
	FString Kind;
	if (!V.IsValid() || !V->TryGetObjectField(TEXT("lobby"), Lobby) || !(*Lobby)->TryGetStringField(TEXT("centre"), Kind))
	{
		return;
	}
	const TSharedPtr<FJsonObject> Room = V->GetObjectField(TEXT("room"));
	// the hall's middle is the glazed core: the piece hangs in the ring beside it (lobby_layout.py place_centre: "at")
	FVector Mid = JsonVec3(Room, TEXT("centre"));
	const TArray<TSharedPtr<FJsonValue>>* AtArr = nullptr;
	if ((*Lobby)->TryGetArrayField(TEXT("at"), AtArr) && AtArr->Num() >= 2)
	{
		Mid = FVector((*AtArr)[0]->AsNumber(), (*AtArr)[1]->AsNumber(), 0.f);
	}
	float H = Room->GetNumberField(TEXT("height"));
	const FTransform At(C.GetRotation(), C.TransformPosition(FVector(Mid.X, Mid.Y, 0.f)));
	// the ceiling over that spot (a dome comes down toward its rim): a trace up from head height
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(LobbyCeiling), true, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, At.GetLocation() + FVector(0.f, 0.f, 120.f), At.GetLocation() + FVector(0.f, 0.f, H + 400.f), ECC_Visibility, Q))
		{
			H = Hit.ImpactPoint.Z - At.GetLocation().Z - 2.f;
		}
	}
	const FVector Top = At.GetLocation() + FVector(0.f, 0.f, H);
	auto Anim = [this, S, &At](ELobbyAnim Kind2) -> FLobbyAnim&
	{
		FLobbyAnim& A = LobbyAnims.AddDefaulted_GetRef();
		A.Kind = Kind2;
		A.Station = S;
		A.Frame = At;
		return A;
	};
	LobbyPoints.Add({ S, TEXT("centre"), Top - FVector(0.f, 0.f, FMath::Min(H * 0.35f, 260.f)), At.TransformPosition(FVector(0.f, 330.f, 0.f)) });
	if (Kind == TEXT("chandelier"))
	{
		// the sputnik: a hub 55 cm under the ceiling on a rod, turning slowly, a warm light in it
		const FVector Hub = Top - FVector(0.f, 0.f, 62.f);
		if (UStaticMeshComponent* Rod = LobbyMesh(TEXT("ChandelierRod"), FTransform(FRotator::ZeroRotator, Hub + FVector(0, 0, 8.f), FVector(1.f, 1.f, 0.54f)), false))
		{
			(void)Rod;
		}
		UStaticMeshComponent* Body = LobbyMesh(TEXT("Chandelier"), FTransform(FRotator::ZeroRotator, Hub, FVector(0.85f)), false);
		FLobbyAnim& A = Anim(ELobbyAnim::Spin);
		A.A = Body;
		A.Base = Hub;
		A.Speed = 6.f;
		A.Far = 9000.f;
		AddLight(this, RootComponent, Hub, 5000.f, 900.f, FLinearColor(1.f, 0.82f, 0.58f));
	}
	else if (Kind == TEXT("pendulum"))
	{
		// a Foucault pendulum from the apex: a wire to 2 m over the floor, a brass bob; its plane turns with the game's day
		const FVector Hinge = Top - FVector(0.f, 0.f, 30.f);
		const float Wire = FMath::Max(200.f, Hinge.Z - At.GetLocation().Z - 206.f);
		USceneComponent* Pivot = NewObject<USceneComponent>(this);
		Pivot->SetMobility(EComponentMobility::Movable);
		Pivot->SetupAttachment(RootComponent);
		Pivot->RegisterComponent();
		Pivot->SetWorldLocation(Hinge);
		AddInstanceComponent(Pivot);
		LobbyMesh(TEXT("ChandelierRod"), FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, 1.f, Wire / 100.f)), false, Pivot);
		LobbyMesh(TEXT("PendulumBob"), FTransform(FRotator(180.f, 0.f, 0.f), FVector(0.f, 0.f, Wire)), false, Pivot);
		FLobbyAnim& A = Anim(ELobbyAnim::Pendulum);
		A.A = Pivot;
		A.Base = Hinge;
		A.P0 = FMath::Asin(FMath::Clamp(90.f / Wire, 0.f, 0.5f));                       // amplitude 0.9 m
		A.P1 = 2.f * PI * FMath::Sqrt(Wire / 980.f);                                      // its period, s
		A.P2 = 0.6f + S;
		A.Speed = 2.f * PI * FMath::Sin(FMath::DegreesToRadians(55.f)) / 86400.f;        // Foucault: the plane turns by sin(latitude) a day
		A.Far = 12000.f;
		bool bPlate = false;
		(*Lobby)->TryGetBoolField(TEXT("plate"), bPlate);
		if (bPlate)
		{
			LobbyMesh(TEXT("PendulumPlate"), At, false);
		}
	}
	else if (Kind == TEXT("beacon"))
	{
		// the lighthouse lens under the lantern's top, turning once in nine seconds, two narrow beams from its lamp
		const FVector Hang = Top - FVector(0.f, 0.f, 10.f);
		UStaticMeshComponent* Lens = LobbyMesh(TEXT("BeaconLens"), FTransform(Hang), false);
		if (Lens)
		{
			for (int32 Side = 0; Side < 2; Side++)
			{
				USpotLightComponent* Sp = NewObject<USpotLightComponent>(this);
				Sp->SetupAttachment(Lens);
				Sp->RegisterComponent();
				Sp->SetRelativeLocation(FVector(0.f, 0.f, -120.f));
				Sp->SetRelativeRotation(FRotator(-4.f, Side * 180.f, 0.f));
				Sp->SetInnerConeAngle(2.f);
				Sp->SetOuterConeAngle(9.f);
				Sp->SetIntensity(90000.f);
				Sp->SetAttenuationRadius(9000.f);
				Sp->SetLightColor(FLinearColor(1.f, 0.88f, 0.65f));
				Sp->SetCastShadows(false);
				AddInstanceComponent(Sp);
			}
		}
		FLobbyAnim& A = Anim(ELobbyAnim::Spin);
		A.A = Lens;
		A.Base = Hang;
		A.Speed = 40.f;
		A.Far = 40000.f;
		AddLight(this, RootComponent, Hang - FVector(0.f, 0.f, 120.f), 4000.f, 700.f, FLinearColor(1.f, 0.8f, 0.5f));
	}
}

void ALyudCableway::UpdateLobbies(const FVector& Viewer, float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || LobbyAnims.IsEmpty())
	{
		return;
	}
	const float T = World->GetTimeSeconds();
	for (FLobbyAnim& A : LobbyAnims)
	{
		if (FVector::Dist(Viewer, A.Frame.GetLocation()) > A.Far)
		{
			continue;
		}
		USceneComponent* C = A.A.Get();
		switch (A.Kind)
		{
		case ELobbyAnim::Spin:
			if (C)
			{
				C->SetWorldRotation(A.Frame.GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(T * A.Speed + A.P0)));
			}
			break;
		case ELobbyAnim::Pendulum:
			if (C)
			{
				const float Theta = A.P0 * FMath::Cos(2.f * PI * T / A.P1);
				const float Az = A.P2 - A.Speed * (float)UGameCalendar::GameSeconds(World);
				const FVector Dir(FMath::Cos(Az), FMath::Sin(Az), 0.f);
				const FVector Bob = Dir * FMath::Sin(Theta) - FVector::UpVector * FMath::Cos(Theta);
				C->SetWorldRotation(FRotationMatrix::MakeFromZ(Bob).ToQuat());
			}
			break;
		case ELobbyAnim::Fire:
		{
			const float F = 0.78f + 0.22f * FMath::Sin(T * 11.f + 1.f) * FMath::Sin(T * 7.3f) + 0.12f * FMath::Sin(T * 23.f);
			if (ULightComponent* L = A.Light.Get())
			{
				L->SetIntensity(A.Base.X * F);
			}
			if (UMaterialInstanceDynamic* M = A.Mat.Get())
			{
				M->SetScalarParameterValue(TEXT("Intensity"), 6.f * F);
			}
			break;
		}
		case ELobbyAnim::Pulse:
			if (ULightComponent* L = A.Light.Get())
			{
				const float Rate = A.Speed > 1.f ? A.Speed : 0.6f;
				L->SetIntensity(A.Base.X * (1.f + 0.12f * FMath::Sin(T * Rate + A.P0)));
			}
			break;
		case ELobbyAnim::Steam:
			if (C)
			{
				const float U = FMath::Frac(T * A.Speed + A.P0);
				const FVector Local = A.Base + FVector(FMath::Sin(T + A.P2) * 3.f * U, FMath::Cos(T * 0.8f + A.P2) * 3.f * U, U * A.P1);
				C->SetWorldLocation(A.Frame.TransformPosition(Local));
				C->SetWorldScale3D(FVector(0.03f + U * 0.08f));
				if (UMaterialInstanceDynamic* M = A.Mat.Get())
				{
					M->SetScalarParameterValue(TEXT("Opacity"), 0.22f * FMath::Sin(U * PI));
				}
			}
			break;
		case ELobbyAnim::Fish:
			if (C)
			{
				// each perch loops inside the tank (x 20 cm, y +-100, z 95..170 in the prop's frame), nose along its way, tail wagging
				const float P = T * A.P1 + A.P0;
				const FVector Pos(8.f * FMath::Sin(P * 0.9f + 1.f), 88.f * FMath::Sin(P * 0.5f), 138.f + 26.f * FMath::Sin(P * 0.37f + A.P2));
				const FVector Vel(7.2f * FMath::Cos(P * 0.9f + 1.f), 44.f * FMath::Cos(P * 0.5f), 9.6f * FMath::Cos(P * 0.37f + A.P2));
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Vel.Y, Vel.X + 0.001f)) + 14.f * FMath::Sin(T * 12.f * A.P1 + A.P0);
				const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Vel.Z, FMath::Abs(Vel.Y) + 1.f)) * 0.4f;
				C->SetWorldLocationAndRotation(A.Frame.TransformPosition(Pos), A.Frame.GetRotation() * FRotator(Pitch, Yaw, 0.f).Quaternion());
				C->SetWorldScale3D(FVector(1.f));
			}
			break;
		case ELobbyAnim::Bubble:
			if (C)
			{
				const float U = FMath::Frac(T * 0.3f + A.P0);
				const FVector Pos(-6.f + FMath::Sin(T * 3.f + A.P0 * 9.f) * 1.5f, A.P1 + FMath::Cos(T * 2.f + A.P0 * 5.f) * 1.5f, 100.f + U * 70.f);
				C->SetWorldLocation(A.Frame.TransformPosition(Pos));
			}
			break;
		default:
			break;
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GLobbyLook(TEXT("Lobby.Look"),
	TEXT("Lobby.Look <station index> <vending|piece|centre>: stands the hero in front of that lobby piece, looking at it (headless checks)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
		if (!Pawn || Args.Num() < 2)
		{
			return;
		}
		for (TActorIterator<ALyudCableway> It(World); It; ++It)
		{
			for (const ALyudCableway::FLobbyPoint& P : It->LobbyPoints)
			{
				if (P.Station == FCString::Atoi(*Args[0]) && P.Name == FName(*Args[1]))
				{
					Pawn->SetActorLocation(P.Stand + FVector(0.f, 0.f, 92.f), false, nullptr, ETeleportType::TeleportPhysics);
					const FVector Eye = P.Stand + FVector(0.f, 0.f, 160.f);
					if (AController* C = Pawn->GetController())
					{
						C->SetControlRotation((P.Target - Eye).Rotation());
					}
					UE_LOG(LogTemp, Warning, TEXT("LOBBYLOOK %s %s stand %s"), *Args[0], *Args[1], *P.Stand.ToString());
					return;
				}
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("LOBBYLOOK %s %s: no such piece"), *Args[0], *Args[1]);
	}));
