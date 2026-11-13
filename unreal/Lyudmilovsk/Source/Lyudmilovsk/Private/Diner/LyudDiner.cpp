#include "Diner/LyudDiner.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Doors/LyudDoor.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Food/LyudBite.h"
#include "Food/LyudFood.h"
#include "Food/LyudHandUse.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
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
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sky/LyudSky.h"
#include "Sound/SoundBase.h"
#include "UI/LyudGameScreen.h"

namespace LyudDiner
{
	void ApplyShown(IConsoleVariable* Var);

	/** The diner shown (1) or hidden with no collision (0): the benchmark's A/B (tools/bench_systems.sh nodiner). */
	TAutoConsoleVariable<int32> CVarDiner(TEXT("lyud.Diner"), 1, TEXT("The diner by the lower station: shown (1) or hidden with no collision (0)."),
		FConsoleVariableDelegate::CreateStatic(&ApplyShown), ECVF_Default);

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
				if (It->ActorHasTag(TEXT("Diner")) || It->IsA<ALyudDinerDish>())
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

	const APlayerController* Controller(const UWorld* World)
	{
		const AHeroCharacter* H = Hero(World);
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

	/** The vessels (design/diner/tableware.json, cm): radius, half height, where the fill starts over the bottom. */
	struct FVesselSize { float Radius, HalfHeight, FillZ; bool bCentred; };
	FVesselSize Vessel(FName Dish)
	{
		if (Dish == TEXT("mug")) { return { 4.25f, 4.75f, 1.0f, true }; }
		if (Dish == TEXT("tea_glass")) { return { 4.2f, 5.25f, 1.3f, true }; }
		if (Dish == TEXT("bowl")) { return { 8.0f, 3.25f, 1.2f, false }; }
		return { 12.f, 0.7f, 1.4f, false };       // the plate: the food stands on its top (1.4 cm)
	}

	USoundBase* Sound(const TCHAR* Name)
	{
		const FString Pkg = FString::Printf(TEXT("/Game/Audio/Food/%s"), Name);
		return FPackageName::DoesPackageExist(Pkg) ? LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("%s.%s"), *Pkg, Name)) : nullptr;
	}

	/** ALyudDiner::DinerMesh's lookups (weak: the dishes hold their meshes) and the names found to have no mesh (cleared at
	 *  each play start: a mesh may have been imported since). */
	TMap<FName, TWeakObjectPtr<UStaticMesh>> MeshCache;
	TSet<FName> MeshMissing;
}

// ---- dish definitions (Content/Food/food.json, "plated": true) ------------------------------------------------------------------

bool ALyudDiner::FindDishDef(FName Item, FLyudDinerDishDef& Out)
{
	static TMap<FName, FLyudDinerDishDef> Defs;
	static bool bRead = false;
	if (!bRead)
	{
		bRead = true;
		const TSharedPtr<FJsonObject> Root = LyudDiner::ReadJson(TEXT("Food/food.json"));
		const TSharedPtr<FJsonObject>* Foods = nullptr;
		if (Root.IsValid() && Root->TryGetObjectField(TEXT("foods"), Foods))
		{
			for (const auto& It : (*Foods)->Values)
			{
				const TSharedPtr<FJsonObject> O = It.Value->AsObject();
				bool bPlated = false;
				if (!O.IsValid() || !O->TryGetBoolField(TEXT("plated"), bPlated) || !bPlated)
				{
					continue;
				}
				FLyudDinerDishDef D;
				D.Item = FName(*It.Key);
				FString S;
				if (O->TryGetStringField(TEXT("utensil"), S)) { D.Utensil = FName(*S); }
				if (O->TryGetStringField(TEXT("dish"), S)) { D.Dish = FName(*S); }
				if (O->TryGetStringField(TEXT("food"), S)) { D.Food = FName(*S); }
				if (O->TryGetStringField(TEXT("fill"), S)) { D.Fill = FName(*S); }
				if (O->TryGetStringField(TEXT("fill_tint"), S)) { D.FillTint = LyudDiner::Hex(S); }
				const TArray<TSharedPtr<FJsonValue>>* Load = nullptr;
				if (O->TryGetArrayField(TEXT("load"), Load) && Load->Num() > 0)
				{
					D.LoadSmall = FName(*(*Load)[0]->AsString());
					D.LoadBig = FName(*(*Load)[Load->Num() - 1]->AsString());
				}
				const FVector B = LyudDiner::Vec(O, TEXT("bites"));
				if (B.X > 0.f) { D.SmallBite = B.X; D.BigBite = B.Y; }
				Defs.Add(D.Item, D);
			}
		}
	}
	if (const FLyudDinerDishDef* D = Defs.Find(Item))
	{
		Out = *D;
		return true;
	}
	return false;
}

UStaticMesh* ALyudDiner::DinerMesh(FName Name)
{
	if (Name.IsNone())
	{
		return nullptr;
	}
	// asked several times for every dish served and every meal begun: the loaded mesh is remembered (weak: the dishes' components
	// hold it), and so is a name with no mesh; the disk is only asked the first time
	const TWeakObjectPtr<UStaticMesh>* Known = LyudDiner::MeshCache.Find(Name);
	if (Known && Known->IsValid())
	{
		return Known->Get();
	}
	if (LyudDiner::MeshMissing.Contains(Name))
	{
		return nullptr;
	}
	const bool bSeenBefore = Known != nullptr;     // loaded once, collected since: it exists
	const FString N = TEXT("SM_Diner_") + Name.ToString();
	const FString Pkg = TEXT("/Game/Diner/Meshes/") + N;
	UStaticMesh* M = (bSeenBefore || FPackageName::DoesPackageExist(Pkg)) ? LoadObject<UStaticMesh>(nullptr, *(Pkg + TEXT(".") + N)) : nullptr;
	if (M)
	{
		LyudDiner::MeshCache.Add(Name, M);
	}
	else if (!bSeenBefore)
	{
		LyudDiner::MeshMissing.Add(Name);
	}
	return M;
}

// ---- the dish ------------------------------------------------------------------------------------------------------------------

ALyudDinerDish::ALyudDinerDish()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);
	auto Part = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(true);
		// small and moving when held: out of Lumen (docs/benchmark.md)
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		return C;
	};
	Vessel = Part(TEXT("Vessel"));
	VesselGlass = Part(TEXT("VesselGlass"));
	VesselGlass->SetCastShadow(false);
	Food = Part(TEXT("Food"));
	Fill = Part(TEXT("Fill"));
	Fill->SetCastShadow(false);
}

float ALyudDinerDish::HalfHeight() const { return LyudDiner::Vessel(Def.Dish).HalfHeight; }
float ALyudDinerDish::Radius() const { return LyudDiner::Vessel(Def.Dish).Radius; }

void ALyudDinerDish::Init(ALyudDiner* InDiner, const FLyudDinerDishDef& InDef, float InLeft, const FTransform& InRest)
{
	Diner = InDiner;
	Def = InDef;
	Rest = InRest;
	const LyudDiner::FVesselSize V = LyudDiner::Vessel(Def.Dish);
	// the vessel's own origin is its bottom: cups hang from their middle (the root), plates and bowls stand on it
	const float Bottom = V.bCentred ? -V.HalfHeight : 0.f;
	const FName VesselMesh = Def.Dish == TEXT("mug") ? FName(TEXT("Mug")) : Def.Dish == TEXT("tea_glass") ? FName(TEXT("TeaGlass"))
		: Def.Dish == TEXT("bowl") ? FName(TEXT("Bowl")) : FName(TEXT("Plate"));
	Vessel->SetStaticMesh(ALyudDiner::DinerMesh(VesselMesh));
	Vessel->SetRelativeLocation(FVector(0.f, 0.f, Bottom));
	if (UStaticMesh* G = ALyudDiner::DinerMesh(FName(*(VesselMesh.ToString() + TEXT("_Glass")))))
	{
		VesselGlass->SetStaticMesh(G);
		VesselGlass->SetRelativeLocation(FVector(0.f, 0.f, Bottom));
	}
	if (Def.Dish == TEXT("bowl"))
	{
		// soup comes in a bowl on a plate
		VesselGlass->SetStaticMesh(ALyudDiner::DinerMesh(TEXT("Plate")));
		Vessel->SetRelativeLocation(FVector(0.f, 0.f, 1.45f));
	}
	FoodZ = Bottom + V.FillZ;
	if (UStaticMesh* F = ALyudDiner::DinerMesh(Def.Food))
	{
		Food->SetStaticMesh(F);
		Food->SetRelativeLocation(FVector(0.f, 0.f, FoodZ));
	}
	if (UStaticMesh* F = ALyudDiner::DinerMesh(Def.Fill))
	{
		Fill->SetStaticMesh(F);
		Fill->SetRelativeLocation(FVector(0.f, 0.f, FoodZ + (Def.Dish == TEXT("bowl") ? 1.45f : 0.f)));
		if (Def.FillTint.A > 0.f)
		{
			FillMID = Fill->CreateDynamicMaterialInstance(0);
			if (FillMID)
			{
				FillMID->SetVectorParameterValue(TEXT("Color"), Def.FillTint);
			}
		}
	}
	SetActorTransform(Rest);
	SetLeft(InLeft);
}

void ALyudDinerDish::SetLeft(float InLeft)
{
	Left = FMath::Clamp(InLeft, 0.f, 1.f);
	if (Food->GetStaticMesh())
	{
		// the food goes down as it is eaten: flatter and smaller, gone with the last bite
		const float S = FMath::Lerp(0.45f, 1.f, FMath::Sqrt(Left));
		Food->SetRelativeScale3D(FVector(S, S, FMath::Lerp(0.5f, 1.f, Left)));
		Food->SetVisibility(Left > 0.f);
	}
	SetPoured(1.f);
}

void ALyudDinerDish::SetPoured(float Share)
{
	if (Fill->GetStaticMesh())
	{
		const float L = Left * FMath::Clamp(Share, 0.f, 1.f);
		Fill->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Max(L, 0.01f)));
		Fill->SetVisibility(L > 0.02f);
	}
}

void ALyudDinerDish::ShowPrompt(bool bOn)
{
	if (bOn == bPrompted)
	{
		return;
	}
	bPrompted = bOn;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, bOn ? LyudText::Get(IsCup() ? TEXT("ui.diner.drink_prompt") : TEXT("ui.diner.eat_prompt"),
			{ FStringFormatArg(ULyudInventory::ItemName(Def.Item).ToString()) }) : FText::GetEmpty());
	}
}

bool ALyudDinerDish::StartEating()
{
	if (Left <= 0.f)
	{
		return false;
	}
	ShowPrompt(false);
	if (Def.Utensil == TEXT("hand"))
	{
		// picked up and bitten in the hand (the food thread's hand system): from the pocket into the hand, the plate stays
		ULyudInventory* Inv = ULyudInventory::Get(this);
		ULyudFoodSubsystem* FoodSys = ULyudFoodSubsystem::Get(this);
		if (!Inv || !FoodSys)
		{
			return false;
		}
		Inv->Add(Def.Item);
		if (Left < 1.f)
		{
			Inv->Started.Add(Def.Item, Left);
		}
		if (!FoodSys->HoldFromPocket(Def.Item))
		{
			const FLyudItemDef* ID = ULyudInventory::FindDef(Def.Item);
			UE_LOG(LogTemp, Warning, TEXT("DINERTEST hand: %s not taken (count %d, kind %d, hand %d)"), *Def.Item.ToString(), Inv->Count(Def.Item),
				ID ? (int32)ID->Kind : -1, ULyudHandUse::Get(this) ? 1 : 0);
			Inv->Remove(Def.Item);
			return false;
		}
		Left = 0.f;
		Food->SetVisibility(false);
		return true;
	}
	ULyudBiteSubsystem* Bite = ULyudBiteSubsystem::Get(this);
	if (!Bite)
	{
		return false;
	}
	FLyudMeal M;
	M.Item = Def.Item;
	M.Dish = this;
	M.Amount = Left;
	M.SmallBite = Def.SmallBite;
	M.BigBite = Def.BigBite;
	const LyudDiner::FVesselSize V = LyudDiner::Vessel(Def.Dish);
	if (IsCup())
	{
		M.Utensil = ELyudUtensil::Cup;
		M.DishRadius = V.Radius;
		M.DishHalfHeight = V.HalfHeight;
	}
	else
	{
		M.Utensil = Def.Utensil == TEXT("spoon") ? ELyudUtensil::Spoon : ELyudUtensil::Fork;
		M.UtensilMesh = ALyudDiner::DinerMesh(M.Utensil == ELyudUtensil::Spoon ? TEXT("Spoon") : TEXT("Fork"));
		M.LoadMesh = ALyudDiner::DinerMesh(Def.LoadSmall);
		M.LoadMeshBig = ALyudDiner::DinerMesh(Def.LoadBig);
	}
	TWeakObjectPtr<ALyudDinerDish> Self(this);
	M.OnBite = [Self](float L, ELyudBiteSize) { if (Self.IsValid()) { Self->SetLeft(L); } };
	M.OnEnd = [Self](float L) { if (Self.IsValid()) { Self->OnMealEnd(L); } };
	bEating = Bite->BeginMeal(M);
	return bEating;
}

void ALyudDinerDish::OnMealEnd(float InLeft)
{
	bEating = false;
	SetLeft(InLeft);
	// a cup goes back where it stood, empty or not (the hand let go of it)
	SetActorTransform(Rest);
	UE_LOG(LogTemp, Display, TEXT("DINERTEST meal %s ended, left %.2f"), *Def.Item.ToString(), InLeft);
}

void ALyudDinerDish::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Left <= 0.f && !bEating)
	{
		ShowPrompt(false);
		EmptyFor += DeltaTime;
		if (EmptyFor > 45.f)
		{
			// cleared away by the waitress
			if (ALyudDiner* D = Diner.Get())
			{
				D->DishGone(this);
			}
			Destroy();
		}
		return;
	}
	const UWorld* World = GetWorld();
	const AHeroCharacter* Hero = LyudDiner::Hero(World);
	const APlayerController* PC = LyudDiner::Controller(World);
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	const ULyudBiteSubsystem* Bite = ULyudBiteSubsystem::Get(this);
	// left unfinished and walked away from: cleared away too after a couple of minutes. Only an empty dish was ever cleared, so
	// each one left with food on it kept its serving place (and its tick) until the level ended, and with all the places
	// taken nothing more could be served
	if (!bEating && bReady && Hero && FVector::DistSquared(Hero->GetActorLocation(), GetActorLocation()) > FMath::Square(2000.f))
	{
		LeftAloneFor += DeltaTime;
		if (LeftAloneFor > 120.f)
		{
			ShowPrompt(false);
			if (ALyudDiner* D = Diner.Get())
			{
				D->DishGone(this);
			}
			Destroy();
			return;
		}
	}
	else
	{
		LeftAloneFor = 0.f;
	}
	if (bEating || !bReady || !Hero || !PC || (UI && UI->IsOpen()) || (Bite && Bite->IsEating()))
	{
		ShowPrompt(false);
		return;
	}
	const FVector Eye = Hero->EyeLocation();
	const FVector At = GetActorLocation() + FVector(0.f, 0.f, 3.f);
	const float Dist = FVector::Dist(Eye, At);
	const FVector Look = PC->GetControlRotation().Vector();
	// close by and looked at (a cone that widens a little as it comes nearer)
	const bool bLooked = Dist < 175.f && FVector::DotProduct(Look, (At - Eye) / FMath::Max(Dist, 1.f)) > FMath::Lerp(0.95f, 0.88f, FMath::Clamp((175.f - Dist) / 120.f, 0.f, 1.f));
	ShowPrompt(bLooked);
	if (bLooked && PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		StartEating();
	}
}

// ---- the diner --------------------------------------------------------------------------------------------------------------------

ALyudDiner::ALyudDiner()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ALyudDiner* ALyudDiner::Find(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ALyudDiner> It(World);
	return It ? *It : nullptr;
}

FVector ALyudDiner::ToWorld(const FVector& M) const
{
	// the car's frame went through glTF: Blender (x, y, z) m -> Unreal (x, -y, z) cm in the mesh
	return GetActorTransform().TransformPosition(FVector(M.X * 100.f, -M.Y * 100.f, M.Z * 100.f));
}

FVector ALyudDiner::ToLocal(const FVector& W) const
{
	const FVector L = GetActorTransform().InverseTransformPosition(W);
	return FVector(L.X / 100.f, -L.Y / 100.f, L.Z / 100.f);
}

FRotator ALyudDiner::YawToWorld(float LocalYawDeg) const
{
	return FRotator(0.f, GetActorRotation().Yaw - LocalYawDeg, 0.f);
}

void ALyudDiner::BeginPlay()
{
	Super::BeginPlay();
	LyudDiner::MeshMissing.Reset();
	LoadLayout();
	FindParts();
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->OnShopBought.AddUObject(this, &ALyudDiner::OnBought);
	}
	Stream = NewObject<UStaticMeshComponent>(this, TEXT("CoffeeStream"));
	Stream->SetupAttachment(RootComponent);
	Stream->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	if (UMaterialInterface* Coffee = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Diner/Materials/MI_DN_Coffee.MI_DN_Coffee")))
	{
		Stream->SetMaterial(0, Coffee);
	}
	Stream->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Stream->SetCastShadow(false);
	Stream->bAffectDynamicIndirectLighting = false;
	Stream->bAffectDistanceFieldLighting = false;
	Stream->SetVisibility(false);
	Stream->RegisterComponent();
	UpdateState();
	ApplyOpen(bOpen);
	UE_LOG(LogTemp, Display, TEXT("DINERTEST begin: layout %d, %d staff, %d doors, %d+%d lamps, %d serving places, %s"), bLayout ? 1 : 0, Staff.Num(),
		Doors.Num(), InsideLights.Num(), OutsideLights.Num(), Serving.Num(), bOpen ? TEXT("open") : TEXT("closed"));
}

void ALyudDiner::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->OnShopBought.RemoveAll(this);
		UI->SetPrompt(this, FText::GetEmpty());
	}
	Super::EndPlay(Reason);
}

void ALyudDiner::LoadLayout()
{
	const TSharedPtr<FJsonObject> L = LyudDiner::ReadJson(TEXT("Diner/layout.json"));
	if (!L.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Diner: Content/Diner/layout.json unreadable"));
		return;
	}
	bLayout = true;
	const TSharedPtr<FJsonObject>* Box = nullptr;
	if (L->TryGetObjectField(TEXT("box"), Box))
	{
		for (const TCHAR* K : { TEXT("inside"), TEXT("kitchen") })
		{
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if ((*Box)->TryGetArrayField(K, A) && A->Num() == 6)
			{
				InsideBox.Add(FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), 0.0));
				InsideBox.Add(FVector((*A)[2]->AsNumber(), (*A)[3]->AsNumber(), 0.0));
				InsideBox.Add(FVector((*A)[4]->AsNumber() - 0.3, (*A)[5]->AsNumber() + 0.3, 0.0));
			}
		}
	}
	const TSharedPtr<FJsonObject>* Counter = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Spots = nullptr;
	if (L->TryGetObjectField(TEXT("counter"), Counter) && (*Counter)->TryGetArrayField(TEXT("serving"), Spots))
	{
		for (const TSharedPtr<FJsonValue>& V : *Spots)
		{
			FServing S;
			S.Plate = LyudDiner::Vec(V->AsObject(), TEXT("plate"));
			S.Stool = LyudDiner::Vec(V->AsObject(), TEXT("stool"));
			Serving.Add(S);
		}
	}
	const TSharedPtr<FJsonObject>* Coffee = nullptr;
	if (L->TryGetObjectField(TEXT("coffee"), Coffee))
	{
		CoffeeTap = LyudDiner::Vec(*Coffee, TEXT("tap"));
		CoffeeCup = LyudDiner::Vec(*Coffee, TEXT("mug_under_tap"));
		CoffeeMugs = LyudDiner::Vec(*Coffee, TEXT("mugs"));
		CoffeeStand = LyudDiner::Vec(*Coffee, TEXT("stand"));
	}
	// the lamps: inside (with the opal globes) while the diner is lit, the one over the door at night too
	const TArray<TSharedPtr<FJsonValue>>* Lights = nullptr;
	if (L->TryGetArrayField(TEXT("lights"), Lights))
	{
		for (const TSharedPtr<FJsonValue>& V : *Lights)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			UPointLightComponent* P = NewObject<UPointLightComponent>(this);
			P->SetupAttachment(RootComponent);
			P->SetWorldLocation(ToWorld(LyudDiner::Vec(O, TEXT("at"))));
			P->SetIntensityUnits(ELightUnits::Lumens);
			P->SetIntensity((float)O->GetNumberField(TEXT("lumens")));
			P->SetLightColor(LyudDiner::Hex(O->GetStringField(TEXT("colour"))));
			P->SetAttenuationRadius(O->GetStringField(TEXT("kind")) == TEXT("outside") ? 700.f : 520.f);
			P->SetCastShadows(false);
			P->SetLightingChannels(true, true, false);
			P->MaxDrawDistance = 4500.f;
			P->MaxDistanceFadeRange = 500.f;
			P->SetVisibility(false);
			P->RegisterComponent();
			(O->GetStringField(TEXT("kind")) == TEXT("outside") ? OutsideLights : InsideLights).Add(P);
		}
	}
	// its waitresses: counters.json rows of the diner's activity
	const TSharedPtr<FJsonObject> C = LyudDiner::ReadJson(TEXT("PeopleData/counters.json"));
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (C.IsValid() && C->TryGetArrayField(TEXT("counters"), Rows))
	{
		for (const TSharedPtr<FJsonValue>& V : *Rows)
		{
			if (V->AsObject()->GetStringField(TEXT("activity")) == TEXT("work_diner"))
			{
				Staff.Add(V->AsObject()->GetStringField(TEXT("person")));
			}
		}
	}
}

void ALyudDiner::FindParts()
{
	// the doors (import_diner.py tags them "Diner") and the car's meshes: their lamp globes and signs get their own materials
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("Diner")) || *It == this)
		{
			continue;
		}
		if (ALyudDoorBase* D = Cast<ALyudDoorBase>(*It))
		{
			// the street and the back door are locked at night; the kitchen's stays as it is
			if (It->ActorHasTag(TEXT("DinerOuter")))
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
				if (S.StartsWith(TEXT("DN_Opal")) || S.StartsWith(TEXT("DN_Sign_")))
				{
					if (UMaterialInstanceDynamic* MID = M->CreateDynamicMaterialInstance(M->GetMaterialIndex(Slot)))
					{
						GlowMIDs.Add(MID);
					}
				}
			}
		}
	}
	ApplySigns();
}

bool ALyudDiner::IsHeroInside() const
{
	const AHeroCharacter* Hero = LyudDiner::Hero(GetWorld());
	if (!Hero)
	{
		return false;
	}
	const FVector P = ToLocal(Hero->GetActorLocation());
	for (int32 B = 0; B + 2 < InsideBox.Num(); B += 3)
	{
		if (P.X > InsideBox[B].X && P.X < InsideBox[B].Y && P.Y > InsideBox[B + 1].X && P.Y < InsideBox[B + 1].Y
			&& P.Z > InsideBox[B + 2].X && P.Z < InsideBox[B + 2].Y + 1.5f)
		{
			return true;
		}
	}
	return false;
}

void ALyudDiner::UpdateState()
{
	// open while a waitress is on her shift (going there, at work or still about the car); the last one out shuts it
	bool bStaffOn = false, bStaffIn = false;
	if (const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this))
	{
		for (const FString& Key : Staff)
		{
			FPersonInfo P;
			if (!People->GetPerson(People->FindPerson(Key), P))
			{
				continue;
			}
			if (P.Activity == TEXT("work_diner"))
			{
				bStaffOn = true;
			}
			const FVector L = ToLocal(P.Location);
			if (InsideBox.Num() >= 3 && L.X > InsideBox[0].X - 1.f && L.X < InsideBox[0].Y + 1.f && L.Y > InsideBox[1].X - 2.f && L.Y < InsideBox[4].Y + 1.f
				&& FMath::Abs(L.Z) < 2.f)
			{
				bStaffIn = true;
			}
		}
	}
	const bool bWantOpen = ForceOpen >= 0 ? ForceOpen == 1 : bStaffOn;
	if (bWantOpen != bOpen)
	{
		ApplyOpen(bWantOpen);
	}
	bLit = bOpen || bStaffIn;
	TActorIterator<ALyudSky> Sky(GetWorld());
	if (Sky)
	{
		const float Alt = Sky->SunAltitude();
		bNight = bNight ? Alt < 3.f : Alt < 0.5f;
	}
	// locked once nobody is left inside: the hero is never shut in
	const bool bWantLock = !bOpen && !bStaffIn && !IsHeroInside();
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
		UE_LOG(LogTemp, Display, TEXT("DINERTEST doors %s"), bLocked ? TEXT("locked") : TEXT("unlocked"));
	}
	ApplyLights();
}

void ALyudDiner::ApplyOpen(bool bInOpen)
{
	bOpen = bInOpen;
	UE_LOG(LogTemp, Display, TEXT("DINERTEST %s"), bOpen ? TEXT("open") : TEXT("closed"));
}

void ALyudDiner::ApplyLights()
{
	const bool bShown = LyudDiner::CVarDiner.GetValueOnGameThread() != 0;
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
		// the globes glow when lit; the signs' pictures glow a little at night while open (lit from the lamps over them)
		MID->SetScalarParameterValue(TEXT("Emissive"), bLit ? 6.f : 0.f);
		MID->SetScalarParameterValue(TEXT("Glow"), bLit && bNight ? 0.6f : 0.f);
	}
}

void ALyudDiner::ApplySigns()
{
	SignLanguage = LyudText::Language();
	for (UMaterialInstanceDynamic* MID : GlowMIDs)
	{
		// the picture slots are named after their sign (DN_Sign_<name>): the player's language's picture
		const FString Name = MID->Parent ? MID->Parent->GetName() : FString();
		const int32 At = Name.Find(TEXT("DN_Sign_"));
		if (At != INDEX_NONE)
		{
			if (UTexture2D* Tex = LyudText::LoadTexture(TEXT("Diner"), TEXT("T_Sign_") + Name.Mid(At + 8)))
			{
				MID->SetTextureParameterValue(TEXT("Image"), Tex);
			}
		}
	}
}

int32 ALyudDiner::FreeSpotNear(const FVector& WorldPos) const
{
	int32 Best = -1;
	float BestD = TNumericLimits<float>::Max();
	for (int32 I = 0; I < Serving.Num(); ++I)
	{
		const bool bTaken = Serving[I].Dish.IsValid() || Pending.ContainsByPredicate([I](const FPending& P) { return P.Spot == I; });
		const float D = FVector::DistSquared2D(ToWorld(Serving[I].Stool), WorldPos);
		if (!bTaken && D < BestD)
		{
			Best = I;
			BestD = D;
		}
	}
	return Best;
}

ALyudDinerDish* ALyudDiner::SpawnDish(FName Item, const FTransform& At, float Left)
{
	FLyudDinerDishDef Def;
	if (!FindDishDef(Item, Def))
	{
		return nullptr;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ALyudDinerDish* D = GetWorld()->SpawnActor<ALyudDinerDish>(ALyudDinerDish::StaticClass(), At, P);
	if (!D)
	{
		return nullptr;
	}
	FTransform Rest = At;
	const LyudDiner::FVesselSize V = LyudDiner::Vessel(Def.Dish);
	if (V.bCentred)
	{
		Rest.AddToTranslation(FVector(0.f, 0.f, V.HalfHeight));
	}
	D->Init(this, Def, Left, Rest);
	Dishes.Add(D);
	return D;
}

ALyudDinerDish* ALyudDiner::Serve(FName Item)
{
	const AHeroCharacter* Hero = LyudDiner::Hero(GetWorld());
	const int32 Spot = FreeSpotNear(Hero ? Hero->GetActorLocation() : GetActorLocation());
	if (Spot < 0)
	{
		return nullptr;
	}
	ALyudDinerDish* D = SpawnDish(Item, FTransform(YawToWorld(0.f), ToWorld(Serving[Spot].Plate)), 1.f);
	if (D)
	{
		Serving[Spot].Dish = D;
		if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
		{
			Screen->Toast(LyudText::Get(TEXT("ui.diner.served"), { FStringFormatArg(ULyudInventory::ItemName(Item).ToString()) }));
		}
		if (USoundBase* S = LyudDiner::Sound(TEXT("S_diner_plate_01")))
		{
			UGameplayStatics::PlaySoundAtLocation(this, S, D->GetActorLocation(), 0.6f);
		}
		UE_LOG(LogTemp, Display, TEXT("DINERTEST served %s at spot %d"), *Item.ToString(), Spot);
	}
	return D;
}

void ALyudDiner::DishGone(ALyudDinerDish* Dish)
{
	Dishes.Remove(Dish);
}

void ALyudDiner::OnBought(FName Item, int32 Count)
{
	FLyudDinerDishDef Def;
	const AHeroCharacter* Hero = LyudDiner::Hero(GetWorld());
	if (!FindDishDef(Item, Def) || !Hero || FVector::Dist(Hero->GetActorLocation(), GetActorLocation()) > 3000.f)
	{
		return;
	}
	// not into the pocket: the waitress brings it on a plate in a few seconds
	if (ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		Inv->Remove(Item, Count);
	}
	for (int32 I = 0; I < Count; ++I)
	{
		FPending P;
		P.Item = Item;
		P.Spot = FreeSpotNear(Hero->GetActorLocation());
		P.Due = GetWorld()->GetTimeSeconds() + FMath::FRandRange(5.f, 9.f) + 3.f * Pending.Num();
		Pending.Add(P);
	}
}

void ALyudDiner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	StateEvery -= DeltaTime;
	if (StateEvery <= 0.f)
	{
		StateEvery = 1.f;
		UpdateState();
		if (SignLanguage != LyudText::Language())
		{
			ApplySigns();
		}
	}
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 I = Pending.Num() - 1; I >= 0; --I)
	{
		if (Now < Pending[I].Due)
		{
			continue;
		}
		const FPending P = Pending[I];
		Pending.RemoveAt(I);
		// the spot it was meant for may have been taken meanwhile: the nearest free one then
		int32 Spot = P.Spot;
		if (!Serving.IsValidIndex(Spot) || Serving[Spot].Dish.IsValid())
		{
			const AHeroCharacter* Hero = LyudDiner::Hero(GetWorld());
			Spot = FreeSpotNear(Hero ? Hero->GetActorLocation() : GetActorLocation());
		}
		if (Serving.IsValidIndex(Spot))
		{
			if (ALyudDinerDish* D = SpawnDish(P.Item, FTransform(YawToWorld(0.f), ToWorld(Serving[Spot].Plate)), 1.f))
			{
				Serving[Spot].Dish = D;
				if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
				{
					Screen->Toast(LyudText::Get(TEXT("ui.diner.served"), { FStringFormatArg(ULyudInventory::ItemName(P.Item).ToString()) }));
				}
				UE_LOG(LogTemp, Display, TEXT("DINERTEST served %s at spot %d"), *P.Item.ToString(), Spot);
			}
		}
		else if (ULyudInventory* Inv = ULyudInventory::Get(this))
		{
			// every place on the counter is taken: back into the pocket it was bought into (OnBought took it out), not lost
			Inv->Add(P.Item);
			UE_LOG(LogTemp, Display, TEXT("DINERTEST no free place for %s: into the pocket"), *P.Item.ToString());
		}
	}
	TickCoffee(DeltaTime);
}

// ---- self-service coffee ------------------------------------------------------------------------------------------------------

bool ALyudDiner::PourCoffee()
{
	if (Pour != EPour::None || !bOpen)
	{
		return false;
	}
	AHeroCharacter* Hero = LyudDiner::Hero(GetWorld());
	UHeroReachComponent* Reach = Hero ? UHeroReachComponent::Get(Hero) : nullptr;
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Reach || Reach->IsBusy() || !Inv)
	{
		return false;
	}
	if (!Inv->Spend(CoffeePrice))
	{
		if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
		{
			Screen->Toast(LyudText::Get(TEXT("ui.diner.no_money")));
		}
		return false;
	}
	if (ULyudHandUse* Hand = ULyudHandUse::Get(this))
	{
		Hand->PutAway();
	}
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
	bCoffeePrompt = false;
	// a clean mug from the tray (upside down there: it turns over on the way to the tap)
	ALyudDinerDish* Mug = SpawnDish(TEXT("diner_coffee"), FTransform(YawToWorld(0.f), ToWorld(CoffeeMugs)), 1.f);
	if (!Mug)
	{
		// no mug to pour into (food.json has no plated diner_coffee): the coffee was paid for above
		Inv->Earn(CoffeePrice);
		return false;
	}
	Mug->SetPoured(0.f);
	Mug->SetReady(false);
	PourMug = Mug;
	Pour = EPour::ToTray;
	PourT = 0.f;
	UE_LOG(LogTemp, Display, TEXT("DINERTEST coffee: pouring"));
	return true;
}

void ALyudDiner::PoseHandAt(const FVector& Palm, const FVector& Finger, const FVector& Normal, float Curl, const FReachShape* Shape)
{
	AHeroCharacter* Hero = LyudDiner::Hero(GetWorld());
	const APlayerController* PC = LyudDiner::Controller(GetWorld());
	UHeroReachComponent* Reach = Hero ? UHeroReachComponent::Get(Hero) : nullptr;
	if (!Reach || !PC)
	{
		return;
	}
	const FTransform Frame(FRotator(0.f, PC->GetControlRotation().Yaw, 0.f), Hero->EyeLocation());
	FHeroHoldPose H;
	H.Pos = Frame.InverseTransformPosition(Palm);
	H.FingerDir = Frame.InverseTransformVectorNoScale(Finger);
	H.PalmNormal = Frame.InverseTransformVectorNoScale(Normal);
	H.PitchFollow = 0.f;
	H.Curl = Curl;
	H.BlendTime = 0.25f;
	if (Shape)
	{
		const FTransform PalmFrame(FRotationMatrix::MakeFromXZ(Finger, Normal).ToQuat(), Palm);
		H.Shape = Shape->Transformed(PalmFrame.Inverse());
	}
	Reach->SetOverrideHold(H);
}

void ALyudDiner::TickCoffee(float DeltaTime)
{
	const UWorld* World = GetWorld();
	AHeroCharacter* Hero = LyudDiner::Hero(World);
	const APlayerController* PC = LyudDiner::Controller(World);
	if (!Hero || !PC)
	{
		return;
	}
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (Pour == EPour::None)
	{
		// the prompt at the coffee table: standing at it, looking at the urn, nothing poured and waiting there
		const FVector Urn = ToWorld(CoffeeTap + FVector(-0.1, 0.0, 0.1));
		const FVector Eye = Hero->EyeLocation();
		const bool bWaiting = Dishes.ContainsByPredicate([this](const TObjectPtr<ALyudDinerDish>& D)
		{
			return D && D->GetItem() == TEXT("diner_coffee") && !D->IsEmpty() && FVector::Dist(D->GetActorLocation(), ToWorld(CoffeeCup)) < 30.f;
		});
		const ULyudBiteSubsystem* Bite = ULyudBiteSubsystem::Get(this);
		const bool bAt = bOpen && !bWaiting && !(Bite && Bite->IsEating()) && !(UI && UI->IsOpen())
			&& FVector::Dist2D(Hero->GetActorLocation(), ToWorld(CoffeeStand)) < 140.f
			&& FVector::DotProduct(PC->GetControlRotation().Vector(), (Urn - Eye).GetSafeNormal()) > 0.75f;
		if (bAt != bCoffeePrompt && UI)
		{
			UI->SetPrompt(this, bAt ? LyudText::Get(TEXT("ui.diner.coffee_prompt"), { FStringFormatArg(CoffeePrice) }) : FText::GetEmpty());
		}
		bCoffeePrompt = bAt;
		if (bAt && PC->WasInputKeyJustPressed(EKeys::E))
		{
			if (UI)
			{
				UI->SetPrompt(this, FText::GetEmpty());
			}
			bCoffeePrompt = false;
			PourCoffee();
		}
		return;
	}
	ALyudDinerDish* Mug = PourMug.Get();
	UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero);
	if (!Mug || !Reach)
	{
		Pour = EPour::None;
		Stream->SetVisibility(false);
		if (Reach)
		{
			Reach->ClearOverrideHold();
		}
		return;
	}
	PourT += DeltaTime;
	const FVector Right = YawToWorld(0.f).RotateVector(FVector(0.f, -1.f, 0.f));      // the car's +y in the world
	const FVector Along = YawToWorld(0.f).RotateVector(FVector(1.f, 0.f, 0.f));       // +x: from the urn toward the hero
	const float R = Mug->Radius();
	const float HH = Mug->HalfHeight();
	// the hand round a mug standing (upright) with its centre at C: palm on its side toward the hero's right, fingers round it
	auto Grip = [&](const FVector& C, float Curl)
	{
		const FVector Side = (C - Hero->EyeLocation()).GetSafeNormal2D();
		const FVector Out = FVector::CrossProduct(FVector::UpVector, Side).GetSafeNormal();     // the hero's right of the mug
		FReachShape S;
		S.AddCapsule(C, FVector::UpVector, HH * 0.7f, R);
		PoseHandAt(C + Out * (R + 1.f), Side, -Out, Curl, &S);
	};
	const FVector TrayC = ToWorld(CoffeeMugs) + FVector(0.f, 0.f, HH);
	const FVector TapC = ToWorld(CoffeeCup) + FVector(0.f, 0.f, HH);
	const FVector Lever = ToWorld(CoffeeTap) + FVector(0.f, 0.f, 9.f);
	switch (Pour)
	{
	case EPour::ToTray:
		Grip(TrayC, FMath::Clamp(PourT / 0.5f, 0.f, 1.f));
		if (PourT > 0.5f) { Pour = EPour::Carry; PourT = 0.f; }
		break;
	case EPour::Carry:
	{
		const float A = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(PourT / 0.7f, 0.f, 1.f));
		const FVector C = FMath::Lerp(TrayC, TapC, A) + FVector(0.f, 0.f, 6.f * FMath::Sin(A * PI));
		Mug->SetActorLocationAndRotation(C, Mug->RestTransform().GetRotation());
		Grip(C, 1.f);
		if (PourT > 0.7f)
		{
			Mug->SetActorLocation(TapC);
			Pour = EPour::Lever;
			PourT = 0.f;
			if (USoundBase* S = LyudDiner::Sound(TEXT("S_clink_01")))
			{
				UGameplayStatics::PlaySoundAtLocation(this, S, TapC, 0.4f);
			}
		}
		break;
	}
	case EPour::Lever:
	case EPour::Pour:
	{
		// fingers on the lever's knob from above, pressing it down while it pours
		const float Press = Pour == EPour::Pour ? 2.f : 0.f;
		PoseHandAt(Lever + FVector(0.f, 0.f, 2.f - Press), Along * -1.f, -FVector::UpVector, 0.5f, nullptr);
		if (Pour == EPour::Lever && PourT > 0.45f)
		{
			Pour = EPour::Pour;
			PourT = 0.f;
			if (USoundBase* S = LyudDiner::Sound(TEXT("S_diner_pour_01")))
			{
				UGameplayStatics::PlaySoundAtLocation(this, S, TapC, 0.7f);
			}
		}
		if (Pour == EPour::Pour)
		{
			const float Share = FMath::Clamp(PourT / 3.f, 0.f, 0.92f);
			Mug->SetPoured(Share);
			// the stream from the spout down to the coffee's surface in the mug
			const FVector Top = ToWorld(CoffeeTap);
			const float Surface = TapC.Z - HH + 1.f + 7.5f * Share;
			const float Len = FMath::Max(Top.Z - Surface, 1.f);
			Stream->SetWorldLocation(FVector(Top.X, Top.Y, Top.Z - Len / 2.f));
			Stream->SetWorldScale3D(FVector(0.006f, 0.006f, Len / 100.f));
			Stream->SetVisibility(true);
			if (PourT > 3.2f)
			{
				Stream->SetVisibility(false);
				Pour = EPour::Back;
				PourT = 0.f;
			}
		}
		break;
	}
	case EPour::Back:
		if (PourT > 0.2f)
		{
			Reach->ClearOverrideHold();
			// the mug stands under the tap, full: the left button drinks it (ALyudDinerDish)
			Mug->SetPoured(1.f);
			Mug->SetReady(true);
			Pour = EPour::None;
			UE_LOG(LogTemp, Display, TEXT("DINERTEST coffee: poured"));
		}
		break;
	default:
		break;
	}
	// the poured mug stands on the drip tray under the tap from now on (where it goes back between sips)
	if (Pour == EPour::None)
	{
		FTransform T = Mug->RestTransform();
		T.SetLocation(TapC);
		Mug->SetRest(T);
	}
}

// ---- console -----------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommandWithWorld GDinerState(TEXT("lyud.Diner.State"), TEXT("lyud.Diner.State: open/closed, lit, doors, dishes"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ALyudDiner* D = ALyudDiner::Find(World))
		{
			UE_LOG(LogTemp, Display, TEXT("DINERTEST state %s, hero %s"), D->IsOpen() ? TEXT("open") : TEXT("closed"),
				D->IsHeroInside() ? TEXT("inside") : TEXT("outside"));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GDinerServe(TEXT("lyud.Diner.Serve"), TEXT("lyud.Diner.Serve <item>: a dish on the counter in front of the hero"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ALyudDiner* D = ALyudDiner::Find(World))
		{
			D->Serve(FName(Args.Num() > 0 ? *Args[0] : TEXT("diner_spaghetti")));
		}
	}));

static FAutoConsoleCommandWithWorld GDinerCoffee(TEXT("lyud.Diner.Coffee"), TEXT("lyud.Diner.Coffee: pours a mug at the coffee table"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ALyudDiner* D = ALyudDiner::Find(World))
		{
			D->PourCoffee();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GDinerOpen(TEXT("lyud.Diner.Open"), TEXT("lyud.Diner.Open 1|0|-1: force open, closed, or by the staff"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ALyudDiner* D = ALyudDiner::Find(World))
		{
			D->ForceOpen = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : -1;
		}
	}));

static FAutoConsoleCommandWithWorld GDinerEat(TEXT("lyud.Diner.Eat"), TEXT("lyud.Diner.Eat: starts eating the dish nearest the hero (checks without a mouse)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const AHeroCharacter* Hero = LyudDiner::Hero(World);
		ALyudDinerDish* Best = nullptr;
		float BestD = 1e9f;
		for (TActorIterator<ALyudDinerDish> It(World); It && Hero; ++It)
		{
			const float Dd = FVector::Dist(It->GetActorLocation(), Hero->GetActorLocation());
			if (!It->IsEmpty() && Dd < BestD)
			{
				Best = *It;
				BestD = Dd;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("DINERTEST eat %s: %d"), Best ? *Best->GetItem().ToString() : TEXT("none"), Best && Best->StartEating() ? 1 : 0);
	}));
