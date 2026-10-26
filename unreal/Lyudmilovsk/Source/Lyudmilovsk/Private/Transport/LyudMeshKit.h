// A small builder of the cable-car's procedural architecture: boxes, prisms, slabs with a hole, rods, lathes, walls with
// openings, gathered per material into one runtime UStaticMesh with metre UVs (the textures tile at their real size,
// M_TR_Surface) and simple collision (boxes and convex pieces, never the render triangles).
#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/AggregateGeom.h"

class UStaticMesh;

class FLyudMeshKit
{
public:
	/** A quad A B C D, counterclockwise seen from its front. UVs: metres on the face (walls: V down the height). */
	void Quad(FName Mat, const FVector& A, const FVector& B, const FVector& C, const FVector& D);
	void Tri(FName Mat, const FVector& A, const FVector& B, const FVector& C);

	/** A box (centre and rotation in T, half sizes H); Top: another material for the +Z face; collision box. */
	void Box(FName Mat, const FTransform& T, const FVector& H, bool bCollide = true, FName Top = NAME_None);
	/** The same with the centre and the size in frame F. */
	void BoxIn(FName Mat, const FTransform& F, const FVector& Centre, const FVector& Size, bool bCollide = true, FName Top = NAME_None,
		const FRotator& R = FRotator::ZeroRotator);

	/** A convex polygon (counterclockwise in F's XY) extruded from Z0 to Z1 in F; convex collision. */
	void Prism(FName Mat, const FTransform& F, const TArray<FVector2D>& Poly, float Z0, float Z1, bool bCollide = true, FName Top = NAME_None);

	/** A slab: the rectangle X0..X1, Y0..Y1 in F minus a convex hole (polygon in F's XY), Z0..Z1. */
	void SlabWithHole(FName Mat, const FTransform& F, float X0, float X1, float Y0, float Y1, const TArray<FVector2D>& Hole,
		float Z0, float Z1, bool bCollide = true, FName Top = NAME_None);

	/** A rod from A to B (world of the kit), radius R, N sides; optional end caps. */
	void Rod(FName Mat, const FVector& A, const FVector& B, float R, int32 N = 10, bool bCaps = true, bool bCollide = false);

	/** A surface of revolution round F's Z: profile points (radius, z) from the bottom up, each segment its material. */
	void Lathe(const FTransform& F, const TArray<FVector2D>& Profile, const TArray<FName>& SegMats, int32 N, bool bCapTop, bool bCollide);

	/**
	 * A wall in frame F: along X from 0 to Len, thickness T (centred on Y = 0), from Z0 to Z1, with rectangular openings
	 * (U0, U1, Bottom, Top along X/Z). Builds the solid pieces round the openings, their reveals, collision per piece.
	 */
	struct FOpening { float U0, U1, Z0, Z1; };
	void Wall(FName Mat, const FTransform& F, float Len, float T, float Z0, float Z1, const TArray<FOpening>& Openings, bool bCollide = true);

	/** Collision only. */
	void CollideBox(const FTransform& T, const FVector& H);
	void CollideConvex(const TArray<FVector>& Pts);

	bool IsEmpty() const { return Sections.Num() == 0; }
	int32 NumTriangles() const;

	/** The mesh (transient, never saved) with a material slot per used material name, in OutSlots' order. */
	UStaticMesh* Build(UObject* Outer, FName Name, TArray<FName>& OutSlots) const;

private:
	struct FSection
	{
		TArray<FVector3f> Pos, Nrm, Tan;
		TArray<FVector2f> UV;
		TArray<int32> Idx;
	};
	TMap<FName, FSection> Sections;
	FKAggregateGeom Geom;

	FSection& Sec(FName Mat) { return Sections.FindOrAdd(Mat); }
	static FVector2f PlanarUV(const FVector& P, const FVector& N, FVector& OutTangent);
};
