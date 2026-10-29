#include "Railway/LyudLevelCrossing.h"

#include "../Transport/LyudMeshKit.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RailMaterials.h"
#include "RailRun.h"
#include "Railway/LyudRailway.h"
#include "Sound/SoundBase.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

ALyudLevelCrossing::ALyudLevelCrossing()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Tags.Add(TEXT("NoGrass"));
	Tags.Add(TEXT("NoTree"));
}

UMaterialInterface* ALyudLevelCrossing::Mat(FName Name)
{
	return LyudRailMat::Get(this, Name, Mats);
}

void ALyudLevelCrossing::Setup(ULyudRailwaySubsystem* InRailway, const TSharedPtr<FJsonObject>& Built)
{
	Railway = InRailway;
	ULyudRailwaySubsystem* R = Railway;
	const TSharedPtr<FJsonObject> Cr = Built->GetObjectField(TEXT("crossing"));
	S = Cr->GetNumberField(TEXT("s"));
	WarnSeconds = Cr->GetNumberField(TEXT("warn_seconds"));
	const double PathHalf = Cr->GetNumberField(TEXT("path_width")) * 50.0;
	const double Ground = Cr->GetNumberField(TEXT("ground")) * 100.0;
	const TSharedPtr<FJsonObject> Tr = Built->GetObjectField(TEXT("train"));
	HalfTrain = Tr->GetNumberField(TEXT("cars")) * (Tr->GetNumberField(TEXT("car_length")) + 0.9) * 0.5;
	const FVector C = R->LineToWorld(S, 0.0, 0.0);
	const double Rail = C.Z;
	SetActorLocation(C);

	// the deck: rubber panels over and between the rails, timber ramps up from the sand either side
	{
		FLyudMeshKit K;
		K.Box(TEXT("tx_rubber"), FTransform(FVector(0, 0, -5.0)), FVector(PathHalf, 120.0, 5.0), true);
		K.Box(TEXT("tx_rubber"), FTransform(FVector(0, 0, -20.0)), FVector(PathHalf, 260.0, 14.0), true);
		const double Up = Rail - Ground;
		const double Run = FMath::Max(300.0, Up / 0.16);
		for (int32 Sd = -1; Sd <= 1; Sd += 2)
		{
			const double Pitch = FMath::RadiansToDegrees(FMath::Atan2(Up, Run));
			const FVector M(0.0, Sd * (260.0 + Run * 0.5), -Up * 0.5 - 8.0);
			// pitch about X (the ramp climbs along Y): build in a frame turned to face along Y
			K.Box(TEXT("tx_planks_dark"), FTransform(FRotator(Sd * Pitch, 90.0, 0.0), M), FVector(Run * 0.5 + 15.0, PathHalf, 8.0), true);
		}
		TArray<FName> Slots;
		UStaticMesh* Mesh = K.Build(this, TEXT("CrossingDeck"), Slots);
		UStaticMeshComponent* D = NewObject<UStaticMeshComponent>(this, TEXT("Deck"));
		D->SetupAttachment(RootComponent);
		D->SetStaticMesh(Mesh);
		for (int32 I = 0; I < Slots.Num(); I++)
		{
			D->SetMaterial(I, Mat(Slots[I]));
		}
		D->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		D->LightingChannels.bChannel1 = true;
		D->RegisterComponent();
		D->SetRelativeLocation(FVector::ZeroVector);
		AddInstanceComponent(D);
	}

	// two posts, one each side of the line on the path's right-hand verge; each faces its own approach
	UStaticMesh* Post = LyudRailMat::KitMesh(TEXT("SM_RL_XingPost"));
	UStaticMesh* GateBox = LyudRailMat::KitMesh(TEXT("SM_RL_XingGateBox"));
	UStaticMesh* Arm = LyudRailMat::KitMesh(TEXT("SM_RL_XingArm"));
	USoundBase* BellSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Railway/S_xing_bell_01.S_xing_bell_01"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	for (int32 Sd = -1; Sd <= 1; Sd += 2)
	{
		// Sd -1: the south post (meets people coming from the south, facing -Y), +1 the north one
		const FVector PostAt(Sd * -(PathHalf + 60.0), Sd * 420.0, Ground - Rail);
		const FVector Facing(0.0, Sd, 0.0);
		if (Post)
		{
			UStaticMeshComponent* P = NewObject<UStaticMeshComponent>(this);
			P->SetupAttachment(RootComponent);
			P->SetStaticMesh(Post);
			P->LightingChannels.bChannel1 = true;
			P->RegisterComponent();
			// the kit's board faces +-X: turn it to face along the path
			P->SetRelativeLocationAndRotation(PostAt, FRotator(0.0, 90.0, 0.0));
			AddInstanceComponent(P);
		}
		// the lamps: two red discs a side, A and B blink in turn (the kit's lamp places: y -+0.33 m, 2.95 m up)
		for (int32 Lamp = 0; Lamp < 2; Lamp++)
		{
			UMaterialInstanceDynamic* G = LyudRailMat::Glow(this, FLinearColor(1.f, 0.04f, 0.01f), 0.f);
			(Lamp == 0 ? LampsA : LampsB).Add(G);
			FLyudMeshKit K;
			for (int32 F = -1; F <= 1; F += 2)
			{
				// after the post's 90 degree turn the kit's y offset runs along -X, its faces along +-Y
				const FVector At = PostAt + FVector((Lamp == 0 ? 1.0 : -1.0) * 33.0, F * 4.6, 295.0);
				K.Lathe(FTransform(FRotator(F > 0 ? -90.0 : 90.0, 90.0, 0.0).Quaternion() * FRotator(0.0, 0.0, 0.0).Quaternion(), At),
					{ FVector2D(0.0, 0.0), FVector2D(12.5, 0.0) }, { TEXT("glow") }, 18, false, false);
			}
			TArray<FName> Slots;
			UStaticMesh* Mesh = K.Build(this, TEXT("CrossingLamp"), Slots);
			UStaticMeshComponent* LC = NewObject<UStaticMeshComponent>(this);
			LC->SetupAttachment(RootComponent);
			LC->SetStaticMesh(Mesh);
			LC->SetMaterial(0, G);
			LC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			LC->SetCastShadow(false);
			LC->RegisterComponent();
			AddInstanceComponent(LC);
		}
		UPointLightComponent* Gl = NewObject<UPointLightComponent>(this);
		Gl->SetupAttachment(RootComponent);
		Gl->SetLightColor(FLinearColor(1.f, 0.05f, 0.02f));
		Gl->SetIntensityUnits(ELightUnits::Lumens);
		Gl->SetIntensity(0.f);
		Gl->SetAttenuationRadius(700.f);
		Gl->SetCastShadows(false);
		Gl->LightingChannels.bChannel1 = true;
		Gl->RegisterComponent();
		Gl->SetRelativeLocation(PostAt + Facing * 40.0 + FVector(0, 0, 290.0));
		AddInstanceComponent(Gl);
		Glows.Add(Gl);
		// the plate under the lamps: "Caution: train" in the player's language, on both faces
		for (int32 F = -1; F <= 1; F += 2)
		{
			TSharedRef<SWidget> W = SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.95f, 0.78f, 0.05f))
				.Padding(FMargin(10.f, 4.f))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LyudText::Get(TEXT("rail.sign.caution"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
						.ColorAndOpacity(FLinearColor(0.01f, 0.01f, 0.01f)).Justification(ETextJustify::Center).AutoWrapText(true)
				];
			UWidgetComponent* Wc = NewObject<UWidgetComponent>(this);
			Wc->SetupAttachment(RootComponent);
			Wc->SetWidgetSpace(EWidgetSpace::World);
			Wc->SetDrawSize(FVector2D(300.f, 110.f));
			Wc->SetPivot(FVector2D(0.5f, 0.5f));
			Wc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Wc->SetBlendMode(EWidgetBlendMode::Opaque);
			Wc->RegisterComponent();
			Wc->SetRelativeLocationAndRotation(PostAt + FVector(0.0, F * 8.0, 205.0), FRotator(0.0, F > 0 ? 90.0 : -90.0, 0.0));
			Wc->SetWorldScale3D(FVector(0.2f));
			Wc->SetSlateWidget(W);
			AddInstanceComponent(Wc);
		}
		// the barrier beside the post: its arm stands up, comes down across the path
		const FVector BoxAt(Sd * -(PathHalf + 70.0), Sd * 520.0, Ground - Rail);
		if (GateBox)
		{
			UStaticMeshComponent* B = NewObject<UStaticMeshComponent>(this);
			B->SetupAttachment(RootComponent);
			B->SetStaticMesh(GateBox);
			B->LightingChannels.bChannel1 = true;
			B->RegisterComponent();
			B->SetRelativeLocation(BoxAt);
			AddInstanceComponent(B);
		}
		if (Arm)
		{
			UStaticMeshComponent* A = NewObject<UStaticMeshComponent>(this);
			A->SetupAttachment(RootComponent);
			A->SetStaticMesh(Arm);
			A->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			A->LightingChannels.bChannel1 = true;
			A->RegisterComponent();
			A->SetRelativeLocation(BoxAt + FVector(0.0, 0.0, 85.0));
			AddInstanceComponent(A);
			Arms.Add(A);
			// the kit's arm lies along world -Y (Blender's +Y): down = across the path toward the other verge (Sd * +X),
			// up = straight up
			const FVector Along(0.0, -1.0, 0.0);
			const FQuat Down = FQuat::FindBetweenNormals(Along, FVector(Sd, 0.0, 0.0));
			const FQuat UpQ = FQuat::FindBetweenNormals(FVector(Sd, 0.0, 0.0), FVector(0.0, 0.0, 1.0)) * Down;
			ArmDown.Add(Down);
			ArmUp.Add(UpQ);
			A->SetRelativeRotation(UpQ);
		}
	}
	if (BellSound)
	{
		Bell = NewObject<UAudioComponent>(this);
		Bell->SetupAttachment(RootComponent);
		Bell->SetSound(BellSound);
		Bell->bAutoActivate = false;
		Bell->bAllowSpatialization = true;
		Bell->RegisterComponent();
		Bell->SetRelativeLocation(FVector(0.0, 0.0, 380.0));
		AddInstanceComponent(Bell);
	}
}

void ALyudLevelCrossing::UpdateFrom(const LyudRail::FRun& Run, double Now, float DeltaSeconds)
{
	// warn from WarnSeconds before the front reaches the path until the tail has cleared it (+2 s)
	bool bWant = false;
	if (TestUntil > 0.f)
	{
		TestUntil -= DeltaSeconds;
		bWant = true;
	}
	const LyudRail::FTrainState St = Run.At(Now);
	if (St.bInService && FMath::Abs(St.S - S) < HalfTrain + 2.0)
	{
		bWant = true;         // on the crossing
	}
	int32 Dir = 0;
	const double Ahead = Run.NextPass(S - St.Dir * HalfTrain, Now, &Dir);
	if (Ahead >= 0.0 && Ahead < WarnSeconds)
	{
		bWant = true;
	}
	// a train standing at the beach halt with its tail near the crossing doesn't hold it closed for the whole dwell
	if (St.Stop >= 0 && FMath::Abs(St.S - S) > HalfTrain + 1.0 && !(Ahead >= 0.0 && Ahead < WarnSeconds))
	{
		bWant = TestUntil > 0.f;
	}
	if (bWant != bWarning)
	{
		bWarning = bWant;
		WarnTime = 0.f;
		if (Bell)
		{
			if (bWarning)
			{
				Bell->Play();
			}
			else
			{
				Bell->FadeOut(0.6f, 0.f);
			}
		}
	}
	WarnTime += DeltaSeconds;
	// the barriers come down 5 s into the warning (8 s to fall), go up as soon as it ends (6 s)
	const float Target = bWarning && WarnTime > 5.f ? 1.f : 0.f;
	Barrier = FMath::FInterpConstantTo(Barrier, Target, DeltaSeconds, Target > Barrier ? 1.f / 8.f : 1.f / 6.f);
	const float E = Barrier * Barrier * (3.f - 2.f * Barrier);
	for (int32 I = 0; I < Arms.Num(); I++)
	{
		if (Arms[I])
		{
			Arms[I]->SetRelativeRotation(FQuat::Slerp(ArmUp[I], ArmDown[I], E));
		}
	}
	// the lamps blink in turn, ~50 times a minute each
	const bool bA = bWarning && FMath::Frac(WarnTime * 0.85f) < 0.5f;
	const bool bB = bWarning && !bA;
	for (UMaterialInstanceDynamic* M : LampsA)
	{
		M->SetScalarParameterValue(TEXT("Intensity"), bA ? 60.f : 0.f);
	}
	for (UMaterialInstanceDynamic* M : LampsB)
	{
		M->SetScalarParameterValue(TEXT("Intensity"), bB ? 60.f : 0.f);
	}
	for (UPointLightComponent* G : Glows)
	{
		G->SetIntensity(bWarning ? 600.f : 0.f);
	}
}
