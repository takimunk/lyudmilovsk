#include "Railway/LyudRailTrack.h"

#include "../Transport/LyudMeshKit.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Plants/ProceduralPlant.h"
#include "Railway/LyudRailway.h"
#include "RailMaterials.h"
#include "Sky/LyudSky.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
constexpr double SeaZcm = -4300.0;
constexpr double RailHalfGauge = 0.5335 + 0.0325;   // rail head centres, m (1067 mm gauge)

/** A quad whose front faces Hint (the kit wants its corners counterclockwise seen from the front). */
void Q(FLyudMeshKit& K, FName M, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Hint)
{
	const FVector N = FVector::CrossProduct(B - A, D - A);
	if (FVector::DotProduct(N, Hint) >= 0.0)
	{
		K.Quad(M, A, B, C, D);
	}
	else
	{
		K.Quad(M, A, D, C, B);
	}
}
}

ALyudRailTrack::ALyudRailTrack()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
	// the grass and the forest leave its decks and the bank bare
	Tags.Add(TEXT("NoGrass"));
	Tags.Add(TEXT("NoTree"));
}

UMaterialInterface* ALyudRailTrack::Mat(FName Name)
{
	return LyudRailMat::Get(this, Name, Mats);
}

UStaticMeshComponent* ALyudRailTrack::AddKit(const FLyudMeshKit& Kit, FName Name, const FVector& At, bool bSeaBed, bool bCollide)
{
	if (Kit.IsEmpty())
	{
		return nullptr;
	}
	TArray<FName> Slots;
	UStaticMesh* Mesh = Kit.Build(this, Name, Slots);
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, Name);
	C->SetMobility(EComponentMobility::Static);
	C->SetupAttachment(RootComponent);
	C->SetStaticMesh(Mesh);
	for (int32 I = 0; I < Slots.Num(); I++)
	{
		C->SetMaterial(I, Mat(Slots[I]));
	}
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	// outdoors: the sun's lighting channel (as the cable car's pieces)
	C->LightingChannels.bChannel0 = true;
	C->LightingChannels.bChannel1 = true;
	if (bSeaBed)
	{
		C->ComponentTags.Add(TEXT("LyudSeaBed"));
	}
	// placed before it registers: a static component can't be moved after
	C->SetRelativeLocation(At);
	C->RegisterComponent();
	AddInstanceComponent(C);
	return C;
}

UStaticMeshComponent* ALyudRailTrack::AddPiece(const TCHAR* Kit, const FTransform& At, bool bCollide)
{
	UStaticMesh* M = LyudRailMat::KitMesh(Kit);
	if (!M)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Static);
	C->SetupAttachment(RootComponent);
	C->SetStaticMesh(M);
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->LightingChannels.bChannel1 = true;
	C->SetRelativeTransform(At);
	C->RegisterComponent();
	AddInstanceComponent(C);
	return C;
}

void ALyudRailTrack::Build(ULyudRailwaySubsystem* InRailway, const TSharedPtr<FJsonObject>& Built)
{
	Railway = InRailway;
	const double T0 = FPlatformTime::Seconds();
	BuildLine(Built);
	BuildTunnel(Built);
	for (const TSharedPtr<FJsonValue>& V : Built->GetArrayField(TEXT("stops")))
	{
		BuildHalt(V->AsObject());
	}
	BuildIsland(Built);
	UE_LOG(LogTemp, Log, TEXT("Railway: track built in %.0f ms (%d chunks, %d sleepers, %d masts)"), (FPlatformTime::Seconds() - T0) * 1000.0,
		Chunks.Num(), Sleepers ? Sleepers->GetInstanceCount() : 0, Masts ? Masts->GetInstanceCount() : 0);
}

void ALyudRailTrack::BuildLine(const TSharedPtr<FJsonObject>& Built)
{
	ULyudRailwaySubsystem* R = Railway;
	const TSharedPtr<FJsonObject> Ln = Built->GetObjectField(TEXT("line"));
	const double PortalS = Ln->GetNumberField(TEXT("portal_s"));
	const double CrestBelow = Ln->GetNumberField(TEXT("rail_over_ballast")) * 100.0;
	const double CrestHalf = Ln->GetNumberField(TEXT("bank_top_half")) * 100.0;
	const double Slope = Ln->GetNumberField(TEXT("bank_slope"));
	const double TunnelHalf = Built->GetObjectField(TEXT("tunnel"))->GetNumberField(TEXT("width")) * 50.0 - 10.0;
	const TSharedPtr<FJsonObject> Cat = Built->GetObjectField(TEXT("catenary"));
	const double Span = Cat->GetNumberField(TEXT("span"));
	const double WireUp = Cat->GetNumberField(TEXT("height"));
	const double MastSide = Cat->GetNumberField(TEXT("pole_offset")) * Cat->GetNumberField(TEXT("side"));
	const double IslandS = Built->GetObjectField(TEXT("island"))->GetNumberField(TEXT("halt_s"));
	const double Len = R->LineLength();
	const TArray<ULyudRailwaySubsystem::FPoint>& P = R->Points;
	const double Floor = SeaZcm - 1200.0;      // the bank's toe never deeper than this (nobody sees it)

	const int32 NChunks = FMath::CeilToInt(Len / ChunkM);
	for (int32 C = 0; C < NChunks; C++)
	{
		const double S0 = C * ChunkM, S1 = FMath::Min(Len, (C + 1) * ChunkM);
		const FVector O = R->MapToWorld(R->X0 + S0, R->LineY, -43.0);
		FLyudMeshKit K;
		auto L = [&](double S, double SideCm, double Zcm) { const FVector W = R->MapToWorld(R->X0 + S, R->LineY + SideCm / 100.0, 0.0); return FVector(W.X, W.Y, Zcm) - O; };
		const int32 I0 = FMath::FloorToInt(S0 / R->Step), I1 = FMath::Min(P.Num() - 1, FMath::CeilToInt(S1 / R->Step));
		double BoxStart = -1.0;
		for (int32 I = I0; I < I1; I++)
		{
			const double Sa = P[I].S, Sb = P[I + 1].S;
			const double Ra = P[I].Rail * 100.0, Rb = P[I + 1].Rail * 100.0;
			const double Ca = Ra - CrestBelow, Cb = Rb - CrestBelow;
			const bool bTunnel = Sb <= PortalS;
			const double Half = bTunnel ? TunnelHalf : CrestHalf;
			// the crest: ballast
			Q(K, TEXT("tx_ballast"), L(Sa, -Half, Ca), L(Sb, -Half, Cb), L(Sb, Half, Cb), L(Sa, Half, Ca), FVector::UpVector);
			if (!bTunnel)
			{
				// the sides: armour stones down to the bed (or the beach), toe spreading with the height
				const double Ta = FMath::Max(FMath::Min(P[I].Ground * 100.0, Ca - 15.0), Floor);
				const double Tb = FMath::Max(FMath::Min(P[I + 1].Ground * 100.0, Cb - 15.0), Floor);
				const double Wa = Half + Slope * (Ca - Ta), Wb = Half + Slope * (Cb - Tb);
				for (int32 Sd = -1; Sd <= 1; Sd += 2)
				{
					// the top 25 cm of the side is still ballast, then the stones (where the sea's weed grows on them)
					const double Ka = Ca - 25.0, Kb = Cb - 25.0;
					const double Ma = Half + Slope * 25.0, Mb = Half + Slope * 25.0;
					Q(K, TEXT("tx_ballast"), L(Sa, Sd * Half, Ca), L(Sb, Sd * Half, Cb), L(Sb, Sd * Mb, Kb), L(Sa, Sd * Ma, Ka), FVector(0, Sd, 1));
					Q(K, TEXT("tx_armour"), L(Sa, Sd * Ma, Ka), L(Sb, Sd * Mb, Kb), L(Sb, Sd * Wb, Tb), L(Sa, Sd * Wa, Ta), FVector(0, Sd, 1));
				}
			}
			// the rails: a head, its two sides (a 15 cm tall rail on the sleepers)
			for (int32 Sd = -1; Sd <= 1; Sd += 2)
			{
				const double Yc = Sd * RailHalfGauge * 100.0;
				Q(K, TEXT("rail"), L(Sa, Yc - 3.25, Ra), L(Sb, Yc - 3.25, Rb), L(Sb, Yc + 3.25, Rb), L(Sa, Yc + 3.25, Ra), FVector::UpVector);
				for (int32 E = -1; E <= 1; E += 2)
				{
					const double Ye = Yc + E * 3.25;
					Q(K, E * Sd > 0 ? TEXT("rail") : TEXT("rust"), L(Sa, Ye, Ra), L(Sb, Ye, Rb), L(Sb, Ye + E * 2.0, Rb - 15.0), L(Sa, Ye + E * 2.0, Ra - 15.0),
						FVector(0, E, 0.2));
				}
			}
			// collision: the crest, a box per step near the land and the island, per 40 m beyond (the grade is flat there)
			const bool bFine = Sa < IslandS + 300.0;
			if (BoxStart < 0.0)
			{
				BoxStart = Sa;
			}
			if (bFine || Sb - BoxStart >= 40.0 || I + 1 == I1)
			{
				const double Sm = (BoxStart + Sb) * 0.5;
				const double Zm = R->RailZ(Sm) - CrestBelow;
				const double Pitch = FMath::RadiansToDegrees(FMath::Atan2(R->RailZ(BoxStart) - R->RailZ(Sb), (Sb - BoxStart) * 100.0));
				const FVector Centre = L(Sm, 0.0, Zm - 15.0);
				K.CollideBox(FTransform(FRotator(Pitch, 0.0, 0.0), Centre), FVector((Sb - BoxStart) * 50.0 + 2.0, Half, 15.0));
				BoxStart = -1.0;
			}
		}
		// the catenary in this chunk: the contact wire and its messenger, sagging between the masts; droppers
		const double First = PortalS + 12.0;
		for (double Sm = First + FMath::Max(0.0, FMath::CeilToDouble((S0 - First) / Span)) * Span; Sm < S1 && Sm + Span <= Len; Sm += Span)
		{
			const double Se = Sm + Span;
			const int32 N = 5;
			for (int32 J = 0; J < N; J++)
			{
				const double Ta = double(J) / N, Tb = double(J + 1) / N;
				const double Sa = Sm + Span * Ta, Sb = Sm + Span * Tb;
				const double Za = R->RailZ(Sa) + WireUp * 100.0, Zb = R->RailZ(Sb) + WireUp * 100.0;
				K.Rod(TEXT("wire"), L(Sa, 0.0, Za), L(Sb, 0.0, Zb), 0.7f, 4, false, false);
				const double SagA = 75.0 * 4.0 * Ta * (1.0 - Ta), SagB = 75.0 * 4.0 * Tb * (1.0 - Tb);
				K.Rod(TEXT("wire"), L(Sa, 0.0, Za + 95.0 - SagA), L(Sb, 0.0, Zb + 95.0 - SagB), 0.6f, 4, false, false);
				if (J > 0)
				{
					K.Rod(TEXT("wire"), L(Sa, 0.0, Za), L(Sa, 0.0, Za + 95.0 - SagA), 0.3f, 3, false, false);
				}
			}
		}
		// in the tunnel the wire hangs from the roof
		if (S0 < PortalS)
		{
			for (double Sa = FMath::Max(S0, 2.0); Sa < FMath::Min(S1, PortalS + 12.0); Sa += 12.0)
			{
				const double Sb = FMath::Min(Sa + 12.0, PortalS + 12.0);
				K.Rod(TEXT("wire"), L(Sa, 0.0, R->RailZ(Sa) + WireUp * 100.0), L(Sb, 0.0, R->RailZ(Sb) + WireUp * 100.0), 0.7f, 4, false, false);
			}
		}
		// near the coast and the island the bank is the sea's bed for the waves (the depth map's extent stays small)
		const bool bSeaBed = R->X0 + S1 < 3200.0;
		UStaticMeshComponent* Comp = AddKit(K, FName(*FString::Printf(TEXT("Bank_%03d"), C)), O, bSeaBed, true);
		if (Comp)
		{
			Comp->SetCastShadow(S0 < IslandS + 1500.0);
			Chunks.Add(Comp);
		}
	}

	// sleepers to a little past the island (beyond, the bank is under the water and far away)
	if (UStaticMesh* SM = LyudRailMat::KitMesh(TEXT("SM_RL_Sleeper")))
	{
		Sleepers = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, TEXT("Sleepers"));
		Sleepers->SetStaticMesh(SM);
		Sleepers->SetMobility(EComponentMobility::Static);
		Sleepers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sleepers->SetupAttachment(RootComponent);
		Sleepers->SetCullDistances(0, 60000);
		Sleepers->LightingChannels.bChannel1 = true;
		Sleepers->RegisterComponent();
		AddInstanceComponent(Sleepers);
		TArray<FTransform> Ts;
		for (double S = 1.0; S < FMath::Min(Len, IslandS + 900.0); S += 0.62)
		{
			const double Pitch = FMath::RadiansToDegrees(FMath::Atan2(R->RailZ(S + 1.0) - R->RailZ(S - 1.0), 200.0));
			Ts.Add(FTransform(FRotator(Pitch, 0.0, 0.0), R->LineToWorld(S, 0.0, -0.15)));
		}
		Sleepers->AddInstances(Ts, false, true);
	}
	// masts every span from the tunnel's mouth to the end, on the north side, the arm over the track
	if (UStaticMesh* MM = LyudRailMat::KitMesh(TEXT("SM_RL_Mast")))
	{
		Masts = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, TEXT("Masts"));
		Masts->SetStaticMesh(MM);
		Masts->SetMobility(EComponentMobility::Static);
		Masts->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Masts->SetupAttachment(RootComponent);
		Masts->LightingChannels.bChannel1 = true;
		Masts->RegisterComponent();
		AddInstanceComponent(Masts);
		TArray<FTransform> Ts;
		for (double S = PortalS + 12.0; S < Len; S += Span)
		{
			// the kit's arm reaches to world +Y (Blender's -Y): a mast on the north (+Y) side turns round
			Ts.Add(FTransform(FRotator(0.0, MastSide > 0.0 ? 180.0 : 0.0, 0.0), R->LineToWorld(S, MastSide, 0.0)));
		}
		Masts->AddInstances(Ts, false, true);
	}
}

void ALyudRailTrack::BuildTunnel(const TSharedPtr<FJsonObject>& Built)
{
	ULyudRailwaySubsystem* R = Railway;
	const TSharedPtr<FJsonObject> Tu = Built->GetObjectField(TEXT("tunnel"));
	const double MouthX = Tu->GetNumberField(TEXT("mouth_x"));
	const double Rail = Tu->GetNumberField(TEXT("rail")) * 100.0;
	const double W = Tu->GetNumberField(TEXT("width")) * 100.0;
	const double H = Tu->GetNumberField(TEXT("height")) * 100.0;
	const double Depth = Tu->GetNumberField(TEXT("depth")) * 100.0;
	const double Hood = Tu->GetNumberField(TEXT("hood")) * 100.0;
	// the frame: origin on the rail head's level under the mouth's middle, X out to sea, Y north
	const FVector O = R->MapToWorld(MouthX, R->LineY, Rail / 100.0);
	FLyudMeshKit K;
	const double Wall = 100.0, Half = W * 0.5;
	// the bore: two walls, a roof, the floor under the ballast, a dark end
	for (int32 Sd = -1; Sd <= 1; Sd += 2)
	{
		K.Box(TEXT("tx_tunnel"), FTransform(FVector(-Depth * 0.5 - 100.0, Sd * (Half + Wall * 0.5), H * 0.5 - 40.0)), FVector(Depth * 0.5, Wall * 0.5, H * 0.5 + 60.0), true);
	}
	K.Box(TEXT("tx_tunnel"), FTransform(FVector(-Depth * 0.5 - 100.0, 0.0, H + Wall * 0.5)), FVector(Depth * 0.5, Half + Wall, Wall * 0.5), true);
	K.Box(TEXT("dark"), FTransform(FVector(-Depth - 100.0, 0.0, H * 0.5)), FVector(20.0, Half, H * 0.5 + 60.0), false);
	K.Box(TEXT("tx_tunnel"), FTransform(FVector(-Depth * 0.5 - 100.0, 0.0, -80.0)), FVector(Depth * 0.5, Half, 30.0), false);
	// the arch's haunches: chamfers in the bore's top corners
	for (int32 Sd = -1; Sd <= 1; Sd += 2)
	{
		K.Box(TEXT("tx_tunnel"), FTransform(FRotator(0.0, 0.0, Sd * 45.0), FVector(-Depth * 0.5 - 100.0, Sd * (Half - 45.0), H - 45.0)), FVector(Depth * 0.5, 70.0, 20.0), false);
	}
	// the headwall: a concrete face with a rectangular mouth, a coping, a date plate; it stands 18 m wide in the cliff
	{
		const double FW = 1800.0, Z0 = -180.0, Z1 = H + 350.0;
		FTransform F(FRotator(0.0, 90.0, 0.0), FVector(-50.0, -FW * 0.5, 0.0));
		TArray<FLyudMeshKit::FOpening> Op;
		Op.Add({ float(FW * 0.5 - Half), float(FW * 0.5 + Half), float(Z0 - 10.0), float(H) });
		K.Wall(TEXT("tx_concrete"), F, FW, 100.0, Z0, Z1, Op, true);
		K.Box(TEXT("tx_concrete"), FTransform(FVector(-50.0, 0.0, Z1 + 20.0)), FVector(70.0, FW * 0.5 + 20.0, 20.0), false);
		K.Box(TEXT("tx_concrete"), FTransform(FVector(10.0, 0.0, H + 25.0)), FVector(15.0, Half + 30.0, 25.0), false);
		K.Box(TEXT("black"), FTransform(FVector(3.0, 0.0, H + 160.0)), FVector(4.0, 70.0, 22.0), false);
		// wing walls angling back into the cliff either side
		for (int32 Sd = -1; Sd <= 1; Sd += 2)
		{
			K.Box(TEXT("tx_concrete"), FTransform(FRotator(0.0, Sd * -28.0, 0.0), FVector(220.0, Sd * (FW * 0.5 + 260.0), (Z0 + 300.0) * 0.5)),
				FVector(380.0, 50.0, (300.0 - Z0) * 0.5), true);
		}
	}
	// the hood over the bore where the cliff's foot is cut away for it (the landscape has a hole there)
	K.Box(TEXT("tx_concrete"), FTransform(FVector(-Hood * 0.5 - 50.0, 0.0, H + Wall + 30.0)), FVector(Hood * 0.5, Half + Wall + 20.0, 30.0), true);
	for (int32 Sd = -1; Sd <= 1; Sd += 2)
	{
		K.Box(TEXT("tx_concrete"), FTransform(FVector(-Hood * 0.5 - 50.0, Sd * (Half + Wall + 10.0), H * 0.5)), FVector(Hood * 0.5, 30.0, H * 0.5 + 300.0), true);
	}
	AddKit(K, TEXT("Tunnel"), O, false, true);
}

void ALyudRailTrack::BuildHalt(const TSharedPtr<FJsonObject>& St)
{
	ULyudRailwaySubsystem* R = Railway;
	const FString Id = St->GetStringField(TEXT("id"));
	const double S = St->GetNumberField(TEXT("s"));
	const int32 Side = int32(St->GetNumberField(TEXT("side")));
	const TArray<TSharedPtr<FJsonValue>>& Pl = St->GetArrayField(TEXT("platform_s"));
	const double P0 = Pl[0]->AsNumber(), P1 = Pl[1]->AsNumber();
	const double Edge = ULyudRailwaySubsystem::PlatformEdge * 100.0, Wd = ULyudRailwaySubsystem::PlatformWidth * 100.0;
	const double Up = ULyudRailwaySubsystem::PlatformOverRail * 100.0;
	const FVector O = R->LineToWorld(S, 0.0, 0.0);
	FLyudMeshKit K;
	auto L = [&](double Sx, double SideCm, double Zcm) { return R->LineToWorld(Sx, SideCm / 100.0, 0.0) + FVector(0, 0, Zcm) - FVector(0, 0, R->RailZ(Sx)) - O; };
	auto Deck = [&](double Sx) { return R->RailZ(Sx) + Up; };
	// the deck: planks across 1 m bays, a white edge, piles down to the bed every 3 m, a railing at the back
	for (double Sa = P0; Sa < P1 - 0.01; Sa += 1.0)
	{
		const double Sb = FMath::Min(Sa + 1.0, P1);
		const double Za = Deck(Sa), Zb = Deck(Sb);
		const double Ym = Side * (Edge + Wd * 0.5);
		const double Pitch = FMath::RadiansToDegrees(FMath::Atan2(Za - Zb, (Sb - Sa) * 100.0));
		K.Box(TEXT("tx_planks"), FTransform(FRotator(Pitch, 0.0, 0.0), L((Sa + Sb) * 0.5, Ym, (Za + Zb) * 0.5 - 6.0)),
			FVector((Sb - Sa) * 50.0 - 1.0, Wd * 0.5, 6.0), true);
		K.Box(TEXT("white"), FTransform(FRotator(Pitch, 0.0, 0.0), L((Sa + Sb) * 0.5, Side * (Edge + 12.0), (Za + Zb) * 0.5 + 0.5)), FVector((Sb - Sa) * 50.0, 6.0, 0.6), false);
	}
	for (double Sx = P0 + 0.5; Sx <= P1; Sx += 3.0)
	{
		for (const double Yo : { Edge + 30.0, Edge + Wd - 30.0 })
		{
			const double G = FMath::Max(R->GroundZ(Sx), SeaZcm - 1200.0);
			if (Deck(Sx) - G > 30.0)
			{
				K.Rod(TEXT("pile"), L(Sx, Side * Yo, Deck(Sx) - 12.0), L(Sx, Side * Yo, G - 50.0), 12.f, 8, false, false);
			}
		}
		K.Rod(TEXT("wood"), L(Sx, Side * (Edge + Wd - 8.0), Deck(Sx)), L(Sx, Side * (Edge + Wd - 8.0), Deck(Sx) + 100.0), 3.f, 6, true, false);
	}
	for (const double Hr : { 55.0, 100.0 })
	{
		K.Rod(TEXT("wood"), L(P0 + 0.5, Side * (Edge + Wd - 8.0), Deck(P0 + 0.5) + Hr), L(P1, Side * (Edge + Wd - 8.0), Deck(P1) + Hr), 3.f, 6, false, false);
	}
	K.CollideBox(FTransform(L((P0 + P1) * 0.5, Side * (Edge + Wd - 8.0), Deck((P0 + P1) * 0.5) + 55.0)), FVector((P1 - P0) * 50.0, 4.0, 55.0));
	// the ramp off the land end down to the beach (or the island's ground)
	const TSharedPtr<FJsonObject>* Ramp = nullptr;
	if (St->TryGetObjectField(TEXT("ramp"), Ramp) && Ramp && Ramp->IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>& Tp = (*Ramp)->GetArrayField(TEXT("top"));
		const TArray<TSharedPtr<FJsonValue>>& Ft = (*Ramp)->GetArrayField(TEXT("foot"));
		const FVector A = R->MapToWorld(Tp[0]->AsNumber(), Tp[1]->AsNumber(), Tp[2]->AsNumber()) - O;
		const FVector B = R->MapToWorld(Ft[0]->AsNumber(), Ft[1]->AsNumber(), Ft[2]->AsNumber()) - O;
		const FVector D = B - A;
		const double Len = D.Size2D();
		const double Pitch = FMath::RadiansToDegrees(FMath::Atan2(D.Z, Len));
		const FRotator Rot(Pitch, D.Rotation().Yaw, 0.0);
		const FVector M = (A + B) * 0.5 - FVector(0, 0, 6.0);
		K.Box(TEXT("tx_planks"), FTransform(Rot, M), FVector(Len * 0.5 + 10.0, 80.0, 6.0), true);
		for (int32 Sd = -1; Sd <= 1; Sd += 2)
		{
			const FVector Off = FRotator(0.0, Rot.Yaw, 0.0).RotateVector(FVector(0, Sd * 78.0, 0));
			K.Rod(TEXT("wood"), A + Off + FVector(0, 0, 95.0), B + Off + FVector(0, 0, 95.0), 3.f, 6, false, false);
			for (double T = 0.0; T <= 1.0; T += 0.25)
			{
				const FVector Pp = FMath::Lerp(A, B, T) + Off;
				K.Rod(TEXT("wood"), Pp, Pp + FVector(0, 0, 95.0), 3.f, 6, true, false);
				const double G = FMath::Max(R->GroundZ(FMath::Lerp(P0 - Len / 100.0, P0, 1.0 - T)), SeaZcm - 1200.0) - O.Z;
				if (Pp.Z - G > 40.0)
				{
					K.Rod(TEXT("pile"), Pp - FVector(0, 0, 12.0), FVector(Pp.X, Pp.Y, G - 40.0), 10.f, 8, false, false);
				}
			}
		}
	}
	AddKit(K, FName(*FString::Printf(TEXT("Halt_%s"), *Id)), O, false, true);

	// the shelter at the back of the deck, open to the track; the nameboard; two lamps
	const double Mid = S + 3.5;
	const FVector ShelterAt = R->LineToWorld(Mid, Side * (Edge + Wd - 160.0) / 100.0, 0.0);
	AddPiece(TEXT("SM_RL_Shelter"), FTransform(FRotator(0.0, Side < 0 ? 180.0 : 0.0, 0.0), FVector(ShelterAt.X, ShelterAt.Y, Deck(Mid))));
	const double Nb = S - 9.0;
	const FVector BoardAt = R->LineToWorld(Nb, Side * (Edge + Wd - 45.0) / 100.0, 0.0);
	const FVector BoardBase(BoardAt.X, BoardAt.Y, Deck(Nb));
	AddPiece(TEXT("SM_RL_NameBoard"), FTransform(FRotator(0.0, Side < 0 ? 180.0 : 0.0, 0.0), BoardBase));
	// the board's words (rail.stop.<id> and the neighbours either way), a widget on each face
	const TArray<TSharedPtr<FJsonValue>>* AllStops = nullptr;
	FString Prev = TEXT("rail.stop.tunnel"), Next = TEXT("rail.stop.far");
	(void)AllStops;
	if (Id == TEXT("island"))
	{
		Prev = TEXT("rail.stop.beach");
	}
	else
	{
		Next = TEXT("rail.stop.island");
	}
	const FString Key = St->GetStringField(TEXT("name"));
	for (int32 Face = 0; Face < 2; Face++)
	{
		const bool bTrack = Face == 0;
		// the board faces the track (+Y for a south platform) and its back the platform
		const double Ny = (bTrack ? -Side : Side);
		TSharedRef<SWidget> W = SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.93f, 0.93f, 0.9f))
			.Padding(FMargin(16.f, 6.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text(LyudText::Get(Key)).Font(FCoreStyle::GetDefaultFontStyle("Bold", 46)).ColorAndOpacity(FLinearColor(0.02f, 0.02f, 0.02f))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.05f, 0.25f, 0.14f)).Padding(FMargin(10.f, 2.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Left)
						[
							SNew(STextBlock).Text(FText::FromString(FString(TEXT("← ")) + LyudText::Get(bTrack == (Side < 0) ? Prev : Next).ToString()))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 20)).ColorAndOpacity(FLinearColor::White)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right)
						[
							SNew(STextBlock).Text(FText::FromString(LyudText::Get(bTrack == (Side < 0) ? Next : Prev).ToString() + TEXT(" →")))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 20)).ColorAndOpacity(FLinearColor::White)
						]
					]
				]
			];
		UWidgetComponent* Wc = NewObject<UWidgetComponent>(this);
		Wc->SetupAttachment(RootComponent);
		Wc->SetWidgetSpace(EWidgetSpace::World);
		Wc->SetDrawSize(FVector2D(560.f, 190.f));
		Wc->SetPivot(FVector2D(0.5f, 0.5f));
		Wc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Wc->SetBlendMode(EWidgetBlendMode::Opaque);
		Wc->RegisterComponent();
		// a widget's face looks down its -X... it faces +X: turn it to face Ny
		Wc->SetWorldLocationAndRotation(BoardBase + FVector(0.0, Ny * 3.2, 175.0), FRotator(0.0, Ny > 0 ? 90.0 : -90.0, 0.0));
		Wc->SetWorldScale3D(FVector(0.39f));
		Wc->SetSlateWidget(W);
		AddInstanceComponent(Wc);
	}
	// lamps: a post with a warm globe at either end of the deck, lit at dusk
	for (const double Ls : { P0 + 3.0, P1 - 3.0 })
	{
		const FVector Base = R->LineToWorld(Ls, Side * (Edge + Wd - 25.0) / 100.0, 0.0);
		const FVector B(Base.X, Base.Y, Deck(Ls));
		FLyudMeshKit Lk;
		Lk.Rod(TEXT("wood"), FVector::ZeroVector, FVector(0, 0, 330.0), 6.f, 8, true, false);
		Lk.Rod(TEXT("black"), FVector(0, 0, 320.0), FVector(0, -Side * 60.0, 320.0), 2.5f, 6, false, false);
		AddKit(Lk, NAME_None, B, false, false);
		FLyudMeshKit Gk;
		Gk.Lathe(FTransform(FVector(0, -Side * 60.0, 290.0)), { FVector2D(0.0, 0.0), FVector2D(11.0, 6.0), FVector2D(13.0, 18.0), FVector2D(8.0, 28.0), FVector2D(0.0, 30.0) },
			{ TEXT("glow_lamp"), TEXT("glow_lamp"), TEXT("glow_lamp"), TEXT("glow_lamp") }, 12, false, false);
		if (UStaticMeshComponent* G = AddKit(Gk, NAME_None, B, false, false))
		{
			UMaterialInstanceDynamic* M = LyudRailMat::Glow(this, FLinearColor(1.f, 0.74f, 0.42f), 0.f);
			G->SetMaterial(0, M);
			LampGlows.Add(M);
		}
		UPointLightComponent* Pt = NewObject<UPointLightComponent>(this);
		Pt->SetupAttachment(RootComponent);
		Pt->SetMobility(EComponentMobility::Movable);
		Pt->SetLightColor(FLinearColor(1.f, 0.72f, 0.42f));
		Pt->SetIntensityUnits(ELightUnits::Lumens);
		Pt->SetIntensity(0.f);
		Pt->SetAttenuationRadius(1400.f);
		Pt->SetCastShadows(false);
		Pt->LightingChannels.bChannel1 = true;
		Pt->RegisterComponent();
		Pt->SetWorldLocation(B + FVector(0, -Side * 60.0, 280.0));
		AddInstanceComponent(Pt);
		Lamps.Add(Pt);
	}
}

void ALyudRailTrack::BuildIsland(const TSharedPtr<FJsonObject>& Built)
{
	ULyudRailwaySubsystem* R = Railway;
	const TSharedPtr<FJsonObject> Isl = Built->GetObjectField(TEXT("island"));
	const double Cx = Isl->GetNumberField(TEXT("x")), Cy = Isl->GetNumberField(TEXT("y"));
	const double Rx = Isl->GetNumberField(TEXT("rx")), Ry = Isl->GetNumberField(TEXT("ry"));
	UStaticMeshComponent* Ground = AddPiece(TEXT("SM_RL_Island"), FTransform(R->MapToWorld(Cx, Cy, -43.0)), true);
	if (!Ground)
	{
		UE_LOG(LogTemp, Warning, TEXT("Railway: no SM_RL_Island (unreal/scripts/import_railway.sh): the island is missing"));
		return;
	}
	Ground->ComponentTags.Add(TEXT("LyudSeaBed"));
	// the kit's flat paints -> the railway's photographed surfaces (UVs in metres)
	const TArray<FName> SlotNames = Ground->GetMaterialSlotNames();
	for (int32 I = 0; I < SlotNames.Num(); I++)
	{
		const FString N = SlotNames[I].ToString();
		const TCHAR* Tx = N.Contains(TEXT("wetsand")) ? TEXT("tx_wetsand") : (N.Contains(TEXT("grass")) ? TEXT("tx_turf")
			: (N.Contains(TEXT("rock")) ? TEXT("tx_lichen") : nullptr));
		if (Tx)
		{
			Ground->SetMaterial(I, Mat(Tx));
		}
	}
	Ground->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// a few wind-bent pines, junipers and sea buckthorn on its crown (procedural plants like every plant in the game)
	FRandomStream Rnd(int32(Isl->GetNumberField(TEXT("seed"))));
	UWorld* World = GetWorld();
	int32 N = 0;
	for (int32 Tries = 0; Tries < 120 && N < 14; Tries++)
	{
		const double A = Rnd.FRandRange(0.0, 2.0 * PI), Rr = FMath::Sqrt(Rnd.FRand()) * 0.8;
		const double X = Cx + FMath::Cos(A) * Rx * Rr, Y = Cy + FMath::Sin(A) * Ry * Rr;
		if (FMath::Abs(Y - R->LineY) < 14.0)
		{
			continue;    // the track and the halt
		}
		const FVector Top = R->MapToWorld(X, Y, 0.0);
		FHitResult Hit;
		FCollisionQueryParams Qp(SCENE_QUERY_STAT(RailIsland), true);
		if (!World->LineTraceSingleByObjectType(Hit, FVector(Top.X, Top.Y, -2000.0), FVector(Top.X, Top.Y, -6000.0), FCollisionObjectQueryParams(ECC_WorldStatic), Qp)
			|| Hit.GetComponent() != Ground || Hit.ImpactPoint.Z < SeaZcm + 120.0 || Hit.ImpactNormal.Z < 0.85)
		{
			continue;
		}
		const float U = Rnd.FRand();
		const FName Sp = U < 0.55f ? FName(TEXT("DunePine")) : (U < 0.8f ? FName(TEXT("Juniper")) : FName(TEXT("SeaBuckthorn")));
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		P.bDeferConstruction = true;
		AProceduralPlant* Plant = World->SpawnActor<AProceduralPlant>(AProceduralPlant::StaticClass(), FTransform(FRotator(0.0, Rnd.FRandRange(0.0, 360.0), 0.0), Hit.ImpactPoint), P);
		if (Plant)
		{
			Plant->Species = Sp;
			Plant->Seed = 1000 + N;
			Plant->AgeAtStart = Sp == TEXT("DunePine") ? Rnd.FRandRange(18.f, 45.f) : Rnd.FRandRange(8.f, 20.f);
			Plant->FinishSpawning(FTransform(FRotator(0.0, Rnd.FRandRange(0.0, 360.0), 0.0), Hit.ImpactPoint));
			N++;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Railway: island at map (%.0f, %.0f), %d plants"), Cx, Cy, N);
}

void ALyudRailTrack::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// the halts' lamps: on from a little before sunset to after sunrise
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
	const float On = FMath::Clamp((3.f - Alt) / 4.f, 0.f, 1.f);
	if (FMath::Abs(On - LampOn) < 0.01f)
	{
		return;
	}
	LampOn = On;
	for (UPointLightComponent* L : Lamps)
	{
		if (L)
		{
			L->SetIntensity(1800.f * On);
			L->SetVisibility(On > 0.f);
		}
	}
	for (UMaterialInstanceDynamic* M : LampGlows)
	{
		if (M)
		{
			M->SetScalarParameterValue(TEXT("Intensity"), 0.3f + 9.f * On);
		}
	}
}
