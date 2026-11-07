#include "FishingPlaces.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace LyudFishPlaces
{
	namespace
	{
		TSharedPtr<FJsonObject> PlacesJson(const TCHAR* Relative)
		{
			FString Text;
			TSharedPtr<FJsonObject> Root;
			if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / Relative)))
			{
				FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root);
			}
			return Root;
		}

		FVector PlacesVec(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
		{
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (!O.IsValid() || !O->TryGetArrayField(Field, A) || A->Num() < 2)
			{
				return FVector::ZeroVector;
			}
			return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), A->Num() > 2 ? (*A)[2]->AsNumber() : 0.0);
		}

		/** Plot and spot ids of the city plan -> their "ue" point. */
		TMap<FString, FVector> PlanPoints()
		{
			TMap<FString, FVector> Out;
			const TSharedPtr<FJsonObject> Plan = PlacesJson(TEXT("City/city_plan.json"));
			for (const TCHAR* List : { TEXT("plots"), TEXT("spots") })
			{
				const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
				if (!Plan.IsValid() || !Plan->TryGetArrayField(List, Rows)) continue;
				for (const TSharedPtr<FJsonValue>& V : *Rows)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					FString Id;
					if (O.IsValid() && O->TryGetStringField(TEXT("id"), Id) && O->HasField(TEXT("ue")))
					{
						Out.Add(Id, PlacesVec(O, TEXT("ue")));
					}
				}
			}
			return Out;
		}

		/** The object's place: its plan id's point in the city plan when there is one, else its own "ue". */
		FVector PlacesBase(const TSharedPtr<FJsonObject>& O, const TMap<FString, FVector>& Plan)
		{
			FString Id;
			if (O->TryGetStringField(TEXT("plan"), Id))
			{
				if (const FVector* P = Plan.Find(Id)) return *P;
			}
			return PlacesVec(O, TEXT("ue"));
		}

		FPlaces Load()
		{
			FPlaces P;
			const TSharedPtr<FJsonObject> Root = PlacesJson(TEXT("Fishing/fishing.json"));
			if (!Root.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("FISHING no Content/Fishing/fishing.json: no spots, boat or kitchens"));
				return P;
			}
			const TMap<FString, FVector> Plan = PlanPoints();
			const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
			if (Root->TryGetArrayField(TEXT("spots"), Rows))
			{
				for (const TSharedPtr<FJsonValue>& V : *Rows)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					if (!O.IsValid()) continue;
					const FVector Base = PlacesBase(O, Plan);
					FSpot S;
					S.Id = FName(*O->GetStringField(TEXT("id")));
					S.A = FVector2D(Base + PlacesVec(O, TEXT("a")));
					S.B = FVector2D(Base + PlacesVec(O, TEXT("b")));
					S.Radius = float(O->GetNumberField(TEXT("radius")));
					P.Spots.Add(S);
				}
			}
			const TSharedPtr<FJsonObject>* Boat = nullptr;
			if (Root->TryGetObjectField(TEXT("boat"), Boat))
			{
				const FVector Base = PlacesBase(*Boat, Plan);
				P.Boat.bValid = true;
				P.Boat.Mooring = FVector2D(Base + PlacesVec(*Boat, TEXT("mooring")));
				P.Boat.Landing = FVector2D(Base + PlacesVec(*Boat, TEXT("landing")));
				P.Boat.Yaw = float((*Boat)->GetNumberField(TEXT("yaw")));
				P.Boat.Fee = int32((*Boat)->GetNumberField(TEXT("fee")));
				P.Boat.Range = float((*Boat)->GetNumberField(TEXT("range")));
				P.Boat.MinDepth = float((*Boat)->GetNumberField(TEXT("min_depth")));
			}
			if (Root->TryGetArrayField(TEXT("kitchens"), Rows))
			{
				for (const TSharedPtr<FJsonValue>& V : *Rows)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					if (!O.IsValid()) continue;
					FKitchen K;
					K.Id = FName(*O->GetStringField(TEXT("id")));
					K.At = PlacesBase(O, Plan) + PlacesVec(O, TEXT("at"));
					K.Radius = float(O->GetNumberField(TEXT("radius")));
					K.Open = int32(O->GetNumberField(TEXT("open")));
					K.Close = int32(O->GetNumberField(TEXT("close")));
					K.PerDay = int32(O->GetNumberField(TEXT("per_day")));
					P.Kitchens.Add(K);
				}
			}
			UE_LOG(LogTemp, Display, TEXT("FISHING places: %d spots, boat %d, %d kitchens"), P.Spots.Num(), P.Boat.bValid ? 1 : 0, P.Kitchens.Num());
			return P;
		}
	}

	const FPlaces& Get()
	{
		static const FPlaces Places = Load();
		return Places;
	}

	FName SpotAt(const FVector& Where)
	{
		const FVector2D P(Where);
		for (const FSpot& S : Get().Spots)
		{
			const FVector2D AB = S.B - S.A;
			const float T = AB.SizeSquared() > 1.f ? FMath::Clamp(FVector2D::DotProduct(P - S.A, AB) / AB.SizeSquared(), 0.f, 1.f) : 0.f;
			if (FVector2D::Distance(P, S.A + AB * T) <= S.Radius) return S.Id;
		}
		return NAME_None;
	}
}
