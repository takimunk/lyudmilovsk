#include "Restaurant/LyudRestaurant.h"

#include "LyudRestaurantStaff.h"
#include "SLyudRestMenu.h"

#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Doors/LyudDoor.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "LyudGameMenu.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Food/LyudBite.h"
#include "Food/LyudHandUse.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroAnimInstance.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "People/PeopleLife.h"
#include "Plants/GameCalendar.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sky/LyudSky.h"
#include "Sound/SoundBase.h"
#include "UI/LyudGameScreen.h"

namespace LyudRest
{
	void ApplyShown(IConsoleVariable* Var);

	TAutoConsoleVariable<int32> CVarRestaurant(TEXT("lyud.Restaurant"), 1,
		TEXT("The restaurant «Янтарь»: shown (1) or hidden with no collision (0): the benchmark's A/B."),
		FConsoleVariableDelegate::CreateStatic(&ApplyShown), ECVF_Default);
	TAutoConsoleVariable<int32> CVarFestival(TEXT("lyud.Restaurant.Festival"), -1,
		TEXT("-1 the holidays' own calendar, 0 an ordinary day, 1 a festival day (the festival menu)."), ECVF_Default);

	void ApplyShown(IConsoleVariable* Var)
	{
		const bool bOn = Var->GetInt() != 0;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->ActorHasTag(TEXT("Restaurant")) || It->IsA<ALyudRestDish>())
				{
					It->SetActorHiddenInGame(!bOn);
					It->SetActorEnableCollision(bOn);
				}
			}
		}
	}

	AHeroCharacter* Hero(const UWorld* World)
	{
		return Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	}

	APlayerController* Controller(const UWorld* World)
	{
		AHeroCharacter* H = Hero(World);
		return H ? Cast<APlayerController>(H->GetController()) : nullptr;
	}

	TSharedPtr<FJsonObject> ReadJson(const FString& Relative)
	{
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / Relative)))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root);
		}
		return Root;
	}

	FVector Vec(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(Field, A) || A->Num() < 2)
		{
			return FVector::ZeroVector;
		}
		return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), A->Num() > 2 && (*A)[2]->Type == EJson::Number ? (*A)[2]->AsNumber() : 0.0);
	}

	FLinearColor Hex(const FString& H)
	{
		return H.IsEmpty() ? FLinearColor::Transparent : FLinearColor(FColor::FromHex(H));
	}

	double Number(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, double Default)
	{
		double V = Default;
		return O.IsValid() && O->TryGetNumberField(Field, V) ? V : Default;
	}

	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		FString V;
		return O.IsValid() && O->TryGetStringField(Field, V) ? V : FString();
	}

	/** restaurant.json (read once). */
	const TSharedPtr<FJsonObject>& Config()
	{
		static TSharedPtr<FJsonObject> C = ReadJson(TEXT("Restaurant/restaurant.json"));
		return C;
	}

	/** The vessel's own size from its mesh (cm): radius and half height. */
	void MeshSize(const UStaticMesh* M, float& Radius, float& HalfHeight)
	{
		if (!M)
		{
			return;
		}
		const FBox B = M->GetBoundingBox();
		Radius = 0.5f * (float)FMath::Max(B.GetSize().X, B.GetSize().Y);
		HalfHeight = 0.5f * (float)B.GetSize().Z;
	}
}

namespace LyudRest
{
	/** RestMesh's lookups (weak: the restaurant's MeshRefs hold them) and the names found missing (cleared at each play's preload). */
	TMap<FName, TWeakObjectPtr<UStaticMesh>> MeshCache;
	TSet<FName> MeshMissing;
}

// ---- the menu --------------------------------------------------------------------------------------------------------------------

const TArray<FLyudRestDishDef>& ALyudRestaurant::Menu()
{
	static TArray<FLyudRestDishDef> Defs;
	static bool bRead = false;
	if (bRead)
	{
		return Defs;
	}
	bRead = true;
	const TSharedPtr<FJsonObject> M = LyudRest::ReadJson(TEXT("Restaurant/menu.json"));
	const TSharedPtr<FJsonObject> T = LyudRest::ReadJson(TEXT("Restaurant/tableware.json"));
	const TSharedPtr<FJsonObject>* TDishes = nullptr;
	if (T.IsValid())
	{
		T->TryGetObjectField(TEXT("dishes"), TDishes);
	}
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!M.IsValid() || !M->TryGetArrayField(TEXT("dishes"), Rows))
	{
		UE_LOG(LogTemp, Warning, TEXT("Restaurant: Content/Restaurant/menu.json unreadable"));
		return Defs;
	}
	for (const TSharedPtr<FJsonValue>& V : *Rows)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FLyudRestDishDef D;
		D.Id = FName(*LyudRest::Str(O, TEXT("id")));
		D.Dish = FName(*LyudRest::Str(O, TEXT("dish")));
		D.Section = FName(*LyudRest::Str(O, TEXT("section")));
		D.Price = (int32)LyudRest::Number(O, TEXT("price"), 0);
		D.Cook = (float)LyudRest::Number(O, TEXT("cook"), 60);
		const FVector B = LyudRest::Vec(O, TEXT("bites"));
		if (B.X > 0.f)
		{
			D.SmallBite = B.X;
			D.BigBite = B.Y;
		}
		O->TryGetBoolField(TEXT("free"), D.bFree);
		O->TryGetBoolField(TEXT("festival"), D.bFestival);
		D.Special = (int32)LyudRest::Number(O, TEXT("special"), -1);
		const TSharedPtr<FJsonObject>* TD = nullptr;
		if (TDishes && (*TDishes)->TryGetObjectField(D.Dish.ToString(), TD))
		{
			D.Vessel = FName(*LyudRest::Str(*TD, TEXT("vessel")));
			D.FoodZ = 100.f * (float)LyudRest::Number(*TD, TEXT("fill_z"), 0.014);
			D.Fill = FName(*LyudRest::Str(*TD, TEXT("fill")));
			const FString U = LyudRest::Str(*TD, TEXT("utensil"));
			if (!U.IsEmpty())
			{
				D.Utensil = FName(*U);
			}
			D.LoadS = FName(*LyudRest::Str(*TD, TEXT("load_s")));
			D.LoadB = FName(*LyudRest::Str(*TD, TEXT("load_b")));
			D.Stages = FMath::Max(0, (int32)LyudRest::Number(*TD, TEXT("stages"), 5));
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if ((*TD)->TryGetArrayField(TEXT("fill_xy"), A))
			{
				for (const TSharedPtr<FJsonValue>& XYV : *A)
				{
					const TArray<TSharedPtr<FJsonValue>>& P = XYV->AsArray();
					if (P.Num() == 2)
					{
						D.FillXY.Add(FVector2D(P[0]->AsNumber(), P[1]->AsNumber()));
					}
				}
			}
			if ((*TD)->TryGetArrayField(TEXT("stage_fill"), A))
			{
				for (const TSharedPtr<FJsonValue>& SFV : *A)
				{
					D.StageFill.Add((float)SFV->AsNumber());
				}
			}
			D.CupRadius = 100.f * (float)LyudRest::Number(*TD, TEXT("radius"), 0.0);
			D.CupHalfHeight = 100.f * (float)LyudRest::Number(*TD, TEXT("half_height"), 0.0);
			D.Saucer = FName(*LyudRest::Str(*TD, TEXT("saucer")));
			D.SaucerZ = 100.f * (float)LyudRest::Number(*TD, TEXT("saucer_z"), 0.0);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Restaurant: %s has no tableware entry %s"), *D.Id.ToString(), *D.Dish.ToString());
		}
		Defs.Add(D);
	}
	return Defs;
}

const FLyudRestDishDef* ALyudRestaurant::FindDish(FName Id)
{
	return Menu().FindByPredicate([Id](const FLyudRestDishDef& D) { return D.Id == Id; });
}

UStaticMesh* ALyudRestaurant::RestMesh(FName Name)
{
	if (Name.IsNone())
	{
		return nullptr;
	}
	// the restaurant preloads them all (PreloadMeshes) and holds them; this finds the loaded one, else loads it once (a cache miss)
	TMap<FName, TWeakObjectPtr<UStaticMesh>>& Cache = LyudRest::MeshCache;
	TSet<FName>& Missing = LyudRest::MeshMissing;
	if (const TWeakObjectPtr<UStaticMesh>* W = Cache.Find(Name); W && W->IsValid())
	{
		return W->Get();
	}
	if (Missing.Contains(Name))
	{
		return nullptr;
	}
	const FString N = Name.ToString();
	const FString AssetPath = TEXT("/Game/Restaurant/Meshes/") + N + TEXT(".") + N;
	UStaticMesh* M = FindObject<UStaticMesh>(nullptr, *AssetPath);
	if (!M && FPackageName::DoesPackageExist(TEXT("/Game/Restaurant/Meshes/") + N))
	{
		M = LoadObject<UStaticMesh>(nullptr, *AssetPath);
	}
	if (M)
	{
		Cache.Add(Name, M);
	}
	else
	{
		Missing.Add(Name);
	}
	return M;
}

void ALyudRestaurant::PreloadMeshes()
{
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	// an uncooked -game run hasn't scanned /Game at startup (only the editor does)
	if (!GIsEditor && !FPlatformProperties::RequiresCookedData())
	{
		AR.ScanPathsSynchronous({ TEXT("/Game/Restaurant/Meshes") }, false);
	}
	LyudRest::MeshMissing.Reset();
	TArray<FAssetData> Assets;
	AR.GetAssetsByPath(TEXT("/Game/Restaurant/Meshes"), Assets, true);
	TArray<FSoftObjectPath> Paths;
	for (const FAssetData& A : Assets)
	{
		Paths.Add(A.GetSoftObjectPath());
	}
	if (Paths.Num() == 0)
	{
		return;
	}
	TWeakObjectPtr<ALyudRestaurant> Self(this);
	Preload = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, [Self, Paths]()
	{
		if (ALyudRestaurant* R = Self.Get())
		{
			for (const FSoftObjectPath& P : Paths)
			{
				if (UObject* O = P.ResolveObject())
				{
					R->MeshRefs.Add(O);
				}
			}
			UE_LOG(LogTemp, Display, TEXT("RESTTEST preloaded %d meshes"), R->MeshRefs.Num());
		}
	});
}

bool ALyudRestaurant::IsOnMenuToday(const UObject* WorldContext, const FLyudRestDishDef& D)
{
	if (D.bFree || (D.bFestival && !IsFestival(WorldContext)))
	{
		return false;
	}
	if (D.Special >= 0)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
		// FDateTime's day of week: Monday 0 .. Sunday 6
		return World && (int32)UGameCalendar::NowIn(World).GetDayOfWeek() == D.Special;
	}
	return true;
}

namespace LyudRest
{
	/** The holidays thread's ULyudFestivalSubsystem when it is in the game (looked up by name: no link to its code). */
	UObject* FestivalSubsystem(const UObject* WorldContext)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
		static TWeakObjectPtr<UClass> Class;
		static bool bLooked = false;
		if (!bLooked)
		{
			bLooked = true;
			Class = FindFirstObject<UClass>(TEXT("LyudFestivalSubsystem"), EFindFirstObjectOptions::None);
		}
		if (!World || !Class.IsValid())
		{
			return nullptr;
		}
		if (Class->IsChildOf(UWorldSubsystem::StaticClass()))
		{
			return World->GetSubsystemBase(TSubclassOf<UWorldSubsystem>(Class.Get()));
		}
		if (Class->IsChildOf(UGameInstanceSubsystem::StaticClass()) && World->GetGameInstance())
		{
			return World->GetGameInstance()->GetSubsystemBase(TSubclassOf<UGameInstanceSubsystem>(Class.Get()));
		}
		return nullptr;
	}

	/** Calls a bool UFUNCTION of the festival subsystem with no argument or one FName. */
	bool FestivalBool(const UObject* WorldContext, FName Function, const FName* Arg)
	{
		UObject* Sub = FestivalSubsystem(WorldContext);
		UFunction* F = Sub ? Sub->FindFunction(Function) : nullptr;
		if (!F || F->NumParms != (Arg ? 2 : 1))
		{
			return false;
		}
		uint8* Parms = (uint8*)FMemory_Alloca(FMath::Max<int32>(F->ParmsSize, 1));
		FMemory::Memzero(Parms, F->ParmsSize);
		FBoolProperty* Ret = nullptr;
		for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Parms);
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				Ret = CastField<FBoolProperty>(*It);
			}
			else if (FNameProperty* N = CastField<FNameProperty>(*It); N && Arg)
			{
				N->SetPropertyValue_InContainer(Parms, *Arg);
			}
		}
		bool bOut = false;
		if (Ret)
		{
			Sub->ProcessEvent(F, Parms);
			bOut = Ret->GetPropertyValue_InContainer(Parms);
		}
		for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Parms);
		}
		return bOut;
	}
}

bool ALyudRestaurant::IsFestival(const UObject* WorldContext)
{
	const int32 Forced = LyudRest::CVarFestival.GetValueOnGameThread();
	if (Forced >= 0)
	{
		return Forced == 1;
	}
	return LyudRest::FestivalBool(WorldContext, TEXT("IsFestivalToday"), nullptr);
}

bool ALyudRestaurant::IsFestivalDish(const UObject* WorldContext, FName Id)
{
	return IsFestival(WorldContext) && LyudRest::FestivalBool(WorldContext, TEXT("IsFestivalDish"), &Id);
}

namespace LyudRest
{
	/** The fishing branch's ULyudFishingState (a game instance subsystem): fish this kitchen took on its counting day (KitchenTaken[Kitchen],
	 *  KitchenDay), read by name; false without it. */
	bool FishTaken(const UObject* WorldContext, FName Kitchen, int32& OutTaken, int32& OutDay)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
		static TWeakObjectPtr<UClass> Class;
		static bool bLooked = false;
		if (!bLooked)
		{
			bLooked = true;
			Class = FindFirstObject<UClass>(TEXT("LyudFishingState"), EFindFirstObjectOptions::None);
		}
		if (!World || !World->GetGameInstance() || !Class.IsValid() || !Class->IsChildOf(UGameInstanceSubsystem::StaticClass()))
		{
			return false;
		}
		UObject* Sub = World->GetGameInstance()->GetSubsystemBase(TSubclassOf<UGameInstanceSubsystem>(Class.Get()));
		const FMapProperty* Taken = Sub ? CastField<FMapProperty>(Class->FindPropertyByName(TEXT("KitchenTaken"))) : nullptr;
		const FIntProperty* Day = Sub ? CastField<FIntProperty>(Class->FindPropertyByName(TEXT("KitchenDay"))) : nullptr;
		if (!Taken || !Day || !CastField<FNameProperty>(Taken->KeyProp) || !CastField<FIntProperty>(Taken->ValueProp))
		{
			return false;
		}
		const TMap<FName, int32>& Map = *Taken->ContainerPtrToValuePtr<TMap<FName, int32>>(Sub);
		OutTaken = Map.FindRef(Kitchen);
		OutDay = Day->GetPropertyValue_InContainer(Sub);
		return true;
	}
}

bool ALyudRestaurant::IsCatchOfDay(const FLyudRestDishDef& D) const
{
	const UWorld* World = GetWorld();
	return World && D.Id == TEXT("rs_baltic_fish") && CatchDay == UGameCalendar::NowIn(World).GetDayOfYear();
}

int32 ALyudRestaurant::PriceToday(const FLyudRestDishDef& D) const
{
	return IsCatchOfDay(D) ? FMath::RoundToInt(D.Price * 0.75f) : D.Price;
}

void ALyudRestaurant::UpdateFishing()
{
	// fish sold to the kitchen today: the Baltic plate is the catch of the day (fresher, cheaper)
	int32 Taken = 0, Day = 0;
	if (FishSettle > 0)
	{
		// just started or loaded: the next settled read is only the baseline, never a sale
		--FishSettle;
		FishSeenDay = -2;
	}
	else if (LyudRest::FishTaken(this, TEXT("yantar"), Taken, Day))
	{
		// a new counting day starts from nothing; a load can bring a smaller count back: take it as the new baseline
		if (Day != FishSeenDay || Taken < FishSeenTaken)
		{
			FishSeenTaken = FishSeenDay == -2 ? Taken : (Day != FishSeenDay ? 0 : Taken);
			FishSeenDay = Day;
		}
		if (Taken > FishSeenTaken)
		{
			CatchDay = UGameCalendar::NowIn(GetWorld()).GetDayOfYear();
			Effect(TEXT("rest_clink"), ToWorld(BackDoorOut), 0.5f);
			UE_LOG(LogTemp, Display, TEXT("RESTTEST the cook took %d fish at the kitchen door: catch of the day"), Taken - FishSeenTaken);
			FishSeenTaken = Taken;
		}
	}
	// the hero at the back door while open: the cook comes to it
	const AHeroCharacter* H = LyudRest::Hero(GetWorld());
	const FDateTime T = UGameCalendar::NowIn(GetWorld());
	const float Hour = T.GetHour() + T.GetMinute() / 60.f;
	const bool bKitchenOpen = Hour >= KitchenHours.X && Hour < KitchenHours.Y;
	bCookAtDoor = bKitchenOpen && H && FVector::Dist2D(H->GetActorLocation(), KitchenPoint) < KitchenRadius
		&& FMath::Abs(H->GetActorLocation().Z - KitchenPoint.Z) < 400.f;
}

bool ALyudRestaurant::IsInSectionToday(const UObject* WorldContext, const FLyudRestDishDef& D, FName Section)
{
	// a holiday's own dishes show in the festival section as well as in their own
	if (Section == TEXT("festival") && !D.bFree && D.Section != Section && IsFestivalDish(WorldContext, D.Id))
	{
		return true;
	}
	return D.Section == Section && IsOnMenuToday(WorldContext, D);
}

// ---- the dish ----------------------------------------------------------------------------------------------------------------------

ALyudRestDish::ALyudRestDish()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);
	auto Part = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// small and moving when carried or held: out of Lumen (docs/benchmark.md)
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		C->SetCanEverAffectNavigation(false);
		return C;
	};
	Vessel = Part(TEXT("Vessel"));
	VesselGlass = Part(TEXT("VesselGlass"));
	VesselGlass->SetCastShadow(false);
	Under = Part(TEXT("Under"));
	Food = Part(TEXT("Food"));
	Fill = Part(TEXT("Fill"));
	Fill->SetCastShadow(false);
}

void ALyudRestDish::Init(ALyudRestaurant* InOwner, const FLyudRestDishDef& InDef, float InLeft)
{
	Owner = InOwner;
	Def = InDef;
	UStaticMesh* V = ALyudRestaurant::RestMesh(Def.Vessel);
	LyudRest::MeshSize(V, VesselRadius, VesselHalfHeight);
	// a glass's main mesh is only its thick bottom (the walls are _Glass): the sizes of tableware.json win
	if (Def.CupRadius > 0.f)
	{
		VesselRadius = Def.CupRadius;
	}
	if (Def.CupHalfHeight > 0.f)
	{
		VesselHalfHeight = Def.CupHalfHeight;
	}
	// cups hang from their middle (the bite's held-cup pose), everything else stands on its origin
	Bottom = Def.IsCup() ? -VesselHalfHeight : 0.f;
	Vessel->SetStaticMesh(V);
	Vessel->SetRelativeLocation(FVector(0.f, 0.f, Bottom));
	if (UStaticMesh* G = ALyudRestaurant::RestMesh(FName(*(Def.Vessel.ToString() + TEXT("_Glass")))))
	{
		VesselGlass->SetStaticMesh(G);
		VesselGlass->SetRelativeLocation(FVector(0.f, 0.f, Bottom));
	}
	// a coffee cup's saucer: under the cup while it stands or is carried
	if (UStaticMesh* Sc = ALyudRestaurant::RestMesh(Def.Saucer))
	{
		Under->SetStaticMesh(Sc);
		Under->SetRelativeLocation(FVector(0.f, 0.f, Bottom - Def.SaucerZ));
	}
	StageMeshes.Reset();
	for (int32 K = 0; K < Def.Stages; ++K)
	{
		StageMeshes.Add(ALyudRestaurant::RestMesh(FName(*FString::Printf(TEXT("SM_RF_%s_%d"), *Def.Dish.ToString(), K))));
	}
	Food->SetRelativeLocation(FVector(0.f, 0.f, Bottom + Def.FoodZ));
	if (UStaticMesh* F = ALyudRestaurant::RestMesh(Def.Fill))
	{
		Fill->SetStaticMesh(F);
		Fill->SetRelativeLocation(FVector(0.f, 0.f, Bottom + Def.FoodZ));
	}
	SetLeft(InLeft);
}

void ALyudRestDish::SetLeft(float InLeft)
{
	Left = FMath::Clamp(InLeft, 0.f, 1.f);
	// the bite stages: 0 whole .. Stages-1 the last of it, nothing once it is eaten
	const int32 N = StageMeshes.Num();
	if (N > 0)
	{
		const int32 K = FMath::Clamp(FMath::FloorToInt((1.f - Left) * N), 0, N - 1);
		UStaticMesh* M = StageMeshes[K] ? StageMeshes[K].Get() : StageMeshes[0].Get();
		if (Food->GetStaticMesh() != M)
		{
			Food->SetStaticMesh(M);
		}
		Food->SetVisibility(Left > 0.f && M != nullptr);
	}
	if (Fill->GetStaticMesh())
	{
		// a liquid goes down (a soup by its stage's level, where its garnish floats), narrowing with the bowl's sloped walls
		float T = Left;
		if (Def.StageFill.Num() > 0 && N > 0)
		{
			const int32 K = FMath::Clamp(FMath::FloorToInt((1.f - Left) * N), 0, Def.StageFill.Num() - 1);
			T = Left > 0.f ? Def.StageFill[K] : 0.f;
		}
		float XY = 1.f;
		for (int32 I = 1; I < Def.FillXY.Num(); ++I)
		{
			const FVector2D& A0 = Def.FillXY[I - 1];
			const FVector2D& A1 = Def.FillXY[I];
			if (T <= A1.X || I == Def.FillXY.Num() - 1)
			{
				XY = FMath::Lerp(A0.Y, A1.Y, FMath::Clamp((T - A0.X) / FMath::Max(A1.X - A0.X, 1e-3), 0.0, 1.0));
				break;
			}
		}
		Fill->SetRelativeScale3D(FVector(XY, XY, FMath::Max(T, 0.02f)));
		Fill->SetVisibility(T > 0.02f);
	}
}

void ALyudRestDish::SetEdible(bool bOn)
{
	bEdible = bOn;
	SetActorTickEnabled(true);
}

void ALyudRestDish::ShowPrompt(bool bOn)
{
	if (bOn == bPrompted)
	{
		return;
	}
	bPrompted = bOn;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, bOn ? LyudText::Get(Def.IsCup() ? TEXT("ui.rest.drink_prompt") : TEXT("ui.rest.eat_prompt"),
			{ FStringFormatArg(ULyudInventory::ItemName(Def.Id).ToString()) }) : FText::GetEmpty());
	}
}

bool ALyudRestDish::StartEating()
{
	ULyudBiteSubsystem* Bite = ULyudBiteSubsystem::Get(this);
	if (Left <= 0.f || !Bite || bEating)
	{
		return false;
	}
	ShowPrompt(false);
	FLyudMeal M;
	M.Item = Def.Id;
	M.Dish = this;
	M.Amount = Left;
	M.SmallBite = Def.SmallBite;
	M.BigBite = Def.BigBite;
	const FName U = Def.Utensil;
	if (Def.IsCup())
	{
		M.Utensil = ELyudUtensil::Cup;
		M.DishRadius = VesselRadius;
		M.DishHalfHeight = VesselHalfHeight;
	}
	else
	{
		if (U == TEXT("spoon") || U == TEXT("teaspoon"))
		{
			M.Utensil = ELyudUtensil::Spoon;
			M.UtensilMesh = ALyudRestaurant::RestMesh(U == TEXT("spoon") ? TEXT("SM_RT_Spoon") : TEXT("SM_RT_TeaSpoon"));
			if (U == TEXT("teaspoon"))
			{
				M.LoadOffset = FVector(8.28f, 0.f, 0.43f);
			}
		}
		else if (U == TEXT("chopsticks"))
		{
			M.Utensil = ELyudUtensil::Chopsticks;
			M.UtensilMesh = ALyudRestaurant::RestMesh(TEXT("SM_RT_Chopsticks"));
			// the pinch at the tips (tableware.json CHOP_TIP_X)
			M.LoadOffset = FVector(14.5f, 0.f, 0.12f);
		}
		else if (U == TEXT("fingers"))
		{
			// the load's own origin is the pinch point, the piece running on along +x
			M.Utensil = ELyudUtensil::Fingers;
			M.LoadOffset = FVector(1.5f, 0.f, 0.f);
		}
		else
		{
			M.Utensil = ELyudUtensil::Fork;
			M.UtensilMesh = ALyudRestaurant::RestMesh(TEXT("SM_RT_Fork"));
		}
		M.LoadMesh = ALyudRestaurant::RestMesh(Def.LoadS);
		M.LoadMeshBig = ALyudRestaurant::RestMesh(Def.LoadB);
	}
	if (Under->GetStaticMesh() && !SaucerLeft)
	{
		// the saucer stays where it stands
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		P.ObjectFlags |= RF_Transient;
		SaucerLeft = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Under->GetComponentTransform(), P);
		SaucerLeft->SetMobility(EComponentMobility::Movable);
		SaucerLeft->GetStaticMeshComponent()->SetStaticMesh(Under->GetStaticMesh());
		SaucerLeft->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Under->SetVisibility(false);
	}
	TWeakObjectPtr<ALyudRestDish> Self(this);
	M.OnBite = [Self](float L, ELyudBiteSize) { if (Self.IsValid()) { Self->SetLeft(L); } };
	M.OnEnd = [Self](float L) { if (Self.IsValid()) { Self->OnMealEnd(L); } };
	bEating = Bite->BeginMeal(M);
	UE_LOG(LogTemp, Display, TEXT("RESTTEST eat %s (%s) %s"), *Def.Id.ToString(), *U.ToString(), bEating ? TEXT("started") : TEXT("refused"));
	return bEating;
}

void ALyudRestDish::OnMealEnd(float InLeft)
{
	bEating = false;
	SetLeft(InLeft);
	// a cup goes back where it stood (the hand let go of it), onto its saucer
	SetActorTransform(Rest);
	if (SaucerLeft)
	{
		SaucerLeft->Destroy();
		SaucerLeft = nullptr;
		Under->SetVisibility(true);
	}
	UE_LOG(LogTemp, Display, TEXT("RESTTEST meal %s ended, left %.2f"), *Def.Id.ToString(), InLeft);
}

void ALyudRestDish::EndPlay(const EEndPlayReason::Type Reason)
{
	if (SaucerLeft)
	{
		SaucerLeft->Destroy();
		SaucerLeft = nullptr;
	}
	if (bPrompted)
	{
		ShowPrompt(false);
	}
	Super::EndPlay(Reason);
}

void ALyudRestDish::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Left <= 0.f && !bEating)
	{
		ShowPrompt(false);
		EmptyFor += DeltaTime;
	}
	else
	{
		EmptyFor = 0.f;
	}
	if (!bEdible)
	{
		SetActorTickEnabled(Left <= 0.f);
	}
}

// ---- the restaurant -----------------------------------------------------------------------------------------------------------------

ALyudRestaurant::ALyudRestaurant()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ALyudRestaurant* ALyudRestaurant::Find(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ALyudRestaurant> It(World);
	return It ? *It : nullptr;
}

FVector ALyudRestaurant::ToWorld(const FVector& M) const
{
	// the building's frame went through glTF: Blender (x, y, z) m -> Unreal (x, -y, z) cm in the mesh
	return GetActorTransform().TransformPosition(FVector(M.X * 100.f, -M.Y * 100.f, M.Z * 100.f));
}

FVector ALyudRestaurant::ToLocal(const FVector& W) const
{
	const FVector L = GetActorTransform().InverseTransformPosition(W);
	return FVector(L.X / 100.f, -L.Y / 100.f, L.Z / 100.f);
}

FRotator ALyudRestaurant::YawToWorld(float LocalYawDeg) const
{
	return FRotator(0.f, GetActorRotation().Yaw - LocalYawDeg, 0.f);
}

void ALyudRestaurant::BeginPlay()
{
	Super::BeginPlay();
	const TSharedPtr<FJsonObject>& C = LyudRest::Config();
	WaitScale = (float)LyudRest::Number(C, TEXT("wait_scale"), 1.0);
	ClearAfterS = (float)LyudRest::Number(C, TEXT("clear_after_s"), 25.0);
	WaiterSpeed = (float)LyudRest::Number(C, TEXT("waiter_speed_cm_s"), 125.0);
	TableResetS = (float)LyudRest::Number(C, TEXT("table_reset_s"), 90.0);
	BreadAfterS = (float)LyudRest::Number(C, TEXT("bread_after_s"), 12.0);
	const FString Pl = LyudRest::Str(C, TEXT("place_id"));
	PlaceId = Pl.IsEmpty() ? FName(TEXT("restaurant")) : FName(*Pl);
	const TSharedPtr<FJsonObject>* Standin = nullptr;
	if (C.IsValid() && C->TryGetObjectField(TEXT("standin"), Standin))
	{
		WaiterStandinMesh = LyudRest::Str(*Standin, TEXT("mesh"));
		CookStandinMesh = LyudRest::Str(*Standin, TEXT("cook_mesh"));
	}
	WaiterStandinMesh = WaiterStandinMesh.IsEmpty() ? TEXT("Guy_b") : WaiterStandinMesh;
	CookStandinMesh = CookStandinMesh.IsEmpty() ? TEXT("Crowd_11") : CookStandinMesh;
	PreloadMeshes();
	// the fishing's kitchen point: cm from the plot's ue (the actor stands `podium` over it), unrotated, as fishing.json has it
	const TSharedPtr<FJsonObject>* KD = nullptr;
	if (C.IsValid() && C->TryGetObjectField(TEXT("kitchen_door"), KD))
	{
		const FVector Ground = GetActorLocation() - FVector(0.f, 0.f, 100.f * (float)LyudRest::Number(C, TEXT("podium"), 0.6));
		KitchenPoint = Ground + LyudRest::Vec(*KD, TEXT("offset_cm"));
		KitchenRadius = (float)LyudRest::Number(*KD, TEXT("radius_cm"), 220.0);
		const FVector Hrs = LyudRest::Vec(*KD, TEXT("hours"));
		KitchenHours = FVector2D(Hrs.X, Hrs.Y);
	}

	LoadLayout();
	if (KitchenPoint.IsZero())
	{
		KitchenPoint = ToWorld(BackDoorOut);
	}
	FindParts();
	SpawnMenuCards();
	UpdateState();
	UE_LOG(LogTemp, Display, TEXT("RESTTEST begin: layout %d, %d tables, %d seats, %d nodes, %d doors, %d+%d lamps, %d dishes on the menu, %s"),
		bLayout ? 1 : 0, Tables.Num(), Seats.Num(), Nodes.Num(), Doors.Num(), InsideLights.Num(), OutsideLights.Num(), Menu().Num(),
		bOpen ? TEXT("open") : TEXT("closed"));
}

void ALyudRestaurant::EndPlay(const EEndPlayReason::Type Reason)
{
	CloseMenu();
	ReleaseWaiter();
	ReleaseCook();
	for (FProp& Pr : Props)
	{
		GuestLeft(Pr, false);
	}
	Props.Reset();
	for (ALyudRestDish* D : HeroDishes)
	{
		if (D)
		{
			D->Destroy();
		}
	}
	HeroDishes.Reset();
	if (BillFolder)
	{
		BillFolder->Destroy();
		BillFolder = nullptr;
	}
	if (Knife)
	{
		Knife->Destroy();
		Knife = nullptr;
	}
	if (Plaque)
	{
		Plaque->Destroy();
		Plaque = nullptr;
	}
	Preload.Reset();
	for (const int32 Id : HiddenStaff)
	{
		LyudRestaurantStaff::SetPresentedExternally(this, Id, false);
	}
	HiddenStaff.Reset();
	for (AStaticMeshActor* A : KitchenProps)
	{
		if (A)
		{
			A->Destroy();
		}
	}
	KitchenProps.Reset();
	for (FOrder& O : Orders)
	{
		if (ALyudRestDish* D = O.Dish.Get())
		{
			D->Destroy();
		}
	}
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
	for (FTable& T : Tables)
	{
		if (AStaticMeshActor* A = T.MenuCard.Get())
		{
			A->Destroy();
		}
	}
	Super::EndPlay(Reason);
}

void ALyudRestaurant::LoadLayout()
{
	const TSharedPtr<FJsonObject> L = LyudRest::ReadJson(TEXT("Restaurant/layout.json"));
	if (!L.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Restaurant: Content/Restaurant/layout.json unreadable"));
		return;
	}
	bLayout = true;
	const TSharedPtr<FJsonObject>* Box = nullptr;
	if (L->TryGetObjectField(TEXT("box"), Box))
	{
		const TSharedPtr<FJsonObject>* Hall = nullptr;
		if ((*Box)->TryGetObjectField(TEXT("hall"), Hall))
		{
			HallR = (float)LyudRest::Number(*Hall, TEXT("r"), 9.85);
			ChordX = (float)LyudRest::Number(*Hall, TEXT("chord_x"), -8.4);
			const FVector Z = LyudRest::Vec(*Hall, TEXT("z"));
			HallTop = Z.Y > 0.f ? (float)Z.Y : 4.f;
		}
		const TArray<TSharedPtr<FJsonValue>>* K = nullptr;
		if ((*Box)->TryGetArrayField(TEXT("kitchen"), K) && K->Num() == 6)
		{
			KitchenMin = FVector((*K)[0]->AsNumber(), (*K)[2]->AsNumber(), (*K)[4]->AsNumber());
			KitchenMax = FVector((*K)[1]->AsNumber(), (*K)[3]->AsNumber(), (*K)[5]->AsNumber());
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* TablesJ = nullptr;
	if (L->TryGetArrayField(TEXT("tables"), TablesJ))
	{
		for (const TSharedPtr<FJsonValue>& V : *TablesJ)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FTable T;
			T.Id = FName(*LyudRest::Str(O, TEXT("id")));
			T.At = LyudRest::Vec(O, TEXT("at"));
			O->TryGetBoolField(TEXT("hero"), T.bHero);
			T.ServeAt = LyudRest::Vec(O, TEXT("serve_at"));
			T.Node = FName(*LyudRest::Str(O, TEXT("node")));
			const TArray<TSharedPtr<FJsonValue>>* SeatsJ = nullptr;
			if (O->TryGetArrayField(TEXT("seats"), SeatsJ))
			{
				for (const TSharedPtr<FJsonValue>& SV : *SeatsJ)
				{
					const TSharedPtr<FJsonObject> SO = SV->AsObject();
					FSeat S;
					S.At = LyudRest::Vec(SO, TEXT("at"));
					S.Yaw = (float)LyudRest::Number(SO, TEXT("yaw"), 0.0);
					S.Height = (float)LyudRest::Number(SO, TEXT("height"), 0.46);
					S.Place = LyudRest::Vec(SO, TEXT("place"));
					S.Table = Tables.Num();
					T.Seats.Add(Seats.Num());
					Seats.Add(S);
				}
			}
			Tables.Add(T);
		}
	}
	const TSharedPtr<FJsonObject>* Pass = nullptr;
	if (L->TryGetObjectField(TEXT("pass"), Pass))
	{
		const TArray<TSharedPtr<FJsonValue>>* Shelf = nullptr;
		if ((*Pass)->TryGetArrayField(TEXT("shelf"), Shelf))
		{
			for (const TSharedPtr<FJsonValue>& V : *Shelf)
			{
				const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
				if (A.Num() >= 3)
				{
					PassShelf.Add(FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()));
				}
			}
		}
		PassStand = LyudRest::Vec(*Pass, TEXT("waiter"));
		PassYaw = (float)LyudRest::Number(*Pass, TEXT("yaw"), 180.0);
	}
	const TSharedPtr<FJsonObject>* Home = nullptr;
	if (L->TryGetObjectField(TEXT("waiter_home"), Home))
	{
		WaiterHome = LyudRest::Vec(*Home, TEXT("at"));
		HomeYaw = (float)LyudRest::Number(*Home, TEXT("yaw"), 0.0);
	}
	else
	{
		WaiterHome = PassStand;
		HomeYaw = PassYaw;
	}
	MenuSpot = LyudRest::Vec(L, TEXT("menu_spot"));
	const TSharedPtr<FJsonObject>* Entrance = nullptr;
	if (L->TryGetObjectField(TEXT("entrance"), Entrance))
	{
		BackDoorOut = LyudRest::Vec(*Entrance, TEXT("back_outside"));
		BackDoorIn = LyudRest::Vec(*Entrance, TEXT("back_inside"));
		BackDoorOut.Z = BackDoorIn.Z = 0.0;
	}
	const TSharedPtr<FJsonObject>* Cook = nullptr;
	if (L->TryGetObjectField(TEXT("cook"), Cook))
	{
		CookAt = LyudRest::Vec(*Cook, TEXT("at"));
		CookYaw = (float)LyudRest::Number(*Cook, TEXT("yaw"), 180.0);
	}
	const TSharedPtr<FJsonObject>* Walk = nullptr;
	if (L->TryGetObjectField(TEXT("walk"), Walk))
	{
		const TSharedPtr<FJsonObject>* NodesJ = nullptr;
		if ((*Walk)->TryGetObjectField(TEXT("nodes"), NodesJ))
		{
			for (const auto& It : (*NodesJ)->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>& A = It.Value->AsArray();
				if (A.Num() >= 2)
				{
					Nodes.Add(FName(*It.Key), FVector2D(A[0]->AsNumber(), A[1]->AsNumber()));
				}
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* EdgesJ = nullptr;
		if ((*Walk)->TryGetArrayField(TEXT("edges"), EdgesJ))
		{
			for (const TSharedPtr<FJsonValue>& V : *EdgesJ)
			{
				const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
				if (A.Num() == 2)
				{
					const FName X(*A[0]->AsString()), Y(*A[1]->AsString());
					Edges.FindOrAdd(X).AddUnique(Y);
					Edges.FindOrAdd(Y).AddUnique(X);
				}
			}
		}
	}
	// the lamps: inside and the kitchen while open, the soffit's ring at night
	const TArray<TSharedPtr<FJsonValue>>* Lights = nullptr;
	if (L->TryGetArrayField(TEXT("lights"), Lights))
	{
		for (const TSharedPtr<FJsonValue>& V : *Lights)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const bool bOut = LyudRest::Str(O, TEXT("kind")) == TEXT("outside");
			UPointLightComponent* P = NewObject<UPointLightComponent>(this);
			P->SetupAttachment(RootComponent);
			P->SetWorldLocation(ToWorld(LyudRest::Vec(O, TEXT("at"))));
			P->SetIntensityUnits(ELightUnits::Lumens);
			P->SetIntensity((float)LyudRest::Number(O, TEXT("lumens"), 600));
			P->SetLightColor(LyudRest::Hex(LyudRest::Str(O, TEXT("colour"))));
			P->SetAttenuationRadius(bOut ? 650.f : 480.f);
			P->SetCastShadows(false);
			P->SetLightingChannels(true, true, false);
			P->MaxDrawDistance = 6000.f;
			P->MaxDistanceFadeRange = 800.f;
			P->SetVisibility(false);
			P->RegisterComponent();
			(bOut ? OutsideLights : InsideLights).Add(P);
		}
	}
}

void ALyudRestaurant::FindParts()
{
	// the doors (import_restaurant.py tags them "Restaurant", the outer ones also "RestaurantOuter") and the lamp shades and the sign
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("Restaurant")) || *It == this)
		{
			continue;
		}
		if (ALyudDoorBase* D = Cast<ALyudDoorBase>(*It))
		{
			if (It->ActorHasTag(TEXT("RestaurantOuter")))
			{
				Doors.Add(D);
			}
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
		for (UStaticMeshComponent* M : Meshes)
		{
			for (const FName& Slot : M->GetMaterialSlotNames())
			{
				const FString S = Slot.ToString();
				const bool bSign = S.StartsWith(TEXT("RS_Sign"));
				if (S.StartsWith(TEXT("RS_Opal")) || bSign)
				{
					if (UMaterialInstanceDynamic* MID = M->CreateDynamicMaterialInstance(M->GetMaterialIndex(Slot)))
					{
						(bSign ? SignMIDs : GlowMIDs).Add(MID);
					}
				}
			}
		}
	}
	ApplySigns();
}

void ALyudRestaurant::SpawnMenuCards()
{
	// a closed menu card on each of the hero's tables, at the edge between its seats
	UStaticMesh* Card = RestMesh(TEXT("SM_RT_MenuCard"));
	for (FTable& T : Tables)
	{
		if (!T.bHero || T.Seats.Num() == 0)
		{
			continue;
		}
		const FSeat& S = Seats[T.Seats[0]];
		// halfway between the table's middle and the first seat's plate, turned to it
		const FVector P = FMath::Lerp(FVector(T.At.X, T.At.Y, S.Place.Z), S.Place, 0.45f);
		const FVector Side = FVector(-FMath::Sin(FMath::DegreesToRadians(S.Yaw)), FMath::Cos(FMath::DegreesToRadians(S.Yaw)), 0.f) * 0.17f;
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SP.ObjectFlags |= RF_Transient;
		AStaticMeshActor* A = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), ToWorld(P + Side + FVector(0, 0, 0.001)),
			YawToWorld(S.Yaw + 90.f), SP);
		if (!A)
		{
			continue;
		}
		A->SetMobility(EComponentMobility::Movable);
		A->Tags.Add(TEXT("Restaurant"));
		UStaticMeshComponent* C = A->GetStaticMeshComponent();
		if (Card)
		{
			C->SetStaticMesh(Card);
		}
		else
		{
			// no card mesh imported yet: a thin amber board
			C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			C->SetRelativeScale3D(FVector(0.24f, 0.32f, 0.008f));
			A->SetActorLocation(A->GetActorLocation() + FVector(0, 0, 0.4f));
		}
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->bAffectDistanceFieldLighting = false;
		T.MenuCard = A;
	}
}

bool ALyudRestaurant::IsHeroInside() const
{
	const AHeroCharacter* H = LyudRest::Hero(GetWorld());
	if (!H || !bLayout)
	{
		return false;
	}
	const FVector P = ToLocal(H->GetActorLocation());
	const bool bHall = FVector2D(P.X, P.Y).Size() < HallR && P.X > ChordX && P.Z > -0.5f && P.Z < HallTop + 0.5f;
	const bool bKitchen = P.X > KitchenMin.X && P.X < KitchenMax.X && P.Y > KitchenMin.Y && P.Y < KitchenMax.Y && P.Z > -0.5f && P.Z < KitchenMax.Z + 0.5f;
	return bHall || bKitchen;
}

bool ALyudRestaurant::IsHeroSeated() const
{
	const AHeroCharacter* H = LyudRest::Hero(GetWorld());
	return H && H->IsSeated() && HeroSeatIx >= 0;
}

void ALyudRestaurant::RefreshStaff()
{
	// who is on shift (the town life's), their look; once a second
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	auto Pick = [&](FName Role, int32& Person, FString& Mesh)
	{
		const TArray<int32> On = LyudRestaurantStaff::OnShift(this, PlaceId, Role);
		Person = On.Num() > 0 ? On[0] : -1;
		Mesh.Reset();
		FPersonInfo P;
		if (Person >= 0 && People && People->GetPerson(Person, P) && !P.Mesh.IsNone())
		{
			Mesh = P.Mesh.ToString();
		}
	};
	Pick(TEXT("waiter"), StaffWaiter, StaffWaiterMesh);
	Pick(TEXT("cook"), StaffCook, StaffCookMesh);
	// the rest of the staff on shift (more waiters, a second cook, the manager) would stand at the same posts: hidden while here
	TSet<int32> Extra;
	for (const int32 Id : LyudRestaurantStaff::OnShift(this, PlaceId, NAME_None))
	{
		if (Id != StaffWaiter && Id != StaffCook)
		{
			Extra.Add(Id);
		}
	}
	for (const int32 Id : HiddenStaff)
	{
		if (!Extra.Contains(Id))
		{
			LyudRestaurantStaff::SetPresentedExternally(this, Id, false);
		}
	}
	for (const int32 Id : Extra)
	{
		if (!HiddenStaff.Contains(Id))
		{
			LyudRestaurantStaff::SetPresentedExternally(this, Id, true);
		}
	}
	HiddenStaff = MoveTemp(Extra);
}

void ALyudRestaurant::UpdateState()
{
	// open while a waiter is on shift; before the town life has the restaurant's staff, by the hours
	// the town life's staff when this build has them (LyudRestaurantStaff), else the hours of restaurant.json
	const bool bStaffKnown = LyudRestaurantStaff::IsAvailable(this);
	const bool bStaffOn = bStaffKnown && StaffWaiter >= 0;
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	const float Hour = Now.GetHour() + Now.GetMinute() / 60.f;
	const FVector Hours = LyudRest::Vec(LyudRest::Config(), TEXT("hours"));
	const bool bByHours = Hours.Y > Hours.X ? Hour >= Hours.X && Hour < Hours.Y : true;
	const bool bWant = ForceOpen >= 0 ? ForceOpen == 1 : (bStaffKnown ? bStaffOn : bByHours);
	// last orders: the kitchen stops taking them 20 game minutes before the hours' end
	bKitchenClosing = Hours.Y > Hours.X && Hour >= Hours.Y - 20.f / 60.f && Hour < Hours.Y && ForceOpen != 1;
	if (bWant != bOpen)
	{
		bOpen = bWant;
		UE_LOG(LogTemp, Display, TEXT("RESTTEST %s (%02d:%02d, staff %s)"), bOpen ? TEXT("open") : TEXT("closed"), Now.GetHour(), Now.GetMinute(),
			bStaffKnown ? (bStaffOn ? TEXT("on shift") : TEXT("off")) : TEXT("by the hours"));
	}
	bLit = bOpen || (Waiter != EWaiter::Off && Waiter != EWaiter::Idle);
	TActorIterator<ALyudSky> Sky(GetWorld());
	if (Sky)
	{
		const float Alt = Sky->SunAltitude();
		bNight = bNight ? Alt < 4.f : Alt < 1.f;
	}
	// locked once closed and the hero is out (the staff finishing inside don't hold it open)
	// locked a couple of game minutes after everything holds (the last guests and the staff off shift walk out first), never with
	// anybody of the life still in the hall
	const bool bReady = !bOpen && !IsHeroInside() && Props.Num() == 0 && Waiter == EWaiter::Off && !CookBody;
	if (bReady && !bLockReady)
	{
		LockReadySince = Now;
	}
	bLockReady = bReady;
	const bool bWantLock = bReady && (Now - LockReadySince).GetTotalMinutes() >= 2.0 && (bLocked || !bNear || !AnyoneOfLifeInside());
	if (bWantLock != bLocked)
	{
		bLocked = bWantLock;
		for (const TWeakObjectPtr<ALyudDoorBase>& D : Doors)
		{
			if (ALyudDoorBase* Door = D.Get())
			{
				if (bLocked && Door->IsOpen())
				{
					Door->Close(this);
				}
				Door->SetLocked(bLocked);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("RESTTEST doors %s"), bLocked ? TEXT("locked") : TEXT("unlocked"));
	}
	if (SignLanguage != LyudText::Language())
	{
		ApplySigns();
	}
	UpdateFishing();
	ApplyLights();
}

bool ALyudRestaurant::AnyoneOfLifeInside() const
{
	// only bodies being drawn can walk through a closed door: the life's ANpcBody actors (a few dozen near the camera), not every person
	for (TActorIterator<ANpcBody> It(GetWorld()); It; ++It)
	{
		if (It->IsHidden() || *It == WaiterBody || *It == CookBody)
		{
			continue;
		}
		const FVector L = ToLocal(It->GetActorLocation());
		const bool bHall = FVector2D(L.X, L.Y).Size() < HallR + 0.5f && FMath::Abs(L.Z) < 2.f;
		const bool bKitchen = L.X > KitchenMin.X - 0.5f && L.X < KitchenMax.X + 0.5f && L.Y > KitchenMin.Y && L.Y < KitchenMax.Y && FMath::Abs(L.Z) < 2.f;
		if (bHall || bKitchen)
		{
			return true;
		}
	}
	return false;
}

void ALyudRestaurant::ApplyLights()
{
	const bool bShown = LyudRest::CVarRestaurant.GetValueOnGameThread() != 0;
	for (UPointLightComponent* P : InsideLights)
	{
		P->SetVisibility(bLit && bShown);
	}
	for (UPointLightComponent* P : OutsideLights)
	{
		P->SetVisibility(bLit && bNight && bShown);
	}
	for (UMaterialInstanceDynamic* MID : GlowMIDs)
	{
		MID->SetScalarParameterValue(TEXT("Emissive"), bLit ? 6.f : 0.f);
	}
	for (UMaterialInstanceDynamic* MID : SignMIDs)
	{
		// the name's letters burn amber at dusk while open
		MID->SetScalarParameterValue(TEXT("Emissive"), bOpen && bNight ? 14.f : 0.f);
		MID->SetScalarParameterValue(TEXT("Glow"), bOpen && bNight ? 1.f : 0.f);
	}
}

void ALyudRestaurant::ApplySigns()
{
	SignLanguage = LyudText::Language();
	// the plaque on the menu stand's top, tilted up to the reader coming to the door (the stand faces -y, the street side)
	if (!Plaque && !MenuSpot.IsNearlyZero())
	{
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FRotator Rot = (FRotator(0.f, YawToWorld(-90.f).Yaw, 0.f).Quaternion() * FRotator(-55.f, 0.f, 0.f).Quaternion()).Rotator();
		Plaque = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), ToWorld(MenuSpot + FVector(0, 0, 0.035f)), Rot, SP);
		if (Plaque)
		{
			Plaque->SetMobility(EComponentMobility::Movable);
			Plaque->Tags.Add(TEXT("Restaurant"));
			UStaticMeshComponent* C = Plaque->GetStaticMeshComponent();
			C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			// 40 x 20 cm, 1 cm thick (the cube is 1 m): its +X face toward the reader
			C->SetRelativeScale3D(FVector(0.01f, 0.40f, 0.20f));
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->bAffectDistanceFieldLighting = false;
			if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Restaurant/Materials/MI_RS_SignPlaque.MI_RS_SignPlaque")))
			{
				PlaqueMID = C->CreateDynamicMaterialInstance(0, M);
			}
		}
	}
	if (PlaqueMID)
	{
		if (UTexture2D* Tex = LyudText::LoadTexture(TEXT("Restaurant"), TEXT("T_Sign_plaque")))
		{
			PlaqueMID->SetTextureParameterValue(TEXT("Image"), Tex);
		}
	}
}

int32 ALyudRestaurant::PendingCount(bool bHeroOnly) const
{
	int32 N = 0;
	for (const FOrder& O : Orders)
	{
		N += O.State != FOrder::EState::Served && (!bHeroOnly || !O.bGuest) ? 1 : 0;
	}
	return N;
}

FString ALyudRestaurant::StateReport() const
{
	static const TCHAR* WaiterNames[] = { TEXT("off"), TEXT("idle"), TEXT("to_pass"), TEXT("pick"), TEXT("to_table"), TEXT("set_down"),
		TEXT("collect"), TEXT("to_pass_back"), TEXT("drop"), TEXT("to_home") };
	FString Dishes;
	for (const ALyudRestDish* D : HeroDishes)
	{
		if (D)
		{
			Dishes += FString::Printf(TEXT(" %s:%.2f"), *D->GetDef().Id.ToString(), D->GetLeft());
		}
	}
	return FString::Printf(TEXT("%s%s, hero %s (table %d seat %d), %d orders pending, tab %d, bill %s, waiter %s (person %d, %s), dishes:%s, props %d"),
		bOpen ? TEXT("open") : TEXT("closed"), bNight ? TEXT(" night") : TEXT(""), IsHeroSeated() ? TEXT("seated") : (IsHeroInside() ? TEXT("inside") : TEXT("outside")),
		HeroTableIx, HeroSeatIx, PendingCount(), Tab, bBillServed ? TEXT("served") : (bBillAsked ? TEXT("asked") : TEXT("no")),
		WaiterNames[(int32)Waiter], WaiterPerson, *WaiterMesh, *Dishes, Props.Num());
}

void ALyudRestaurant::Toast(const FText& Text) const
{
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->Toast(Text);
	}
}

void ALyudRestaurant::SetPrompt(const FString& Key, const TArray<FStringFormatArg>& Args)
{
	if (Key.IsEmpty() && Prompted.IsEmpty())
	{
		return;
	}
	FString Full = Key;
	for (const FStringFormatArg& A : Args)
	{
		Full += TEXT("|") + (A.Type == FStringFormatArg::String ? A.StringValue : FString::FromInt((int32)A.IntValue));
	}
	if (Full == Prompted)
	{
		return;
	}
	Prompted = Full;
	// another line is up now: what TickHero remembered of the last one no longer holds
	PromptDish = nullptr;
	PromptTab = -1;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, Key.IsEmpty() ? FText::GetEmpty() : LyudText::Get(Key, Args));
	}
}

// ---- the hero's visit -----------------------------------------------------------------------------------------------------------

bool ALyudRestaurant::IsSeatTaken(int32 Seat) const
{
	if (Seat == HeroSeatIx)
	{
		return true;
	}
	return Props.ContainsByPredicate([Seat](const FProp& P) { return P.Seat == Seat; });
}

int32 ALyudRestaurant::SeatNearHero(float MaxCm) const
{
	const AHeroCharacter* H = LyudRest::Hero(GetWorld());
	const APlayerController* PC = LyudRest::Controller(GetWorld());
	if (!H || !PC)
	{
		return -1;
	}
	const FVector Feet = H->GetActorLocation();
	const FVector Look = PC->GetControlRotation().Vector().GetSafeNormal2D();
	int32 Best = -1;
	float BestScore = TNumericLimits<float>::Max();
	for (int32 I = 0; I < Seats.Num(); ++I)
	{
		const FSeat& S = Seats[I];
		if (!Tables[S.Table].bHero || IsSeatTaken(I) || (HeroTableIx >= 0 && S.Table != HeroTableIx && Orders.Num() > 0))
		{
			continue;
		}
		// the chair's middle (behind the seat's front edge)
		const FVector Mid = ToWorld(S.At - FVector(FMath::Cos(FMath::DegreesToRadians(S.Yaw)), FMath::Sin(FMath::DegreesToRadians(S.Yaw)), 0.f) * 0.2f);
		const FVector To = Mid - Feet;
		const float D = (float)To.Size2D();
		if (D > MaxCm)
		{
			continue;
		}
		const float Facing = FVector::DotProduct(Look, To.GetSafeNormal2D());
		if (Facing < 0.2f && D > 45.f)
		{
			continue;
		}
		const float Score = D * (1.6f - Facing);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = I;
		}
	}
	return Best;
}

FVector ALyudRestaurant::StandOffsetFor(int32 Seat) const
{
	// beside the chair on the side with room (the aisle), or behind it: the first place the standing capsule fits
	const AHeroCharacter* H = LyudRest::Hero(GetWorld());
	const FSeat& S = Seats[Seat];
	const FTransform W(YawToWorld(S.Yaw), ToWorld(S.At));
	const TArray<FVector> Tries = { FVector(-25.f, 62.f, 0.f), FVector(-25.f, -62.f, 0.f), FVector(-78.f, 0.f, 0.f), FVector(-40.f, 85.f, 0.f), FVector(-40.f, -85.f, 0.f) };
	if (!H)
	{
		return Tries[0];
	}
	const float Half = H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Radius = H->GetCapsuleComponent()->GetScaledCapsuleRadius();
	FCollisionQueryParams Q(TEXT("RestStand"), false, H);
	for (const FVector& T : Tries)
	{
		const FVector At = W.TransformPosition(T) + FVector(0.f, 0.f, Half + 4.f);
		if (!GetWorld()->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, Half), Q))
		{
			return T;
		}
	}
	return Tries[0];
}

bool ALyudRestaurant::SitHero(int32 Seat)
{
	AHeroCharacter* H = LyudRest::Hero(GetWorld());
	if (!H || H->IsSeated() || H->IsOnBike())
	{
		return false;
	}
	if (Seat < 0)
	{
		Seat = SeatNearHero(400.f);
		for (int32 I = 0; Seat < 0 && I < Seats.Num(); ++I)
		{
			if (Tables[Seats[I].Table].bHero && !IsSeatTaken(I))
			{
				Seat = I;
			}
		}
	}
	if (!Seats.IsValidIndex(Seat) || IsSeatTaken(Seat))
	{
		return false;
	}
	if (ULyudHandUse* Hand = ULyudHandUse::Get(this))
	{
		Hand->PutAway();
	}
	const FSeat& S = Seats[Seat];
	// the seat's frame on this actor: the floor under the seat's front edge, X the way the sitter faces
	const FTransform Local(FRotator(0.f, -S.Yaw, 0.f), FVector(S.At.X * 100.f, -S.At.Y * 100.f, S.At.Z * 100.f));
	H->SitOn(RootComponent, Local, S.Height * 100.f);
	H->SetStandUpOffset(StandOffsetFor(Seat));
	// looking at the table
	if (APlayerController* PC = LyudRest::Controller(GetWorld()))
	{
		PC->SetControlRotation(FRotator(-28.f, YawToWorld(S.Yaw).Yaw, 0.f));
	}
	if (HeroTableIx != S.Table)
	{
		ResetHeroTable();
	}
	HeroSeatIx = Seat;
	HeroTableIx = S.Table;
	SatAt = GetWorld()->GetTimeSeconds();
	AwayFor = 0.f;
	// bread and water first, unasked, once a visit (when someone is there to bring them)
	if (!bFreeBrought && bOpen)
	{
		bFreeBrought = true;
		const float After = BreadAfterS * FMath::Max(WaitScale, 0.05f);
		for (const FLyudRestDishDef& D : Menu())
		{
			if (D.bFree)
			{
				AddOrder(D.Id, Seat, SatAt + After, false);
			}
		}
	}
	SetPrompt(FString());
	UE_LOG(LogTemp, Display, TEXT("RESTTEST sit: table %s seat %d"), *Tables[S.Table].Id.ToString(), Seat);
	return true;
}

void ALyudRestaurant::StandHero()
{
	AHeroCharacter* H = LyudRest::Hero(GetWorld());
	if (ULyudBiteSubsystem* Bite = ULyudBiteSubsystem::Get(this))
	{
		Bite->EndMeal();
	}
	CloseMenu();
	if (H && H->IsSeated() && HeroSeatIx >= 0)
	{
		H->StandUp();
		UE_LOG(LogTemp, Display, TEXT("RESTTEST stand: from seat %d"), HeroSeatIx);
	}
	HeroSeatIx = -1;
	SetPrompt(FString());
}

void ALyudRestaurant::OpenMenu()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (Menu_.IsValid() || !UI)
	{
		return;
	}
	SetPrompt(FString());
	Menu_ = SNew(SLyudRestMenu).Owner(this);
	if (Viewport)
	{
		Viewport->AddViewportWidgetContent(Menu_.ToSharedRef(), 70);
	}
	UI->Lock(true);
	UI->SetExternalScreen(this);
	if (const AHeroCharacter* H = LyudRest::Hero(GetWorld()))
	{
		Effect(TEXT("rest_menu"), H->EyeLocation() + H->GetActorForwardVector() * 40.f, 0.6f);
	}
	UE_LOG(LogTemp, Display, TEXT("RESTTEST menu open (%d dishes, festival %d)"), Menu().Num(), IsFestival(this) ? 1 : 0);
}

void ALyudRestaurant::CloseMenu()
{
	if (!Menu_.IsValid())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(Menu_.ToSharedRef());
		}
	}
	Menu_.Reset();
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetExternalScreen(nullptr);
		UI->Lock(false);
	}
	UE_LOG(LogTemp, Display, TEXT("RESTTEST menu closed"));
}

ALyudRestaurant::FOrder& ALyudRestaurant::AddOrder(FName Item, int32 Seat, float Due, bool bGuest)
{
	FOrder& O = Orders.AddDefaulted_GetRef();
	O.Id = ++NextOrderId;
	O.Item = Item;
	O.Seat = Seat;
	O.Due = Due;
	O.bGuest = bGuest;
	return O;
}

ALyudRestaurant::EOrderResult ALyudRestaurant::PlaceOrder(const TMap<FName, int32>& Items)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!bOpen)
	{
		return EOrderResult::Closed;
	}
	if (bKitchenClosing)
	{
		return EOrderResult::KitchenClosing;
	}
	if (!Inv || HeroSeatIx < 0)
	{
		return EOrderResult::NotSeated;
	}
	int32 Total = 0;
	for (const TPair<FName, int32>& I : Items)
	{
		const FLyudRestDishDef* D = FindDish(I.Key);
		if (!D || I.Value <= 0)
		{
			return EOrderResult::Unknown;
		}
		Total += PriceToday(*D) * I.Value;
	}
	if (Total <= 0)
	{
		return EOrderResult::Unknown;
	}
	if (!Inv->Spend(Total))
	{
		Toast(LyudText::Get(TEXT("ui.rest.no_money")));
		return EOrderResult::NoMoney;
	}
	Tab += Total;
	const float Now = GetWorld()->GetTimeSeconds();
	FString Log;
	for (const TPair<FName, int32>& I : Items)
	{
		const FLyudRestDishDef* D = FindDish(I.Key);
		for (int32 K = 0; K < I.Value; ++K)
		{
			// a second of the same dish comes a little after the first
			AddOrder(I.Key, HeroSeatIx, Now + D->Cook * WaitScale + K * 4.f * WaitScale, false).Paid = PriceToday(*D);
		}
		Log += FString::Printf(TEXT(" %s x%d (%.0f s)"), *I.Key.ToString(), I.Value, D->Cook * WaitScale);
	}
	bBillAsked = bBillServed = false;
	if (BillFolder)
	{
		BillFolder->Destroy();
		BillFolder = nullptr;
	}
	Toast(LyudText::Get(TEXT("ui.rest.order_toast"), { FStringFormatArg(Total) }));
	UE_LOG(LogTemp, Display, TEXT("RESTTEST order:%s paid %d, money left %d"), *Log, Total, Inv->Rubles);
	return EOrderResult::Ok;
}

void ALyudRestaurant::OnGameSaving_Implementation()
{
	SavedRefund = 0;
	for (const FOrder& O : Orders)
	{
		// what was paid for it (the catch of the day is cheaper than its list price), nothing for the free bread and water
		if (!O.bGuest && O.State != FOrder::EState::Served)
		{
			SavedRefund += O.Paid;
		}
	}
}

void ALyudRestaurant::OnGameLoaded_Implementation()
{
	// the world went back to the save: this visit's table, orders and guests don't belong to it
	StandHero();
	for (FProp& Pr : Props)
	{
		GuestLeft(Pr, false);
	}
	Props.Reset();
	ResetHeroTable();
	for (FOrder& O : Orders)
	{
		if (ALyudRestDish* D = O.Dish.Get())
		{
			D->Destroy();
		}
	}
	Orders.Reset();
	ReleaseWaiter();
	// the save's objects (the pocket's roubles, this actor's SavedRefund) are read after this call: refund on the next tick
	RefundNextTick();
	// the fishing's counts come back a tick later too: don't take them for new sales
	FishSettle = 2;
	FishSeenDay = -2;
}

void ALyudRestaurant::RefundNextTick()
{
	TWeakObjectPtr<ALyudRestaurant> Self(this);
	GetWorldTimerManager().SetTimerForNextTick([Self]()
	{
		ALyudRestaurant* R = Self.Get();
		if (!R || R->SavedRefund <= 0)
		{
			return;
		}
		if (ULyudInventory* Inv = ULyudInventory::Get(R))
		{
			Inv->Earn(R->SavedRefund);
		}
		R->Toast(LyudText::Get(TEXT("ui.rest.refund"), { FStringFormatArg(R->SavedRefund) }));
		UE_LOG(LogTemp, Display, TEXT("RESTTEST refund on load: %d"), R->SavedRefund);
		R->SavedRefund = 0;
	});
}

void ALyudRestaurant::AskBill()
{
	if (Tab <= 0 || bBillAsked)
	{
		return;
	}
	bBillAsked = true;
	UE_LOG(LogTemp, Display, TEXT("RESTTEST bill asked (tab %d)"), Tab);
}

void ALyudRestaurant::ResetHeroTable()
{
	for (ALyudRestDish* D : HeroDishes)
	{
		if (D && !OnTray.Contains(D))
		{
			D->Destroy();
		}
	}
	HeroDishes.Reset();
	// the hero's orders only: the townspeople's go on being cooked and served
	for (FOrder& O : Orders)
	{
		if (!O.bGuest && O.State == FOrder::EState::Ready && O.Dish.IsValid())
		{
			O.Dish->Destroy();
		}
	}
	Orders.RemoveAll([](const FOrder& O) { return !O.bGuest && O.State != FOrder::EState::Carried; });
	if (BillFolder)
	{
		BillFolder->Destroy();
		BillFolder = nullptr;
	}
	Tab = 0;
	bBillAsked = bBillServed = false;
	bFreeBrought = false;
	HeroTableIx = -1;
}

void ALyudRestaurant::DishGone(ALyudRestDish* Dish)
{
	HeroDishes.Remove(Dish);
}

FTransform ALyudRestaurant::PlaceTransform(int32 Seat, int32 Slot, const FLyudRestDishDef& Def) const
{
	// the seat's place on the table: the main plate there, a cup or glass to the right, a second plate to the left, the basket and
	// the carafe toward the table's middle
	const FSeat& S = Seats[Seat];
	const FTable& T = Tables[S.Table];
	const float A = FMath::DegreesToRadians(S.Yaw);
	const FVector F(FMath::Cos(A), FMath::Sin(A), 0.f);
	const FVector R(FMath::Sin(A), -FMath::Cos(A), 0.f);
	FVector P = S.Place;
	const FVector Mid(T.At.X, T.At.Y, S.Place.Z);
	switch (Slot)
	{
	case 0: break;
	case 1: P += R * 0.21f + F * 0.12f; break;
	case 2: P += -R * 0.22f + F * 0.04f; break;
	case 3: P = FMath::Lerp(S.Place, Mid, 0.75f) - R * 0.12f; break;
	case 4: P = FMath::Lerp(S.Place, Mid, 0.75f) + R * 0.14f; break;
	default: P += R * 0.08f * (Slot - 4) + F * 0.2f; break;
	}
	return FTransform(YawToWorld(S.Yaw + 90.f), ToWorld(P));
}

ALyudRestDish* ALyudRestaurant::SpawnDish(FName Item, const FTransform& At, float Left)
{
	const FLyudRestDishDef* D = FindDish(Item);
	if (!D)
	{
		return nullptr;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.ObjectFlags |= RF_Transient;
	ALyudRestDish* Dish = GetWorld()->SpawnActor<ALyudRestDish>(ALyudRestDish::StaticClass(), At, P);
	if (Dish)
	{
		Dish->Init(this, *D, Left);
		FTransform Rest = At;
		Rest.AddToTranslation(FVector(0.f, 0.f, -Dish->BottomZ() + D->SaucerZ));
		Dish->SetRest(Rest);
	}
	return Dish;
}

bool ALyudRestaurant::FinishDish(FName Id)
{
	for (ALyudRestDish* D : HeroDishes)
	{
		if (D && !D->IsEmpty() && !D->IsEating() && (Id.IsNone() || D->GetDef().Id == Id))
		{
			D->SetLeft(0.f);
			UE_LOG(LogTemp, Display, TEXT("RESTTEST finished %s"), *D->GetDef().Id.ToString());
			return true;
		}
	}
	return false;
}

void ALyudRestaurant::LookAtLocal(const FVector& M)
{
	const AHeroCharacter* H = LyudRest::Hero(GetWorld());
	APlayerController* PC = LyudRest::Controller(GetWorld());
	if (H && PC)
	{
		PC->SetControlRotation((ToWorld(M) - H->EyeLocation()).Rotation());
	}
}

bool ALyudRestaurant::EatNearest(FName Id)
{
	const AHeroCharacter* H = LyudRest::Hero(GetWorld());
	const APlayerController* PC = LyudRest::Controller(GetWorld());
	if (!H || !PC)
	{
		return false;
	}
	ALyudRestDish* Best = nullptr;
	float BestDot = -2.f;
	for (ALyudRestDish* D : HeroDishes)
	{
		if (D && !D->IsEmpty() && !OnTray.Contains(D) && (Id.IsNone() || D->GetDef().Id == Id))
		{
			const float Dot = FVector::DotProduct(PC->GetControlRotation().Vector(), (D->GetActorLocation() - H->EyeLocation()).GetSafeNormal());
			if (Dot > BestDot)
			{
				BestDot = Dot;
				Best = D;
			}
		}
	}
	return Best && Best->StartEating();
}

void ALyudRestaurant::TickHero(float DeltaTime)
{
	UWorld* World = GetWorld();
	AHeroCharacter* H = LyudRest::Hero(World);
	APlayerController* PC = LyudRest::Controller(World);
	if (!H || !PC)
	{
		return;
	}
	// the seat let go of him another way (Space, a step)
	if (HeroSeatIx >= 0 && !H->IsSeated())
	{
		HeroSeatIx = -1;
		CloseMenu();
	}
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	ULyudBiteSubsystem* Bite = ULyudBiteSubsystem::Get(World);
	const bool bEating = Bite && Bite->IsEating();
	const bool bInside = IsHeroInside();

	// away from his table long enough: it is laid again for the next guest
	if (HeroTableIx >= 0 && HeroSeatIx < 0)
	{
		const FTable& T = Tables[HeroTableIx];
		AwayFor = FVector::Dist2D(H->GetActorLocation(), ToWorld(T.At)) > 400.f ? AwayFor + DeltaTime : 0.f;
		if (AwayFor > TableResetS && PendingCount() == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("RESTTEST table %s laid again"), *T.Id.ToString());
			ResetHeroTable();
		}
	}

	if (Menu_.IsValid())
	{
		// Esc (the game menu opens over everything: the card goes down with it), Tab
		const ULyudGameMenuSubsystem* GameMenu = World->GetSubsystem<ULyudGameMenuSubsystem>();
		if (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::Tab) || (GameMenu && GameMenu->IsOpen()) || HeroSeatIx < 0)
		{
			CloseMenu();
		}
		return;
	}
	if (UI && UI->IsOpen())
	{
		SetPrompt(FString());
		return;
	}
	if (HeroSeatIx < 0)
	{
		// standing inside by a free chair of a free table: E sits down
		const int32 Seat = bInside && !H->IsSeated() && !bEating ? SeatNearHero(95.f) : -1;
		SetPrompt(Seat >= 0 ? TEXT("ui.rest.sit_prompt") : FString());
		if (Seat >= 0 && PC->WasInputKeyJustPressed(EKeys::E))
		{
			SitHero(Seat);
		}
		return;
	}
	if (bEating)
	{
		SetPrompt(FString());
		return;
	}
	// seated: what is looked at on the table (the menu card, a dish), else getting up
	const FVector Eye = H->EyeLocation();
	const FVector Look = PC->GetControlRotation().Vector();
	auto Looked = [&](const FVector& At, float MinDot) { return FVector::DotProduct(Look, (At - Eye).GetSafeNormal()) > MinDot; };
	ALyudRestDish* Dish = nullptr;
	float BestDot = 0.93f;
	for (ALyudRestDish* D : HeroDishes)
	{
		if (D && !D->IsEmpty() && !OnTray.Contains(D) && FVector::Dist(D->GetActorLocation(), Eye) < 160.f)
		{
			const float Dot = FVector::DotProduct(Look, (D->GetActorLocation() + FVector(0, 0, 3) - Eye).GetSafeNormal());
			if (Dot > BestDot)
			{
				BestDot = Dot;
				Dish = D;
			}
		}
	}
	const AStaticMeshActor* Card = Tables[HeroTableIx].MenuCard.Get();
	const bool bCard = !Dish && Card && Looked(Card->GetActorLocation(), 0.94f);
	const bool bBill = !Dish && !bCard && BillFolder && Looked(BillFolder->GetActorLocation(), 0.94f);
	if (Dish)
	{
		// the line is built only when another dish comes under the look (not its name and arguments every frame)
		if (PromptDish.Get() != Dish)
		{
			const FLyudRestDishDef& D = Dish->GetDef();
			SetPrompt(D.IsCup() ? TEXT("ui.rest.drink_prompt") : TEXT("ui.rest.eat_prompt"), { FStringFormatArg(ULyudInventory::ItemName(D.Id).ToString()) });
			PromptDish = Dish;
		}
		if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			SetPrompt(FString());
			Dish->StartEating();
		}
	}
	else if (bCard)
	{
		SetPrompt(TEXT("ui.rest.menu_prompt"));
		if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			OpenMenu();
		}
	}
	else if (bBill)
	{
		if (PromptTab != Tab)
		{
			SetPrompt(TEXT("ui.rest.bill_prompt"), { FStringFormatArg(Tab) });
			PromptTab = Tab;
		}
	}
	else
	{
		SetPrompt(TEXT("ui.rest.stand_prompt"));
	}
	if (PC->WasInputKeyJustPressed(EKeys::E))
	{
		StandHero();
	}
}

void ALyudRestaurant::TickKnife()
{
	// the steak: a knife in the left hand on the plate's near side, sawing while the fork is in the meat
	AHeroCharacter* H = LyudRest::Hero(GetWorld());
	const ULyudBiteSubsystem* Bite = ULyudBiteSubsystem::Get(this);
	const ALyudRestDish* Dish = Bite ? Cast<ALyudRestDish>(Bite->MealDish()) : nullptr;
	UHeroAnimInstance* Anim = H ? Cast<UHeroAnimInstance>(H->GetMesh()->GetAnimInstance()) : nullptr;
	const bool bKnife = Dish && Dish->GetDef().Utensil == TEXT("knife_fork") && Anim;
	if (!bKnife)
	{
		if (Knife)
		{
			Knife->Destroy();
			Knife = nullptr;
		}
		if (Anim && Anim->Table.Weight > 0.f)
		{
			Anim->Table = FBikeRiderPose();
		}
		return;
	}
	if (!Knife)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		P.ObjectFlags |= RF_Transient;
		Knife = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Dish->GetActorTransform(), P);
		Knife->SetMobility(EComponentMobility::Movable);
		UStaticMeshComponent* C = Knife->GetStaticMeshComponent();
		C->SetStaticMesh(RestMesh(TEXT("SM_RT_Knife")));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
	}
	// in the yaw frame of the eyes: the knife comes from the left, its tip toward the plate's middle, a little down
	const APlayerController* PC = LyudRest::Controller(GetWorld());
	const FRotator Yaw(0.f, PC ? PC->GetControlRotation().Yaw : H->GetActorRotation().Yaw, 0.f);
	const FVector Fwd = Yaw.Vector(), Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
	const float Saw = Bite->ScoopPhase() >= 0.f ? FMath::Sin(Bite->ScoopPhase() * PI * 4.f) * 3.f : 0.f;
	const FVector Plate = Dish->GetActorLocation();
	const FVector TipDir = (Fwd * 0.55f + Right * 0.75f + FVector(0, 0, -0.38f)).GetSafeNormal();
	const FVector Tip = Plate + FVector(0.f, 0.f, 2.5f) - Right * 2.f + TipDir * Saw;
	const FVector Grip = Tip - TipDir * 15.f;
	Knife->SetActorLocationAndRotation(Grip, FRotationMatrix::MakeFromXZ(TipDir, FVector::UpVector).ToQuat());
	// the left hand on the handle: index finger along its back, palm down onto it
	const FTransform CS = H->GetMesh()->GetComponentTransform();
	FBikeRiderPose Pose;
	Pose.Weight = 1.f;
	FBikeRiderPose::FHand& L = Pose.Hands[0];
	L.Weight = 1.f;
	L.Grip = CS.InverseTransformPosition(Grip - TipDir * 4.f + FVector(0, 0, 1.f));
	L.Axis = CS.InverseTransformVectorNoScale(TipDir);
	L.Radius = 0.9f;
	L.PalmNormal = CS.InverseTransformVectorNoScale(FVector(0, 0, -1.f));
	L.FingerDir = CS.InverseTransformVectorNoScale(TipDir);
	L.Close = 0.85f;
	Anim->Table = Pose;
}

void ALyudRestaurant::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bLayout || LyudRest::CVarRestaurant.GetValueOnGameThread() == 0)
	{
		return;
	}
	StaffEvery -= DeltaTime;
	if (StaffEvery <= 0.f)
	{
		StaffEvery = 1.f;
		RefreshStaff();
	}
	StateEvery -= DeltaTime;
	if (StateEvery <= 0.f)
	{
		StateEvery = 1.f;
		UpdateState();
	}
	// the camera near enough to see anything of it (bodies, hands, guests are only worked on then)
	FVector Cam = GetActorLocation();
	if (const APlayerController* PC0 = UGameplayStatics::GetPlayerController(this, 0))
	{
		FRotator R;
		PC0->GetPlayerViewPoint(Cam, R);
	}
	bNear = FVector::Dist(Cam, GetActorLocation()) < 8000.f;
	// far and nothing in hand (no visit, no order, the waiter at rest): the hours, the lights and the locks need no more than
	// twice a second. Everything below goes by DeltaTime or the world's seconds, so a long step is safe.
	const bool bQuiet = !bNear && HeroSeatIx < 0 && HeroTableIx < 0 && !Menu_.IsValid() && !Knife && Job == EJob::None
		&& (Waiter == EWaiter::Off || Waiter == EWaiter::Idle) && PendingCount(false) == 0;
	const float WantInterval = bQuiet ? 0.5f : 0.f;
	if (GetActorTickInterval() != WantInterval)
	{
		SetActorTickInterval(WantInterval);
	}
	PropsEvery -= DeltaTime;
	if (PropsEvery <= 0.f && bNear)
	{
		PropsEvery = 2.f;
		TickProps();
	}
	// the kitchen: what is cooked is ready at the pass
	const float Now = GetWorld()->GetTimeSeconds();
	for (FOrder& O : Orders)
	{
		if (O.State == FOrder::EState::Cooking && Now >= O.Due)
		{
			O.State = FOrder::EState::Ready;
			UE_LOG(LogTemp, Display, TEXT("RESTTEST ready at the pass: %s"), *O.Item.ToString());
		}
	}
	for (FOrder& O : Orders)
	{
		if (O.State == FOrder::EState::Ready && !O.Dish.IsValid())
		{
			PutOnPass(O);
		}
	}
	TickKitchen(DeltaTime);
	TickHero(DeltaTime);
	if (bNear)
	{
		TickGuests(DeltaTime);
	}
	TickWaiter(DeltaTime);
	TickKnife();
}

// ---- console -------------------------------------------------------------------------------------------------------------------------

namespace LyudRest
{
	ALyudRestaurant* Get(UWorld* World)
	{
		ALyudRestaurant* R = ALyudRestaurant::Find(World);
		if (!R)
		{
			UE_LOG(LogTemp, Warning, TEXT("RESTTEST no restaurant in this world"));
		}
		return R;
	}

	FAutoConsoleCommandWithWorld GState(TEXT("lyud.Restaurant.State"), TEXT("Prints the restaurant's state"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ALyudRestaurant* R = Get(World)) { UE_LOG(LogTemp, Display, TEXT("RESTTEST state: %s"), *R->StateReport()); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GOpen(TEXT("lyud.Restaurant.Open"), TEXT("lyud.Restaurant.Open 1|0|-1: forced open, closed, by the staff/hours"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ALyudRestaurant* R = Get(World)) { R->ForceOpen = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1; }
		}));
	FAutoConsoleCommandWithWorldAndArgs GSit(TEXT("lyud.Restaurant.Sit"), TEXT("lyud.Restaurant.Sit [seat]: sits the hero at a free hero seat"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ALyudRestaurant* R = Get(World))
			{
				const bool bOk = R->SitHero(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : -1);
				UE_LOG(LogTemp, Display, TEXT("RESTTEST sit command %s"), bOk ? TEXT("ok") : TEXT("refused"));
			}
		}));
	FAutoConsoleCommandWithWorld GStand(TEXT("lyud.Restaurant.Stand"), TEXT("Gets the hero up from his chair"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (ALyudRestaurant* R = Get(World)) { R->StandHero(); } }));
	FAutoConsoleCommandWithWorldAndArgs GMenu(TEXT("lyud.Restaurant.Menu"), TEXT("lyud.Restaurant.Menu [0]: opens (closes) the menu card"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ALyudRestaurant* R = Get(World)) { (Args.Num() > 0 && Args[0] == TEXT("0")) ? R->CloseMenu() : R->OpenMenu(); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GOrder(TEXT("lyud.Restaurant.Order"), TEXT("lyud.Restaurant.Order <id> [id..]: orders these dishes (the hero must be seated)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ALyudRestaurant* R = Get(World))
			{
				TMap<FName, int32> Items;
				for (const FString& A : Args)
				{
					Items.FindOrAdd(FName(*A))++;
				}
				const bool bOk = R->PlaceOrder(Items) == ALyudRestaurant::EOrderResult::Ok;
				UE_LOG(LogTemp, Display, TEXT("RESTTEST order command %s"), bOk ? TEXT("ok") : TEXT("refused"));
			}
		}));
	FAutoConsoleCommandWithWorld GBill(TEXT("lyud.Restaurant.Bill"), TEXT("Asks for the bill"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (ALyudRestaurant* R = Get(World)) { R->AskBill(); } }));
	FAutoConsoleCommandWithWorldAndArgs GEat(TEXT("lyud.Restaurant.Eat"), TEXT("lyud.Restaurant.Eat [id]: starts eating that dish (or the one nearest the look)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ALyudRestaurant* R = Get(World))
			{
				const bool bOk = R->EatNearest(Args.Num() > 0 ? FName(*Args[0]) : NAME_None);
				UE_LOG(LogTemp, Display, TEXT("RESTTEST eat command %s"), bOk ? TEXT("ok") : TEXT("refused"));
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GFinish(TEXT("lyud.Restaurant.Finish"), TEXT("lyud.Restaurant.Finish [id]: eats that dish up at once"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ALyudRestaurant* R = Get(World)) { R->FinishDish(Args.Num() > 0 ? FName(*Args[0]) : NAME_None); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GLook(TEXT("lyud.Restaurant.Look"), TEXT("lyud.Restaurant.Look x y z: the hero looks at that point of the building's frame (m)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ALyudRestaurant* R = Get(World); R && Args.Num() >= 3)
			{
				R->LookAtLocal(FVector(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2])));
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GWait(TEXT("lyud.Restaurant.WaitScale"), TEXT("lyud.Restaurant.WaitScale <x>: the cooking and serving waits times x"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ALyudRestaurant* R = Get(World)) { R->WaitScale = Args.Num() > 0 ? FMath::Max(0.01f, FCString::Atof(*Args[0])) : 1.f; }
		}));
}
