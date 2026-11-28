#include "Spacecraft/LyudSpacecraft.h"

#include "Spacecraft/FlightModel.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/**
 * The cabin of «Ласточка» (docs/spacecraft.md, tools/spacecraft/ENGINE_NOTES.md "For the cabin programmer",
 * tools/spacecraft/CABIN_NOTES.md). It builds itself from Content/Spacecraft/kit.json and the SM_SC_* meshes, draws
 * what LyudFlight::FState says, animates what the director asks and answers geometry questions. It decides nothing.
 *
 * Everything lives in ALyudSpacecraft::FCabinData (helpers as its static members, so that no name here can meet another
 * file's in a unity build). tools/spacecraftsim/cabincheck.py reads this file: every kit.json path goes through
 * K.Num / K.Vec / K.Frame / K.Arr or FKit::ENum / EVec / EFrame / EStr / EArr, every mesh through LoadMesh, every
 * dynamic material through MakeMid(.., slot, expected material) and every parameter through FParamNames.
 */

struct ALyudSpacecraft::FCabinData
{
	// ------------------------------------------------------------------ indices

	/** ALyudSpacecraft::Parts. */
	enum EPart : int32
	{
		PtHatch, PtWheel,
		PtToggle0, PtToggleLast = PtToggle0 + 7,
		PtButton, PtLaunchCover, PtChuteHandle, PtChuteCover,
		PtPlatter, PtRecord, PtTonearm,
		PtSleeve0, PtSleeveLast = PtSleeve0 + 7,
		PtThermos, PtCap, PtBook,
		PtGlobe, PtBall,
		PtNeedle0, PtNeedleLast = PtNeedle0 + 8,
		PtFireShell, PtChuteRig, PtChute, PtDrogue,
		PtCount
	};

	/** ALyudSpacecraft::Mids. */
	enum EMid : int32
	{
		MidLabels, MidDials, MidNeedle, MidBall, MidGlobe, MidDigits, MidBar, MidLamps, MidCrt, MidGlass, MidGlassCover,
		MidPlasma, MidRecordLabel, MidLampLens,
		MidSleeve0, MidSleeveLast = MidSleeve0 + 7,
		MidCount
	};

	enum { NumToggles = 8, NumSleeves = 8, MaxNeedles = 9, NumLamps = 24, NumCabinLamps = 3 };

	/** What a needle shows. */
	enum ESource : uint8 { SrcAltLong, SrcAltShort, SrcG, SrcPress, SrcO2, SrcCo2, SrcTemp, SrcVolts, SrcFuel, SrcNone };

	/** How an interaction point finds its place and whether it can be seen from the eye. */
	enum EKind : uint8
	{
		KFixed, KWheel, KHatchEdge, KHatchOutside, KHatchOpening, KLaunch, KLaunchCover, KChuteHandle, KChuteCover, KNeedle, KBook
	};

	/** Which set a mesh is shown with (SetShownInside / SetShownOutside); the Own ones are shown by their own rule. */
	enum ESet : uint8 { SetInside, SetOutside, SetShared, SetInsideOwn, SetOutsideOwn };

	// ------------------------------------------------------------------ names

	/** Every material parameter the cabin sets (tools/spacecraft/MATERIAL_NOTES.md), made once. */
	struct FParamNames
	{
		// cabincheck.py reads these lines: the parameter's name, then after PARAM the materials that must have it
		const FName Backlight = TEXT("Backlight");     // PARAM M_SC_Dials M_SC_Needle M_SC_Ball M_SC_Labels
		const FName Atlas = TEXT("Atlas");             // PARAM M_SC_Labels M_SC_Lamps M_SC_Sleeve
		const FName Labels = TEXT("Labels");           // PARAM M_SC_CRT
		const FName Value0 = TEXT("Value0");           // PARAM M_SC_Digits
		const FName Value1 = TEXT("Value1");           // PARAM M_SC_Digits
		const FName Value2 = TEXT("Value2");           // PARAM M_SC_Digits
		const FName Win0 = TEXT("Win0");               // PARAM M_SC_Digits
		const FName Win1 = TEXT("Win1");               // PARAM M_SC_Digits
		const FName Win2 = TEXT("Win2");               // PARAM M_SC_Digits
		const FName Opt0 = TEXT("Opt0");               // PARAM M_SC_Digits
		const FName Opt1 = TEXT("Opt1");               // PARAM M_SC_Digits
		const FName Opt2 = TEXT("Opt2");               // PARAM M_SC_Digits
		const FName Level = TEXT("Level");             // PARAM M_SC_Bar
		const FName LampsA = TEXT("LampsA");           // PARAM M_SC_Lamps
		const FName LampsB = TEXT("LampsB");           // PARAM M_SC_Lamps
		const FName LampsC = TEXT("LampsC");           // PARAM M_SC_Lamps
		const FName LampsD = TEXT("LampsD");           // PARAM M_SC_Lamps
		const FName LampsE = TEXT("LampsE");           // PARAM M_SC_Lamps
		const FName LampsF = TEXT("LampsF");           // PARAM M_SC_Lamps
		const FName Mode = TEXT("Mode");               // PARAM M_SC_CRT
		const FName PosEuropa = TEXT("PosEuropa");     // PARAM M_SC_CRT
		const FName PosPlanet = TEXT("PosPlanet");     // PARAM M_SC_CRT
		const FName Craft01 = TEXT("Craft01");         // PARAM M_SC_CRT
		const FName Phase = TEXT("Phase");             // PARAM M_SC_CRT
		const FName Att = TEXT("Att");                 // PARAM M_SC_CRT
		const FName GLoad = TEXT("GLoad");             // PARAM M_SC_CRT
		const FName Power = TEXT("Power");             // PARAM M_SC_CRT
		const FName Cross = TEXT("Cross");             // PARAM M_SC_Globe
		const FName Frost = TEXT("Frost");             // PARAM M_SC_Glass
		const FName Scorch = TEXT("Scorch");           // PARAM M_SC_Glass
		const FName Wet = TEXT("Wet");                 // PARAM M_SC_Glass
		const FName Fog = TEXT("Fog");                 // PARAM M_SC_Glass
		const FName Light = TEXT("Light");             // PARAM M_SC_Glass
		const FName Plasma = TEXT("Plasma");           // PARAM M_SC_Plasma
		const FName Heat = TEXT("Heat");               // PARAM M_SC_Plasma
		const FName Flow = TEXT("Flow");               // PARAM M_SC_Plasma
		const FName Cell = TEXT("Cell");               // PARAM M_SC_RecordLabel M_SC_Sleeve
		// the glb's own flat material of the lamp lenses (an Interchange glTF instance): UNVERIFIED names, harmless if absent
		const FName EmissiveStrength = TEXT("EmissiveStrength"); // PARAM glb
		const FName EmissiveFactor = TEXT("EmissiveFactor");     // PARAM glb
	};
	static const FParamNames& P()
	{
		static const FParamNames Names;
		return Names;
	}

	/** The action ids PlayActionAnim / IsAnimating / PortholeView take (the director's words). */
	struct FIdNames
	{
		const FName Hatch = TEXT("hatch"), Wheel = TEXT("wheel"), Cover = TEXT("cover"), Launch = TEXT("launch");
		const FName ChuteCover = TEXT("chute_cover"), ChuteHandle = TEXT("chute_handle"), Buckle = TEXT("buckle");
		const FName Book = TEXT("book"), Thermos = TEXT("thermos");
		const FName SwLight = TEXT("sw_light"), SwCrt = TEXT("sw_crt"), SwRadio = TEXT("sw_radio");
		const FName Big = TEXT("big"), Left = TEXT("left"), Right = TEXT("right");
	};
	static const FIdNames& Ids()
	{
		static const FIdNames Names;
		return Names;
	}

	// ------------------------------------------------------------------ kit.json

	/** Content/Spacecraft/kit.json by dotted paths (blender/spacecraft/KIT_NOTES.md). Used in Build only. */
	struct FKit
	{
		TSharedPtr<FJsonObject> Root;
		TArray<TSharedPtr<FJsonValue>> Empty;

		static TSharedPtr<FJsonObject> ReadFile(const TCHAR* Rel)
		{
			FString FileText;
			TSharedPtr<FJsonObject> Out;
			if (FFileHelper::LoadFileToString(FileText, *(FPaths::ProjectContentDir() / Rel)))
			{
				FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FileText), Out);
			}
			return Out;
		}

		static TSharedPtr<FJsonValue> Walk(const TSharedPtr<FJsonObject>& From, const TCHAR* Path)
		{
			TArray<FString> Keys;
			FString(Path).ParseIntoArray(Keys, TEXT("."));
			TSharedPtr<FJsonObject> Obj = From;
			TSharedPtr<FJsonValue> Val;
			for (int32 I = 0; I < Keys.Num(); ++I)
			{
				if (!Obj.IsValid())
				{
					return TSharedPtr<FJsonValue>();
				}
				Val = Obj->TryGetField(Keys[I]);
				if (!Val.IsValid())
				{
					return TSharedPtr<FJsonValue>();
				}
				Obj.Reset();
				if (Val->Type == EJson::Object)
				{
					Obj = Val->AsObject();
				}
			}
			return Val;
		}

		static TSharedPtr<FJsonObject> AsObj(const TSharedPtr<FJsonValue>& V)
		{
			if (V.IsValid() && V->Type == EJson::Object)
			{
				return V->AsObject();
			}
			return TSharedPtr<FJsonObject>();
		}

		static double ToNum(const TSharedPtr<FJsonValue>& V, double Def)
		{
			return V.IsValid() && V->Type == EJson::Number ? V->AsNumber() : Def;
		}

		static FVector ToVec(const TSharedPtr<FJsonValue>& V, const FVector& Def)
		{
			if (V.IsValid() && V->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
				if (A.Num() >= 3)
				{
					return FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber());
				}
			}
			return Def;
		}

		/** A kit frame {at, x, y, z, rot}: rot is [pitch, yaw, roll], the same as the axes (cabincheck.py compares them). */
		static FTransform ToFrame(const TSharedPtr<FJsonValue>& V, const FTransform& Def)
		{
			const TSharedPtr<FJsonObject> O = AsObj(V);
			if (!O.IsValid())
			{
				return Def;
			}
			const FVector At = ToVec(O->TryGetField(TEXT("at")), Def.GetLocation());
			const FVector Rot = ToVec(O->TryGetField(TEXT("rot")), FVector::ZeroVector);
			return FTransform(FRotator(Rot.X, Rot.Y, Rot.Z), At);
		}

		double Num(const TCHAR* Path, double Def) const { return ToNum(Walk(Root, Path), Def); }
		FVector Vec(const TCHAR* Path, const FVector& Def) const { return ToVec(Walk(Root, Path), Def); }
		FTransform Frame(const TCHAR* Path, const FTransform& Def) const { return ToFrame(Walk(Root, Path), Def); }
		const TArray<TSharedPtr<FJsonValue>>& Arr(const TCHAR* Path) const
		{
			const TSharedPtr<FJsonValue> V = Walk(Root, Path);
			if (V.IsValid() && V->Type == EJson::Array)
			{
				return V->AsArray();
			}
			return Empty;
		}

		// an element of one of the kit's lists: the list's path is said only for cabincheck.py
		static double ENum(const TSharedPtr<FJsonValue>& E, const TCHAR* /*ListPath*/, const TCHAR* Rel, double Def) { return ToNum(Walk(AsObj(E), Rel), Def); }
		static FVector EVec(const TSharedPtr<FJsonValue>& E, const TCHAR* /*ListPath*/, const TCHAR* Rel, const FVector& Def) { return ToVec(Walk(AsObj(E), Rel), Def); }
		static FTransform EFrame(const TSharedPtr<FJsonValue>& E, const TCHAR* /*ListPath*/, const TCHAR* Rel, const FTransform& Def) { return ToFrame(Walk(AsObj(E), Rel), Def); }
		static FString EStr(const TSharedPtr<FJsonValue>& E, const TCHAR* /*ListPath*/, const TCHAR* Rel)
		{
			const TSharedPtr<FJsonValue> V = Walk(AsObj(E), Rel);
			return V.IsValid() && V->Type == EJson::String ? V->AsString() : FString();
		}
		static TArray<TSharedPtr<FJsonValue>> EArr(const TSharedPtr<FJsonValue>& E, const TCHAR* /*ListPath*/, const TCHAR* Rel)
		{
			const TSharedPtr<FJsonValue> V = Walk(AsObj(E), Rel);
			if (V.IsValid() && V->Type == EJson::Array)
			{
				return V->AsArray();
			}
			return TArray<TSharedPtr<FJsonValue>>();
		}
	};

	/** The kit while Build runs (a field, not a parameter: a local named K would shadow LyudBirds' K in a unity build);
	 *  its JSON is let go at the end of Build. */
	FKit K;

	// ------------------------------------------------------------------ small helpers

	static float Smooth01(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	/** Between the two ends of a hinge's travel as the kit gives them ("blend them, no sign to guess"). */
	static FTransform Blend(const FTransform& A, const FTransform& B, float T)
	{
		return FTransform(FQuat::Slerp(A.GetRotation(), B.GetRotation(), T).GetNormalized(), FMath::Lerp(A.GetLocation(), B.GetLocation(), T));
	}

	/** Band-limited noise -1..1 from a small fixed table (no generator at run time): one lump per unit of X. */
	static float Noise(float X)
	{
		struct FTable
		{
			float V[64];
			FTable()
			{
				uint32 S = 7151u;
				for (float& F : V)
				{
					S = S * 1664525u + 1013904223u;
					F = float(S >> 8) / 8388608.f - 1.f;
				}
			}
		};
		static const FTable Lumps;
		const float F = FMath::FloorToFloat(X);
		const int32 I = int32(F) & 63;
		const float A = Smooth01(X - F);
		return FMath::Lerp(Lumps.V[I], Lumps.V[(I + 1) & 63], A);
	}

	static UStaticMesh* LoadMesh(const TCHAR* Name)
	{
		const FString Path = FString(TEXT("/Game/Spacecraft/Meshes/")) + Name + TEXT(".") + Name;
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path);
		if (!M)
		{
			UE_LOG(LogTemp, Warning, TEXT("Spacecraft: missing %s (unreal/scripts/import_spacecraft.sh)"), *Path);
		}
		return M;
	}

	template <typename T>
	static T* NewPart(ALyudSpacecraft& O, const TCHAR* Name, USceneComponent* Parent)
	{
		T* C = NewObject<T>(&O, MakeUniqueObjectName(&O, T::StaticClass(), FName(Name)), RF_Transient);
		C->SetupAttachment(Parent ? Parent : O.GetRootComponent());
		C->SetMobility(EComponentMobility::Movable);
		return C;
	}

	/** A piece of the kit: no collision (the boxes do it), out of the distance field and the indirect light, hidden. */
	static UStaticMeshComponent* AddMesh(ALyudSpacecraft& O, const TCHAR* Name, UStaticMesh* Mesh, USceneComponent* Parent, const FTransform& At, ESet Set, bool bShadow)
	{
		UStaticMeshComponent* C = NewPart<UStaticMeshComponent>(O, Name, Parent);
		C->SetStaticMesh(Mesh);
		C->SetRelativeTransform(At);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(bShadow);
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->SetReceivesDecals(false);
		// Europa: the star is channel 1, the planet's fill channel 2; the cabin's own lamps are channel 0
		if (Set == SetOutside || Set == SetOutsideOwn)
		{
			C->SetLightingChannels(false, true, true);
		}
		else if (Set == SetShared)
		{
			C->SetLightingChannels(true, true, true);
		}
		else
		{
			C->SetLightingChannels(true, true, false);
		}
		C->SetVisibility(false);
		C->RegisterComponent();
		if (Set == SetInside)
		{
			O.InsideMeshes.Add(C);
		}
		else if (Set == SetOutside)
		{
			O.OutsideMeshes.Add(C);
		}
		else if (Set == SetShared)
		{
			O.SharedMeshes.Add(C);
		}
		return C;
	}

	/**
	 * The dynamic instance of a slot: made once per EMid from the mesh asset's own material (never from another dynamic
	 * instance), then put on every component that asks with the same EMid. Expect: the material's name as
	 * import_spacecraft.py assigns it (null: one of the glb's flat materials).
	 */
	static UMaterialInstanceDynamic* MakeMid(ALyudSpacecraft& O, int32 Mid, UStaticMeshComponent* C, const TCHAR* Slot, const TCHAR* Expect)
	{
		if (!C || !C->GetStaticMesh())
		{
			return O.Mids[Mid];
		}
		const int32 Index = C->GetMaterialIndex(FName(Slot));
		if (Index == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("Spacecraft: missing slot %s on %s"), Slot, *C->GetStaticMesh()->GetName());
			return O.Mids[Mid];
		}
		if (!O.Mids[Mid])
		{
			UMaterialInterface* Base = C->GetMaterial(Index);
			if (const UMaterialInstanceDynamic* Dyn = Cast<UMaterialInstanceDynamic>(Base))
			{
				Base = Dyn->Parent;
			}
			if (!Base)
			{
				UE_LOG(LogTemp, Warning, TEXT("Spacecraft: missing material in slot %s of %s"), Slot, *C->GetStaticMesh()->GetName());
				return nullptr;
			}
			if (Expect && Base->GetName() != Expect)
			{
				UE_LOG(LogTemp, Warning, TEXT("Spacecraft: slot %s of %s has %s, not %s (unreal/scripts/import_spacecraft.sh)"), Slot, *C->GetStaticMesh()->GetName(), *Base->GetName(), Expect);
			}
			O.Mids[Mid] = UMaterialInstanceDynamic::Create(Base, &O);
		}
		C->SetMaterial(Index, O.Mids[Mid]);
		return O.Mids[Mid];
	}

	static void Scalar(ALyudSpacecraft& O, int32 Mid, const FName& Name, float V)
	{
		if (UMaterialInstanceDynamic* M = O.Mids.IsValidIndex(Mid) ? O.Mids[Mid].Get() : nullptr)
		{
			M->SetScalarParameterValue(Name, V);
		}
	}

	static void Vector(ALyudSpacecraft& O, int32 Mid, const FName& Name, const FLinearColor& V)
	{
		if (UMaterialInstanceDynamic* M = O.Mids.IsValidIndex(Mid) ? O.Mids[Mid].Get() : nullptr)
		{
			M->SetVectorParameterValue(Name, V);
		}
	}

	static void Texture(ALyudSpacecraft& O, int32 Mid, const FName& Name, UTexture2D* Tex)
	{
		UMaterialInstanceDynamic* M = O.Mids.IsValidIndex(Mid) ? O.Mids[Mid].Get() : nullptr;
		if (M && Tex)
		{
			M->SetTextureParameterValue(Name, Tex);
		}
	}

	static void SetRel(ALyudSpacecraft& O, int32 Part, const FTransform& T)
	{
		if (USceneComponent* C = O.Parts.IsValidIndex(Part) ? O.Parts[Part].Get() : nullptr)
		{
			C->SetRelativeLocationAndRotation(T.GetLocation(), T.GetRotation());
		}
	}

	static void SetRot(ALyudSpacecraft& O, int32 Part, const FQuat& Q)
	{
		if (USceneComponent* C = O.Parts.IsValidIndex(Part) ? O.Parts[Part].Get() : nullptr)
		{
			C->SetRelativeRotation(Q);
		}
	}

	static void Show(ALyudSpacecraft& O, int32 Part, bool bShow)
	{
		USceneComponent* C = O.Parts.IsValidIndex(Part) ? O.Parts[Part].Get() : nullptr;
		if (C && C->IsVisible() != bShow)
		{
			C->SetVisibility(bShow);
		}
	}

	// ------------------------------------------------------------------ the conventions nobody has seen in the engine yet

	/** A needle at the dial angle A (degrees clockwise from 12 o'clock as the hero sees it): the kit's needle frame has
	 *  +X out of the face and +Z to 12 o'clock, its +Y to the hero's LEFT, so +A about +X is clockwise. Flip: the sign. */
	static FQuat NeedleTurn(float ADeg)
	{
		return FQuat(FVector::XAxisVector, FMath::DegreesToRadians(ADeg));
	}

	/** The little globe turned so that the map's point (Lat, Lon east) looks along the frame's +X (at the hero, under
	 *  the fixed crosshair) with north up: that point's own direction is FRotator(Lat, -Lon, 0).Vector() (kit.json
	 *  panel.globe.doc). Flip: the sign of Lon. */
	static FQuat GlobeTurn(float LatDeg, float LonDeg)
	{
		return FRotator(LatDeg, -LonDeg, 0.f).Quaternion().Inverse();
	}

	/**
	 * The attitude ball (equirectangular: V = pitch -90..90, U round; level flight = the kit's frame): the ball's point
	 * (pitch, heading) is brought to the middle of the window, then the roll is about the line of sight. UNVERIFIED
	 * signs (MODEL_NOTES: yaw is positive to the LEFT in the model): flip PitchSign / YawSign / RollSign here.
	 */
	static FQuat BallTurn(float RollDeg, float PitchDeg, float YawDeg)
	{
		const float PitchSign = 1.f, YawSign = 1.f, RollSign = -1.f;
		return FQuat(FVector::XAxisVector, FMath::DegreesToRadians(RollSign * RollDeg)) * FRotator(PitchSign * PitchDeg, YawSign * YawDeg, 0.f).Quaternion().Inverse();
	}

	/** The CRT's attitude page (M_SC_CRT Att): x = roll in radians, y = pitch as a share of the screen's height.
	 *  UNVERIFIED signs: flip here. */
	static FLinearColor CrtAtt(float RollDeg, float PitchDeg)
	{
		return FLinearColor(FMath::DegreesToRadians(float(FRotator::NormalizeAxis(RollDeg))), FMath::Clamp(PitchDeg / 180.f, -0.5f, 0.5f), 0.f, 0.f);
	}

	/**
	 * The model's sub-craft point (latitude, longitude from the island's, east +; the model's island is at latitude
	 * IslandLatModel) on the planet MAP, whose own island is at (MapLat, MapLon): the model's planet is tipped about the
	 * island's east axis so that its island lands on the map's and the track's heading over it is kept (the same choice
	 * as MATERIAL_NOTES "Orientation" for the planet outside).
	 */
	static void ModelToMap(double LatDeg, double LonRelDeg, double& OutLat, double& OutLon)
	{
		const double IslandLatModel = 18.0, MapLat = 54.96, MapLon = 20.40;
		const double La = FMath::DegreesToRadians(LatDeg), Lo = FMath::DegreesToRadians(LonRelDeg);
		// right-handed: x through the island's meridian on the equator, y east, z north
		const double X = FMath::Cos(La) * FMath::Cos(Lo), Y = FMath::Cos(La) * FMath::Sin(Lo), Z = FMath::Sin(La);
		const double Tip = FMath::DegreesToRadians(MapLat - IslandLatModel);
		const double X2 = X * FMath::Cos(Tip) - Z * FMath::Sin(Tip);
		const double Z2 = X * FMath::Sin(Tip) + Z * FMath::Cos(Tip);
		OutLat = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Z2, -1.0, 1.0)));
		OutLon = MapLon + FMath::RadiansToDegrees(FMath::Atan2(Y, X2));
	}

	// ------------------------------------------------------------------ the kit's measures (cabin frame, cm)

	bool bKit = false;
	float InnerRFloor = 130.f, InnerRKnee = 102.f, KneeZ = 155.f, CeilingZ = 205.f, OuterR = 155.f, Waterline = -10.3f;
	FVector SeatEyes = FVector(-23.27, -64.13, 112.0), SeatLook = FVector(0.8713, 0.4848, -0.0756), SeatPelvis = FVector(-5.0, -56.0, 42.0);
	float CouchYaw = 24.f;
	FVector StandCentre = FVector(-40.0, 0.0, 0.0), StandInside = FVector(-85.88, 0.0, 0.0), StandOutside = FVector(-208.48, 0.0, 34.62);
	FVector HatchCentre = FVector(-134.92, 0.0, 95.0);
	float HatchRadius = 60.f, SillZ = 31.9f;
	FVector PortCamera[3] = { FVector(77.75, 0.0, 120.02), FVector(0.0, -78.74, 115.02), FVector(0.0, 78.74, 115.02) };
	FVector PortDir[3] = { FVector(0.9808, 0.0, 0.195), FVector(0.0, -0.9808, 0.195), FVector(0.0, 0.9808, 0.195) };
	FVector PanelCentre = FVector(74.03, 0.0, 66.73), Speaker = FVector(69.09, 52.0, 51.51), JetsAt = FVector(0.0, 0.0, -30.0);
	FVector ChuteAttach = FVector(0.0, 0.0, 245.0);

	FTransform HatchClosed, HatchOpen, WheelInHatch;
	float WheelTurns = 1.5f;
	FTransform ToggleOff[NumToggles], ToggleOn[NumToggles];
	FTransform ButtonOut, ButtonIn, CoverClosed, CoverOpen;
	FTransform HandleRest, HandlePulled, ChuteCoverClosed, ChuteCoverOpen;
	FTransform PlatterAt, ArmRest, ArmLeadIn, ArmRunOut;
	float ArmLiftDeg = 4.f, ArmLength = 18.5f;
	FTransform SleeveAt[NumSleeves], SleeveOutAt;
	int32 SleeveCell[NumSleeves] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	FTransform ThermosAt, BookAt, CapAt, ThermosSip, BookHeld;
	FTransform GlobeAt, BallAt;

	struct FNeedle
	{
		FTransform At;
		ESource Source = SrcNone;
		float VMin = 0.f, VMax = 1.f, AMin = -120.f, AMax = 120.f, PerRev = 10.f;
		/** The spring's angle and rate; the tenths of a degree on the mesh now. */
		float Angle = 0.f, Rate = 0.f;
		int32 Shown = MIN_int32;
	};
	FNeedle Needles[MaxNeedles];
	int32 NumNeedles = 0;

	int32 DigitFirst[3] = { 0, 6, 12 }, DigitCount[3] = { 6, 6, 5 };
	uint8 LampClass[NumLamps] = {};

	struct FPoint
	{
		FName Id;
		FString IdString;
		FVector At = FVector::ZeroVector;
		float Radius = 4.f, Reach = 120.f;
		EKind Kind = KFixed;
		/** False: only for ActionPoint (sounds). */
		bool bAction = true;
	};
	TArray<FPoint> Points;

	// ------------------------------------------------------------------ what moves

	struct FAnim
	{
		float Cur = 0.f, Target = 0.f, Shown = -1.f;
		bool Step(float Dt, float Seconds)
		{
			if (Cur == Target)
			{
				return false;
			}
			Cur = FMath::FInterpConstantTo(Cur, Target, Dt, 1.f / FMath::Max(Seconds, 0.02f));
			return true;
		}
		void Set(bool bOn, bool bSnap)
		{
			Target = bOn ? 1.f : 0.f;
			if (bSnap)
			{
				Cur = Target;
			}
		}
		bool Moving() const { return Cur != Target; }
		bool Dirty() const { return Shown != Cur; }
	};
	FAnim Hatch, Wheel, Cover, ChuteCover, Book, Toggle[NumToggles];
	/** One-shot moves: seconds since the start, negative = at rest. */
	float ButtonT = -1.f, HandleT = -1.f, SipT = -1.f, CrtFlickT = -1.f;
	bool bBuckled = false;
	/** Where the door, the two covers and the arm are now (cabin frame): the interaction points ride on them. */
	FTransform DoorNow, CoverNow, ChuteCoverNow, ArmNow;

	// the record player
	int32 Cell = -1, SleeveOut = -1;
	bool bSpin = false;
	float Spin01 = 0.f, PlatterDeg = 0.f, RecordPitch = 1.f, ArmTarget = 0.f, Arm = 0.f, ArmShown = -1.f;
	int32 PlatterShown = MIN_int32;

	// loose things in weightlessness
	float FeltG = 1.f, Float01 = 0.f, FloatClock = 0.f, FloatShown = -1.f;
	FVector LooseShown[3] = { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector };

	// ------------------------------------------------------------------ what is on the instruments now (set on change)

	bool bCabinLightAllowed = true, bLightWanted = false;
	float Light01 = 0.f, LightShown = -1.f;
	float Backlight = 0.f, BacklightShown = -1.f, CrtPower = 0.f, CrtPowerShown = -1.f;
	float DigitValue[3] = { -1.f, -1.f, -1.f };
	int32 DigitOpt[3] = { -1, -1, -1 };
	int32 SpeedPoint = -1;
	float LampCode[NumLamps];
	bool bLampsFresh = true;
	float BarShown = -1.f;
	int32 CrtModeShown = -1, CrtPhaseShown = -99;
	FVector CrtEuropaShown = FVector(-1.0), CrtPlanetShown = FVector(-1.0);
	float CraftShown = -1.f, PeakG = 0.f;
	FVector2D CrtAttShown = FVector2D(1000.0, 1000.0), CrtGShown = FVector2D(-1.0, -1.0);
	int32 GlobeLatShown = MIN_int32, GlobeLonShown = MIN_int32;
	int32 BallShown[3] = { MIN_int32, MIN_int32, MIN_int32 };
	bool bAltCoarse = false;
	int32 RealWorldShown = -1;

	// the outside
	float GlassShown[4] = { -1.f, -1.f, -1.f, -1.f };
	float PlasmaShown = -1.f, HeatShown = -1.f;
	float Plasma = 0.f;
	int32 ChuteStage = 0;
	FVector ChuteShown = FVector(-1000.0);
	float ChuteInflateShown = -1.f, JetsShown = -1.f;

	FCabinData()
	{
		for (float& C : LampCode)
		{
			C = -1.f;
		}
	}

	/** Everything cached as "what the materials have" forgotten: the next ApplyState / SetExterior sends it all. */
	void Forget()
	{
		LightShown = BacklightShown = CrtPowerShown = BarShown = CraftShown = -1.f;
		PlasmaShown = HeatShown = ChuteInflateShown = JetsShown = -1.f;
		for (int32 I = 0; I < 3; ++I)
		{
			DigitValue[I] = -1.f;
			DigitOpt[I] = -1;
			BallShown[I] = MIN_int32;
		}
		SpeedPoint = -1;
		for (float& C : LampCode)
		{
			C = -1.f;
		}
		bLampsFresh = true;
		CrtModeShown = -1;
		CrtPhaseShown = -99;
		CrtEuropaShown = CrtPlanetShown = FVector(-1.0);
		CrtAttShown = FVector2D(1000.0, 1000.0);
		CrtGShown = FVector2D(-1.0, -1.0);
		GlobeLatShown = GlobeLonShown = MIN_int32;
		RealWorldShown = -1;
		for (float& GlassValue : GlassShown)
		{
			GlassValue = -1.f;
		}
		ChuteShown = FVector(-1000.0);
		for (int32 I = 0; I < NumNeedles; ++I)
		{
			Needles[I].Shown = MIN_int32;
		}
		Hatch.Shown = Wheel.Shown = Cover.Shown = ChuteCover.Shown = Book.Shown = -1.f;
		for (FAnim& T : Toggle)
		{
			T.Shown = -1.f;
		}
		ArmShown = -1.f;
		PlatterShown = MIN_int32;
		FloatShown = -1.f;
	}

	/** The built-in measures, for a question asked before Build. */
	static const FCabinData& Defaults()
	{
		static const FCabinData Built;
		return Built;
	}

	void LoadKit();
	void BuildMeshes(ALyudSpacecraft& O);
	void BuildCollision(ALyudSpacecraft& O);
	void BuildPoints();
	void ApplyLanguage(ALyudSpacecraft& O);
	void ApplyShown(ALyudSpacecraft& O);
	void ApplyOwnShown(ALyudSpacecraft& O);
	void ApplyParts(ALyudSpacecraft& O, bool bAll);
	void ApplyRecord(ALyudSpacecraft& O);
	void ApplyLight(ALyudSpacecraft& O);
	void Instruments(ALyudSpacecraft& O, const LyudFlight::FState& S, float Dt);
	void TickParts(ALyudSpacecraft& O, float Dt);
	void TickLoose(ALyudSpacecraft& O, float Dt);
	bool PointNow(const FPoint& Pt, const FVector& Eye, bool bRules, FVector& Out) const;
	float InnerRadiusAt(float Z) const { return Z < KneeZ ? FMath::Lerp(InnerRFloor, InnerRKnee, FMath::Clamp(Z / FMath::Max(KneeZ, 1.f), 0.f, 1.f)) : InnerRKnee; }
};

// ------------------------------------------------------------------ the kit

void ALyudSpacecraft::FCabinData::LoadKit()
{
	bKit = K.Root.IsValid();
	InnerRFloor = float(K.Num(TEXT("cabin.inner_radius_floor"), InnerRFloor));
	InnerRKnee = float(K.Num(TEXT("cabin.inner_radius_knee"), InnerRKnee));
	KneeZ = float(K.Num(TEXT("cabin.knee_z"), KneeZ));
	CeilingZ = float(K.Num(TEXT("cabin.ceiling_z"), CeilingZ));
	OuterR = float(K.Num(TEXT("cabin.outer_radius"), OuterR));
	Waterline = float(K.Num(TEXT("afloat.waterline_z"), Waterline));
	SeatEyes = K.Vec(TEXT("seat.eyes"), SeatEyes);
	SeatLook = K.Vec(TEXT("seat.look_direction"), SeatLook).GetSafeNormal();
	SeatPelvis = K.Vec(TEXT("seat.pelvis"), SeatPelvis);
	CouchYaw = float(K.Num(TEXT("seat.couch_yaw"), CouchYaw));
	StandCentre = K.Vec(TEXT("stand.centre"), StandCentre);
	StandInside = K.Vec(TEXT("stand.inside_hatch"), StandInside);
	StandOutside = K.Vec(TEXT("stand.outside_hatch"), StandOutside);
	HatchCentre = K.Vec(TEXT("hatch.centre"), HatchCentre);
	HatchRadius = float(K.Num(TEXT("hatch.clear_radius"), HatchRadius));
	SillZ = float(K.Num(TEXT("hatch.sill_z"), SillZ));
	PortCamera[0] = K.Vec(TEXT("portholes.big.look.camera"), PortCamera[0]);
	PortCamera[1] = K.Vec(TEXT("portholes.left.look.camera"), PortCamera[1]);
	PortCamera[2] = K.Vec(TEXT("portholes.right.look.camera"), PortCamera[2]);
	PortDir[0] = K.Vec(TEXT("portholes.big.look.direction"), PortDir[0]).GetSafeNormal();
	PortDir[1] = K.Vec(TEXT("portholes.left.look.direction"), PortDir[1]).GetSafeNormal();
	PortDir[2] = K.Vec(TEXT("portholes.right.look.direction"), PortDir[2]).GetSafeNormal();
	PanelCentre = K.Vec(TEXT("panel.centre"), PanelCentre);
	Speaker = K.Vec(TEXT("panel.speaker"), Speaker);
	ChuteAttach = K.Vec(TEXT("chute.attach"), ChuteAttach);
	{
		const TArray<TSharedPtr<FJsonValue>>& Jets = K.Arr(TEXT("cabin.jets"));
		FVector Sum = FVector::ZeroVector;
		for (const TSharedPtr<FJsonValue>& J : Jets)
		{
			Sum += FKit::ToVec(J, JetsAt);
		}
		if (Jets.Num() > 0)
		{
			JetsAt = Sum / double(Jets.Num());
		}
	}

	const FTransform Id = FTransform::Identity;
	HatchClosed = K.Frame(TEXT("hatch.closed"), FTransform(FRotator(11.244f, -180.f, 0.f), FVector(-141.3, -76.0, 96.27)));
	HatchOpen = K.Frame(TEXT("hatch.open"), HatchClosed);
	WheelInHatch = K.Frame(TEXT("hatch.wheel.in_hatch"), FTransform(FRotator(0.f, -180.f, 0.f), FVector(-13.5, -76.0, 0.0)));
	WheelTurns = float(K.Num(TEXT("hatch.wheel.turns_to_open"), WheelTurns));
	{
		const TArray<TSharedPtr<FJsonValue>>& List = K.Arr(TEXT("panel.toggles"));
		for (int32 I = 0; I < NumToggles; ++I)
		{
			const FTransform Def(FRotator(-7.f, -180.f, 0.f), FVector(67.96, -42.0 + 7.0 * I + (I >= 5 ? 21.0 : 0.0), 50.3));
			ToggleOff[I] = List.IsValidIndex(I) ? FKit::EFrame(List[I], TEXT("panel.toggles[]"), TEXT("off"), Def) : Def;
			ToggleOn[I] = List.IsValidIndex(I) ? FKit::EFrame(List[I], TEXT("panel.toggles[]"), TEXT("on"), ToggleOff[I]) : ToggleOff[I];
		}
	}
	ButtonOut = K.Frame(TEXT("panel.launch.button"), FTransform(FRotator(18.f, -180.f, 0.f), FVector(68.25, 0.0, 50.21)));
	ButtonIn = K.Frame(TEXT("panel.launch.pressed"), ButtonOut);
	CoverClosed = K.Frame(TEXT("panel.launch.cover_closed"), FTransform(FRotator(18.f, -180.f, 0.f), FVector(69.25, 0.0, 53.46)));
	CoverOpen = K.Frame(TEXT("panel.launch.cover_open"), CoverClosed);
	HandleRest = K.Frame(TEXT("chute.handle.rest"), FTransform(FVector(11.89, -41.61, 199.83)));
	HandlePulled = K.Frame(TEXT("chute.handle.pulled"), HandleRest);
	ChuteCoverClosed = K.Frame(TEXT("chute.cover.closed"), FTransform(FVector(17.97, -41.9, 201.84)));
	ChuteCoverOpen = K.Frame(TEXT("chute.cover.open"), ChuteCoverClosed);
	PlatterAt = K.Frame(TEXT("player.platter"), FTransform(FRotator(0.f, 110.f, 0.f), FVector(41.29, -81.28, 84.4)));
	ArmRest = K.Frame(TEXT("player.tonearm.rest"), FTransform(FRotator(0.f, 103.565f, 0.f), FVector(25.92, -97.52, 87.8)));
	ArmLeadIn = K.Frame(TEXT("player.tonearm.lead_in"), ArmRest);
	ArmRunOut = K.Frame(TEXT("player.tonearm.run_out"), ArmLeadIn);
	ArmLiftDeg = float(K.Num(TEXT("player.tonearm.lift_deg"), ArmLiftDeg));
	ArmLength = float(K.Num(TEXT("player.tonearm.length"), ArmLength));
	{
		const TArray<TSharedPtr<FJsonValue>>& Rack = K.Arr(TEXT("player.sleeve_rack"));
		for (int32 I = 0; I < NumSleeves; ++I)
		{
			const FTransform Def(FRotator(-10.24f, 96.f, 0.f), FVector(11.03 + 0.13 * I, -104.97 - 1.17 * I, 67.87 + 0.21 * I));
			SleeveAt[I] = Rack.IsValidIndex(I) ? FKit::ToFrame(Rack[I], Def) : Def;
		}
		// the sleeve of the record that is on the platter: taken out and propped in front of the rack, a little askew
		const FVector Step = SleeveAt[0].GetLocation() - SleeveAt[1].GetLocation();
		SleeveOutAt = FTransform(SleeveAt[0].GetRotation() * FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(14.f)),
			SleeveAt[0].GetLocation() + Step * 3.0 + SleeveAt[0].GetUnitAxis(EAxis::Z) * 4.0);
	}
	ThermosAt = K.Frame(TEXT("props.thermos"), FTransform(FVector(56.99, 96.05, 60.58)));
	BookAt = K.Frame(TEXT("props.book"), FTransform(FVector(36.78, 107.2, 71.09)));
	CapAt = K.Frame(TEXT("props.blanket_and_cap"), FTransform(FRotator(0.f, -24.f, 0.f), FVector(-5.0, 56.0, 0.0)));
	GlobeAt = K.Frame(TEXT("panel.globe.frame"), FTransform(FRotator(18.f, -180.f, 0.f), FVector(86.46, -4.7, 75.83)));
	BallAt = K.Frame(TEXT("panel.ball.frame"), FTransform(FRotator(18.f, -180.f, 0.f), FVector(84.74, 13.0, 76.39)));

	// held by the seated hero (made up here, not in the kit): the thermos tipped at his mouth, the book before his eyes
	ThermosSip = FTransform(FRotator(40.f, CouchYaw, 0.f), SeatEyes + SeatLook * 16.0 + FVector(0.0, 0.0, -24.0));
	BookHeld = FTransform(FRotationMatrix::MakeFromZY(-SeatLook, FVector::UpVector).ToQuat(), SeatEyes + SeatLook * 38.0 + FVector(0.0, 0.0, -11.0));

	// the dials: one needle each, the altimeter two
	NumNeedles = 0;
	for (const TSharedPtr<FJsonValue>& D : K.Arr(TEXT("panel.dials")))
	{
		const FString DialId = FKit::EStr(D, TEXT("panel.dials[]"), TEXT("id"));
		static const TCHAR* const DialIds[] = { TEXT("gmeter"), TEXT("press"), TEXT("o2"), TEXT("co2"), TEXT("temp"), TEXT("volts"), TEXT("fuel") };
		ESource Source = SrcNone;
		for (int32 I = 0; I < int32(UE_ARRAY_COUNT(DialIds)); ++I)
		{
			if (DialId == DialIds[I])
			{
				Source = ESource(SrcG + I);
			}
		}
		const bool bAlt = DialId == TEXT("alt");
		for (const TSharedPtr<FJsonValue>& N : FKit::EArr(D, TEXT("panel.dials[]"), TEXT("needles")))
		{
			if (NumNeedles >= MaxNeedles || (!bAlt && Source == SrcNone))
			{
				continue;
			}
			FNeedle& Out = Needles[NumNeedles++];
			Out = FNeedle();
			Out.At = FKit::EFrame(N, TEXT("panel.dials[].needles[]"), TEXT("frame"), Id);
			Out.At.SetScale3D(FVector(FKit::ENum(N, TEXT("panel.dials[].needles[]"), TEXT("scale"), 0.03)));
			Out.VMin = float(FKit::ENum(D, TEXT("panel.dials[]"), TEXT("min"), 0.0));
			Out.VMax = float(FKit::ENum(D, TEXT("panel.dials[]"), TEXT("max"), 1.0));
			Out.AMin = float(FKit::ENum(D, TEXT("panel.dials[]"), TEXT("a_min"), -120.0));
			Out.AMax = float(FKit::ENum(D, TEXT("panel.dials[]"), TEXT("a_max"), 120.0));
			Out.PerRev = float(FMath::Max(1.0, FKit::ENum(N, TEXT("panel.dials[].needles[]"), TEXT("per_rev"), 10.0)));
			Out.Source = bAlt ? (FKit::EStr(N, TEXT("panel.dials[].needles[]"), TEXT("id")) == TEXT("short") ? SrcAltShort : SrcAltLong) : Source;
			Out.Angle = bAlt ? 0.f : Out.AMin;
		}
	}
	{
		const TArray<TSharedPtr<FJsonValue>>& Windows = K.Arr(TEXT("panel.digits.windows"));
		for (int32 I = 0; I < 3 && I < Windows.Num(); ++I)
		{
			DigitFirst[I] = int32(FMath::RoundToInt(FKit::ENum(Windows[I], TEXT("panel.digits.windows[]"), TEXT("first_index"), DigitFirst[I])));
			DigitCount[I] = int32(FMath::RoundToInt(FKit::ENum(Windows[I], TEXT("panel.digits.windows[]"), TEXT("digits"), DigitCount[I])));
		}
	}
	for (int32 L = 0; L < NumLamps && L < LyudFlight::LampCount; ++L)
	{
		// class 0 green, 1 amber, 2 red, 3 white (M_SC_Lamps)
		const char C = LyudFlight::LampDef(L).Colour[0];
		LampClass[L] = C == 'g' ? 0 : (C == 'a' ? 1 : (C == 'r' ? 2 : 3));
	}
	for (int32 I = 0; I < NumSleeves; ++I)
	{
		SleeveCell[I] = I;
	}
	if (const TSharedPtr<FJsonObject> Records = FKit::ReadFile(TEXT("Spacecraft/records.json")))
	{
		// the rack holds the records in the file's order; each shows its own cell of the sleeve atlas
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Records->TryGetArrayField(TEXT("records"), List))
		{
			for (int32 I = 0; I < NumSleeves && I < List->Num(); ++I)
			{
				SleeveCell[I] = FMath::Clamp(int32(FMath::RoundToInt(FKit::ENum((*List)[I], TEXT("records.json"), TEXT("cell"), I))), 0, 7);
			}
		}
	}
}

void ALyudSpacecraft::FCabinData::BuildMeshes(ALyudSpacecraft& O)
{
	USceneComponent* Root = O.GetRootComponent();
	const FTransform Id = FTransform::Identity;

	// the two shells, the console, the player on its shelf
	AddMesh(O, TEXT("CabinOuter"), LoadMesh(TEXT("SM_SC_CabinOuter")), Root, Id, SetOutside, true);
	UStaticMeshComponent* Inner = AddMesh(O, TEXT("CabinInner"), LoadMesh(TEXT("SM_SC_CabinInner")), Root, Id, SetInside, false);
	UStaticMeshComponent* PanelMesh = AddMesh(O, TEXT("Panel"), LoadMesh(TEXT("SM_SC_Panel")), Root, Id, SetInside, false);
	AddMesh(O, TEXT("Player"), LoadMesh(TEXT("SM_SC_Player")), Root, Id, SetInside, false);

	// the door on its hinge, the wheel on the door
	UStaticMeshComponent* Door = AddMesh(O, TEXT("Hatch"), LoadMesh(TEXT("SM_SC_Hatch")), Root, HatchClosed, SetShared, true);
	O.Parts[PtHatch] = Door;
	O.Parts[PtWheel] = AddMesh(O, TEXT("HatchWheel"), LoadMesh(TEXT("SM_SC_HatchWheel")), Door, WheelInHatch, SetShared, false);

	// the three panes: one instance for the frost, the soot, the spray and the cloud's white
	{
		UStaticMesh* Side = LoadMesh(TEXT("SM_SC_Glass_Side"));
		UStaticMeshComponent* Big = AddMesh(O, TEXT("GlassBig"), LoadMesh(TEXT("SM_SC_Glass_Big")), Root, K.Frame(TEXT("portholes.big.glass"), FTransform(FRotator(11.244f, 0.f, 0.f), FVector(121.1, 0.0, 128.64))), SetShared, false);
		UStaticMeshComponent* Left = AddMesh(O, TEXT("GlassLeft"), Side, Root, K.Frame(TEXT("portholes.left.glass"), FTransform(FRotator(11.244f, -90.f, 0.f), FVector(0.0, -122.09, 123.64))), SetShared, false);
		UStaticMeshComponent* Right = AddMesh(O, TEXT("GlassRight"), Side, Root, K.Frame(TEXT("portholes.right.glass"), FTransform(FRotator(11.244f, 90.f, 0.f), FVector(0.0, 122.09, 123.64))), SetShared, false);
		MakeMid(O, MidGlass, Big, TEXT("SC_porthole"), TEXT("M_SC_Glass"));
		MakeMid(O, MidGlass, Left, TEXT("SC_porthole"), TEXT("M_SC_Glass"));
		MakeMid(O, MidGlass, Right, TEXT("SC_porthole"), TEXT("M_SC_Glass"));
	}

	// the couches; the folded blanket and the cap on the second one
	{
		UStaticMesh* Couch = LoadMesh(TEXT("SM_SC_Couch"));
		for (const TSharedPtr<FJsonValue>& F : K.Arr(TEXT("seat.couch_frames")))
		{
			AddMesh(O, TEXT("Couch"), Couch, Root, FKit::ToFrame(F, Id), SetInside, true);
		}
		AddMesh(O, TEXT("Blanket"), LoadMesh(TEXT("SM_SC_Blanket")), Root, CapAt, SetInside, false);
		O.Parts[PtCap] = AddMesh(O, TEXT("Cap"), LoadMesh(TEXT("SM_SC_Cap")), Root, CapAt, SetInside, false);
	}
	O.Parts[PtThermos] = AddMesh(O, TEXT("Thermos"), LoadMesh(TEXT("SM_SC_Thermos")), Root, ThermosAt, SetInside, false);
	O.Parts[PtBook] = AddMesh(O, TEXT("Book"), LoadMesh(TEXT("SM_SC_Book")), Root, BookAt, SetInside, false);

	// the panel's moving parts
	{
		UStaticMesh* Needle = LoadMesh(TEXT("SM_SC_Needle"));
		for (int32 I = 0; I < NumNeedles; ++I)
		{
			UStaticMeshComponent* C = AddMesh(O, TEXT("Needle"), Needle, Root, Needles[I].At, SetInside, false);
			O.Parts[PtNeedle0 + I] = C;
			MakeMid(O, MidNeedle, C, TEXT("SC_needle"), TEXT("M_SC_Needle"));
		}
		UStaticMeshComponent* Globe = AddMesh(O, TEXT("Globe"), LoadMesh(TEXT("SM_SC_Globe")), Root, GlobeAt, SetInside, false);
		O.Parts[PtGlobe] = Globe;
		MakeMid(O, MidGlobe, Globe, TEXT("SC_globe"), TEXT("M_SC_Globe"));
		UStaticMeshComponent* Ball = AddMesh(O, TEXT("Ball"), LoadMesh(TEXT("SM_SC_Ball")), Root, BallAt, SetInside, false);
		O.Parts[PtBall] = Ball;
		MakeMid(O, MidBall, Ball, TEXT("SC_ball"), TEXT("M_SC_Ball"));
		UStaticMesh* Lever = LoadMesh(TEXT("SM_SC_Toggle"));
		for (int32 I = 0; I < NumToggles; ++I)
		{
			O.Parts[PtToggle0 + I] = AddMesh(O, TEXT("Toggle"), Lever, Root, ToggleOff[I], SetInside, false);
		}
		O.Parts[PtButton] = AddMesh(O, TEXT("LaunchButton"), LoadMesh(TEXT("SM_SC_LaunchButton")), Root, ButtonOut, SetInside, false);
		O.Parts[PtLaunchCover] = AddMesh(O, TEXT("LaunchCover"), LoadMesh(TEXT("SM_SC_LaunchCover")), Root, CoverClosed, SetInside, false);
	}
	MakeMid(O, MidLabels, PanelMesh, TEXT("SC_labels"), TEXT("MI_SC_Labels_ru"));
	MakeMid(O, MidLabels, Inner, TEXT("SC_labels"), TEXT("MI_SC_Labels_ru"));
	MakeMid(O, MidLabels, Door, TEXT("SC_labels"), TEXT("MI_SC_Labels_ru"));
	MakeMid(O, MidDials, PanelMesh, TEXT("SC_dials"), TEXT("M_SC_Dials"));
	MakeMid(O, MidDigits, PanelMesh, TEXT("SC_digits"), TEXT("M_SC_Digits"));
	MakeMid(O, MidLamps, PanelMesh, TEXT("SC_lamps"), TEXT("M_SC_Lamps"));
	MakeMid(O, MidCrt, PanelMesh, TEXT("SC_crt"), TEXT("M_SC_CRT"));
	MakeMid(O, MidBar, PanelMesh, TEXT("SC_bar"), TEXT("M_SC_Bar"));
	MakeMid(O, MidGlassCover, PanelMesh, TEXT("SC_glass"), TEXT("MI_SC_GlassCover"));
	MakeMid(O, MidLampLens, Inner, TEXT("SC_lamp"), nullptr);

	// overhead: the parachute handle under its cover
	O.Parts[PtChuteHandle] = AddMesh(O, TEXT("ChuteHandle"), LoadMesh(TEXT("SM_SC_ChuteHandle")), Root, HandleRest, SetInside, false);
	O.Parts[PtChuteCover] = AddMesh(O, TEXT("ChuteCover"), LoadMesh(TEXT("SM_SC_ChuteCover")), Root, ChuteCoverClosed, SetInside, false);

	// the record player: the record rides on the platter (one transform a frame while it turns)
	{
		UStaticMeshComponent* Platter = AddMesh(O, TEXT("Platter"), LoadMesh(TEXT("SM_SC_Platter")), Root, PlatterAt, SetInside, false);
		O.Parts[PtPlatter] = Platter;
		const FTransform RecordAt = K.Frame(TEXT("player.record"), FTransform(PlatterAt.GetRotation(), PlatterAt.GetLocation() + FVector(0.0, 0.0, 1.6)));
		UStaticMeshComponent* Record = AddMesh(O, TEXT("Record"), LoadMesh(TEXT("SM_SC_Record")), Platter, RecordAt.GetRelativeTransform(PlatterAt), SetInsideOwn, false);
		O.Parts[PtRecord] = Record;
		MakeMid(O, MidRecordLabel, Record, TEXT("SC_record_label"), TEXT("M_SC_RecordLabel"));
		O.Parts[PtTonearm] = AddMesh(O, TEXT("Tonearm"), LoadMesh(TEXT("SM_SC_Tonearm")), Root, ArmRest, SetInside, false);
		UStaticMesh* Sleeve = LoadMesh(TEXT("SM_SC_Sleeve"));
		for (int32 I = 0; I < NumSleeves; ++I)
		{
			UStaticMeshComponent* C = AddMesh(O, TEXT("Sleeve"), Sleeve, Root, SleeveAt[I], SetInside, false);
			O.Parts[PtSleeve0 + I] = C;
			MakeMid(O, MidSleeve0 + I, C, TEXT("SC_sleeve"), TEXT("M_SC_Sleeve"));
			Scalar(O, MidSleeve0 + I, P().Cell, float(SleeveCell[I]));
		}
	}

	// outside: the plasma's shell, the two canopies on a pivot at the riser fitting
	{
		UStaticMeshComponent* Fire = AddMesh(O, TEXT("FireShell"), LoadMesh(TEXT("SM_SC_FireShell")), Root, Id, SetOutsideOwn, false);
		O.Parts[PtFireShell] = Fire;
		MakeMid(O, MidPlasma, Fire, TEXT("SC_fire"), TEXT("M_SC_Plasma"));
		USceneComponent* Rig = NewPart<USceneComponent>(O, TEXT("ChuteRig"), Root);
		Rig->SetRelativeLocation(ChuteAttach);
		Rig->RegisterComponent();
		O.Parts[PtChuteRig] = Rig;
		O.Parts[PtChute] = AddMesh(O, TEXT("Chute"), LoadMesh(TEXT("SM_SC_Chute")), Rig, Id, SetOutsideOwn, false);
		O.Parts[PtDrogue] = AddMesh(O, TEXT("Drogue"), LoadMesh(TEXT("SM_SC_Drogue")), Rig, Id, SetOutsideOwn, false);
	}

	// the three cabin lamps (no shadows: the cabin is small and its shells are closed) and the jets' glow under the shield
	for (const TSharedPtr<FJsonValue>& L : K.Arr(TEXT("lamps")))
	{
		if (O.CabinLamps.Num() >= NumCabinLamps)
		{
			break;
		}
		UPointLightComponent* Lamp = NewPart<UPointLightComponent>(O, TEXT("CabinLamp"), Root);
		Lamp->SetRelativeLocation(FKit::EVec(L, TEXT("lamps[]"), TEXT("light"), FVector(0.0, 0.0, 180.0)));
		Lamp->SetIntensityUnits(ELightUnits::Candelas);
		Lamp->SetIntensity(0.f);
		Lamp->SetAttenuationRadius(380.f);
		Lamp->SetLightColor(FLinearColor(1.f, 0.86f, 0.66f));
		Lamp->SetSourceRadius(5.f);
		Lamp->SetCastShadows(false);
		Lamp->SetVisibility(false);
		Lamp->RegisterComponent();
		O.CabinLamps.Add(Lamp);
	}
	{
		UPointLightComponent* Jet = NewPart<UPointLightComponent>(O, TEXT("JetLight"), Root);
		Jet->SetRelativeLocation(JetsAt + FVector(0.0, 0.0, -30.0));
		Jet->SetIntensityUnits(ELightUnits::Candelas);
		Jet->SetIntensity(0.f);
		Jet->SetAttenuationRadius(1200.f);
		Jet->SetLightColor(FLinearColor(1.f, 0.72f, 0.42f));
		Jet->SetSourceRadius(40.f);
		Jet->SetCastShadows(false);
		Jet->SetVisibility(false);
		Jet->RegisterComponent();
		O.JetLight = Jet;
	}

	// what never changes on the instruments
	Vector(O, MidDigits, P().Win0, FLinearColor(float(DigitFirst[0]), float(DigitCount[0]), 2.f, 0.f));
	Vector(O, MidDigits, P().Win1, FLinearColor(float(DigitFirst[1]), float(DigitCount[1]), 0.f, 0.f));
	Scalar(O, MidGlobe, P().Cross, 0.f);
}

/**
 * The kit exports nothing for collision (KIT_NOTES "Collision"): boxes do it. A floor, a ring of boxes for the cone and
 * one for the dome, a roof (ALyudSky::IsPlayerOutdoors traces upward), the console, the couches, the player's shelf.
 * The hatch's opening is a recess shut by a plug at the door's place, open or not: the hero goes through as a scripted
 * move with his collision off.
 */
void ALyudSpacecraft::FCabinData::BuildCollision(ALyudSpacecraft& O)
{
	auto Box = [&O](const FVector& Centre, const FVector& Half, const FRotator& Rot)
	{
		UBoxComponent* B = NewPart<UBoxComponent>(O, TEXT("Blocker"), nullptr);
		B->SetBoxExtent(Half);
		B->SetRelativeLocationAndRotation(Centre, Rot);
		B->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		B->SetCanEverAffectNavigation(false);
		B->SetGenerateOverlapEvents(false);
		B->RegisterComponent();
		O.Parts.Add(B);
	};
	const float Thick = 24.f, Bottom = -25.f, Top = CeilingZ + 10.f;
	const float SlopeDeg = FMath::RadiansToDegrees(FMath::Atan2(InnerRFloor - InnerRKnee, FMath::Max(KneeZ, 1.f)));
	const float PerZ = (InnerRKnee - InnerRFloor) / FMath::Max(KneeZ, 1.f);
	auto RadiusAt = [this, PerZ](float Z) { return InnerRFloor + PerZ * Z; };

	// the floor and the shield under it: two squares each, 45 degrees apart
	for (int32 I = 0; I < 2; ++I)
	{
		Box(FVector(0.0, 0.0, -12.5), FVector(InnerRFloor - 12.f, InnerRFloor - 12.f, 12.5f), FRotator(0.f, 45.f * float(I), 0.f));
		Box(FVector(0.0, 0.0, -41.0), FVector(80.f, 80.f, 16.f), FRotator(0.f, 45.f * float(I), 0.f));
	}
	// the wall: the cone up to the knee, then the dome's foot, both left open where the hatch is (at -X)
	const int32 Segments = 16;
	const float HalfStep = PI / float(Segments);
	const float ConeMid = 0.5f * (Bottom + KneeZ), ConeHalf = 0.5f * (KneeZ - Bottom) / FMath::Cos(FMath::DegreesToRadians(SlopeDeg)) + 2.f;
	const float DomeMid = 0.5f * (KneeZ - 5.f + Top), DomeHalf = 0.5f * (Top - KneeZ + 5.f);
	for (int32 I = 0; I < Segments; ++I)
	{
		const float YawDeg = 360.f * float(I) / float(Segments);
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(YawDeg, 180.f)) < 34.f)
		{
			continue;
		}
		const FVector Out(FMath::Cos(FMath::DegreesToRadians(YawDeg)), FMath::Sin(FMath::DegreesToRadians(YawDeg)), 0.0);
		const float ConeR = RadiusAt(ConeMid) + 0.5f * Thick + 1.f;
		Box(Out * ConeR + FVector(0.0, 0.0, ConeMid), FVector(0.5f * Thick, (RadiusAt(Bottom) + Thick) * FMath::Tan(HalfStep) + 3.f, ConeHalf), FRotator(SlopeDeg, YawDeg, 0.f));
		const float DomeR = InnerRKnee - 2.f + 0.5f * Thick;
		Box(Out * DomeR + FVector(0.0, 0.0, DomeMid), FVector(0.5f * Thick, (DomeR + Thick) * FMath::Tan(HalfStep) + 3.f, DomeHalf), FRotator(0.f, YawDeg, 0.f));
	}
	{
		// the hatch's sector: the sill below, the plug where the door is, the lintel above
		const float Wide = HatchRadius + 10.f;
		const float SillMid = 0.5f * (Bottom + SillZ - 1.f);
		Box(FVector(-(RadiusAt(SillMid) + 0.5f * Thick + 1.f), 0.0, SillMid), FVector(0.5f * Thick, Wide, 0.5f * (SillZ - 1.f - Bottom)), FRotator(SlopeDeg, 180.f, 0.f));
		const FVector AxisOut = K.Vec(TEXT("hatch.axis_out"), FVector(-0.9808, 0.0, 0.195)).GetSafeNormal();
		Box(HatchCentre + AxisOut * 8.0, FVector(6.f, Wide, HatchRadius + 8.f), AxisOut.Rotation());
		const float LintelZ = float(HatchCentre.Z) + HatchRadius + 6.f;
		Box(FVector(-(InnerRKnee + 10.f + 0.5f * Thick), 0.0, 0.5f * (LintelZ + Top)), FVector(0.5f * Thick, Wide, 0.5f * (Top - LintelZ)), FRotator(0.f, 180.f, 0.f));
	}
	Box(FVector(0.0, 0.0, CeilingZ + 15.0), FVector(InnerRKnee + 10.f, InnerRKnee + 10.f, 15.f), FRotator::ZeroRotator);

	// the console and the player's shelf from the pieces' bounds (the bounds are in Blender's Y: mirrored here when that
	// lands them nearer the piece's own place)
	auto BoundsBox = [&Box](const FVector& Min, const FVector& Max, double HintY, float Shrink, float MinZ)
	{
		FVector Lo = Min, Hi = Max;
		if (FMath::Abs(-0.5 * (Min.Y + Max.Y) - HintY) < FMath::Abs(0.5 * (Min.Y + Max.Y) - HintY))
		{
			Lo.Y = -Max.Y;
			Hi.Y = -Min.Y;
		}
		Lo.Z = FMath::Min(Lo.Z, double(MinZ));
		const FVector Half = (Hi - Lo) * 0.5 - FVector(Shrink);
		if (Half.X > 1.0 && Half.Y > 1.0 && Half.Z > 1.0)
		{
			Box((Hi + Lo) * 0.5, Half, FRotator::ZeroRotator);
		}
	};
	BoundsBox(K.Vec(TEXT("pieces.SM_SC_Panel.bounds.min"), FVector(65.7, -72.0, 34.0)), K.Vec(TEXT("pieces.SM_SC_Panel.bounds.max"), FVector(125.1, 72.0, 93.1)), 0.0, 1.f, 1000.f);
	BoundsBox(K.Vec(TEXT("pieces.SM_SC_Player.bounds.min"), FVector(11.6, 60.3, 74.0)), K.Vec(TEXT("pieces.SM_SC_Player.bounds.max"), FVector(61.9, 106.6, 113.7)),
		K.Vec(TEXT("player.frame.at"), FVector(36.76, -83.46, 74.0)).Y, 1.f, 62.f);

	// the couches: the seat and the reclined back, a little inside the tube frame so the aisle stays a capsule wide
	for (const TSharedPtr<FJsonValue>& F : K.Arr(TEXT("seat.couch_frames")))
	{
		const FTransform Couch = FKit::ToFrame(F, FTransform::Identity);
		Box(Couch.TransformPosition(FVector(12.0, 0.0, 20.0)), FVector(31.f, 24.f, 20.f), Couch.Rotator());
		Box(Couch.TransformPosition(FVector(-31.0, 0.0, 59.0)), FVector(14.f, 24.f, 59.f), Couch.Rotator());
	}
}

// ------------------------------------------------------------------ the interaction points

void ALyudSpacecraft::FCabinData::BuildPoints()
{
	Points.Reset();
	auto Add = [this](const TCHAR* Id, const FVector& At, float PtRadius, float Reach, EKind Kind, bool bAction)
	{
		FPoint& Pt = Points.AddDefaulted_GetRef();
		Pt.Id = FName(Id);
		Pt.IdString = Id;
		Pt.At = At;
		Pt.Radius = PtRadius;
		Pt.Reach = Reach;
		Pt.Kind = Kind;
		Pt.bAction = bAction;
	};
	// kit.json "interact": the kit's ids are not the director's; each is asked for by name here
	const TArray<TSharedPtr<FJsonValue>>& Interact = K.Arr(TEXT("interact"));
	auto InteractAt = [&Interact](const TCHAR* KitId, const FVector& Def)
	{
		for (const TSharedPtr<FJsonValue>& E : Interact)
		{
			if (FKit::EStr(E, TEXT("interact[]"), TEXT("id")) == KitId)
			{
				return FKit::EVec(E, TEXT("interact[]"), TEXT("at"), Def);
			}
		}
		return Def;
	};
	// reaches: the seated eye is 110..142 cm from the switches and 139 from the big porthole's point (he leans), the
	// net with the thermos and the book is across the cabin from his couch
	const float ReachPanel = 150.f, Near = 120.f, PortReach = 175.f, NetReach = 190.f, HatchReach = 165.f;

	// the hatch first: ActionPoint("hatch") is the opening's middle
	Add(TEXT("hatch"), HatchCentre, HatchRadius * 0.6f, HatchReach, KHatchOpening, true);
	Add(TEXT("hatch"), FVector(-8.0, -128.0, 0.0), 12.f, HatchReach, KHatchEdge, true);
	Add(TEXT("hatch"), FVector(10.0, -76.0, 0.0), 40.f, HatchReach, KHatchOutside, true);
	Add(TEXT("wheel"), InteractAt(TEXT("hatch_wheel"), FVector(-126.09, 0.0, 93.25)), 17.f, HatchReach, KWheel, true);

	static const TCHAR* const SwitchIds[NumToggles] = { TEXT("sw_power"), TEXT("sw_air"), TEXT("sw_gyro"), TEXT("sw_seal"), TEXT("sw_arm"), TEXT("sw_light"), TEXT("sw_crt"), TEXT("sw_radio") };
	Add(SwitchIds[0], InteractAt(TEXT("sw_power"), FVector(66.06, -42.0, 50.92)), 3.f, ReachPanel, KFixed, true);
	Add(SwitchIds[1], InteractAt(TEXT("sw_air"), FVector(66.06, -35.0, 50.92)), 3.f, ReachPanel, KFixed, true);
	Add(SwitchIds[2], InteractAt(TEXT("sw_gyro"), FVector(66.06, -28.0, 50.92)), 3.f, ReachPanel, KFixed, true);
	Add(SwitchIds[3], InteractAt(TEXT("sw_seal"), FVector(66.06, -21.0, 50.92)), 3.f, ReachPanel, KFixed, true);
	Add(SwitchIds[4], InteractAt(TEXT("sw_arm"), FVector(66.06, -14.0, 50.92)), 3.f, ReachPanel, KFixed, true);
	Add(SwitchIds[5], InteractAt(TEXT("sw_light"), FVector(66.06, 14.0, 50.92)), 3.f, ReachPanel, KFixed, true);
	Add(SwitchIds[6], InteractAt(TEXT("sw_crt"), FVector(66.06, 21.0, 50.92)), 3.f, ReachPanel, KFixed, true);
	Add(SwitchIds[7], InteractAt(TEXT("sw_radio"), FVector(66.06, 28.0, 50.92)), 3.f, ReachPanel, KFixed, true);
	// the button is seen only with its cover up; the cover's point rides on the cover (its own middle)
	Add(TEXT("launch"), InteractAt(TEXT("launch"), FVector(65.39, 0.0, 51.13)), 2.5f, ReachPanel, KLaunch, true);
	Add(TEXT("cover"), FVector(1.4, 0.0, -3.3), 3.4f, ReachPanel, KLaunchCover, true);

	Add(TEXT("port_big"), K.Vec(TEXT("portholes.big.look.interact"), FVector(99.32, 0.0, 124.31)), 30.f, PortReach, KFixed, true);
	Add(TEXT("port_left"), K.Vec(TEXT("portholes.left.look.interact"), FVector(0.0, -100.32, 119.31)), 18.f, PortReach, KFixed, true);
	Add(TEXT("port_right"), K.Vec(TEXT("portholes.right.look.interact"), FVector(0.0, 100.32, 119.31)), 18.f, PortReach, KFixed, true);

	Add(TEXT("player"), InteractAt(TEXT("player"), FVector(41.29, -81.28, 89.0)), 12.f, Near + 20.f, KFixed, true);
	Add(TEXT("player"), InteractAt(TEXT("sleeves"), FVector(10.43, -99.2, 83.08)), 12.f, Near + 20.f, KFixed, true);
	Add(TEXT("needle"), FVector(ArmLength, 0.0, 0.0), 3.5f, Near, KNeedle, true);
	Add(TEXT("thermos"), InteractAt(TEXT("thermos"), FVector(55.91, 93.61, 75.34)), 8.f, NetReach, KFixed, true);
	Add(TEXT("book"), InteractAt(TEXT("book"), FVector(35.58, 104.5, 70.56)), 9.f, NetReach, KBook, true);
	Add(TEXT("chute_handle"), InteractAt(TEXT("chute"), FVector(11.7, -40.97, 194.88)), 6.f, Near, KChuteHandle, true);
	Add(TEXT("chute_cover"), FVector(4.3, 0.0, -6.0), 7.f, Near, KChuteCover, true);

	// the couches. His own: the seat sits him down, the buckle is at his right hip, the headrest is the nap. The second
	// one: its back sits him down too (he always takes his own), the folded blanket on its seat is the nap
	const FTransform Couch1(FRotator(0.f, CouchYaw, 0.f), SeatPelvis - FVector(0.0, 0.0, K.Vec(TEXT("seat.pelvis_in_couch"), FVector(0.0, 0.0, 42.0)).Z));
	const FVector Couch2Seat = InteractAt(TEXT("couch_2"), FVector(4.14, 51.93, 47.0));
	Add(TEXT("sit"), InteractAt(TEXT("couch_1"), FVector(4.14, -51.93, 47.0)), 20.f, Near + 20.f, KFixed, true);
	Add(TEXT("buckle"), Couch1.TransformPosition(FVector(2.0, 27.0, 48.0)), 6.f, Near, KFixed, true);
	Add(TEXT("nap"), Couch1.TransformPosition(FVector(-36.0, 0.0, 108.0)), 12.f, Near, KFixed, true);
	Add(TEXT("nap"), Couch2Seat, 16.f, PortReach, KFixed, true);
	Add(TEXT("sit"), CapAt.TransformPosition(FVector(-30.0, 0.0, 85.0)), 16.f, Near + 20.f, KFixed, true);

	// only for sounds
	Add(TEXT("speaker"), Speaker, 0.f, 0.f, KFixed, false);
	Add(TEXT("panel"), PanelCentre, 0.f, 0.f, KFixed, false);
	Add(TEXT("couch"), SeatPelvis, 0.f, 0.f, KFixed, false);
	Add(TEXT("jets"), JetsAt, 0.f, 0.f, KFixed, false);
	{
		const TArray<TSharedPtr<FJsonValue>>& Fans = K.Arr(TEXT("fans"));
		static const TCHAR* const FanIds[2] = { TEXT("fan_0"), TEXT("fan_1") };
		for (int32 I = 0; I < 2; ++I)
		{
			const FVector Def(72.15, I == 0 ? -56.37 : 56.37, 139.69);
			Add(FanIds[I], Fans.IsValidIndex(I) ? FKit::EVec(Fans[I], TEXT("fans[]"), TEXT("at"), Def) : Def, 0.f, 0.f, KFixed, false);
		}
		// four places on the wall for the hull's creaks, the roar and the drips
		static const TCHAR* const HullIds[4] = { TEXT("hull_0"), TEXT("hull_1"), TEXT("hull_2"), TEXT("hull_3") };
		for (int32 I = 0; I < 4; ++I)
		{
			const float A = FMath::DegreesToRadians(45.f + 90.f * float(I));
			const float Z = I % 2 == 0 ? 60.f : 150.f;
			Add(HullIds[I], FVector(FMath::Cos(A) * InnerRadiusAt(Z), FMath::Sin(A) * InnerRadiusAt(Z), Z), 0.f, 0.f, KFixed, false);
		}
	}
}

/** Where a point is now in the cabin frame; with bRules, false when the eye cannot get at it (the far side of the
 *  door, a button under its cover). Occlusion only: never the flight's phase or the hero's posture. */
bool ALyudSpacecraft::FCabinData::PointNow(const FPoint& Pt, const FVector& Eye, bool bRules, FVector& Out) const
{
	Out = Pt.At;
	switch (Pt.Kind)
	{
	case KWheel:
	{
		Out = DoorNow.TransformPosition(WheelInHatch.GetLocation());
		// the hub is on the door's inner face
		return !bRules || FVector::DotProduct(Eye - DoorNow.GetLocation(), DoorNow.GetUnitAxis(EAxis::X)) < 0.0;
	}
	case KHatchEdge:
		Out = DoorNow.TransformPosition(Pt.At);
		return !bRules || (Hatch.Cur < 0.5f && FVector::DotProduct(Eye - DoorNow.GetLocation(), DoorNow.GetUnitAxis(EAxis::X)) < 0.0);
	case KHatchOutside:
		Out = DoorNow.TransformPosition(Pt.At);
		return !bRules || FVector::DotProduct(Eye - DoorNow.GetLocation(), DoorNow.GetUnitAxis(EAxis::X)) > 0.0;
	case KHatchOpening:
		return !bRules || Hatch.Cur >= 0.5f;
	case KLaunch:
		return !bRules || Cover.Cur > 0.5f;
	case KLaunchCover:
		Out = CoverNow.TransformPosition(Pt.At);
		return true;
	case KChuteHandle:
		return !bRules || ChuteCover.Cur > 0.5f;
	case KChuteCover:
		Out = ChuteCoverNow.TransformPosition(Pt.At);
		return true;
	case KNeedle:
		Out = ArmNow.TransformPosition(Pt.At);
		return true;
	case KBook:
		if (Book.Cur > 0.5f)
		{
			Out = BookHeld.GetLocation();
		}
		return true;
	default:
		return true;
	}
}

void ALyudSpacecraft::FCabinData::ApplyLanguage(ALyudSpacecraft& O)
{
	UTexture2D* Plates = LyudText::LoadTexture(TEXT("Spacecraft"), TEXT("T_SC_labels"));
	Texture(O, MidLabels, P().Atlas, Plates);
	Texture(O, MidLamps, P().Atlas, Plates);
	Texture(O, MidCrt, P().Labels, Plates);
	UTexture2D* Sleeves = LyudText::LoadTexture(TEXT("Spacecraft"), TEXT("T_SC_sleeves"));
	for (int32 I = 0; I < NumSleeves; ++I)
	{
		Texture(O, MidSleeve0 + I, P().Atlas, Sleeves);
	}
}

// ------------------------------------------------------------------ showing

void ALyudSpacecraft::FCabinData::ApplyLight(ALyudSpacecraft& O)
{
	const float Q = FMath::RoundToFloat(Light01 * 16.f) / 16.f;
	if (Q == LightShown)
	{
		return;
	}
	LightShown = Q;
	const bool bOn = Q > 0.f && O.bShownInside;
	for (UPointLightComponent* L : O.CabinLamps)
	{
		if (L)
		{
			L->SetIntensity(22.f * Q);
			if (L->IsVisible() != bOn)
			{
				L->SetVisibility(bOn);
			}
		}
	}
	// the cabin's light on the panes and the instrument glasses, the lenses' own glow
	const FLinearColor OnGlass(0.02f + 1.2f * Q, 0.02f + 1.f * Q, 0.03f + 0.8f * Q, 0.f);
	Vector(O, MidGlass, P().Light, OnGlass);
	Vector(O, MidGlassCover, P().Light, OnGlass);
	Scalar(O, MidLampLens, P().EmissiveStrength, 6.f * Q);
	Vector(O, MidLampLens, P().EmissiveFactor, FLinearColor(Q, 0.9f * Q, 0.7f * Q, 1.f));
}

/** The parts with a rule of their own: the record (one is on), the fire shell, the canopies, the jets' glow. */
void ALyudSpacecraft::FCabinData::ApplyOwnShown(ALyudSpacecraft& O)
{
	Show(O, PtRecord, O.bShownInside && Cell >= 0);
	Show(O, PtFireShell, O.bShownOutside && Plasma > 0.002f);
	Show(O, PtChute, O.bShownOutside && ChuteStage == 2);
	Show(O, PtDrogue, O.bShownOutside && ChuteStage == 1);
	if (UPointLightComponent* Jet = O.JetLight)
	{
		const bool bJet = O.bShownOutside && JetsShown > 0.f;
		if (Jet->IsVisible() != bJet)
		{
			Jet->SetVisibility(bJet);
		}
	}
}

void ALyudSpacecraft::FCabinData::ApplyShown(ALyudSpacecraft& O)
{
	const bool bIn = O.bShownInside, bOut = O.bShownOutside;
	for (UStaticMeshComponent* C : O.InsideMeshes)
	{
		if (C && C->IsVisible() != bIn)
		{
			C->SetVisibility(bIn);
		}
	}
	for (UStaticMeshComponent* C : O.OutsideMeshes)
	{
		if (C && C->IsVisible() != bOut)
		{
			C->SetVisibility(bOut);
		}
	}
	for (UStaticMeshComponent* C : O.SharedMeshes)
	{
		if (C && C->IsVisible() != (bIn || bOut))
		{
			C->SetVisibility(bIn || bOut);
		}
	}
	ApplyOwnShown(O);
	LightShown = -1.f;
	ApplyLight(O);
	if (!bIn && !bOut)
	{
		// nobody looks and nothing ticks: whatever was moving is where it was going
		Hatch.Cur = Hatch.Target;
		Wheel.Cur = Wheel.Target;
		Cover.Cur = Cover.Target;
		ChuteCover.Cur = ChuteCover.Target;
		Book.Cur = Book.Target;
		for (FAnim& T : Toggle)
		{
			T.Cur = T.Target;
		}
		ButtonT = HandleT = SipT = CrtFlickT = -1.f;
		Toggle[6].Cur = Toggle[6].Target = 0.f;
		Spin01 = bSpin ? 1.f : 0.f;
		Arm = ArmTarget;
		ApplyParts(O, true);
	}
	O.SetActorTickEnabled(bIn || bOut);
}

// ------------------------------------------------------------------ the moving parts

void ALyudSpacecraft::FCabinData::ApplyParts(ALyudSpacecraft& O, bool bAll)
{
	if (bAll || Hatch.Dirty())
	{
		Hatch.Shown = Hatch.Cur;
		DoorNow = Blend(HatchClosed, HatchOpen, Smooth01(Hatch.Cur));
		SetRel(O, PtHatch, DoorNow);
	}
	if (bAll || Wheel.Dirty())
	{
		// sealed = home; unsealing spins it turns_to_open turns about its hub
		Wheel.Shown = Wheel.Cur;
		SetRot(O, PtWheel, WheelInHatch.GetRotation() * FQuat(FVector::XAxisVector, FMath::DegreesToRadians((1.f - Smooth01(Wheel.Cur)) * WheelTurns * 360.f)));
	}
	if (bAll || Cover.Dirty())
	{
		Cover.Shown = Cover.Cur;
		CoverNow = Blend(CoverClosed, CoverOpen, Smooth01(Cover.Cur));
		SetRel(O, PtLaunchCover, CoverNow);
	}
	if (bAll || ChuteCover.Dirty())
	{
		ChuteCover.Shown = ChuteCover.Cur;
		ChuteCoverNow = Blend(ChuteCoverClosed, ChuteCoverOpen, Smooth01(ChuteCover.Cur));
		SetRel(O, PtChuteCover, ChuteCoverNow);
	}
	for (int32 I = 0; I < NumToggles; ++I)
	{
		if (bAll || Toggle[I].Dirty())
		{
			// an over-centre spring: slow off the stop, fast into the other one
			Toggle[I].Shown = Toggle[I].Cur;
			const float A = Toggle[I].Target > 0.5f ? FMath::Square(Toggle[I].Cur) : 1.f - FMath::Square(1.f - Toggle[I].Cur);
			SetRel(O, PtToggle0 + I, Blend(ToggleOff[I], ToggleOn[I], A));
		}
	}
	if (bAll)
	{
		SetRel(O, PtButton, ButtonOut);
		SetRel(O, PtChuteHandle, HandleRest);
		ArmShown = -1.f;
		FloatShown = -1.f;
	}
	if (bAll || FMath::Abs(Arm - ArmShown) > 0.002f || (Arm == ArmTarget && ArmShown != Arm))
	{
		// 0 on its rest .. 1 on the first groove (lifted on the way) .. 2 at the label
		ArmShown = Arm;
		if (Arm <= 1.f)
		{
			ArmNow = Blend(ArmRest, ArmLeadIn, Smooth01(Arm));
			ArmNow.SetRotation(ArmNow.GetRotation() * FRotator(ArmLiftDeg * FMath::Sin(PI * FMath::Clamp(Arm, 0.f, 1.f)), 0.f, 0.f).Quaternion());
		}
		else
		{
			ArmNow = Blend(ArmLeadIn, ArmRunOut, FMath::Clamp(Arm - 1.f, 0.f, 1.f));
		}
		SetRel(O, PtTonearm, ArmNow);
	}
}

/** The record's label, and its sleeve out of the rack. */
void ALyudSpacecraft::FCabinData::ApplyRecord(ALyudSpacecraft& O)
{
	if (Cell >= 0)
	{
		Scalar(O, MidRecordLabel, P().Cell, float(Cell));
	}
	SleeveOut = -1;
	for (int32 I = 0; I < NumSleeves; ++I)
	{
		if (Cell >= 0 && SleeveOut < 0 && SleeveCell[I] == Cell)
		{
			SleeveOut = I;
		}
	}
	for (int32 I = 0; I < NumSleeves; ++I)
	{
		SetRel(O, PtSleeve0 + I, I == SleeveOut ? SleeveOutAt : SleeveAt[I]);
	}
	ApplyOwnShown(O);
}

void ALyudSpacecraft::FCabinData::TickParts(ALyudSpacecraft& O, float Dt)
{
	Hatch.Step(Dt, 1.5f);
	Wheel.Step(Dt, 0.8f);
	Cover.Step(Dt, 0.28f);
	ChuteCover.Step(Dt, 0.3f);
	Book.Step(Dt, 0.7f);
	for (FAnim& T : Toggle)
	{
		T.Step(Dt, 0.09f);
	}
	if (CrtFlickT >= 0.f)
	{
		// the CRT mode switch is a momentary one: over and back
		CrtFlickT += Dt;
		if (CrtFlickT > 0.16f)
		{
			CrtFlickT = -1.f;
			Toggle[6].Target = 0.f;
		}
	}
	// the arm: carried between its rest and the first groove, then it follows the groove
	if (Arm != ArmTarget)
	{
		if (Arm >= 1.f && ArmTarget >= 1.f && FMath::Abs(ArmTarget - Arm) < 0.05f)
		{
			Arm = ArmTarget;
		}
		else
		{
			Arm = FMath::FInterpConstantTo(Arm, ArmTarget, Dt, 1.6f);
		}
	}
	ApplyParts(O, false);

	if (ButtonT >= 0.f)
	{
		// in at once, held a moment, back on its spring
		ButtonT += Dt;
		const float In = ButtonT < 0.07f ? ButtonT / 0.07f : (ButtonT < 0.2f ? 1.f : FMath::Max(0.f, 1.f - (ButtonT - 0.2f) / 0.14f));
		SetRel(O, PtButton, Blend(ButtonOut, ButtonIn, In));
		if (ButtonT > 0.34f)
		{
			ButtonT = -1.f;
		}
	}
	if (HandleT >= 0.f)
	{
		// a tug that does not give: down its travel and back
		HandleT += Dt;
		const float Out = HandleT < 0.12f ? HandleT / 0.12f : FMath::Max(0.f, 1.f - (HandleT - 0.12f) / 0.4f);
		SetRel(O, PtChuteHandle, Blend(HandleRest, HandlePulled, Smooth01(Out)));
		if (HandleT > 0.52f)
		{
			HandleT = -1.f;
		}
	}

	// the platter: 33 1/3 turns a minute (200 degrees a second) times the model's drag, 1.5 s to speed and to rest
	const float SpinWant = bSpin ? 1.f : 0.f;
	if (Spin01 != SpinWant)
	{
		Spin01 = FMath::FInterpConstantTo(Spin01, SpinWant, Dt, 1.f / 1.5f);
	}
	if (Spin01 > 0.f)
	{
		PlatterDeg = FMath::Fmod(PlatterDeg + 200.f * Spin01 * RecordPitch * Dt, 360.f);
		const int32 Q = FMath::RoundToInt(PlatterDeg * 10.f);
		if (Q != PlatterShown)
		{
			PlatterShown = Q;
			SetRot(O, PtPlatter, PlatterAt.GetRotation() * FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(PlatterDeg)));
		}
	}
}

/**
 * The thermos, the cap and the book: with no load they drift a few centimetres on their tethers (slow sines of the
 * cabin's own clock) and come back under any load. Also the thermos at the hero's mouth for a sip and the book held
 * before his eyes. Three transforms, only while something of this moves.
 */
void ALyudSpacecraft::FCabinData::TickLoose(ALyudSpacecraft& O, float Dt)
{
	const bool bFloat = FeltG < 0.05f;
	if (Float01 != (bFloat ? 1.f : 0.f))
	{
		Float01 = FMath::FInterpConstantTo(Float01, bFloat ? 1.f : 0.f, Dt, bFloat ? 0.3f : 2.5f);
	}
	const bool bSip = SipT >= 0.f;
	if (bSip)
	{
		SipT += Dt;
	}
	if (Float01 <= 0.f && !bSip && !Book.Dirty() && FloatShown == 0.f)
	{
		return;
	}
	if (Float01 > 0.f)
	{
		FloatClock += Dt;
	}
	const float Amp = Smooth01(Float01);
	const float Sip = bSip ? Smooth01(FMath::Min(SipT / 0.7f, (2.6f - SipT) / 0.7f)) : 0.f;
	const float Held = Smooth01(Book.Cur);
	FloatShown = Float01;
	Book.Shown = Book.Cur;
	if (SipT > 2.6f)
	{
		SipT = -1.f;
	}
	struct FLoose
	{
		int32 Part;
		const FTransform* Base;
		float Side, Up, Lift, Phase;
	};
	const FLoose Loose[3] = {
		{ PtThermos, &ThermosAt, 1.2f, 1.8f, 1.5f, 0.f },
		{ PtCap, &CapAt, 2.5f, 2.f, 5.f, 2.1f },
		{ PtBook, &BookAt, 0.8f, 1.2f, 1.f, 4.4f },
	};
	const float C = FloatClock;
	for (int32 I = 0; I < 3; ++I)
	{
		const FLoose& L = Loose[I];
		const FVector Off(Amp * L.Side * FMath::Sin(C * 0.61f + L.Phase), Amp * L.Side * FMath::Sin(C * 0.47f + L.Phase * 1.7f),
			Amp * (L.Lift + L.Up * FMath::Sin(C * 0.39f + L.Phase * 0.6f)));
		const FRotator Tilt(Amp * 3.f * FMath::Sin(C * 0.33f + L.Phase), Amp * 5.f * FMath::Sin(C * 0.27f + L.Phase * 2.f), Amp * 3.f * FMath::Sin(C * 0.41f + L.Phase * 1.3f));
		FTransform T(L.Base->GetRotation() * Tilt.Quaternion(), L.Base->GetLocation() + Off);
		if (I == 0 && Sip > 0.f)
		{
			T = Blend(T, ThermosSip, Sip);
		}
		if (I == 2 && Held > 0.f)
		{
			T = Blend(T, BookHeld, Held);
		}
		if (Amp <= 0.f || !T.GetLocation().Equals(LooseShown[I], 0.03))
		{
			LooseShown[I] = T.GetLocation();
			SetRel(O, L.Part, T);
		}
	}
}

// ------------------------------------------------------------------ the instruments

void ALyudSpacecraft::FCabinData::Instruments(ALyudSpacecraft& O, const LyudFlight::FState& S, float DtIn)
{
	const float Dt = FMath::Clamp(DtIn, 0.f, 0.1f);
	const LyudFlight::FInstruments& In = S.I;
	const bool bPower = In.bPowered;

	// for Tick: the load the loose things feel, the platter's drag
	FeltG = float(S.GLoad);
	RecordPitch = float(S.Record.Pitch);

	// in the real world (under the canopy from the whiteout's peak, afloat) the sun is channel 0; on Europa and in
	// space the star is channel 1 and the planet's fill channel 2
	const int32 Real = (S.bRealWorld || S.Phase == LyudFlight::EPhase::Afloat) ? 1 : 0;
	if (Real != RealWorldShown)
	{
		RealWorldShown = Real;
		for (UStaticMeshComponent* C : O.OutsideMeshes)
		{
			if (C)
			{
				C->SetLightingChannels(Real != 0, Real == 0, Real == 0);
			}
		}
		static const int32 Own[3] = { PtFireShell, PtChute, PtDrogue };
		for (const int32 Part : Own)
		{
			if (UStaticMeshComponent* C = Cast<UStaticMeshComponent>(O.Parts.IsValidIndex(Part) ? O.Parts[Part].Get() : nullptr))
			{
				C->SetLightingChannels(Real != 0, Real == 0, Real == 0);
			}
		}
	}
	if (!O.bShownInside)
	{
		return;
	}

	// the levers: the five of the checklist, the light, the radio (the CRT's is momentary: PlayActionAnim)
	for (int32 I = 0; I < LyudFlight::SwitchCount && I < NumToggles; ++I)
	{
		Toggle[I].Target = S.Switches[I] ? 1.f : 0.f;
	}
	Toggle[5].Target = In.bCabinLight ? 1.f : 0.f;
	Toggle[7].Target = In.bRadio ? 1.f : 0.f;

	// the cabin lamps
	bLightWanted = In.bCabinLight;
	const float LightWant = bLightWanted && bCabinLightAllowed ? 1.f : 0.f;
	if (Light01 != LightWant)
	{
		Light01 = FMath::FInterpConstantTo(Light01, LightWant, Dt, 6.f);
	}
	ApplyLight(O);

	// the panel's backlight: dark till the power switch, then the print, the needles and the ball glow
	if (Backlight != (bPower ? 1.f : 0.f))
	{
		Backlight = FMath::FInterpConstantTo(Backlight, bPower ? 1.f : 0.f, Dt, bPower ? 2.5f : 5.f);
	}
	const float BackQ = FMath::RoundToFloat(Backlight * 32.f) / 32.f;
	if (BackQ != BacklightShown)
	{
		BacklightShown = BackQ;
		Scalar(O, MidDials, P().Backlight, BackQ);
		Scalar(O, MidNeedle, P().Backlight, BackQ);
		Scalar(O, MidBall, P().Backlight, BackQ);
		Scalar(O, MidLabels, P().Backlight, BackQ);
	}

	// the digits: the clock HHMMSS, the range in km, the speed in km/s with two decimals or in m/s
	{
		const float Clock = float(In.ClockDigits[0] * 100000 + In.ClockDigits[1] * 10000 + In.ClockDigits[2] * 1000 + In.ClockDigits[3] * 100 + In.ClockDigits[4] * 10 + In.ClockDigits[5]);
		if (Clock != DigitValue[0])
		{
			DigitValue[0] = Clock;
			Scalar(O, MidDigits, P().Value0, Clock);
		}
		const float Range = float(FMath::Clamp(In.RangeKm, 0, 999999));
		if (Range != DigitValue[1])
		{
			DigitValue[1] = Range;
			Scalar(O, MidDigits, P().Value1, Range);
		}
		const float Speed = FMath::Clamp(FMath::RoundToFloat(float(In.SpeedShown) * (In.bSpeedMs ? 1.f : 100.f)), 0.f, 99999.f);
		if (Speed != DigitValue[2])
		{
			DigitValue[2] = Speed;
			Scalar(O, MidDigits, P().Value2, Speed);
		}
		const int32 Point = In.bSpeedMs ? 0 : 2;
		if (Point != SpeedPoint)
		{
			SpeedPoint = Point;
			Vector(O, MidDigits, P().Win2, FLinearColor(float(DigitFirst[2]), float(DigitCount[2]), 0.f, float(Point)));
		}
		// (keep leading zeros, blank, colon steady): blank without power; the clock's colon blinks only while it runs
		const float Blank = bPower ? 0.f : 1.f;
		const int32 Opt0 = (bPower ? 0 : 1) + (S.bFlying ? 0 : 2);
		if (Opt0 != DigitOpt[0])
		{
			DigitOpt[0] = Opt0;
			Vector(O, MidDigits, P().Opt0, FLinearColor(1.f, Blank, S.bFlying ? 0.f : 1.f, 0.f));
		}
		const int32 Opt1 = bPower ? 0 : 1;
		if (Opt1 != DigitOpt[1])
		{
			DigitOpt[1] = DigitOpt[2] = Opt1;
			Vector(O, MidDigits, P().Opt1, FLinearColor(0.f, Blank, 0.f, 0.f));
			Vector(O, MidDigits, P().Opt2, FLinearColor(0.f, Blank, 0.f, 0.f));
		}
	}

	// the lamp board: mode + 4 x class per lamp, four lamps a vector. The model blinks bLit on its own clock (a save
	// keeps the phase) and lights every bulb for the lamp test. Without power: one amber standby lamp, slowly blinking
	{
		const bool bStandbyLamp = true;
		bool bChanged[6] = { false, false, false, false, false, false };
		for (int32 L = 0; L < NumLamps && L < LyudFlight::LampCount; ++L)
		{
			float Code = 4.f * LampClass[L] + (bPower && (S.Lamps[L].bLit || In.bLampTest) ? 1.f : 0.f);
			if (!bPower && L == 0 && bStandbyLamp)
			{
				Code = 4.f * 1.f + 2.f;
			}
			if (Code != LampCode[L])
			{
				LampCode[L] = Code;
				bChanged[L / 4] = true;
			}
		}
		if (bChanged[0]) { Vector(O, MidLamps, P().LampsA, FLinearColor(LampCode[0], LampCode[1], LampCode[2], LampCode[3])); }
		if (bChanged[1]) { Vector(O, MidLamps, P().LampsB, FLinearColor(LampCode[4], LampCode[5], LampCode[6], LampCode[7])); }
		if (bChanged[2]) { Vector(O, MidLamps, P().LampsC, FLinearColor(LampCode[8], LampCode[9], LampCode[10], LampCode[11])); }
		if (bChanged[3]) { Vector(O, MidLamps, P().LampsD, FLinearColor(LampCode[12], LampCode[13], LampCode[14], LampCode[15])); }
		if (bChanged[4]) { Vector(O, MidLamps, P().LampsE, FLinearColor(LampCode[16], LampCode[17], LampCode[18], LampCode[19])); }
		if (bChanged[5]) { Vector(O, MidLamps, P().LampsF, FLinearColor(LampCode[20], LampCode[21], LampCode[22], LampCode[23])); }
	}

	// the shield's bar
	{
		const float Level = bPower ? FMath::RoundToFloat(float(In.Hull01) * 100.f) / 100.f : 0.f;
		if (Level != BarShown)
		{
			BarShown = Level;
			Scalar(O, MidBar, P().Level, Level);
		}
	}

	// the CRT: the tube warms up; page 0 the trajectory, 1 the attitude, 2 the load now and its peak
	{
		if (CrtPower != (bPower ? 1.f : 0.f))
		{
			CrtPower = FMath::FInterpConstantTo(CrtPower, bPower ? 1.f : 0.f, Dt, bPower ? 0.8f : 4.f);
		}
		const float PowerQ = FMath::RoundToFloat(CrtPower * 32.f) / 32.f;
		if (PowerQ != CrtPowerShown)
		{
			CrtPowerShown = PowerQ;
			Scalar(O, MidCrt, P().Power, PowerQ);
		}
		if (S.Phase == LyudFlight::EPhase::Parked || S.Phase == LyudFlight::EPhase::Prelaunch)
		{
			PeakG = 0.f;
		}
		PeakG = FMath::Max(PeakG, float(In.GMeter));
		const bool bPage = In.CrtMode != CrtModeShown;
		if (bPage)
		{
			CrtModeShown = In.CrtMode;
			Scalar(O, MidCrt, P().Mode, float(In.CrtMode));
		}
		if (In.CrtPhase != CrtPhaseShown)
		{
			CrtPhaseShown = In.CrtPhase;
			Scalar(O, MidCrt, P().Phase, float(In.CrtPhase));
		}
		if (bPower && In.CrtMode == 0)
		{
			const FVector Europa(In.CrtEuropaX, In.CrtEuropaY, In.CrtEuropaR), Planet(In.CrtPlanetX, In.CrtPlanetY, In.CrtPlanetR);
			if (!Europa.Equals(CrtEuropaShown, 1e-3))
			{
				CrtEuropaShown = Europa;
				Vector(O, MidCrt, P().PosEuropa, FLinearColor(float(Europa.X), float(Europa.Y), float(Europa.Z), 0.f));
			}
			if (!Planet.Equals(CrtPlanetShown, 1e-3))
			{
				CrtPlanetShown = Planet;
				Vector(O, MidCrt, P().PosPlanet, FLinearColor(float(Planet.X), float(Planet.Y), float(Planet.Z), 0.f));
			}
			const float Craft = FMath::RoundToFloat(float(In.CrtProgress) * 2000.f) / 2000.f;
			if (Craft != CraftShown)
			{
				CraftShown = Craft;
				Scalar(O, MidCrt, P().Craft01, Craft);
			}
		}
		else if (bPower && In.CrtMode == 1)
		{
			const FLinearColor Att = CrtAtt(float(S.RollDeg), float(S.PitchDeg));
			const FVector2D Now(Att.R, Att.G);
			if (bPage || !Now.Equals(CrtAttShown, 0.003))
			{
				CrtAttShown = Now;
				Vector(O, MidCrt, P().Att, Att);
			}
		}
		else if (bPower)
		{
			const FVector2D Now(FMath::RoundToFloat(float(In.GMeter) * 50.f) / 50.f, FMath::RoundToFloat(PeakG * 50.f) / 50.f);
			if (bPage || !Now.Equals(CrtGShown, 1e-4))
			{
				CrtGShown = Now;
				Vector(O, MidCrt, P().GLoad, FLinearColor(float(Now.X), float(Now.Y), 0.f, 0.f));
			}
		}
	}

	// the needles: a light pointer on a hairspring (about 2 Hz, a little under-damped), trembling with the shake; set
	// on the mesh in tenths of a degree, so a needle at rest costs nothing
	{
		if (In.AltDialKm >= 100.0)
		{
			bAltCoarse = true;
		}
		else if (In.AltDialKm < 95.0)
		{
			bAltCoarse = false;
		}
		// the altimeter's unit: 1 km a figure near the ground, 100 km a figure far out (the needles swing over once)
		const float AltUnits = float(In.AltDialKm / (bAltCoarse ? 100.0 : 1.0));
		const float SourceValue[SrcNone] = { AltUnits, AltUnits, float(In.GMeter), float(In.PressMmHg), float(In.O2Pct), float(In.Co2Pct), float(In.TempC), float(In.Volts), float(In.FuelPct) };
		const float Tremble = 1.6f * float(S.Shake);
		const float Clock = float(FMath::Fmod(S.T, 512.0)) * 17.f;
		const float W = 13.f, Zeta = 0.6f;
		for (int32 I = 0; I < NumNeedles; ++I)
		{
			FNeedle& N = Needles[I];
			if (N.Source >= SrcNone)
			{
				continue;
			}
			const bool bWrap = N.Source <= SrcAltShort;
			float Target = bWrap ? 0.f : N.AMin;
			if (bPower && bWrap)
			{
				Target = FMath::Frac(SourceValue[N.Source] / N.PerRev) * 360.f;
			}
			else if (bPower)
			{
				Target = N.AMin + FMath::Clamp((SourceValue[N.Source] - N.VMin) / FMath::Max(N.VMax - N.VMin, 1e-3f), -0.02f, 1.03f) * (N.AMax - N.AMin);
			}
			const float Err = bWrap ? FMath::FindDeltaAngleDegrees(N.Angle, Target) : Target - N.Angle;
			N.Rate += (W * W * Err - 2.f * Zeta * W * N.Rate) * Dt;
			N.Angle += N.Rate * Dt;
			if (bWrap)
			{
				N.Angle = FMath::Fmod(N.Angle + 720.f, 360.f);
			}
			const int32 Q = FMath::RoundToInt((N.Angle + Tremble * Noise(Clock + 9.7f * float(I))) * 10.f);
			if (Q != N.Shown)
			{
				N.Shown = Q;
				SetRot(O, PtNeedle0 + I, N.At.GetRotation() * NeedleTurn(float(Q) * 0.1f));
			}
		}
	}

	// the «Глобус»: the map turned under the fixed crosshair (clockwork: it turns without power too)
	{
		double MapLat = 0.0, MapLon = 0.0;
		ModelToMap(In.GlobeLatDeg, In.GlobeLonRelDeg, MapLat, MapLon);
		const int32 LatQ = FMath::RoundToInt(float(MapLat) * 10.f), LonQ = FMath::RoundToInt(float(MapLon) * 10.f);
		if (LatQ != GlobeLatShown || LonQ != GlobeLonShown)
		{
			GlobeLatShown = LatQ;
			GlobeLonShown = LonQ;
			SetRot(O, PtGlobe, GlobeAt.GetRotation() * GlobeTurn(float(LatQ) * 0.1f, float(LonQ) * 0.1f));
		}
	}

	// the attitude ball: caged till the gyros are on
	if (S.Switches[LyudFlight::SwGyro])
	{
		const int32 Q[3] = { FMath::RoundToInt(float(S.RollDeg) * 5.f), FMath::RoundToInt(float(S.PitchDeg) * 5.f), FMath::RoundToInt(float(S.YawDeg) * 5.f) };
		if (Q[0] != BallShown[0] || Q[1] != BallShown[1] || Q[2] != BallShown[2])
		{
			BallShown[0] = Q[0];
			BallShown[1] = Q[1];
			BallShown[2] = Q[2];
			SetRot(O, PtBall, BallAt.GetRotation() * BallTurn(float(Q[0]) * 0.2f, float(Q[1]) * 0.2f, float(Q[2]) * 0.2f));
		}
	}
}

// ------------------------------------------------------------------ ALyudSpacecraft

ALyudSpacecraft::ALyudSpacecraft()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
}

void ALyudSpacecraft::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	if (!Data)
	{
		Data = new FCabinData();
	}
	Data->K.Root = FCabinData::FKit::ReadFile(TEXT("Spacecraft/kit.json"));
	const bool bKitRead = Data->K.Root.IsValid();
	if (!bKitRead)
	{
		UE_LOG(LogTemp, Warning, TEXT("Spacecraft: missing Content/Spacecraft/kit.json (blender/spacecraft/spacecraft_kit.py): built-in measures, no parts placed from lists"));
	}
	Data->LoadKit();
	Parts.Reset();
	Parts.SetNum(FCabinData::PtCount);
	Mids.Reset();
	Mids.SetNum(FCabinData::MidCount);
	Data->BuildMeshes(*this);
	Data->BuildCollision(*this);
	Data->BuildPoints();
	Data->K.Root.Reset();
	Data->ApplyLanguage(*this);
	if (!LanguageHandle.IsValid())
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Language")))
		{
			LanguageHandle = Variable->OnChangedDelegate().AddUObject(this, &ALyudSpacecraft::OnLanguageChanged);
		}
	}
	// everything hidden, cold and where the director last put it
	Data->Forget();
	Data->ApplyParts(*this, true);
	Data->ApplyRecord(*this);
	bShownInside = bShownOutside = false;
	Data->ApplyShown(*this);
	UE_LOG(LogTemp, Display, TEXT("Spacecraft: cabin built (%d inside, %d outside, %d shared meshes, %d needles, %d points, kit %s)"),
		InsideMeshes.Num(), OutsideMeshes.Num(), SharedMeshes.Num(), Data->NumNeedles, Data->Points.Num(), bKitRead ? TEXT("read") : TEXT("missing"));
}

void ALyudSpacecraft::Unbuild()
{
	if (!bBuilt)
	{
		return;
	}
	bBuilt = false;
	bShownInside = bShownOutside = false;
	SetActorTickEnabled(false);
	auto Drop = [](USceneComponent* C)
	{
		if (IsValid(C) && !C->IsBeingDestroyed())
		{
			C->DestroyComponent();
		}
	};
	for (USceneComponent* C : Parts) { Drop(C); }
	for (UStaticMeshComponent* C : InsideMeshes) { Drop(C); }
	for (UStaticMeshComponent* C : OutsideMeshes) { Drop(C); }
	for (UStaticMeshComponent* C : SharedMeshes) { Drop(C); }
	for (UPointLightComponent* C : CabinLamps) { Drop(C); }
	Drop(JetLight);
	Parts.Reset();
	InsideMeshes.Reset();
	OutsideMeshes.Reset();
	SharedMeshes.Reset();
	CabinLamps.Reset();
	JetLight = nullptr;
	// the instances go too: they hold the materials and the atlases (Build makes them again, once)
	Mids.Reset();
}

void ALyudSpacecraft::ReleaseData()
{
	if (LanguageHandle.IsValid())
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Language")))
		{
			Variable->OnChangedDelegate().Remove(LanguageHandle);
		}
		LanguageHandle.Reset();
	}
	delete Data;
	Data = nullptr;
	bBuilt = false;
}

void ALyudSpacecraft::EndPlay(const EEndPlayReason::Type Reason)
{
	ReleaseData();
	Super::EndPlay(Reason);
}

void ALyudSpacecraft::BeginDestroy()
{
	// an actor destroyed without EndPlay (a failed spawn, a world torn down before play) must not leak its data
	ReleaseData();
	Super::BeginDestroy();
}

void ALyudSpacecraft::OnLanguageChanged(IConsoleVariable* /*Variable*/)
{
	if (Data && bBuilt)
	{
		Data->ApplyLanguage(*this);
	}
}

void ALyudSpacecraft::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Data || !bBuilt || (!bShownInside && !bShownOutside))
	{
		return;
	}
	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 0.1f);
	Data->TickParts(*this, Dt);
	if (bShownInside)
	{
		Data->TickLoose(*this, Dt);
	}
}

void ALyudSpacecraft::ApplyState(const LyudFlight::FState& State, float DeltaSeconds)
{
	if (Data && bBuilt)
	{
		Data->Instruments(*this, State, DeltaSeconds);
	}
}

// ---- what the hero looks at

FString ALyudSpacecraft::ActionAt(const FVector& Eye, const FVector& LookDir, FVector* OutWhere) const
{
	if (!Data || !bBuilt)
	{
		return FString();
	}
	const FTransform& ToWorld = GetActorTransform();
	const FVector E = ToWorld.InverseTransformPosition(Eye);
	const FVector D = ToWorld.InverseTransformVectorNoScale(LookDir).GetSafeNormal();
	int32 Best = INDEX_NONE;
	float BestAngle = 1000.f;
	FVector BestAt = FVector::ZeroVector;
	for (int32 I = 0; I < Data->Points.Num(); ++I)
	{
		const FCabinData::FPoint& Pt = Data->Points[I];
		FVector At;
		if (!Pt.bAction || !Data->PointNow(Pt, E, true, At))
		{
			continue;
		}
		const FVector To = At - E;
		const float Dist = float(To.Size());
		if (Dist > Pt.Reach || Dist < 1.f)
		{
			continue;
		}
		// the nearest to the line of sight wins; each target has its own widest angle by its size as seen from here
		// (neighbouring switches are 7 cm apart: about 3 degrees from the couch)
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(FVector::DotProduct(To, D)) / Dist, -1.f, 1.f)));
		const float Widest = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan(Pt.Radius / Dist)) * 1.6f + 1.2f, 3.f, 12.f);
		if (Angle <= Widest && Angle < BestAngle)
		{
			Best = I;
			BestAngle = Angle;
			BestAt = At;
		}
	}
	if (Best == INDEX_NONE)
	{
		return FString();
	}
	if (OutWhere)
	{
		*OutWhere = ToWorld.TransformPosition(BestAt);
	}
	return Data->Points[Best].IdString;
}

FVector ALyudSpacecraft::ActionPoint(FName Action) const
{
	// unknown: the cabin's middle, a metre up
	FVector Local(0.0, 0.0, 100.0);
	if (Data)
	{
		for (const FCabinData::FPoint& Pt : Data->Points)
		{
			if (Pt.Id == Action)
			{
				Data->PointNow(Pt, FVector::ZeroVector, false, Local);
				break;
			}
		}
	}
	return GetActorTransform().TransformPosition(Local);
}

void ALyudSpacecraft::PlayActionAnim(FName Action, bool bOn, bool bSnap)
{
	if (!Data)
	{
		return;
	}
	const FCabinData::FIdNames& Id = FCabinData::Ids();
	if (Action == Id.Hatch)
	{
		Data->Hatch.Set(bOn, bSnap);
	}
	else if (Action == Id.Wheel)
	{
		Data->Wheel.Set(bOn, bSnap);
	}
	else if (Action == Id.Cover)
	{
		Data->Cover.Set(bOn, bSnap);
	}
	else if (Action == Id.ChuteCover)
	{
		Data->ChuteCover.Set(bOn, bSnap);
	}
	else if (Action == Id.Book)
	{
		Data->Book.Set(bOn, bSnap);
	}
	else if (Action == Id.Launch)
	{
		Data->ButtonT = bOn && !bSnap ? 0.f : -1.f;
	}
	else if (Action == Id.ChuteHandle)
	{
		Data->HandleT = bOn && !bSnap ? 0.f : -1.f;
	}
	else if (Action == Id.Thermos)
	{
		Data->SipT = bOn && !bSnap ? 0.f : -1.f;
		Data->FloatShown = -1.f;
	}
	else if (Action == Id.Buckle)
	{
		// the belts are part of the couch's mesh: nothing to move, the state is kept for whoever asks
		Data->bBuckled = bOn;
	}
	else if (Action == Id.SwCrt)
	{
		if (!bSnap)
		{
			Data->Toggle[6].Target = 1.f;
			Data->CrtFlickT = 0.f;
		}
	}
	else if (Action == Id.SwLight || Action == Id.SwRadio)
	{
		// their levers follow FState (bCabinLight, bRadio) in ApplyState
	}
	if (bSnap && bBuilt)
	{
		Data->ApplyParts(*this, false);
	}
}

bool ALyudSpacecraft::IsAnimating(FName Action) const
{
	if (!Data || !bBuilt || (!bShownInside && !bShownOutside))
	{
		return false;
	}
	const FCabinData::FIdNames& Id = FCabinData::Ids();
	if (Action == Id.Hatch)
	{
		return Data->Hatch.Moving();
	}
	if (Action == Id.Wheel)
	{
		return Data->Wheel.Moving();
	}
	if (Action == Id.Cover)
	{
		return Data->Cover.Moving();
	}
	if (Action == Id.ChuteCover)
	{
		return Data->ChuteCover.Moving();
	}
	return false;
}

// ---- places (the measures have their built-in values before Build)

FTransform ALyudSpacecraft::SeatEye() const
{
	const FCabinData& D = Data ? *Data : FCabinData::Defaults();
	return FTransform(D.SeatLook.Rotation(), D.SeatEyes) * GetActorTransform();
}

FTransform ALyudSpacecraft::SeatPelvis() const
{
	const FCabinData& D = Data ? *Data : FCabinData::Defaults();
	return FTransform(FRotator(0.f, D.CouchYaw, 0.f), D.SeatPelvis) * GetActorTransform();
}

FTransform ALyudSpacecraft::PortholeView(FName Which) const
{
	const FCabinData& D = Data ? *Data : FCabinData::Defaults();
	const FCabinData::FIdNames& Id = FCabinData::Ids();
	const int32 I = Which == Id.Left ? 1 : (Which == Id.Right ? 2 : 0);
	return FTransform(D.PortDir[I].Rotation(), D.PortCamera[I]) * GetActorTransform();
}

FVector ALyudSpacecraft::HatchExit() const
{
	return GetActorTransform().TransformPosition(Data ? Data->StandOutside : FVector(-208.48, 0.0, 34.62));
}

FVector ALyudSpacecraft::HatchInside() const
{
	return GetActorTransform().TransformPosition(Data ? Data->StandInside : FVector(-85.88, 0.0, 0.0));
}

FVector ALyudSpacecraft::StandPoint() const
{
	return GetActorTransform().TransformPosition(Data ? Data->StandCentre : FVector(-40.0, 0.0, 0.0));
}

float ALyudSpacecraft::WaterlineZ() const
{
	return Data ? Data->Waterline : -10.3f;
}

bool ALyudSpacecraft::IsInside(const FVector& World) const
{
	// the pressure shell as a cone up to the knee and a cylinder above it
	const FVector L = GetActorTransform().InverseTransformPosition(World);
	const float CeilingZ = Data ? Data->CeilingZ : 205.f;
	if (L.Z < -2.0 || L.Z > CeilingZ + 2.0)
	{
		return false;
	}
	const float WallR = Data ? Data->InnerRadiusAt(float(L.Z)) : 102.f;
	return FVector2D(L).SizeSquared() < FMath::Square(double(WallR) + 4.0);
}

// ---- the outside

void ALyudSpacecraft::SetExterior(const FLyudCraftExterior& Exterior)
{
	if (!Data || !bBuilt)
	{
		return;
	}
	FCabinData& D = *Data;
	// the glass: frost, soot, spray, the cloud's white (256 steps each)
	const float Glass[4] = { Exterior.Frost, Exterior.Scorch, Exterior.Wet, Exterior.Fog };
	bool bGlass[4] = { false, false, false, false };
	for (int32 I = 0; I < 4; ++I)
	{
		const float Q = FMath::RoundToFloat(FMath::Clamp(Glass[I], 0.f, 1.f) * 256.f) / 256.f;
		bGlass[I] = Q != D.GlassShown[I];
		D.GlassShown[I] = Q;
	}
	if (bGlass[0]) { FCabinData::Scalar(*this, FCabinData::MidGlass, FCabinData::P().Frost, D.GlassShown[0]); }
	if (bGlass[1]) { FCabinData::Scalar(*this, FCabinData::MidGlass, FCabinData::P().Scorch, D.GlassShown[1]); }
	if (bGlass[2]) { FCabinData::Scalar(*this, FCabinData::MidGlass, FCabinData::P().Wet, D.GlassShown[2]); }
	if (bGlass[3]) { FCabinData::Scalar(*this, FCabinData::MidGlass, FCabinData::P().Fog, D.GlassShown[3]); }
	// the plasma's shell: its strength, its colour by the shield's heat, its streaming faster at the peak.
	// PlasmaKelvin, SunLocal and Time have no parameter in these materials (M_SC_Plasma takes its colour from Heat
	// and runs on the engine's clock; M_SC_Glass is unlit): read by nobody here
	const float PlasmaQ = FMath::RoundToFloat(FMath::Clamp(Exterior.Plasma, 0.f, 1.f) * 200.f) / 200.f;
	if (PlasmaQ != D.PlasmaShown)
	{
		D.PlasmaShown = PlasmaQ;
		D.Plasma = PlasmaQ;
		FCabinData::Scalar(*this, FCabinData::MidPlasma, FCabinData::P().Plasma, PlasmaQ);
		FCabinData::Scalar(*this, FCabinData::MidPlasma, FCabinData::P().Flow, 0.6f + 0.7f * PlasmaQ);
		D.ApplyOwnShown(*this);
	}
	const float HeatQ = FMath::RoundToFloat(FMath::Clamp(Exterior.Heat, 0.f, 1.f) * 100.f) / 100.f;
	if (HeatQ != D.HeatShown)
	{
		D.HeatShown = HeatQ;
		FCabinData::Scalar(*this, FCabinData::MidPlasma, FCabinData::P().Heat, HeatQ);
	}
}

void ALyudSpacecraft::SetChute(int32 Stage, float Inflate, FVector2D SwingDeg, float YawDeg)
{
	if (!Data || !bBuilt)
	{
		return;
	}
	FCabinData& D = *Data;
	Stage = FMath::Clamp(Stage, 0, 2);
	if (Stage != D.ChuteStage)
	{
		D.ChuteStage = Stage;
		D.ChuteInflateShown = -1.f;
		D.ApplyOwnShown(*this);
	}
	if (Stage == 0)
	{
		return;
	}
	// the canopy leans and twists about the riser fitting
	const FVector Pose(SwingDeg.X, SwingDeg.Y, YawDeg);
	if (!Pose.Equals(D.ChuteShown, 0.05))
	{
		D.ChuteShown = Pose;
		FCabinData::SetRot(*this, FCabinData::PtChuteRig, FRotator(SwingDeg.X, YawDeg, SwingDeg.Y).Quaternion());
	}
	// from a streamer (a tenth as wide, a little longer) to the full canopy
	const float Q = FMath::RoundToFloat(FMath::Clamp(Inflate, 0.f, 1.f) * 100.f) / 100.f;
	if (Q != D.ChuteInflateShown)
	{
		D.ChuteInflateShown = Q;
		const int32 Part = Stage == 2 ? FCabinData::PtChute : FCabinData::PtDrogue;
		if (USceneComponent* C = Parts.IsValidIndex(Part) ? Parts[Part].Get() : nullptr)
		{
			const float A = FCabinData::Smooth01(Q);
			const float Wide = FMath::Lerp(0.1f, 1.f, A);
			C->SetRelativeScale3D(FVector(Wide, Wide, FMath::Lerp(1.08f, 1.f, A)));
		}
	}
}

void ALyudSpacecraft::SetJets(float Jets01)
{
	if (!Data || !bBuilt)
	{
		return;
	}
	const float Q = FMath::RoundToFloat(FMath::Clamp(Jets01, 0.f, 1.f) * 20.f) / 20.f;
	if (Q != Data->JetsShown)
	{
		// no flame mesh in the kit: the jets are their glow on the sea and the shield
		Data->JetsShown = Q;
		if (UPointLightComponent* Jet = JetLight)
		{
			Jet->SetIntensity(12000.f * Q);
		}
		Data->ApplyOwnShown(*this);
	}
}

// ---- the record player

void ALyudSpacecraft::SetRecord(int32 RecordCell, bool bSpinning, float Arm01)
{
	if (!Data)
	{
		return;
	}
	Data->bSpin = bSpinning;
	Data->ArmTarget = FMath::Clamp(Arm01, 0.f, 2.f);
	const int32 NewCell = FMath::Clamp(RecordCell, -1, 7);
	if (NewCell != Data->Cell)
	{
		Data->Cell = NewCell;
		if (bBuilt)
		{
			Data->ApplyRecord(*this);
		}
	}
}

// ---- lamps and showing

void ALyudSpacecraft::SetCabinLight(bool bOn)
{
	if (Data)
	{
		Data->bCabinLightAllowed = bOn;
	}
}

void ALyudSpacecraft::SetShownInside(bool bShown)
{
	if (bShownInside == bShown)
	{
		return;
	}
	bShownInside = bShown;
	if (Data && bBuilt)
	{
		Data->ApplyShown(*this);
	}
}

void ALyudSpacecraft::SetShownOutside(bool bShown)
{
	if (bShownOutside == bShown)
	{
		return;
	}
	bShownOutside = bShown;
	if (Data && bBuilt)
	{
		Data->ApplyShown(*this);
	}
}
