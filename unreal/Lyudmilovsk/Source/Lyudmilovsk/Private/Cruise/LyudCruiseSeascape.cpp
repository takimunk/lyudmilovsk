#include "Cruise/LyudCruiseSeascape.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Cruise/LyudCruise.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "../Transport/LyudMeshKit.h"
#include "Sea/LyudSea.h"
#include "SeascapeMath.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sky/LyudSky.h"
#include "Weather/LyudRainFog.h"

namespace
{
TAutoConsoleVariable<float> CVarCruiseEarth(TEXT("lyud.Cruise.EarthRadius"), 2.5e6f,
	TEXT("The Earth's radius the cruise's seascape sinks far islands by, m (\"hull down\": the real 6.4e6 hides too little inside the haze; smaller = islands rise later and faster)."),
	ECVF_Default);
TAutoConsoleVariable<float> CVarCruiseRange(TEXT("lyud.Cruise.IslandRange"), 18000.f,
	TEXT("The cruise's islands are shown within this many metres of the ship (the coast and the volcanoes have their own, longer ranges)."),
	ECVF_Default);

constexpr float SlowBeyondM = 3000.f;      // pieces farther than this are moved ten times a second
constexpr float ShadowWithinM = 1500.f;
constexpr float LightPeriodS = 10.f;       // the Far Light: one flash in ten seconds
constexpr float PierPeriodS = 6.f;

UStaticMesh* SeascapeMesh(const FString& Name)
{
	// Interchange puts a glTF scene's meshes in <folder>/<file>/StaticMeshes (or straight in the folder)
	for (const TCHAR* Dir : { TEXT("/Game/Cruise/Meshes/seascape_kit/StaticMeshes"), TEXT("/Game/Cruise/Meshes/seascape_kit") })
	{
		const FString P = FString::Printf(TEXT("%s/%s.%s"), Dir, *Name, *Name);
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return M;
		}
	}
	return nullptr;
}

// (not "ReadJson": LyudQuests.cpp has one of that name and signature in its own unnamed namespace: a unity build would see two)
TSharedPtr<FJsonObject> SeascapeJson(const TCHAR* Rel)
{
	FString Text;
	TSharedPtr<FJsonObject> J;
	if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), Rel)))
	{
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J);
	}
	return J;
}
}

ALyudCruiseSeascape::ALyudCruiseSeascape()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

int32 ALyudCruiseSeascape::AddMesh(const FString& Name)
{
	if (Name.IsEmpty())
	{
		return INDEX_NONE;
	}
	UStaticMesh* M = SeascapeMesh(Name);
	return M ? Meshes.Add(M) : INDEX_NONE;
}

UStaticMeshComponent* ALyudCruiseSeascape::MakeComp(const TCHAR* Name, UStaticMesh* Mesh)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), Name));
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->SetCastShadow(false);
	// moved all the time and miles off: out of the Lumen scene and the distance fields, never culled by distance
	C->bAffectDynamicIndirectLighting = false;
	C->bAffectDistanceFieldLighting = false;
	C->bNeverDistanceCull = true;
	C->LDMaxDrawDistance = 0.f;
	C->SetUsingAbsoluteLocation(true);
	C->SetUsingAbsoluteRotation(true);
	C->SetUsingAbsoluteScale(true);
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	C->SetVisibility(false);
	AddInstanceComponent(C);
	return C;
}

void ALyudCruiseSeascape::DressGlow(UStaticMeshComponent* Comp)
{
	if (!Comp || !GlowMaster)
	{
		return;
	}
	const TArray<FName> Slots = Comp->GetMaterialSlotNames();
	for (int32 I = 0; I < Slots.Num(); I++)
	{
		const FString S = Slots[I].ToString();
		for (const TPair<FName, TObjectPtr<UMaterialInstanceDynamic>>& G : Glows)
		{
			if (S.Contains(G.Key.ToString()))
			{
				Comp->SetMaterial(I, G.Value);
			}
		}
	}
}

int32 ALyudCruiseSeascape::FindPiece(FName Id) const
{
	return Pieces.IndexOfByPredicate([Id](const FPiece& P) { return P.Id == Id; });
}

bool ALyudCruiseSeascape::Setup()
{
	const TSharedPtr<FJsonObject> Kit = SeascapeJson(TEXT("Cruise/seascape.json"));
	const TSharedPtr<FJsonObject>* Islands = nullptr;
	if (!Kit.IsValid() || !Kit->TryGetObjectField(TEXT("islands"), Islands) || !Islands)
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudCruiseSeascape: no Content/Cruise/seascape.json (blender/ship/seascape_kit.py): no islands"));
		return false;
	}
	GlowMaster = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Transport/Materials/M_TR_Glow.M_TR_Glow"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (GlowMaster)
	{
		UMaterialInstanceDynamic* Lamp = UMaterialInstanceDynamic::Create(GlowMaster, this);
		Lamp->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.93f, 0.72f));
		Glows.Add(TEXT("glow_lamp"), Lamp);
		UMaterialInstanceDynamic* Window = UMaterialInstanceDynamic::Create(GlowMaster, this);
		Window->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.78f, 0.45f));
		Glows.Add(TEXT("glow_window"), Window);
	}
	// the chart places of cruise.json win over the kit's own (tools/ship/cruise.py may move an island)
	const TSharedPtr<FJsonObject> Cruise = SeascapeJson(TEXT("Cruise/cruise.json"));
	TMap<FString, TSharedPtr<FJsonObject>> Charted;
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Cruise.IsValid() && Cruise->TryGetArrayField(TEXT("islands"), List) && List)
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
			if (O.IsValid())
			{
				Charted.Add(O->GetStringField(TEXT("id")), O);
			}
		}
	}
	int32 Missing = 0;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& It : (*Islands)->Values)
	{
		const TSharedPtr<FJsonObject> O = It.Value.IsValid() ? It.Value->AsObject() : nullptr;
		if (!O.IsValid())
		{
			continue;
		}
		FPiece P;
		P.Id = FName(*It.Key);
		P.Near = AddMesh(O->GetStringField(TEXT("mesh")));
		FString FarName, LightsName;
		O->TryGetStringField(TEXT("far"), FarName);
		O->TryGetStringField(TEXT("lights"), LightsName);
		P.Far = AddMesh(FarName);
		P.Lights = AddMesh(LightsName);
		if (P.Near == INDEX_NONE)
		{
			Missing++;
			continue;
		}
		const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
		if (O->TryGetArrayField(TEXT("chart"), At) && At && At->Num() >= 2)
		{
			P.Chart = FVector2D((*At)[0]->AsNumber(), (*At)[1]->AsNumber());
		}
		P.YawDeg = float(O->GetNumberField(TEXT("yaw")));
		P.TopM = float(O->GetNumberField(TEXT("top")));
		P.RadiusM = float(O->GetNumberField(TEXT("radius")));
		const bool bVolcano = It.Key.StartsWith(TEXT("volcano_"));
		P.RangeM = bVolcano ? 60000.f : (It.Key == TEXT("home") ? 26000.f : 0.f);      // 0: lyud.Cruise.IslandRange
		P.FarM = P.Far != INDEX_NONE ? 4000.f : 1e9f;
		const TSharedPtr<FJsonObject>* Socks = nullptr;
		if (O->TryGetObjectField(TEXT("sockets"), Socks) && Socks)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& S : (*Socks)->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
				if (S.Value.IsValid() && S.Value->TryGetArray(A) && A && A->Num() == 3 && (*A)[0]->Type == EJson::Number)
				{
					P.Sockets.Add(FName(*S.Key), FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber()) * 100.0);
				}
				else if (S.Key == TEXT("eye_axis_deg") && S.Value.IsValid())
				{
					P.EyeAxisDeg = float(S.Value->AsNumber());
					P.bFaceSun = true;
				}
			}
		}
		if (const TSharedPtr<FJsonObject>* C = Charted.Find(It.Key))
		{
			P.Chart = FVector2D((*C)->GetNumberField(TEXT("x")), (*C)->GetNumberField(TEXT("y")));
			// the bay the ship anchors in: the piece is turned (by a half turn at most: its long axis stays) and shifted
			// so that its "anchorage" socket lies on the chart's bay point
			const TArray<TSharedPtr<FJsonValue>>* Bay = nullptr;
			const FVector* Anchor = P.Sockets.Find(TEXT("anchorage"));
			if (Anchor && (*C)->TryGetArrayField(TEXT("bay"), Bay) && Bay && Bay->Num() >= 2)
			{
				const FVector2D BayAt((*Bay)[0]->AsNumber(), (*Bay)[1]->AsNumber());
				const FVector2D Want = BayAt - P.Chart;
				const FVector2D Sock(Anchor->X * 0.01, Anchor->Y * 0.01);
				FVector2D Turned = Sock.GetRotated(P.YawDeg);
				if (FVector2D::DotProduct(Turned, Want) < 0.0)
				{
					P.YawDeg += 180.f;
					Turned = Sock.GetRotated(P.YawDeg);
				}
				P.Chart = BayAt - Turned;
			}
		}
		const FString CompName = FString::Printf(TEXT("Seascape_%s"), *It.Key);
		UStaticMeshComponent* C = MakeComp(*CompName, Meshes[P.Near]);
		DressGlow(C);
		P.Comp = Comps.Add(C);
		if (P.Lights != INDEX_NONE)
		{
			const FString LName = CompName + TEXT("_Lights");
			UStaticMeshComponent* L = MakeComp(*LName, Meshes[P.Lights]);
			DressGlow(L);
			P.LightsComp = Comps.Add(L);
		}
		Pieces.Add(MoveTemp(P));
	}
	// the points of light: the Far Light's lamp, the pier's light at home
	UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	auto AddStar = [&](FName Piece, FName Socket, float Period, const FLinearColor& Colour)
	{
		const int32 PieceIndex = FindPiece(Piece);
		if (PieceIndex == INDEX_NONE || !Pieces[PieceIndex].Sockets.Contains(Socket) || !Ball)
		{
			return;
		}
		FStar S;
		S.Piece = PieceIndex;
		S.Socket = Socket;
		S.PeriodS = Period;
		const FString Name = FString::Printf(TEXT("Star_%s"), *Socket.ToString());
		UStaticMeshComponent* C = MakeComp(*Name, Ball);
		if (GlowMaster)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(GlowMaster, this);
			M->SetVectorParameterValue(TEXT("Color"), Colour);
			M->SetScalarParameterValue(TEXT("Intensity"), 40.f);
			C->SetMaterial(0, M);
			S.Mid = StarMids.Add(M);
		}
		S.Comp = Comps.Add(C);
		Stars.Add(S);
	};
	AddStar(TEXT("lighthouse"), TEXT("lamp"), LightPeriodS, FLinearColor(1.f, 0.95f, 0.8f));
	AddStar(TEXT("home"), TEXT("pier_light"), PierPeriodS, FLinearColor(1.f, 0.9f, 0.7f));
	// the beam: two crossed cards along +X from the lamp, widening; both faces
	if (FindPiece(TEXT("lighthouse")) != INDEX_NONE)
	{
		FLyudMeshKit K;
		const float L0 = 250.f, L1 = 110000.f;
		auto Card = [&K, L0, L1](const FVector& Side0, const FVector& Side1)
		{
			const FVector A(L0, 0.f, 0.f), B(L1, 0.f, 0.f);
			K.Quad(TEXT("beam"), A - Side0, B - Side1, B + Side1, A + Side0);
			K.Quad(TEXT("beam"), A + Side0, B + Side1, B - Side1, A - Side0);
		};
		Card(FVector(0.f, 70.f, 0.f), FVector(0.f, 2600.f, 0.f));
		Card(FVector(0.f, 0.f, 70.f), FVector(0.f, 0.f, 1100.f));
		TArray<FName> Slots;
		BeamMesh = K.Build(this, MakeUniqueObjectName(this, UStaticMesh::StaticClass(), TEXT("SeascapeBeam")), Slots);
		if (BeamMesh)
		{
			Beam = MakeComp(TEXT("SeascapeBeamComp"), BeamMesh);
			if (UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Transport/Materials/M_TR_Glass.M_TR_Glass"), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				BeamMid = UMaterialInstanceDynamic::Create(Glass, this);
				BeamMid->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.96f, 0.85f));
				BeamMid->SetScalarParameterValue(TEXT("Opacity"), 0.05f);
				Beam->SetMaterial(0, BeamMid);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("LyudCruiseSeascape: %d pieces, %d without their mesh (unreal/scripts/import_cruise.sh)"), Pieces.Num(), Missing);
	return true;
}

void ALyudCruiseSeascape::HideAll()
{
	for (FPiece& P : Pieces)
	{
		if (P.bShown)
		{
			P.bShown = false;
			P.bMeshHidden = false;
			Comps[P.Comp]->SetVisibility(false);
		}
		if (P.bLit && P.LightsComp != INDEX_NONE)
		{
			P.bLit = false;
			Comps[P.LightsComp]->SetVisibility(false);
		}
	}
	for (FStar& S : Stars)
	{
		if (S.bShown)
		{
			S.bShown = false;
			Comps[S.Comp]->SetVisibility(false);
		}
	}
	if (Beam && bBeamShown)
	{
		bBeamShown = false;
		Beam->SetVisibility(false);
	}
}

void ALyudCruiseSeascape::Place(const FLyudCruiseView& View, const FTransform& Stage, float DeltaSeconds)
{
	if (!Sea.IsValid())
	{
		Sea = ALyudSea::Get(this);
	}
	if (!Sky.IsValid())
	{
		TActorIterator<ALyudSky> It(GetWorld());
		if (It)
		{
			Sky = *It;
		}
	}
	WeatherClock -= DeltaSeconds;
	if (WeatherClock <= 0.f)
	{
		WeatherClock = 0.5f;
		if (const ULyudRainFogSubsystem* Fog = ULyudRainFogSubsystem::Get(this))
		{
			VisibilityM = Fog->ReadWeather().VisibilityM;
		}
	}
	const float Datum = Sea.IsValid() ? Sea->DatumZ() : float(Stage.GetLocation().Z);
	const float Level = Sea.IsValid() ? Sea->SeaLevel() : Datum;
	FVector Eye = Stage.GetLocation() + FVector(0.0, 0.0, 800.0);
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		Eye = Cam->GetCameraLocation();
	}
	const double EyeM = FMath::Clamp((Eye.Z - Level) * 0.01, 2.0, 60.0);
	const double Earth = FMath::Max(CVarCruiseEarth.GetValueOnGameThread(), 1.e5f);
	const float IslandRange = CVarCruiseRange.GetValueOnGameThread();
	LyudSeascape::FShip Ship;
	Ship.Chart = { View.ChartPosM.X, View.ChartPosM.Y };
	Ship.Heading = View.HeadingRad;
	Ship.StageYaw = FMath::DegreesToRadians(Stage.GetRotation().Rotator().Yaw);
	// the ship's chart place is her middle
	const FVector Pivot = Stage.TransformPosition(FVector(3100.0, 0.0, 0.0));
	const float TurnedDeg = FMath::Abs(FMath::RadiansToDegrees(float(LyudSeascape::WrapAngle(View.HeadingRad - LastHeading))));
	const bool bTurning = TurnedDeg > 0.02f;
	LastHeading = View.HeadingRad;
	// a low sun: the Needle's Eye looks at it
	float SunYawDeg = 0.f, SunLock = 0.f;
	if (const ALyudSky* S = Sky.Get())
	{
		const FVector D = S->SunDirection();
		SunYawDeg = FMath::RadiansToDegrees(float(FMath::Atan2(D.Y, D.X)));
		const float Alt = S->SunAltitude();
		SunLock = FMath::SmoothStep(-8.f, -3.f, Alt) * (1.f - FMath::SmoothStep(10.f, 22.f, Alt));
	}
	const float StageMinusHeadingDeg = FMath::RadiansToDegrees(float(Ship.StageYaw - Ship.Heading));
	for (FPiece& P : Pieces)
	{
		const LyudSeascape::FVec2 L = LyudSeascape::ChartToStage(Ship, { P.Chart.X, P.Chart.Y });
		const double Dist = FMath::Sqrt(L.X * L.X + L.Y * L.Y);
		const double Hidden = LyudSeascape::HullDownM(Dist, EyeM, Earth);
		P.DistM = float(Dist);
		bool bShow = LyudSeascape::IsShown(Dist, P.RangeM > 0.f ? P.RangeM : IslandRange, VisibilityM, P.TopM, Hidden);
		if (P.RangeM > 30000.f && Dist < P.RadiusM + 300.0)
		{
			bShow = false;      // the ship inside a backdrop cone (a route that crosses one): better no cone than its inside
		}
		UStaticMeshComponent* C = Comps[P.Comp];
		if (bShow != P.bShown)
		{
			P.bShown = bShow;
			C->SetVisibility(bShow && !P.bMeshHidden);
			P.SlowClock = 1.f;
		}
		// the landing at the Far Light: the real rock stands exactly here while the ship lies at the stop (ULyudCruise shows
		// it); the copy's mesh gives way, the piece stays "shown" for its lamp and beam
		static const FName LandingPiece(TEXT("lighthouse"));
		const bool bHideMesh = bShow && bRealRock && P.Id == LandingPiece;
		if (bHideMesh != P.bMeshHidden)
		{
			P.bMeshHidden = bHideMesh;
			C->SetVisibility(bShow && !bHideMesh);
		}
		if (!bShow)
		{
			continue;
		}
		const bool bFar = LyudSeascape::UseFar(Dist, P.FarM, P.bFar);
		if (bFar != P.bFar)
		{
			P.bFar = bFar;
			C->SetStaticMesh(Meshes[bFar ? P.Far : P.Near]);
			// the two levels need not have their slots in the same order: no glow left on a slot that is rock now
			C->EmptyOverrideMaterials();
			DressGlow(C);
		}
		const bool bShadow = Dist < ShadowWithinM;
		if (bShadow != P.bShadow)
		{
			P.bShadow = bShadow;
			C->SetCastShadow(bShadow);
		}
		// far pieces move by less than a pixel a frame: ten times a second is enough (every frame while she turns)
		P.SlowClock += DeltaSeconds;
		if (Dist > SlowBeyondM && !bTurning && P.SlowClock < 0.1f)
		{
			continue;
		}
		P.SlowClock = 0.f;
		float ChartYaw = P.YawDeg;
		if (P.bFaceSun && SunLock > 0.f)
		{
			// the eye's axis is a line: of its two ways take the one nearer the piece's own yaw
			float Locked = SunYawDeg - P.EyeAxisDeg - StageMinusHeadingDeg;
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(P.YawDeg, Locked)) > 90.f)
			{
				Locked += 180.f;
			}
			ChartYaw = P.YawDeg + SunLock * FMath::FindDeltaAngleDegrees(P.YawDeg, Locked);
		}
		const float Yaw = FMath::RadiansToDegrees(float(LyudSeascape::YawToStage(Ship, FMath::DegreesToRadians(ChartYaw))));
		const FVector At(Pivot.X + L.X * 100.0, Pivot.Y + L.Y * 100.0, Datum - Hidden * 100.0);
		P.World = FTransform(FRotator(0.f, Yaw, 0.f), At);
		C->SetWorldLocationAndRotation(At, FRotator(0.f, Yaw, 0.f));
		if (P.LightsComp != INDEX_NONE)
		{
			Comps[P.LightsComp]->SetWorldLocationAndRotation(At, FRotator(0.f, Yaw, 0.f));
		}
	}
	UpdateLights(Eye, DeltaSeconds);
}

void ALyudCruiseSeascape::UpdateLights(const FVector& Eye, float DeltaSeconds)
{
	const float Alt = Sky.IsValid() ? Sky->SunAltitude() : 20.f;
	Dark = FMath::Clamp((4.f - Alt) / 8.f, 0.f, 1.f);        // 0 day .. 1 night, as the train's lamps
	Dusk = FMath::Clamp((12.f - Alt) / 14.f, 0.f, 1.f);
	const float Glow = Dusk + Dark;
	if (FMath::Abs(Glow - LastGlow) > 0.02f)
	{
		LastGlow = Glow;
		if (const TObjectPtr<UMaterialInstanceDynamic>* M = Glows.Find(TEXT("glow_window")))
		{
			// seen from miles off: strong, or the windows are lost in the haze
			(*M)->SetScalarParameterValue(TEXT("Intensity"), 0.2f + 30.f * Dusk + 60.f * Dark);
		}
		if (const TObjectPtr<UMaterialInstanceDynamic>* M = Glows.Find(TEXT("glow_lamp")))
		{
			(*M)->SetScalarParameterValue(TEXT("Intensity"), 0.5f + 40.f * Dusk + 80.f * Dark);
		}
	}
	for (FPiece& P : Pieces)
	{
		if (P.LightsComp == INDEX_NONE)
		{
			continue;
		}
		const bool bLit = P.bShown && Dusk > 0.15f;
		if (bLit != P.bLit)
		{
			P.bLit = bLit;
			Comps[P.LightsComp]->SetVisibility(bLit);
		}
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	FVector LampAt = FVector::ZeroVector;
	bool bLamp = false;
	for (FStar& S : Stars)
	{
		const FPiece& P = Pieces[S.Piece];
		const bool bShow = P.bShown && Dark > 0.15f && P.DistM < S.RangeM;
		UStaticMeshComponent* C = Comps[S.Comp];
		if (bShow != S.bShown)
		{
			S.bShown = bShow;
			C->SetVisibility(bShow);
		}
		if (!bShow)
		{
			continue;
		}
		const FVector At = P.World.TransformPosition(P.Sockets.FindRef(S.Socket));
		const FVector To = Eye - At;
		float Flash = 0.6f;
		if (S.PeriodS > 0.f)
		{
			Flash = float(LyudSeascape::BeamAt(Now, S.PeriodS, FMath::Atan2(To.Y, To.X), 0.14));
		}
		// kept a pixel or two wide whatever the distance: about a thousandth of the range, swelling in the flash
		const float SizeCm = FMath::Max(90.f, float(To.Size()) * 0.0011f) * (0.45f + 0.9f * FMath::Sqrt(Flash));
		C->SetWorldLocation(At);
		C->SetWorldScale3D(FVector(SizeCm / 100.f));
		if (StarMids.IsValidIndex(S.Mid))
		{
			StarMids[S.Mid]->SetScalarParameterValue(TEXT("Intensity"), (15.f + 260.f * Flash) * Dark);
		}
		if (S.Socket == TEXT("lamp"))
		{
			LampAt = At;
			bLamp = P.DistM < 9000.f;
		}
	}
	if (Beam)
	{
		if (bLamp != bBeamShown)
		{
			bBeamShown = bLamp;
			Beam->SetVisibility(bLamp);
		}
		if (bLamp)
		{
			const float Yaw = FMath::RadiansToDegrees(float(LyudSeascape::BeamAngle(Now, LightPeriodS)));
			Beam->SetWorldLocationAndRotation(LampAt, FRotator(-1.5f, Yaw, 0.f));
			if (BeamMid)
			{
				BeamMid->SetScalarParameterValue(TEXT("Opacity"), 0.06f * Dark);
			}
		}
	}
	(void)DeltaSeconds;
}

bool ALyudCruiseSeascape::GetIslandWorld(FName Id, FVector& OutOrigin, float* OutDistM) const
{
	const int32 I = FindPiece(Id);
	if (I == INDEX_NONE || !Pieces[I].bShown)
	{
		return false;
	}
	OutOrigin = Pieces[I].World.GetLocation();
	if (OutDistM)
	{
		*OutDistM = Pieces[I].DistM;
	}
	return true;
}

bool ALyudCruiseSeascape::GetSocketWorld(FName Island, FName Socket, FVector& OutWorld) const
{
	const int32 I = FindPiece(Island);
	if (I == INDEX_NONE || !Pieces[I].bShown)
	{
		return false;
	}
	const FVector* S = Pieces[I].Sockets.Find(Socket);
	if (!S)
	{
		return false;
	}
	OutWorld = Pieces[I].World.TransformPosition(*S);
	return true;
}

int32 ALyudCruiseSeascape::NumShown() const
{
	int32 N = 0;
	for (const FPiece& P : Pieces)
	{
		N += P.bShown ? 1 : 0;
	}
	return N;
}

FString ALyudCruiseSeascape::Report() const
{
	FString Out = FString::Printf(TEXT("seascape: %d pieces, %d shown, visibility %.0f m, dark %.2f"), Pieces.Num(), NumShown(), VisibilityM, Dark);
	for (const FPiece& P : Pieces)
	{
		Out += FString::Printf(TEXT("\n  %-14s chart %.0f %.0f  %.0f m  %s%s"), *P.Id.ToString(), P.Chart.X, P.Chart.Y, P.DistM,
			P.bShown ? TEXT("shown") : TEXT("hidden"), P.bFar ? TEXT(" far") : TEXT(""));
	}
	return Out;
}
