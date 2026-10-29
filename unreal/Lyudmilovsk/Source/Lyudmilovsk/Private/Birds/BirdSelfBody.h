#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BirdLook.h"
#include "BirdSelfBody.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/** What the body is doing this frame (from the flight model and the pawn). */
struct FBirdBodyPose
{
	float SpreadL = 1.f, SpreadR = 1.f;   // 0 pulled in .. 1 spread
	float PhaseL = 0.f, PhaseR = 0.f;     // stroke phase 0..1 (0..0.5 down)
	bool BeatL = false, BeatR = false;    // a burst is under way
	float Flare = 0.f;
	float Load = 1.f;                     // g on the wings
	float Speed = 0.f;                    // airspeed, m/s
	float Fold = 0.f;                     // 1 = perched or walking: wings folded on the back
	float Legs = 0.f;                     // 0 tucked .. 1 down
	float TailFan = 0.f;                  // 0 closed .. 1 fanned
	float Ruffle = 0.f;                   // after a knock: feathers out of place
};

/**
 * The player bird as you see it from its own eye: a body clad in contour feathers, two wings of separate
 * flight feathers (primaries, secondaries, tertials, three rows of coverts, scapulars, alula), a tail of
 * twelve, legs and toes, and the beak (on the head, which the camera is). Built in code for any species
 * from its size (species.json) and its paint (BirdLook); the feathers are instances of one mesh whose
 * material reads the paint from per-instance custom data. Origin at the centre of mass, X forward, Z up,
 * centimetres. The wings and tail are re-posed every frame from FBirdBodyPose.
 */
UCLASS(ClassGroup = Birds)
class UBirdSelfBody : public USceneComponent
{
	GENERATED_BODY()

public:
	UBirdSelfBody();

	/** Builds the body for a species (sizes in cm). Head is where the beak goes (the camera's component). */
	void Build(FName SpeciesId, float LengthCm, float WingspanCm, USceneComponent* Head);

	void ApplyPose(const FBirdBodyPose& Pose, float Time);

	/** Where the eye is, relative to the body (cm): the head pivots about it. */
	FVector EyeOffset() const { return Eye; }
	/** The neck's base, relative to the body. */
	FVector NeckOffset() const { return Neck; }
	/** Below the centre to the feet when standing (cm). */
	float StandHeight() const { return Stand; }
	float BodyRadius() const { return Radius; }
	/** Show or hide the whole bird (the camera flight in and out shows it from outside). */
	void SetShown(bool bShown, bool bWithHead);
	/** Seen from its own eye: the wings are drawn as through a wider lens (K = tan(screen half-FOV) /
	 *  tan(wide half-FOV)), pulled toward the line of sight so the wings beside the bird reach into the
	 *  screen's edges. K = 1 is the true view. */
	void SetViewSqueeze(const FVector& CamLoc, const FRotator& CamRot, float K)
	{
		// kept in the body's space
		ViewLoc = GetComponentTransform().InverseTransformPosition(CamLoc);
		ViewRot = (GetComponentQuat().Inverse() * CamRot.Quaternion()).Rotator();
		ViewK = FMath::Clamp(K, 0.05f, 1.f);
	}
	/** 0..1: in first person, how far the wings are drawn forward of their roots, beside the eye instead of
	 *  behind it (with the wide first-person view that puts them at the screen's edges). */
	void SetViewForward(float Amount) { ViewForward = FMath::Clamp(Amount, 0.f, 1.f); }
	/** Torso length, cm. */
	float TorsoLength() const { return L; }
	/** The sky's light on the upper side of the feathers (0 at night .. ~0.2 by day): the close-up wing is never a pure silhouette. */
	void SetSkyFill(float Amount);

	static const FName FeatherMaterialPath;
	static const FName BodyMaterialPath;

private:
	struct FFeather
	{
		enum EGroup : uint8 { Primary, Secondary, Tertial, GreaterCovert, LesserCovert, PrimaryCovert, Alula, Scapular, Tail };
		EGroup Group;
		int8 Side;         // -1 left, +1 right, 0 tail
		int16 Index;       // within its group, inner to outer (tail: left to right)
		int16 Count;
		float Length, Width;
		float Along;       // where on its bone it sits, 0..1
		float Layer;       // stacking offset along the wing's normal, cm
		float Phase;       // flutter phase
		int32 Instance;
	};

	void BuildFeathers(const BirdLook::FLook& Look);
	void BuildBodyFeathers(const BirdLook::FLook& Look);
	void PoseWing(int8 Side, const FBirdBodyPose& Pose, float Time, TArray<FTransform>& Out);
	void PoseTail(const FBirdBodyPose& Pose, float Time, TArray<FTransform>& Out);
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, USceneComponent* Parent, FName Name);

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Flight;   // wings and tail, posed per frame
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Contour;  // body feathers, fixed
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Shell;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Beak;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LegParts;
	UPROPERTY() TObjectPtr<UMaterialInterface> FeatherMaterial;
	UPROPERTY() TObjectPtr<UMaterialInterface> BodyMaterial;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> FeatherMID;

	TArray<FFeather> Feathers;
	BirdLook::EWing Wing = BirdLook::EWing::Round;
	BirdLook::ETail TailShape = BirdLook::ETail::Square;
	float L = 15.f;           // torso length, cm
	float HalfW = 2.5f, HalfH = 2.5f;
	float HalfSpan = 11.f;    // shoulder to wing tip
	float Bone[3] = { 2, 2, 2 };   // humerus, forearm, hand
	float Primary = 5.f, Secondary = 3.f, TailLen = 5.f;
	float StrokeAmp = 0.9f, GlideDihedral = 0.05f;
	FVector Shoulder = FVector::ZeroVector, TailBase = FVector::ZeroVector, Hip = FVector::ZeroVector;
	FVector Eye = FVector::ZeroVector, Neck = FVector::ZeroVector;
	float Stand = 3.f, Radius = 2.f;
	TArray<FVector> LegRest;
	float ViewForward = 0.f;
	FVector ViewLoc = FVector::ZeroVector;
	FRotator ViewRot = FRotator::ZeroRotator;
	float ViewK = 1.f;
	FVector Squeeze(const FVector& Local) const;
};
