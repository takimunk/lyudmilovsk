#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudCruiseSeascape.generated.h"

class ALyudSea;
class ALyudSky;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
struct FLyudCruiseView;

/**
 * What passes the motor ship on her cruise (docs/ship.md): the islands of the chart, the home coast and the far
 * volcanoes, shown round a ship that stands still at the cruise stage. Each frame while the hero is aboard every piece
 * is put at Stage + Rot(StageYaw - Heading) * (its chart place - the ship's chart place) (LyudSeascape::ChartToStage),
 * sunk "hull down" by the sea's curve the flat world lacks (LyudSeascape::HullDownM, lyud.Cruise.EarthRadius), hidden
 * beyond its range or the air's visibility; the height fog hazes it like everything else. So when the ship turns the
 * scenery turns round her; the sun, the wind and the waves' directions stay the world's (accepted).
 *
 * Pieces: blender/ship/seascape_kit.py -> /Game/Cruise/Meshes/seascape_kit (unreal/scripts/import_cruise.sh), listed
 * with their sockets in Content/Cruise/seascape.json; the places on the chart are cruise.json's when it has the island.
 * One movable mesh component a piece, no collision (nobody reaches them), out of Lumen and the distance fields, shadows
 * only within 1.5 km, a far level beyond 4 km, pieces beyond 3 km moved ten times a second. A missing kit shows nothing.
 *
 * At night: the Far Light's lamp flashes as its beam sweeps past the ship and a soft beam turns over the water; the
 * pier's light; the town's windows along the home coast. The Needle's Eye turns so that its eye looks at a low sun.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ALyudCruiseSeascape : public AActor
{
	GENERATED_BODY()

public:
	ALyudCruiseSeascape();

	/** Reads the two json files and makes the pieces, all hidden. False: no seascape.json. */
	bool Setup();
	/** Every frame while the hero is aboard. Stage: the ship frame to the world (ULyudCruise::StageTransform). */
	void Place(const FLyudCruiseView& View, const FTransform& Stage, float DeltaSeconds);
	/** Everything out of sight (the hero went ashore). */
	void HideAll();

	/** Where an island's origin is shown now (world cm) and how far off it is (m); false: unknown or not shown. */
	bool GetIslandWorld(FName Id, FVector& OutOrigin, float* OutDistM = nullptr) const;
	/** A socket of seascape.json ("lamp", "colony", "anchorage"...) in the world now; false: unknown or not shown. */
	bool GetSocketWorld(FName Island, FName Socket, FVector& OutWorld) const;
	int32 NumShown() const;
	FString Report() const;

private:
	struct FPiece
	{
		FName Id;
		int32 Near = INDEX_NONE, Far = INDEX_NONE, Lights = INDEX_NONE;   // indices into Meshes
		int32 Comp = INDEX_NONE, LightsComp = INDEX_NONE;                 // indices into Comps
		FVector2D Chart = FVector2D::ZeroVector;      // m
		float YawDeg = 0.f;                            // on the chart
		float TopM = 30.f, RadiusM = 100.f;
		float RangeM = 18000.f, FarM = 4000.f;
		TMap<FName, FVector> Sockets;                  // piece frame, cm
		float EyeAxisDeg = 0.f;
		bool bFaceSun = false;
		// state
		bool bShown = false, bFar = false, bShadow = false, bLit = false;
		float DistM = 0.f;
		float SlowClock = 0.f;
		FTransform World = FTransform::Identity;
	};
	TArray<FPiece> Pieces;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> Meshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Comps;
	/** Emissive instances by slot name ("glow_lamp", "glow_window"), put on every piece's glow slots. */
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UMaterialInstanceDynamic>> Glows;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> GlowMaster;

	/** A point of light seen from miles off: a small unlit ball at a socket, kept a pixel or two wide by its scale. */
	struct FStar
	{
		int32 Piece = INDEX_NONE;
		FName Socket;
		int32 Comp = INDEX_NONE;
		int32 Mid = INDEX_NONE;       // into StarMids
		float PeriodS = 0.f;          // 0: a steady light
		float RangeM = 20000.f;
		bool bShown = false;
	};
	TArray<FStar> Stars;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> StarMids;
	/** The Far Light's beam: two crossed cards a kilometre long turning about the lamp. */
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Beam;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> BeamMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BeamMid;
	bool bBeamShown = false;

	TWeakObjectPtr<ALyudSea> Sea;
	TWeakObjectPtr<ALyudSky> Sky;
	float VisibilityM = 30000.f;
	float WeatherClock = 0.f;
	float LastHeading = 0.f;
	float Dark = 0.f, Dusk = 0.f;
	float LastGlow = -1.f;

	int32 AddMesh(const FString& Name);
	UStaticMeshComponent* MakeComp(const TCHAR* Name, UStaticMesh* Mesh);
	void DressGlow(UStaticMeshComponent* Comp);
	int32 FindPiece(FName Id) const;
	void UpdateLights(const FVector& Eye, float DeltaSeconds);
};
