#include "Birds/BirdSpecies.h"

#include "Animation/AnimSequence.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	TArray<FBirdSpecies> GSpecies;
	bool GLoaded = false;

	FLinearColor FromHex(const FString& Hex)
	{
		return FLinearColor(FColor::FromHex(Hex));
	}

	template <typename E>
	E EnumFrom(const FString& Name, E Fallback)
	{
		const UEnum* En = StaticEnum<E>();
		const int64 V = En->GetValueByNameString(Name);
		return V == INDEX_NONE ? Fallback : (E)V;
	}

	void Load()
	{
		GLoaded = true;
		GSpecies.Reset();
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("BirdsData/species.json");
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogTemp, Warning, TEXT("Birds: no %s"), *Path);
			return;
		}
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Birds: %s is not valid JSON"), *Path);
			return;
		}
		for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("species")))
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid())
			{
				continue;
			}
			FBirdSpecies S;
			S.Id = FName(O->GetStringField(TEXT("id")));
			S.Key = O->GetStringField(TEXT("key"));
			O->TryGetStringField(TEXT("latin"), S.Latin);
			S.LengthCm = O->GetNumberField(TEXT("lengthCm"));
			S.WingspanCm = O->GetNumberField(TEXT("wingspanCm"));
			S.MassG = O->GetNumberField(TEXT("massG"));
			S.Flight = EnumFrom(O->GetStringField(TEXT("flight")), EBirdFlightStyle::Flapping);
			S.WingbeatHz = O->GetNumberField(TEXT("wingbeatHz"));
			S.CruiseMs = O->GetNumberField(TEXT("cruiseMs"));
			S.Gait = EnumFrom(O->GetStringField(TEXT("gait")), EBirdGait::Hop);
			S.StrideCm = O->GetNumberField(TEXT("strideCm"));
			const TArray<TSharedPtr<FJsonValue>>* Flock;
			if (O->TryGetArrayField(TEXT("flockSize"), Flock) && Flock->Num() == 2)
			{
				S.FlockMin = (int32)(*Flock)[0]->AsNumber();
				S.FlockMax = (int32)(*Flock)[1]->AsNumber();
			}
			O->TryGetNumberField(TEXT("fleeM"), S.FleeM);
			O->TryGetBoolField(TEXT("predator"), S.bPredator);
			for (const TSharedPtr<FJsonValue>& C : O->GetArrayField(TEXT("palette")))
			{
				S.Palette.Add(FromHex(C->AsString()));
			}
			for (const TSharedPtr<FJsonValue>& C : O->GetArrayField(TEXT("calls")))
			{
				S.Calls.Add(FName(C->AsString()));
			}
			const TArray<TSharedPtr<FJsonValue>>* Diet;
			if (O->TryGetArrayField(TEXT("diet"), Diet))
			{
				for (const TSharedPtr<FJsonValue>& C : *Diet)
				{
					S.Diet.Add(FName(C->AsString()));
				}
			}
			auto Ints = [&O](const TCHAR* Field, TArray<int32>& Out)
			{
				const TArray<TSharedPtr<FJsonValue>>* Arr;
				if (O->TryGetArrayField(Field, Arr))
				{
					for (const TSharedPtr<FJsonValue>& C : *Arr)
					{
						Out.Add((int32)C->AsNumber());
					}
				}
			};
			Ints(TEXT("months"), S.Months);
			Ints(TEXT("winterMonths"), S.WinterMonths);
			const TSharedPtr<FJsonObject>* Late;
			if (O->TryGetObjectField(TEXT("stragglers"), Late))
			{
				auto MonthDay = [](const FString& T)
				{
					FString M, D;
					return T.Split(TEXT("-"), &M, &D) ? FCString::Atoi(*M) * 100 + FCString::Atoi(*D) : 0;
				};
				S.Stragglers = (int32)(*Late)->GetNumberField(TEXT("count"));
				S.StragglersFrom = MonthDay((*Late)->GetStringField(TEXT("from")));
				S.StragglersUntil = MonthDay((*Late)->GetStringField(TEXT("until")));
			}
			FString Dimorphism;
			S.bFemalePlumage = O->TryGetStringField(TEXT("dimorphism"), Dimorphism) && Dimorphism == TEXT("plumage");
			O->TryGetNumberField(TEXT("femaleScale"), S.FemaleScale);
			O->TryGetNumberField(TEXT("femaleStreamer"), S.FemaleStreamer);
			const TArray<TSharedPtr<FJsonValue>>* Winter;
			if (O->TryGetArrayField(TEXT("winterPalette"), Winter))
			{
				for (const TSharedPtr<FJsonValue>& C : *Winter)
				{
					S.WinterPalette.Add(FromHex(C->AsString()));
				}
			}
			GSpecies.Add(MoveTemp(S));
		}
	}
}

FText FBirdSpecies::DisplayName() const
{
	return LyudText::Get(FString::Printf(TEXT("bird.%s.name"), *Key));
}

const TArray<FBirdSpecies>& UBirdSpeciesLibrary::All()
{
	if (!GLoaded)
	{
		Load();
	}
	return GSpecies;
}

void UBirdSpeciesLibrary::Reload()
{
	Load();
}

const FBirdSpecies* UBirdSpeciesLibrary::Find(FName Id)
{
	return All().FindByPredicate([Id](const FBirdSpecies& S) { return S.Id == Id; });
}

bool UBirdSpeciesLibrary::GetSpecies(FName Id, FBirdSpecies& Out)
{
	if (const FBirdSpecies* S = Find(Id))
	{
		Out = *S;
		return true;
	}
	return false;
}

TArray<FName> UBirdSpeciesLibrary::AllSpeciesIds()
{
	TArray<FName> Ids;
	for (const FBirdSpecies& S : All())
	{
		Ids.Add(S.Id);
	}
	return Ids;
}

USkeletalMesh* UBirdSpeciesLibrary::LoadSkeletalMesh(FName Id)
{
	const FString N = Id.ToString();
	return LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Birds/%s/SK_%s.SK_%s"), *N, *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

bool UBirdSpeciesLibrary::HasSkeletalMesh(FName Id)
{
	const FString N = Id.ToString();
	return FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/Birds/%s/SK_%s"), *N, *N));
}

bool UBirdSpeciesLibrary::HasPose(FName Id, FName Pose)
{
	const FString N = Id.ToString(), P = Pose.ToString();
	return FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/Birds/%s/SM_%s_%s"), *N, *N, *P));
}

USkeletalMesh* UBirdSpeciesLibrary::LoadHeroMesh(FName Id)
{
	const FString N = Id.ToString();
	return LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Birds/%s/SK_%s_Hero.SK_%s_Hero"), *N, *N, *N), nullptr,
		LOAD_NoWarn | LOAD_Quiet);
}

bool UBirdSpeciesLibrary::HasFirstPersonBody(FName Id)
{
	const USkeletalMesh* Mesh = LoadHeroMesh(Id);
	return Mesh && Mesh->GetMaterials().ContainsByPredicate([](const FSkeletalMaterial& M) { return M.MaterialSlotName.ToString().EndsWith(TEXT("Skull")); });
}

namespace
{
	// blender/birds/hero_pigeon/morphs.json has the same list (the textures are made from it)
	struct FPigeonMorph { const TCHAR* Name; float Share; };
	const FPigeonMorph PigeonMorphs[] = {
		{ TEXT(""), 0.27f },             // blue bar, the wild type
		{ TEXT("Checker"), 0.28f },
		{ TEXT("DarkChecker"), 0.14f },
		{ TEXT("Spread"), 0.08f },       // melanic, near black-slate all over
		{ TEXT("Pied"), 0.10f },
		{ TEXT("AshRed"), 0.05f },
		{ TEXT("Barless"), 0.03f },
		{ TEXT("White"), 0.03f },
		{ TEXT("Brown"), 0.02f },
	};
}

int32 UBirdSpeciesLibrary::PigeonMorphCount()
{
	return UE_ARRAY_COUNT(PigeonMorphs);
}

FString UBirdSpeciesLibrary::PigeonMorphName(int32 Morph)
{
	return Morph >= 0 && Morph < PigeonMorphCount() ? FString(PigeonMorphs[Morph].Name) : FString();
}

int32 UBirdSpeciesLibrary::PickPigeonMorph(int32 Seed)
{
	FRandomStream R(Seed * 7919 + 17);
	float U = R.FRand(), Total = 0.f;
	for (const FPigeonMorph& M : PigeonMorphs)
	{
		Total += M.Share;
	}
	U *= Total;
	for (int32 i = 0; i < PigeonMorphCount(); ++i)
	{
		U -= PigeonMorphs[i].Share;
		if (U <= 0.f)
		{
			return i;
		}
	}
	return 0;
}

UAnimSequence* UBirdSpeciesLibrary::LoadClip(FName Id, FName Clip)
{
	const FString N = Id.ToString(), C = Clip.ToString();
	return LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Birds/%s/Anims/A_%s_%s.A_%s_%s"), *N, *N, *C, *N, *C), nullptr,
		LOAD_NoWarn | LOAD_Quiet);
}

UStaticMesh* UBirdSpeciesLibrary::LoadPose(FName Id, FName Pose)
{
	const FString N = Id.ToString(), P = Pose.ToString();
	return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Birds/%s/SM_%s_%s.SM_%s_%s"), *N, *N, *P, *N, *P), nullptr,
		LOAD_NoWarn | LOAD_Quiet);
}

void UBirdSpeciesLibrary::SetLODScreenSizes(USkeletalMesh* Mesh, const TArray<float>& ScreenSizes)
{
	if (!Mesh)
	{
		return;
	}
	Mesh->Modify();
	for (int32 i = 0; i < Mesh->GetLODNum() && i < ScreenSizes.Num(); ++i)
	{
		if (FSkeletalMeshLODInfo* Info = Mesh->GetLODInfo(i))
		{
			Info->ScreenSize = ScreenSizes[i];
		}
	}
}

FName UBirdSpeciesLibrary::ClipForState(EBirdState State)
{
	switch (State)
	{
	case EBirdState::Perch: return TEXT("Idle");
	case EBirdState::Look: return TEXT("Look");
	case EBirdState::Alert: return TEXT("Alert");
	case EBirdState::Hop: return TEXT("Hop");
	case EBirdState::Walk: return TEXT("Walk");
	case EBirdState::Peck: return TEXT("Peck");
	case EBirdState::Eat: return TEXT("Eat");
	case EBirdState::Preen: return TEXT("Preen");
	case EBirdState::Call: return TEXT("Call");
	case EBirdState::Takeoff: return TEXT("Takeoff");
	case EBirdState::Fly: return TEXT("Flap");
	case EBirdState::Glide: return TEXT("Glide");
	case EBirdState::Land: return TEXT("Land");
	case EBirdState::Sleep: return TEXT("Sleep");
	}
	return TEXT("Idle");
}
