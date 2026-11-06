#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class FJsonObject;

/**
 * The public spaces' kit (docs/city/public_spaces.md): every bench, lamp, railing, stall and swing is built here in
 * code, once, as a small static mesh with its colours in the vertex colour (one material, /Game/City/M_CityProp, for
 * all of them). Lengths in metres in the builders, centimetres in the mesh. Front = +X, along = +Y, up = +Z; a prop's
 * origin is on the ground.
 *
 * Vertex data the material reads: colour RGB (linear) and A = roughness; UV0 = metres on the surface (unused by the
 * kit's own faces); UV1.x = pattern (0 none, 1 paving slabs, 2 setts, 3 boards, 4 masonry courses, 5 gravel,
 * 6 rubber), UV1.y = slab size m; UV2.x = glow (lamp glass: lit at night by the material's Glow parameter).
 */
namespace CityKit
{
	enum class EPattern : uint8 { None = 0, Slabs = 1, Setts = 2, Boards = 3, Masonry = 4, Gravel = 5, Rubber = 6 };

	struct FSkin
	{
		FLinearColor Color = FLinearColor(0.5f, 0.5f, 0.5f);
		float Rough = 0.7f;
		EPattern Pattern = EPattern::None;
		float Tile = 0.f;
		float Glow = 0.f;
	};

	struct FMesh
	{
		TArray<FVector3f> Pos, Nrm;
		TArray<FVector4f> Col;
		TArray<FVector2f> UV0, UV1, UV2;
		TArray<int32> Idx;
		/** Collision: boxes (centre, half size, yaw deg; metres) for the player; none = the mesh's bounds as one box. */
		struct FCollider { FVector C, Half; float Yaw = 0.f; };
		TArray<FCollider> Colliders;
		void Collide(const FVector& C, const FVector& Half, float Yaw = 0.f) { Colliders.Add({ C, Half, Yaw }); }

		int32 Vert(const FVector& P, const FVector& N, const FSkin& S, const FVector2f& UV = FVector2f::ZeroVector);
		/** A triangle facing along Out (the winding is fixed to face it). */
		void Tri(int32 A, int32 B, int32 C, const FVector& Out);
		/** A flat quad, corners in order round it, facing N; metres. */
		void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FSkin& S);
		/** The same with given UV0 (metres) at the corners. */
		void QuadUV(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector2f& Ua, const FVector2f& Ub,
			const FVector2f& Uc, const FVector2f& Ud, const FSkin& S);
		/** A box: centre, half sizes (m), turned by Rot; all six faces (Bottom false: no bottom face). */
		void Box(const FVector& C, const FVector& Half, const FSkin& S, const FRotator& Rot = FRotator::ZeroRotator, bool bBottom = false);
		/** A box between two points (a bar, a leg, a slat): its square section Size x Size2 (m); Up picks the section's turn. */
		void Bar(const FVector& A, const FVector& B, float Size, float Size2, const FSkin& S, const FVector& Up = FVector::UpVector);
		/** A cylinder (open ends unless capped), axis A -> B, radius R0 at A and R1 at B. */
		void Tube(const FVector& A, const FVector& B, float R0, float R1, int32 Sides, const FSkin& S, bool bCapA = false, bool bCapB = true);
		/** A surface of revolution round Z through Centre: Profile (r, z) bottom to top, the outward side drawn; Skin per span. */
		void Lathe(const FVector& Centre, const TArray<FVector2D>& Profile, int32 Sides, TFunctionRef<FSkin(int32 Span, int32 Side)> Skin,
			float A0 = 0.f, float A1 = 2.f * PI);
		void Sphere(const FVector& C, float R, int32 Sides, const FSkin& S);
		/** A disc (ring when R0 > 0) at height Z facing up. */
		void Disc(const FVector& C, float R0, float R1, int32 Sides, const FSkin& S, float Ry = -1.f);

		UStaticMesh* Build(UObject* Outer, FName Name, bool bCollision) const;
	};

	/** A colour from 0..255 sRGB, as the designs are written. */
	FLinearColor Srgb(uint8 R, uint8 G, uint8 B);

	/** The skins of the town (painted iron, timber, granite, concrete...). */
	FSkin Iron();
	FSkin Timber();
	FSkin Granite();
	FSkin Concrete();
	FSkin Paint(uint8 R, uint8 G, uint8 B, float Rough = 0.55f);

	/** The kit meshes by kind (see public_spaces.json props): Variant picks colours; Param a size (flowerbed radius...). */
	void BuildProp(FMesh& M, const FString& Kind, int32 Variant, const TSharedPtr<FJsonObject>& Extra);
	/** Whether a kind blocks the player (benches, lamps, railings yes; umbrellas, loungers, towels no). */
	bool PropCollides(const FString& Kind);
	/** Whether a kind carries glowing glass (its component gets the lamps' MID). */
	bool PropGlows(const FString& Kind);
	/** A unit cube (1 m), for the blocks' collision (instanced, hidden). */
	void BuildBlock(FMesh& M, const FString& Skin);
	/** A block of the boxes list drawn at its true size into a merged mesh: centre (m, relative to the mesh's origin), yaw,
	 *  full size (along, across, up); the top's UV0 runs in metres from the block's corner (slab joints meet its edges), the
	 *  sides' along the face and in world height (masonry courses line up between blocks). WorldZ0 = the centre's world z, m. */
	void AddBlock(FMesh& M, const FVector& C, float Yaw, const FVector& Size, const FString& Skin, float WorldZ0);
	/** The square fountain's stone from its profile (fountain.json-like "stone" levels, metres). */
	void BuildFountainStone(FMesh& M, const TSharedPtr<FJsonObject>& Stone);
}
