#include "World/LyudWires.h"

#include "../Transport/LyudMeshKit.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static TAutoConsoleVariable<int32> CVarWires(TEXT("lyud.Wires"), 1,
	TEXT("The Academgorodok's overhead power lines (ALyudWires): 1 put up when the outdoor map starts, 0 not."));

namespace
{
	// the pieces, cm (tools/academ_wires.py has the layout: pole height, crossarm height, conductors across it)
	constexpr float PoleBaseW = 26.f, PoleBaseD = 18.f;     // SV-95: a tapered rectangular post, wide across the line
	constexpr float PoleTopW = 16.f, PoleTopD = 14.f;
	constexpr float ArmLen = 190.f, ArmT = 6.3f;            // steel angle 63 x 63
	constexpr float WireR = 0.55f;                          // aluminium conductor, ~11 mm across
	const float Phases[] = { -75.f, -25.f, 25.f, 75.f };

	FVector V(const TArray<TSharedPtr<FJsonValue>>& A) { return FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()) * 100.0; }

	/** A post tapering from W0 x D0 at Base to W1 x D1 at Base + Up * Height; W across the line (Across), D along it. */
	void TaperPost(FLyudMeshKit& K, FName Mat, const FVector& Base, const FVector& Up, const FVector& Across, float Height, float W0, float D0, float W1, float D1)
	{
		const FVector Along = FVector::CrossProduct(Up, Across).GetSafeNormal();
		const FVector Top = Base + Up * Height;
		auto Corner = [&](const FVector& C, float W, float D, int32 I)
		{
			const float SX = (I == 0 || I == 3) ? -1.f : 1.f, SY = I < 2 ? -1.f : 1.f;
			return C + Across * (SX * W * 0.5f) + Along * (SY * D * 0.5f);
		};
		for (int32 I = 0; I < 4; I++)
		{
			const int32 J = (I + 1) % 4;
			K.Quad(Mat, Corner(Base, W0, D0, I), Corner(Base, W0, D0, J), Corner(Top, W1, D1, J), Corner(Top, W1, D1, I));
		}
		K.Quad(Mat, Corner(Top, W1, D1, 0), Corner(Top, W1, D1, 1), Corner(Top, W1, D1, 2), Corner(Top, W1, D1, 3));
	}

	/** A porcelain pin insulator (the TF kind: a skirted bell with a neck groove for the tie) on its steel pin, standing at At. */
	void Insulator(FLyudMeshKit& K, const FVector& At)
	{
		K.Rod(TEXT("steel"), At - FVector(0.f, 0.f, 3.f), At + FVector(0.f, 0.f, 4.f), 1.1f, 6, true, false);
		const TArray<FVector2D> Profile = { { 0.0, 2.0 }, { 4.6, 2.0 }, { 6.0, 4.5 }, { 3.6, 6.0 }, { 5.0, 8.5 }, { 3.0, 10.0 }, { 2.5, 12.0 },
			{ 3.4, 14.0 }, { 3.0, 16.0 }, { 0.0, 16.5 } };
		TArray<FName> Mats;
		Mats.Init(TEXT("porcelain"), Profile.Num() - 1);
		K.Lathe(FTransform(At), Profile, Mats, 10, false, false);
	}
}

ALyudWires::ALyudWires()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Tags.Add(TEXT("Academ"));
}

UMaterialInterface* ALyudWires::Mat(FName Name)
{
	if (TObjectPtr<UMaterialInterface>* Found = Mats.Find(Name))
	{
		return *Found;
	}
	UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInterface* Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Transport/Materials/M_TR_Surface.M_TR_Surface"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Transport/Materials/M_TR_Paint.M_TR_Paint"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto Tex = [](const TCHAR* T, const TCHAR* Suffix)
	{
		const FString N = FString::Printf(TEXT("T_TR_%s_%s"), T, Suffix);
		return LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Transport/Textures/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
	};
	UMaterialInstanceDynamic* M = nullptr;
	// concrete and galvanized steel on the stations' textured surface (metre UVs), porcelain and aluminium as paint
	struct FSurf { const TCHAR* Tex; FLinearColor Tint; float Tile, Metal, Rough; };
	const FSurf* Sf = nullptr;
	static const FSurf Concrete = { TEXT("concrete"), FLinearColor(0.78f, 0.77f, 0.74f), 1.6f, 0.f, 1.15f };
	static const FSurf Steel = { TEXT("steel"), FLinearColor(0.42f, 0.43f, 0.44f), 0.6f, 0.7f, 1.5f };
	Sf = Name == TEXT("concrete") ? &Concrete : (Name == TEXT("steel") ? &Steel : nullptr);
	if (Sf && Surface && Tex(Sf->Tex, TEXT("C")))
	{
		M = UMaterialInstanceDynamic::Create(Surface, this);
		M->SetTextureParameterValue(TEXT("BaseTex"), Tex(Sf->Tex, TEXT("C")));
		if (UTexture* N = Tex(Sf->Tex, TEXT("N")))
		{
			M->SetTextureParameterValue(TEXT("NormalTex"), N);
		}
		if (UTexture* R = Tex(Sf->Tex, TEXT("R")))
		{
			M->SetTextureParameterValue(TEXT("RoughTex"), R);
		}
		M->SetVectorParameterValue(TEXT("Tint"), Sf->Tint);
		M->SetScalarParameterValue(TEXT("Tile"), Sf->Tile);
		M->SetScalarParameterValue(TEXT("Metallic"), Sf->Metal);
		M->SetScalarParameterValue(TEXT("RoughMul"), Sf->Rough);
	}
	else
	{
		M = UMaterialInstanceDynamic::Create(Paint ? Paint : Basic, this);
		FLinearColor C(0.55f, 0.55f, 0.53f);
		float Rough = 0.6f, Metal = 0.f;
		if (Name == TEXT("porcelain"))
		{
			C = FLinearColor(0.78f, 0.77f, 0.72f);       // glazed, a little dirty
			Rough = 0.18f;
		}
		else if (Name == TEXT("aluminium"))
		{
			C = FLinearColor(0.38f, 0.38f, 0.37f);       // weathered: dull grey
			Rough = 0.5f;
			Metal = 0.85f;
		}
		else if (Name == TEXT("steel"))
		{
			C = FLinearColor(0.3f, 0.3f, 0.31f);
			Rough = 0.7f;
			Metal = 0.6f;
		}
		M->SetVectorParameterValue(TEXT("Color"), C);
		M->SetScalarParameterValue(TEXT("Roughness"), Rough);
		M->SetScalarParameterValue(TEXT("Metallic"), Metal);
	}
	Mats.Add(Name, M);
	return M;
}

bool ALyudWires::Build()
{
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("BirdsData/wires.json");
	TSharedPtr<FJsonObject> Doc;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Doc) || !Doc)
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudWires: no %s"), *Path);
		return false;
	}
	const float PoleH = (float)Doc->GetNumberField(TEXT("pole_height")) * 100.f;
	const float ArmZ = (float)Doc->GetNumberField(TEXT("arm_z")) * 100.f;
	FLyudMeshKit P, L;
	for (const TSharedPtr<FJsonValue>& Item : Doc->GetArrayField(TEXT("poles")))
	{
		const TSharedPtr<FJsonObject>& O = Item->AsObject();
		const FVector Base = V(O->GetArrayField(TEXT("at"))) - FVector(0.f, 0.f, 60.f);   // set into the ground
		const float Along = FMath::DegreesToRadians((float)O->GetNumberField(TEXT("along")));
		const FVector Dir(FMath::Cos(Along), FMath::Sin(Along), 0.f);
		const FVector Across(-Dir.Y, Dir.X, 0.f);
		TaperPost(P, TEXT("concrete"), Base, FVector::UpVector, Across, PoleH + 60.f, PoleBaseW, PoleBaseD, PoleTopW, PoleTopD);
		P.CollideBox(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Along), 0.f), Base + FVector(0.f, 0.f, (PoleH + 60.f) * 0.5f)),
			FVector(PoleBaseD * 0.5f, PoleBaseW * 0.5f, (PoleH + 60.f) * 0.5f));
		// the crossarm on the pole's face, its two braces, the insulators on top
		const FVector ArmC = Base + FVector(0.f, 0.f, 60.f + ArmZ) + Dir * (PoleTopD * 0.5f + ArmT * 0.5f);
		P.Box(TEXT("steel"), FTransform(FRotator(0.f, FMath::RadiansToDegrees(Along), 0.f), ArmC), FVector(ArmT * 0.5f, ArmLen * 0.5f, ArmT * 0.5f), false);
		for (const float S : { -1.f, 1.f })
		{
			P.Rod(TEXT("steel"), ArmC + Across * (S * 55.f), Base + FVector(0.f, 0.f, 60.f + ArmZ - 55.f) + Dir * (PoleTopD * 0.5f + 1.f), 1.4f, 6, true, false);
		}
		for (const float Ph : Phases)
		{
			Insulator(P, ArmC + Across * Ph + FVector(0.f, 0.f, ArmT * 0.5f));
		}
		// an end or a corner pole leans on a strut, set against the pull of its wires
		const TArray<TSharedPtr<FJsonValue>>* Strut;
		if (O->TryGetArrayField(TEXT("strut"), Strut) && Strut->Num() == 2)
		{
			const FVector SD((*Strut)[0]->AsNumber(), (*Strut)[1]->AsNumber(), 0.0);
			const FVector Foot = Base + SD * 260.f;
			const FVector Head = Base + FVector(0.f, 0.f, 60.f + PoleH * 0.68f) + SD * 10.f;
			const FVector Up = (Head - Foot).GetSafeNormal();
			TaperPost(P, TEXT("concrete"), Foot, Up, FVector::CrossProduct(Up, SD).GetSafeNormal() * -1.f, (Head - Foot).Size(), 22.f, 16.f, 16.f, 14.f);
		}
		NumPoles++;
	}
	// the conductors: parabolic sag, a rod per piece (birds sit on the same curve: tools/bird_habitats.py)
	for (const TSharedPtr<FJsonValue>& Item : Doc->GetArrayField(TEXT("wires")))
	{
		const TSharedPtr<FJsonObject>& O = Item->AsObject();
		const FVector A = V(O->GetArrayField(TEXT("a"))), B = V(O->GetArrayField(TEXT("b")));
		const float Sag = (float)O->GetNumberField(TEXT("sag")) * 100.f;
		const int32 N = FMath::Clamp(FMath::CeilToInt((float)FVector::Dist(A, B) / 150.f), 4, 32);
		FVector Prev = A;
		for (int32 I = 1; I <= N; I++)
		{
			const float T = (float)I / N;
			const FVector Pt = A + (B - A) * T - FVector(0.f, 0.f, 4.f * Sag * T * (1.f - T));
			L.Rod(TEXT("aluminium"), Prev, Pt, WireR, 6, false, false);
			Prev = Pt;
		}
		NumWires++;
	}
	// wall brackets for the drops: a steel bar with two insulators
	for (const TSharedPtr<FJsonValue>& Item : Doc->GetArrayField(TEXT("brackets")))
	{
		const FVector At = V(Item->AsObject()->GetArrayField(TEXT("at")));
		P.Rod(TEXT("steel"), At - FVector(30.f, 0.f, 6.f), At + FVector(30.f, 0.f, -6.f), 1.6f, 6, true, false);
		for (const float S : { -15.f, 15.f })
		{
			Insulator(P, At + FVector(S, 0.f, -20.f));
		}
	}
	auto Make = [this](FLyudMeshKit& K, const TCHAR* Name, bool bCollide, float DrawCm)
	{
		TArray<FName> Slots;
		UStaticMesh* Mesh = K.Build(this, Name, Slots);
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, Name);
		C->SetStaticMesh(Mesh);
		for (int32 I = 0; I < Slots.Num(); I++)
		{
			C->SetMaterial(I, Mat(Slots[I]));
		}
		C->SetMobility(EComponentMobility::Static);
		C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		C->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		C->SetCachedMaxDrawDistance(DrawCm);
		C->LightingChannels.bChannel1 = true;      // the outdoor sun
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		AddInstanceComponent(C);
		return C;
	};
	Poles = Make(P, TEXT("WiresPoles"), true, 150000.f);
	Lines = Make(L, TEXT("WiresLines"), false, 45000.f);
	UE_LOG(LogTemp, Display, TEXT("LyudWires: %d poles, %d wires (%d + %d triangles)"), NumPoles, NumWires, P.NumTriangles(), L.NumTriangles());
	return true;
}

bool ULyudWiresSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void ULyudWiresSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (CVarWires.GetValueOnGameThread() == 0 || !InWorld.GetMapName().Contains(TEXT("Lvl_Scene01")))
	{
		return;
	}
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	if (ALyudWires* Wires = InWorld.SpawnActor<ALyudWires>(SP))
	{
		Wires->Build();
	}
}
