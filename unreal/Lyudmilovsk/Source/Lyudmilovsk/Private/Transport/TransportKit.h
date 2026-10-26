// The cable-car kit's shared numbers and loaders for LyudCableway.cpp and LyudStationBuild.cpp.
#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace LyudTransportKit
{
	// sizes in cm, the network's (tools/transport.py) and the kit's (blender/transport/optimize.py)
	inline constexpr float IslandHalf = 345.f;      // the platform's edge: 4 cm from the cabin's sill (its sill 1.007 m off its centre, tracks 4.5 m)
	inline constexpr float HullClear = 313.f;       // nothing over the deck farther out than this (the cabins' hulls overhang the edge)
	inline constexpr float IslandLen = 1350.f;      // from the station's centre to each end
	inline constexpr float DeckThick = 40.f;
	inline constexpr float ShaftHalf = 125.f;       // the lift shaft's glass (centre line)
	inline constexpr float LaneIn = 128.f;          // the stair's lane round it, from here
	inline constexpr float LaneOut = 246.f;         // to the tower's outer wall's inner face
	inline constexpr float TowerHalf = 250.f;       // the outer wall's centre line
	inline constexpr float TowerOut = 254.f;        // its outer face
	inline constexpr float MaxRiser = 18.f;
	inline constexpr float MaxFlightRise = 180.f;   // one flight along a side of the tower
	inline constexpr float LevelLiftMin = 450.f;    // no lift under 4.5 m
	inline constexpr float CanopyLow = 335.f, CanopyHigh = 395.f;   // the butterfly roof over the platform: at the middle, at the edges
	inline constexpr float GalleryHalf = 150.f;     // the ground gallery's clear half width (the connector's 3.0 m)
	inline constexpr float GalleryH = 300.f;        // its clear height; its roof's top at 340
	inline constexpr float HeadOver = 520.f;        // the tower's head over the top deck

	/** The cabin as a room (blender/transport/cabin.py, kit.json "cabin"), in the actor's frame (cm, origin on the floor). */
	struct FCabinKit
	{
		bool bRoom = false;                     // the kit has the hollow cabin (its inside, door leaf, hanger)
		float DoorX = 0.f, DoorW = 110.f, DoorH = 212.f, Slide = -114.f, Out = 8.f, SillY = 104.f;
		float WallY = 85.f, EndX = 155.f;
		FVector Lamp = FVector(0, 0, 195.f), Board = FVector(0, -80.f, 224.f), Plate = FVector(150.f, -119.f, 55.f);
		TArray<FTransform> Seats;
		TArray<TPair<FVector, FVector>> Collision;   // centre, full size
		struct FPoster { FVector At, Normal, Up; float W = 56.f; };
		TArray<FPoster> Posters;                     // the route diagram on the inside wall
	};

	/** Blender metres (x, y, z) -> the actor's centimetres (x, -y, z). */
	inline FVector FromBlender(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		return A.Num() >= 3 ? FVector(A[0]->AsNumber(), -A[1]->AsNumber(), A[2]->AsNumber()) * 100.f : FVector::ZeroVector;
	}

	struct FKit
	{
		float CabinGrip = 386.3f, CabinFloor = 22.f;
		FCabinKit Cabin;
		float PylonFeet = 398.5f, PylonShaft = 2207.3f, PylonHeadSaddle = 86.9f;
		float CornerSaddle = 0.8f;
		bool bLoaded = false;
	};

	inline FKit& Kit()
	{
		static FKit K;
		if (!K.bLoaded)
		{
			K.bLoaded = true;
			FString Text;
			if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Transport/kit.json"))))
			{
				TSharedPtr<FJsonObject> J;
				if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J.IsValid())
				{
					if (const TSharedPtr<FJsonObject>* C; J->TryGetObjectField(TEXT("cabin"), C))
					{
						K.CabinGrip = (*C)->GetNumberField(TEXT("grip")) * 100.f;
						K.CabinFloor = (*C)->GetNumberField(TEXT("floor")) * 100.f;
						const TSharedPtr<FJsonObject>* Door;
						const TSharedPtr<FJsonObject>* Sockets;
						const TSharedPtr<FJsonObject>* In;
						if ((*C)->TryGetObjectField(TEXT("door"), Door) && (*C)->TryGetObjectField(TEXT("sockets"), Sockets)
							&& (*C)->TryGetObjectField(TEXT("inside"), In))
						{
							FCabinKit& R = K.Cabin;
							R.bRoom = true;
							R.DoorX = (*Door)->GetNumberField(TEXT("x")) * 100.f;
							R.DoorW = (*Door)->GetNumberField(TEXT("clear_w")) * 100.f;
							R.DoorH = (*Door)->GetNumberField(TEXT("clear_h")) * 100.f;
							R.Slide = (*Door)->GetNumberField(TEXT("slide")) * 100.f;
							R.Out = (*Door)->GetNumberField(TEXT("out")) * 100.f;
							R.SillY = (*Door)->GetNumberField(TEXT("sill_y")) * 100.f;
							R.WallY = (*In)->GetNumberField(TEXT("wall_y")) * 100.f;
							R.EndX = (*In)->GetNumberField(TEXT("end_x")) * 100.f;
							R.Lamp = FromBlender((*Sockets)->GetArrayField(TEXT("lamp")));
							R.Board = FromBlender((*Sockets)->GetArrayField(TEXT("board")));
							R.Plate = FromBlender((*Sockets)->GetArrayField(TEXT("plate")));
							for (const TSharedPtr<FJsonValue>& V : (*Sockets)->GetArrayField(TEXT("seats")))
							{
								const TSharedPtr<FJsonObject> S = V->AsObject();
								R.Seats.Add(FTransform(FRotator(0.f, S->GetNumberField(TEXT("yaw")), 0.f), FromBlender(S->GetArrayField(TEXT("at")))));
							}
							const TArray<TSharedPtr<FJsonValue>>* Posters;
							if ((*Sockets)->TryGetArrayField(TEXT("posters"), Posters))
							{
								for (const TSharedPtr<FJsonValue>& V : *Posters)
								{
									const TSharedPtr<FJsonObject> P = V->AsObject();
									FCabinKit::FPoster& O = R.Posters.AddDefaulted_GetRef();
									O.At = FromBlender(P->GetArrayField(TEXT("at")));
									O.Normal = FromBlender(P->GetArrayField(TEXT("normal"))).GetSafeNormal();
									O.Up = FromBlender(P->GetArrayField(TEXT("up"))).GetSafeNormal();
									O.W = P->GetNumberField(TEXT("w")) * 100.f;
								}
							}
							for (const TSharedPtr<FJsonValue>& V : (*C)->GetArrayField(TEXT("collision")))
							{
								const TSharedPtr<FJsonObject> B = V->AsObject();
								FVector Size = FromBlender(B->GetArrayField(TEXT("s")));
								Size.Y = FMath::Abs(Size.Y);
								R.Collision.Add({ FromBlender(B->GetArrayField(TEXT("c"))), Size });
							}
						}
					}
					if (const TSharedPtr<FJsonObject>* P; J->TryGetObjectField(TEXT("pylon_line"), P))
					{
						K.PylonFeet = (*P)->GetNumberField(TEXT("feet")) * 100.f;
						K.PylonShaft = (*P)->GetNumberField(TEXT("shaft")) * 100.f;
						K.PylonHeadSaddle = (*P)->GetNumberField(TEXT("saddle_over_head_base")) * 100.f;
					}
					if (const TSharedPtr<FJsonObject>* P; J->TryGetObjectField(TEXT("pylon_corner"), P))
					{
						K.CornerSaddle = (*P)->GetNumberField(TEXT("saddle"));
					}
				}
			}
		}
		return K;
	}

	inline UStaticMesh* LoadMesh(const FString& Name)
	{
		// Interchange names the asset after the glTF mesh; some versions prefix it again
		for (const FString& N : { Name, TEXT("SM_") + Name })
		{
			const FString Path = FString::Printf(TEXT("/Game/Transport/Meshes/transport_kit/StaticMeshes/%s.%s"), *N, *N);
			if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return M;
			}
		}
		return nullptr;
	}

	inline UStaticMesh* BasicShape(FName Kind)
	{
		const TCHAR* Path = Kind == TEXT("cyl") ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder") : TEXT("/Engine/BasicShapes/Cube.Cube");
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	struct FPaint { FLinearColor Color; float Rough; float Metal; };
	inline const TMap<FName, FPaint>& Palette()
	{
		static const TMap<FName, FPaint> P = {
			{ TEXT("ivory"), { FLinearColor(0.80f, 0.76f, 0.66f), 0.45f, 0.f } },
			{ TEXT("amber"), { FLinearColor(0.86f, 0.42f, 0.08f), 0.35f, 0.f } },
			{ TEXT("concrete"), { FLinearColor(0.50f, 0.48f, 0.45f), 0.9f, 0.f } },
			{ TEXT("terrazzo"), { FLinearColor(0.60f, 0.56f, 0.50f), 0.45f, 0.f } },
			{ TEXT("steel"), { FLinearColor(0.32f, 0.33f, 0.35f), 0.4f, 0.8f } },
			{ TEXT("chrome"), { FLinearColor(0.80f, 0.80f, 0.82f), 0.15f, 1.f } },
			{ TEXT("rope"), { FLinearColor(0.10f, 0.10f, 0.11f), 0.5f, 0.7f } },
			{ TEXT("dark"), { FLinearColor(0.05f, 0.045f, 0.04f), 0.7f, 0.f } },
			{ TEXT("seat"), { FLinearColor(0.62f, 0.26f, 0.06f), 0.5f, 0.f } },
			{ TEXT("mosaic"), { FLinearColor(0.15f, 0.36f, 0.42f), 0.5f, 0.f } },
			{ TEXT("plaster"), { FLinearColor(0.74f, 0.70f, 0.62f), 0.85f, 0.f } },
		};
		return P;
	}
}

