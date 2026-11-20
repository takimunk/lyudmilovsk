#include "Garden/LyudGarden.h"

#include "GardenModel.h"
#include "../People/MiniJson.h"
#include "../Weather/WeatherModel.h"
#include "../Weather/WindModel.h"
#include "City/LyudCityThings.h"
#include "City/SLyudCityPage.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Letters/LyudLetters.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "People/LyudFriends.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "Plants/GameCalendar.h"
#include "Plants/ProceduralPlant.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "Weather/LyudWeather.h"
#if __has_include("Dev/LyudDev.h")
#include "Dev/LyudDev.h"
#define LYUD_GARDEN_DEV 1
#else
#define LYUD_GARDEN_DEV 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudGarden, Log, All);

namespace LyudGardenImpl
{
	/** The garden is drawn in full (its plants) within this of the site's middle (cm), and let go beyond Far. */
	constexpr double NearCm = 9000.0;
	constexpr double FarCm = 11000.0;
	/** A cell is reached from this far (cm from the eye); a prop, the neighbour and the market's scales from this. */
	constexpr double CellReach = 320.0;
	constexpr double PropReach = 230.0;
	constexpr double PropCone = 0.8;
	/** The bed is stepped this often (real seconds): it grows by the hour. */
	constexpr double StepSeconds = 10.0;
	/** A plant is re-shaped when its growth has moved this much. */
	constexpr float ReshapeStep = 0.025f;
	/** Heights (cm): the bed's planks and the soil's top over the ground. */
	constexpr float BedHeight = 25.f;
	constexpr float SoilTop = 21.f;
	/** The neighbour talks to him within this (cm), no oftener than this (s). */
	constexpr double AdviceNear = 900.0;
	constexpr float AdviceEvery = 70.f;
	constexpr int32 PageSize = 9;

	FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	std::string Std(const FString& S)
	{
		const FTCHARToUTF8 Conv(*S);
		return std::string(Conv.Get(), (size_t)Conv.Length());
	}
	int32 DaysOf(const FDateTime& T) { return (int32)((T.GetDate() - FDateTime(1970, 1, 1)).GetTotalDays()); }
	FVector V3(const LyudJson::FValue& V) { return FVector(V[(size_t)0].F(), V[(size_t)1].F(), V[(size_t)2].F()); }
	uint32 Hash(uint32 X)
	{
		X ^= X >> 16; X *= 0x7feb352dU; X ^= X >> 15; X *= 0x846ca68bU; X ^= X >> 16;
		return X;
	}
	/** A line key with a number 1..N, not the one said last time when it can help it. */
	FString Pick(const FString& Base, int32 N, const FString& Last, uint32 Seed)
	{
		for (int32 Try = 0; Try < 4; ++Try)
		{
			const FString K = FString::Printf(TEXT("%s.%d"), *Base, (int32)(Hash(Seed + Try * 7919u) % (uint32)FMath::Max(1, N)) + 1);
			if (K != Last)
			{
				return K;
			}
		}
		return FString::Printf(TEXT("%s.1"), *Base);
	}
	const TCHAR* const Seeds[] = { TEXT("seeds_radish"), TEXT("seeds_dill"), TEXT("seeds_sunflower") };
}

using namespace LyudGardenImpl;

ULyudGarden::ULyudGarden() = default;

ULyudGarden::~ULyudGarden()
{
	delete Chain;
	delete Season;
}

ULyudGarden* ULyudGarden::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudGarden>() : nullptr;
}

void ULyudGarden::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Model = MakePimpl<LyudGarden::FModel>();
	if (!Chain)
	{
		Chain = new LyudWeather::FChain(LyudWeather::GetClimate());
		Season = new LyudWeather::FSeason();
	}
	Load();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("Garden"), this);
		Saves->OnNewGame.AddWeakLambda(this, [this]()
		{
			CloseSow();
			if (Model)
			{
				Model->Init();
			}
			Saved.Reset();
			ContestDay = -1;
			ContestQuality = 0;
			JamJars = 0;
			Harvested.Reset();
			SeedsGiven.Reset();
			ShowCells();
		});
	}
}

void ULyudGarden::Deinitialize()
{
	CloseSow();
	SetPrompt(EPrompt::None, FText::GetEmpty());
	if (NeighbourBody)
	{
		if (UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this))
		{
			Life->SetPresentedExternally(NeighbourId, false);
		}
	}
	ClearSite();
	Super::Deinitialize();
}

void ULyudGarden::Load()
{
	FString Text;
	const FString File = FPaths::ProjectContentDir() / TEXT("Garden/garden.json");
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		UE_LOG(LogLyudGarden, Warning, TEXT("Garden: %s not found: no allotment"), *File);
		return;
	}
	std::string Error;
	const std::string Json = Std(Text);
	if (!Model->Data.Load(Json, Error))
	{
		UE_LOG(LogLyudGarden, Warning, TEXT("Garden: %s"), *Str(Error));
		return;
	}
	Model->Init();
	LyudJson::FValue Root;
	if (!LyudJson::Parse(Json, Root, Error))
	{
		return;
	}
	const LyudJson::FValue& SiteV = Root["site"];
	SiteAt = V3(SiteV["ue"]);
	SiteYaw = SiteV["yaw"].F();
	SiteSize = FVector2D(SiteV["size"][(size_t)0].F(14.f), SiteV["size"][(size_t)1].F(26.f));
	BedSize = FVector2D(Root["bed_size"][(size_t)0].F(1.2f), Root["bed_size"][(size_t)1].F(3.6f));
	CellSize = FVector2D(Root["cell_size"][(size_t)0].F(0.6f), Root["cell_size"][(size_t)1].F(1.2f));
	Beds.Reset();
	HeroBed = -1;
	for (const LyudJson::FValue& B : Root["beds"].Items)
	{
		FBedPlace P;
		P.Id = Str(B["id"].S());
		P.Keeper = Str(B["keeper"].S());
		P.At = V3(B["ue"]);
		P.Yaw = B["yaw"].F();
		for (const LyudJson::FValue& C : B["cells"].Items)
		{
			P.Cells.Add(V3(C));
		}
		for (const LyudJson::FValue& C : B["crops"].Items)
		{
			P.Crops.Add(Str(C.S()));
		}
		if (P.Keeper == TEXT("hero"))
		{
			HeroBed = Beds.Num();
		}
		Beds.Add(P);
	}
	for (const LyudJson::FValue& P : Root["props"].Items)
	{
		Props.Add(Str(P["id"].S()), V3(P["ue"]));
		PropYaw.Add(Str(P["id"].S()), P["yaw"].F());
	}
	const LyudJson::FValue& N = Root["neighbour"];
	NeighbourKey = Str(N["person"].S());
	NeighbourStand = V3(N["stand"]);
	NeighbourYaw = N["stand_yaw"].F();
	NeighbourHours = FVector2D(N["hours"][(size_t)0].F(7.f), N["hours"][(size_t)1].F(10.5f));
	static const TCHAR* const DayNames[] = { TEXT("mon"), TEXT("tue"), TEXT("wed"), TEXT("thu"), TEXT("fri"), TEXT("sat"), TEXT("sun") };
	for (const LyudJson::FValue& D : N["days"].Items)
	{
		for (int32 I = 0; I < 7; ++I)
		{
			if (Str(D.S()) == DayNames[I])
			{
				NeighbourDays.Add(I);
			}
		}
	}
	MarketAt = V3(Root["market"]["ue"]);
	MarketHours = FIntPoint(Root["market"]["hours"][(size_t)0].I(7), Root["market"]["hours"][(size_t)1].I(18));
	for (const auto& KV : Root["sell"].Fields)
	{
		TArray<int32> Prices;
		for (const LyudJson::FValue& P : KV.second.Items)
		{
			Prices.Add(P.I());
		}
		SellPrices.Add(Str(KV.first), Prices);
	}
	const LyudJson::FValue& J = Root["jam"];
	JamNeed = J["strawberries"].I(6);
	JamHours = J["hours"].F(20.f);
	ContestWeekday = J["contest_weekday"].I(6);
	ContestHour = J["contest_hour"].I(16);
	for (const LyudJson::FValue& P : J["prize"].Items)
	{
		Prizes.Add(P.I());
	}
	if (HeroBed < 0 || Beds[HeroBed].Cells.Num() != (int32)Model->Bed.Cells.size())
	{
		UE_LOG(LogLyudGarden, Warning, TEXT("Garden: the hero's bed doesn't match the cells"));
		return;
	}
	bLoaded = true;
	UE_LOG(LogLyudGarden, Display, TEXT("Garden: %d crops, %d beds"), (int32)Model->Data.Crops.size(), Beds.Num());
	BuildSite();
	if (bSavedPending)
	{
		bSavedPending = false;
		OnGameLoaded_Implementation();
	}
}

void ULyudGarden::OnGameSaving_Implementation()
{
	if (!bLoaded || !Model)
	{
		return;
	}
	std::map<std::string, std::string> Out;
	Model->Save(Out);
	Saved.Reset();
	for (const std::pair<const std::string, std::string>& KV : Out)
	{
		Saved.Add(Str(KV.first), Str(KV.second));
	}
	Saved.Add(TEXT("g.contest"), FString::Printf(TEXT("%d|%d"), ContestDay, ContestQuality));
	Saved.Add(TEXT("g.jars"), FString::FromInt(JamJars));
	Saved.Add(TEXT("g.harvested"), FString::Join(Harvested.Array(), TEXT(",")));
	for (const TPair<FString, int32>& KV : SeedsGiven)
	{
		Saved.Add(TEXT("g.seeds.") + KV.Key, FString::FromInt(KV.Value));
	}
}

void ULyudGarden::OnGameLoaded_Implementation()
{
	CloseSow();
	if (!bLoaded)
	{
		bSavedPending = true;
		return;
	}
	std::map<std::string, std::string> In;
	for (const TPair<FString, FString>& KV : Saved)
	{
		In[Std(KV.Key)] = Std(KV.Value);
	}
	Model->LoadState(In);
	ContestDay = -1;
	ContestQuality = 0;
	if (const FString* C = Saved.Find(TEXT("g.contest")))
	{
		FString A, B;
		if (C->Split(TEXT("|"), &A, &B))
		{
			ContestDay = FCString::Atoi(*A);
			ContestQuality = FCString::Atoi(*B);
		}
	}
	JamJars = Saved.Contains(TEXT("g.jars")) ? FCString::Atoi(*Saved[TEXT("g.jars")]) : 0;
	Harvested.Reset();
	if (const FString* H = Saved.Find(TEXT("g.harvested")))
	{
		TArray<FString> Parts;
		H->ParseIntoArray(Parts, TEXT(","));
		Harvested.Append(Parts);
	}
	SeedsGiven.Reset();
	for (const TPair<FString, FString>& KV : Saved)
	{
		if (KV.Key.StartsWith(TEXT("g.seeds.")))
		{
			SeedsGiven.Add(KV.Key.RightChop(8), FCString::Atoi(*KV.Value));
		}
	}
	// the bed grows on from where it was saved to now (in the weather of the days between)
	StepModel();
	ShowCells();
}

// --- time and weather --------------------------------------------------------------------------------------------------------

double ULyudGarden::NowHours() const
{
	const FDateTime T = UGameCalendar::NowIn(GetWorld());
	return LyudWeather::HoursSince2000(T.GetYear(), T.GetMonth(), T.GetDay(), T.GetTimeOfDay().GetTotalHours());
}

int32 ULyudGarden::Today() const
{
	return DaysOf(UGameCalendar::NowIn(GetWorld()));
}

void ULyudGarden::StepModel()
{
	if (!bLoaded || !Model->Bed.bRented)
	{
		if (bLoaded)
		{
			Model->Now = NowHours();
		}
		return;
	}
	const double Hours = NowHours();
	const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(this);
	TArray<uint8> Before;
	for (const LyudGarden::FCell& C : Model->Bed.Cells)
	{
		Before.Add((uint8)C.State);
	}
	// the hours behind are the weather's own chain; the last hour is what the sky shows now (forced weather included)
	Model->AdvanceTo(Hours, [this, Hours, Weather](double H)
	{
		LyudGarden::FAir A;
		const LyudWeather::FWeatherState S = (Weather && Hours - H < 1.5) ? Weather->GetModelState() : LyudWeather::Full(H, *Chain, *Season);
		A.TempC = S.TempC;
		A.RainMm = S.PrecipRate;
		A.Snow = S.SnowFraction;
		A.Cloud = S.CloudCover;
		A.Wind = S.WindSpeed;
		A.SunElev = (float)LyudWeather::SunElevationDeg(H, Chain->GetClimate());
		return A;
	});
	// what happened while he was away is told when he is here
	const APawn* Hero = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bHere = Hero && FVector::Dist2D(Hero->GetActorLocation(), SiteAt) < NearCm;
	for (int32 I = 0; I < Before.Num(); ++I)
	{
		const LyudGarden::FCell& C = Model->Bed.Cells[(size_t)I];
		if ((uint8)C.State == Before[I] || !bHere)
		{
			continue;
		}
		const FText Crop = LyudText::Get(TEXT("garden.crop.") + Str(C.Crop));
		if (C.State == LyudGarden::ECell::Dead)
		{
			Toast(LyudText::Get(TEXT("garden.toast.dead"), { FStringFormatArg(Crop.ToString()) }));
		}
		else if (C.State == LyudGarden::ECell::Spoilt)
		{
			Toast(LyudText::Get(TEXT("garden.toast.spoilt"), { FStringFormatArg(Crop.ToString()) }));
		}
	}
}

void ULyudGarden::Soon(float& Coldest, float& Hottest) const
{
	Coldest = 99.f;
	Hottest = -99.f;
	const double H = NowHours();
	for (float T = 0.f; T <= 14.f; T += 2.f)
	{
		const LyudWeather::FWeatherState S = LyudWeather::Instant(H + T, *Chain);
		Coldest = FMath::Min(Coldest, S.TempC);
		Hottest = FMath::Max(Hottest, S.TempC);
	}
}

// --- tick ----------------------------------------------------------------------------------------------------------------------

void ULyudGarden::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !bLoaded || !Model)
	{
		return;
	}
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	const double Real = World->GetTimeSeconds();
	if (Real >= StepClock)
	{
		StepClock = Real + StepSeconds;
		StepModel();
		TickJam();
		TickContest();
		if (bNear)
		{
			ShowCells();
			ShowPlants(true);
		}
	}
	NearClock -= DeltaTime;
	if (NearClock <= 0.f && Hero)
	{
		NearClock = 1.f;
		const double D = FVector::Dist2D(Hero->GetActorLocation(), SiteAt);
		const bool bWasNear = bNear;
		bNear = D < NearCm || (bNear && D < FarCm);
		if (bNear != bWasNear)
		{
			ShowCells();
			ShowPlants(bNear);
		}
		TickNeighbour(1.f);
		// a jar of jam waits on the bench by the shed
		if (JamJars > 0 && Props.Contains(TEXT("bench")) && FVector::Dist2D(Hero->GetActorLocation(), Props[TEXT("bench")]) < 600.0)
		{
			if (ULyudInventory* Inv = ULyudInventory::Get(this))
			{
				Inv->Add(TEXT("jam_strawberry"), JamJars);
				Toast(LyudText::Get(TEXT("garden.toast.jam_ready"), { FStringFormatArg(LyudText::Get(TEXT("npc.") + NeighbourKey + TEXT(".name")).ToString()) }));
				Note(TEXT("garden.diary.jam"));
				JamJars = 0;
			}
		}
	}
	if (NeighbourBody)
	{
		NeighbourBody->Drive(NeighbourStand, NeighbourYaw, EPersonPose::Shovel, 0, 0.f, 0.f, DeltaTime, false);
		AdviceClock -= DeltaTime;
		if (AdviceClock <= 0.f && Hero && FVector::Dist2D(Hero->GetActorLocation(), NeighbourStand) < AdviceNear)
		{
			AdviceClock = AdviceEvery;
			Advise();
		}
	}
	if (Page.IsValid())
	{
		TickSow(PC);
		return;
	}
	const bool bAtMarket = Hero && FVector::Dist2D(Hero->GetActorLocation(), MarketAt) < 1500.0;
	if (PC && Hero && (bNear || bAtMarket))
	{
		PromptClock -= DeltaTime;
		if (PromptClock <= 0.f)
		{
			PromptClock = 0.12f;
			TickPrompt(PC);
		}
		if (Prompt != EPrompt::None && ANpcTalkDirector::UsePressed(PC))
		{
			Act(PC);
		}
	}
	else if (Prompt != EPrompt::None)
	{
		SetPrompt(EPrompt::None, FText::GetEmpty());
	}
}

// --- the place -----------------------------------------------------------------------------------------------------------------

UMaterialInstanceDynamic* ULyudGarden::Mat(const FLinearColor& Colour)
{
	const FColor C = Colour.ToFColor(false);
	const uint32 Key = C.DWColor();
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = Mats.Find(Key))
	{
		return *Found;
	}
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInstanceDynamic* M = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr;
	if (M)
	{
		M->SetVectorParameterValue(TEXT("Color"), Colour);
		Mats.Add(Key, M);
	}
	return M;
}

UStaticMeshComponent* ULyudGarden::AddBox(const FVector& At, const FVector& HalfCm, float Yaw, const FLinearColor& Colour, bool bCollide)
{
	static UStaticMesh* Cube = nullptr;
	if (!Cube)
	{
		Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	if (!Site || !Cube)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Site, NAME_None, RF_Transient | RF_DuplicateTransient);
	C->SetStaticMesh(Cube);
	C->SetMobility(EComponentMobility::Static);
	C->SetupAttachment(Site->GetRootComponent());
	// the engine cube is 100 cm
	C->SetWorldTransform(FTransform(FRotator(0.f, Yaw, 0.f), At, HalfCm / 50.0));
	C->SetMaterial(0, Mat(Colour));
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetCastShadow(true);
	C->RegisterComponent();
	return C;
}

UStaticMeshComponent* ULyudGarden::AddCylinder(const FVector& At, float RadiusCm, float HalfHeightCm, const FLinearColor& Colour)
{
	static UStaticMesh* Cyl = nullptr;
	if (!Cyl)
	{
		Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	}
	if (!Site || !Cyl)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Site, NAME_None, RF_Transient | RF_DuplicateTransient);
	C->SetStaticMesh(Cyl);
	C->SetMobility(EComponentMobility::Static);
	C->SetupAttachment(Site->GetRootComponent());
	C->SetWorldTransform(FTransform(FRotator::ZeroRotator, At, FVector(RadiusCm / 50.0, RadiusCm / 50.0, HalfHeightCm / 50.0)));
	C->SetMaterial(0, Mat(Colour));
	C->RegisterComponent();
	return C;
}

void ULyudGarden::BuildSite()
{
	UWorld* World = GetWorld();
	if (!World || Site)
	{
		return;
	}
	// the ground under the yard: the terrace's, found once (the plan's height is the terrace's exact level)
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(GardenGround), false);
	if (World->LineTraceSingleByChannel(Hit, SiteAt + FVector(0, 0, 600), SiteAt - FVector(0, 0, 600), ECC_WorldStatic, Q))
	{
		const float Dz = Hit.ImpactPoint.Z - SiteAt.Z;
		SiteAt.Z += Dz;
		for (FBedPlace& B : Beds)
		{
			B.At.Z += Dz;
			for (FVector& C : B.Cells)
			{
				C.Z += Dz;
			}
		}
		for (TPair<FString, FVector>& P : Props)
		{
			P.Value.Z += Dz;
		}
		NeighbourStand.Z += Dz;
	}
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Site = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FRotator(0.f, SiteYaw, 0.f), SiteAt), SP);
	if (!Site)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Site, TEXT("Root"), RF_Transient);
	Root->SetMobility(EComponentMobility::Static);
	Site->SetRootComponent(Root);
	Root->RegisterComponent();
	Root->SetWorldTransform(FTransform(FRotator(0.f, SiteYaw, 0.f), SiteAt));

	const FRotator R(0.f, SiteYaw, 0.f);
	auto Local = [this, &R](float X, float Y, float Z) { return SiteAt + R.RotateVector(FVector(X * 100.f, Y * 100.f, Z)); };
	const FLinearColor Earth(0.17f, 0.13f, 0.08f), Wood(0.33f, 0.24f, 0.15f), Grey(0.42f, 0.42f, 0.4f), Green(0.16f, 0.24f, 0.09f);
	// the yard's earth over the concrete, and the paths between the beds
	AddBox(Local(0.f, 0.f, 2.f), FVector(SiteSize.X * 50.f, SiteSize.Y * 50.f, 2.f), SiteYaw, Green, false);
	AddBox(Local(0.f, 0.f, 4.5f), FVector(80.f, SiteSize.Y * 45.f, 0.5f), SiteYaw, FLinearColor(0.36f, 0.33f, 0.28f), false);
	// the fence: posts every two metres, two rails; the gate's gap at the seaward side's south end
	const float HX = SiteSize.X * 0.5f, HY = SiteSize.Y * 0.5f;
	auto Rail = [&](float X0, float Y0, float X1, float Y1)
	{
		const FVector A = Local(X0, Y0, 0.f), B = Local(X1, Y1, 0.f);
		const float Len = FVector::Dist2D(A, B);
		const float Yaw = (B - A).Rotation().Yaw;
		for (float Z : { 45.f, 95.f })
		{
			AddBox((A + B) * 0.5f + FVector(0, 0, Z), FVector(Len * 0.5f, 2.f, 4.f), Yaw, Wood, true);
		}
		const int32 Posts = FMath::Max(1, FMath::RoundToInt(Len / 200.f));
		for (int32 I = 0; I <= Posts; ++I)
		{
			AddBox(FMath::Lerp(A, B, (float)I / Posts) + FVector(0, 0, 60.f), FVector(4.f, 4.f, 60.f), SiteYaw, Wood, true);
		}
	};
	Rail(-HX, -HY, -HX, HY);
	Rail(-HX, HY, HX, HY);
	Rail(-HX, -HY, HX, -HY);
	Rail(HX, -HY + 2.2f, HX, HY);
	// the beds: planks round each, the soil (the hero's cell by cell, so each shows its own state)
	CellSoil.Reset();
	for (int32 B = 0; B < Beds.Num(); ++B)
	{
		const FBedPlace& P = Beds[B];
		const FRotator BR(0.f, P.Yaw, 0.f);
		const float BX = BedSize.X * 50.f, BY = BedSize.Y * 50.f;
		AddBox(P.At + BR.RotateVector(FVector(BX, 0.f, BedHeight * 0.5f)), FVector(2.5f, BY + 2.5f, BedHeight * 0.5f), P.Yaw, Wood, true);
		AddBox(P.At + BR.RotateVector(FVector(-BX, 0.f, BedHeight * 0.5f)), FVector(2.5f, BY + 2.5f, BedHeight * 0.5f), P.Yaw, Wood, true);
		AddBox(P.At + BR.RotateVector(FVector(0.f, BY, BedHeight * 0.5f)), FVector(BX, 2.5f, BedHeight * 0.5f), P.Yaw, Wood, true);
		AddBox(P.At + BR.RotateVector(FVector(0.f, -BY, BedHeight * 0.5f)), FVector(BX, 2.5f, BedHeight * 0.5f), P.Yaw, Wood, true);
		if (B == HeroBed)
		{
			for (const FVector& C : P.Cells)
			{
				CellSoil.Add(AddBox(C + FVector(0, 0, SoilTop * 0.5f), FVector(CellSize.X * 50.f - 1.f, CellSize.Y * 50.f - 1.f, SoilTop * 0.5f), P.Yaw, Earth, false));
			}
			// the film over hoops: a low tunnel, down on cold nights
			Film = AddBox(P.At + FVector(0, 0, BedHeight + 30.f), FVector(BX + 4.f, BY + 4.f, 30.f), P.Yaw, FLinearColor(0.85f, 0.9f, 0.92f), false);
			if (UMaterialInterface* Clear = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Academgorodok/Materials/M_Academ_GlassClear.M_Academ_GlassClear")))
			{
				Film->SetMaterial(0, Clear);
			}
			Film->SetCastShadow(false);
		}
		else
		{
			AddBox(P.At + FVector(0, 0, SoilTop * 0.5f), FVector(BX - 1.f, BY - 1.f, SoilTop * 0.5f), P.Yaw, Earth, false);
		}
	}
	// the committee's board, the rain barrel, the shed, the bench
	if (const FVector* At = Props.Find(TEXT("board")))
	{
		const float Yaw = PropYaw.FindRef(TEXT("board"));
		const FVector Side = FRotator(0.f, Yaw, 0.f).RotateVector(FVector(0.f, 50.f, 0.f));
		AddBox(*At + Side + FVector(0, 0, 90.f), FVector(4.f, 4.f, 90.f), Yaw, Wood, true);
		AddBox(*At - Side + FVector(0, 0, 90.f), FVector(4.f, 4.f, 90.f), Yaw, Wood, true);
		AddBox(*At + FVector(0, 0, 140.f), FVector(2.f, 55.f, 35.f), Yaw, FLinearColor(0.75f, 0.72f, 0.62f), true);
	}
	if (const FVector* At = Props.Find(TEXT("barrel")))
	{
		AddCylinder(*At + FVector(0, 0, 45.f), 30.f, 45.f, FLinearColor(0.2f, 0.3f, 0.35f));
		AddCylinder(*At + FVector(0, 0, 89.f), 27.f, 1.f, FLinearColor(0.05f, 0.08f, 0.1f));
	}
	if (const FVector* At = Props.Find(TEXT("shed")))
	{
		const float Yaw = PropYaw.FindRef(TEXT("shed"));
		AddBox(*At + FVector(0, 0, 110.f), FVector(100.f, 100.f, 110.f), Yaw, FLinearColor(0.3f, 0.36f, 0.3f), true);
		AddBox(*At + FVector(0, 0, 225.f), FVector(115.f, 115.f, 5.f), Yaw, Grey, true);
	}
	if (const FVector* At = Props.Find(TEXT("bench")))
	{
		const float Yaw = PropYaw.FindRef(TEXT("bench"));
		AddBox(*At + FVector(0, 0, 44.f), FVector(20.f, 70.f, 3.f), Yaw, Wood, true);
		const FVector Side = FRotator(0.f, Yaw, 0.f).RotateVector(FVector(0.f, 60.f, 0.f));
		AddBox(*At + Side + FVector(0, 0, 21.f), FVector(15.f, 3.f, 21.f), Yaw, Wood, true);
		AddBox(*At - Side + FVector(0, 0, 21.f), FVector(15.f, 3.f, 21.f), Yaw, Wood, true);
	}
	CellLook.Init(255, CellSoil.Num());
	const int32 Slots = Beds.Num() * (int32)Model->Bed.Cells.size();
	Plants.Init(nullptr, Slots);
	PlantGrowth.Init(-1.f, Slots);
	PlantSpecies.Init(FString(), Slots);
	ShowCells();
}

void ULyudGarden::ClearSite()
{
	for (AProceduralPlant* P : Plants)
	{
		if (IsValid(P))
		{
			P->Destroy();
		}
	}
	Plants.Reset();
	if (NeighbourBody)
	{
		NeighbourBody->Destroy();
		NeighbourBody = nullptr;
	}
	if (Site)
	{
		Site->Destroy();
		Site = nullptr;
	}
	CellSoil.Reset();
	Film = nullptr;
}

void ULyudGarden::ShowCells()
{
	if (!bLoaded || CellSoil.Num() != (int32)Model->Bed.Cells.size())
	{
		return;
	}
	for (int32 I = 0; I < CellSoil.Num(); ++I)
	{
		const LyudGarden::FCell& C = Model->Bed.Cells[(size_t)I];
		// the look as a small code: the state, how wet, how weedy (the material changes only when it does)
		const uint8 Wet = C.Water > 0.6f ? 2 : (C.Water > 0.3f ? 1 : 0);
		const uint8 Weedy = C.State == LyudGarden::ECell::Grass ? 3 : (C.Weeds > 0.6f ? 2 : (C.Weeds > 0.3f ? 1 : 0));
		const uint8 Look = (uint8)((Model->Bed.bRented ? 1 : 0) << 7 | Wet << 2 | Weedy);
		if (Look == CellLook[I] || !CellSoil[I])
		{
			continue;
		}
		CellLook[I] = Look;
		const FLinearColor Dry(0.36f, 0.27f, 0.18f), Damp(0.24f, 0.17f, 0.11f), Soaked(0.15f, 0.1f, 0.07f), Weeds(0.2f, 0.3f, 0.1f);
		FLinearColor Col = Wet == 2 ? Soaked : (Wet == 1 ? Damp : Dry);
		Col = FMath::Lerp(Col, Weeds, Weedy / 3.f * 0.85f);
		CellSoil[I]->SetMaterial(0, Mat(Col));
	}
	if (Film)
	{
		Film->SetVisibility(Model->Bed.bCovered);
	}
}

AProceduralPlant* ULyudGarden::SpawnPlant(const FString& Species, const FVector& At, int32 Seed, float Growth)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SP.bDeferConstruction = true;
	AProceduralPlant* P = World->SpawnActor<AProceduralPlant>(AProceduralPlant::StaticClass(), FTransform(FRotator(0.f, (float)(Seed % 360), 0.f), At), SP);
	if (!P)
	{
		return nullptr;
	}
	P->Species = FName(*Species);
	P->Seed = Seed;
	P->Pot = EPlantPot::None;
	P->bDemoGrowth = false;
	// it stands on the bed's soil, not the terrace under it; the garden handles E itself
	P->bSnapToGround = false;
	P->bAllowPick = false;
	P->SummaryDistance = 6000.f;
	P->SetGrowth(FMath::Max(0.02f, Growth), UGameCalendar::DayOfYear(UGameCalendar::NowIn(World)));
	P->FinishSpawning(FTransform(FRotator(0.f, (float)(Seed % 360), 0.f), At));
	return P;
}

void ULyudGarden::ShowPlant(int32 Slot, const FString& Species, const FVector& At, float Growth, bool bDead)
{
	if (!Plants.IsValidIndex(Slot))
	{
		return;
	}
	AProceduralPlant* P = Plants[Slot];
	if (Species.IsEmpty() || Growth < 0.f)
	{
		if (IsValid(P))
		{
			P->Destroy();
		}
		Plants[Slot] = nullptr;
		PlantSpecies[Slot].Reset();
		PlantGrowth[Slot] = -1.f;
		return;
	}
	if (!IsValid(P) || PlantSpecies[Slot] != Species)
	{
		if (IsValid(P))
		{
			P->Destroy();
		}
		P = SpawnPlant(Species, At, 1000 + Slot * 37, Growth);
		Plants[Slot] = P;
		PlantSpecies[Slot] = Species;
		PlantGrowth[Slot] = -1.f;
	}
	if (!P)
	{
		return;
	}
	// a dead plant keeps its size and goes brown; growth is re-shaped only when it has moved
	const float Shown = bDead ? -2.f - Growth : Growth;
	if (FMath::Abs(Shown - PlantGrowth[Slot]) >= ReshapeStep || (PlantGrowth[Slot] < -1.5f) != bDead)
	{
		PlantGrowth[Slot] = Shown;
		P->SetGrowth(FMath::Max(0.02f, Growth), UGameCalendar::DayOfYear(UGameCalendar::NowIn(GetWorld())));
		P->SetWithered(bDead);
	}
}

void ULyudGarden::ShowPlants(bool bShowNear)
{
	if (!bLoaded || Plants.Num() == 0)
	{
		return;
	}
	const int32 N = (int32)Model->Bed.Cells.size();
	const float Day = UGameCalendar::DayOfYear(UGameCalendar::NowIn(GetWorld()));
	for (int32 B = 0; B < Beds.Num(); ++B)
	{
		const FBedPlace& P = Beds[B];
		for (int32 I = 0; I < N && I < P.Cells.Num(); ++I)
		{
			const int32 Slot = B * N + I;
			FString Species;
			float Growth = -1.f;
			bool bDead = false;
			if (bShowNear && B == HeroBed)
			{
				const LyudGarden::FCell& C = Model->Bed.Cells[(size_t)I];
				const LyudGarden::FCrop* Crop = Model->Data.Crop(C.Crop);
				if (Crop && C.State != LyudGarden::ECell::Grass && C.State != LyudGarden::ECell::Dug && LyudGarden::Stage(C) >= 1)
				{
					Species = Str(Crop->Species);
					Growth = C.State == LyudGarden::ECell::Growing || C.State == LyudGarden::ECell::Dead ? C.Growth : 1.f;
					bDead = C.State == LyudGarden::ECell::Dead || C.State == LyudGarden::ECell::Spoilt;
				}
			}
			else if (bShowNear && P.Crops.IsValidIndex(I))
			{
				const LyudGarden::FCrop* Crop = Model->Data.Crop(Std(P.Crops[I]));
				Growth = Crop ? LyudGarden::NeighbourGrowth(Crop->Id, Day) : -1.f;
				if (Crop && Growth >= 0.08f)
				{
					Species = Str(Crop->Species);
				}
			}
			ShowPlant(Slot, Species, P.Cells[I] + FVector(0, 0, SoilTop), Growth, bDead);
		}
	}
}

// --- the hero ------------------------------------------------------------------------------------------------------------------

int32 ULyudGarden::CellInView(const FVector& Eye, const FVector& Dir) const
{
	if (HeroBed < 0 || Dir.Z > -0.05f)
	{
		return -1;
	}
	const FBedPlace& P = Beds[HeroBed];
	const float Z = P.At.Z + SoilTop;
	const float T = (Z - Eye.Z) / Dir.Z;
	if (T <= 0.f || T > CellReach)
	{
		return -1;
	}
	const FVector Hit = Eye + Dir * T;
	const FVector L = FRotator(0.f, P.Yaw, 0.f).UnrotateVector(Hit - P.At);
	const float HX = BedSize.X * 50.f, HY = BedSize.Y * 50.f;
	if (FMath::Abs(L.X) > HX + 10.f || FMath::Abs(L.Y) > HY + 10.f)
	{
		return -1;
	}
	// the nearest cell's middle (the cells are listed across, then along)
	int32 Best = -1;
	float BestD = 1e9f;
	for (int32 I = 0; I < P.Cells.Num(); ++I)
	{
		const float D = FVector::DistSquared2D(P.Cells[I], Hit);
		if (D < BestD)
		{
			BestD = D;
			Best = I;
		}
	}
	return Best;
}

void ULyudGarden::SetPrompt(EPrompt Kind, const FText& Text)
{
	if (Kind == Prompt && Text.EqualTo(PromptText))
	{
		return;
	}
	Prompt = Kind;
	PromptText = Text;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, Kind == EPrompt::None ? FText::GetEmpty() : Text);
	}
}

void ULyudGarden::TickPrompt(APlayerController* PC)
{
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	const ULyudInventoryUI* UI = World ? ULyudInventoryUI::Get(World) : nullptr;
	const ULyudCityThings* City = World ? ULyudCityThings::Get(World) : nullptr;
	const ULyudLetters* Letters = World ? ULyudLetters::Get(World) : nullptr;
	const ULyudFriends* Friends = World ? ULyudFriends::Get(World) : nullptr;
	const bool bCalm = Hero && UI && UI->CanOpen(true) && !Hero->IsOnBike() && !(City && City->HasModeUp())
		&& !(Letters && (Letters->IsOffering() || Letters->HasModeUp())) && !(Friends && (Friends->IsOffering() || Friends->HasModeUp()));
	if (!bCalm)
	{
		SetPrompt(EPrompt::None, FText::GetEmpty());
		return;
	}
	const FVector Eye = Hero->GetPawnViewLocation();
	const FVector Dir = Hero->GetControlRotation().Vector();
	const ULyudInventory* Inv = ULyudInventory::Get(World);
	auto InView = [&](const FVector& At, double Reach)
	{
		const FVector To = At - Eye;
		return FVector::Dist2D(At, Hero->GetActorLocation()) < Reach && FVector::DotProduct(To.GetSafeNormal(), Dir) > PropCone;
	};
	auto Has = [Inv](const TCHAR* Item) { return Inv && Inv->Count(FName(Item)) > 0; };
	const FText NeighbourName = LyudText::Get(TEXT("npc.") + NeighbourKey + TEXT(".name"));

	// the market's control scales: the harvest handed in, the jar for the contest
	if (FVector::Dist2D(Hero->GetActorLocation(), MarketAt) < 1500.0)
	{
		if (InView(MarketAt + FVector(0, 0, 60), PropReach + 60.0))
		{
			const FDateTime Now = UGameCalendar::NowIn(World);
			const int32 Weekday = ((int32)Now.GetDayOfWeek() + 7) % 7;
			if (Now.GetHour() < MarketHours.X || Now.GetHour() >= MarketHours.Y)
			{
				SetPrompt(EPrompt::Scales, LyudText::Get(TEXT("garden.prompt.scales_closed"), { FStringFormatArg(MarketHours.X) }));
				return;
			}
			if (Has(TEXT("jam_strawberry")) && Weekday < 5)
			{
				SetPrompt(EPrompt::Scales, LyudText::Get(ContestDay >= 0 ? TEXT("garden.toast.contest_already") : TEXT("garden.prompt.contest")));
				if (ContestDay < 0)
				{
					return;
				}
			}
			int32 Things = 0;
			const int32 Roubles = SellValue(&Things);
			if (Things > 0)
			{
				SetPrompt(EPrompt::Scales, LyudText::Get(TEXT("garden.prompt.sell"), { FStringFormatArg(Things), FStringFormatArg(Roubles) }));
				return;
			}
		}
		if (!bNear)
		{
			SetPrompt(EPrompt::None, FText::GetEmpty());
			return;
		}
	}
	// the neighbour: strawberries for jam
	if (NeighbourBody && InView(NeighbourBody->GetActorLocation() + FVector(0, 0, 100), PropReach + 40.0))
	{
		if (Model->JamQuality > 0)
		{
			SetPrompt(EPrompt::Neighbour, LyudText::Get(TEXT("garden.prompt.jam_wait")));
			return;
		}
		if (Inv && Inv->Count(TEXT("strawberries")) >= JamNeed && JamJars == 0)
		{
			SetPrompt(EPrompt::Neighbour, LyudText::Get(TEXT("garden.prompt.jam_ask"), { FStringFormatArg(NeighbourName.ToString()) }));
			return;
		}
	}
	// the board at the gate, the barrel
	if (const FVector* Board = Props.Find(TEXT("board")))
	{
		if (InView(*Board + FVector(0, 0, 140), PropReach))
		{
			SetPrompt(EPrompt::Board, Model->Bed.bRented ? LyudText::Get(TEXT("garden.prompt.board"))
				: LyudText::Get(TEXT("garden.prompt.rent"), { FStringFormatArg(Model->Data.R.Rent) }));
			return;
		}
	}
	if (const FVector* Barrel = Props.Find(TEXT("barrel")))
	{
		if (InView(*Barrel + FVector(0, 0, 80), PropReach))
		{
			const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(World);
			const bool bIce = Weather && Weather->GetModelState().TempC < -3.f;
			SetPrompt(EPrompt::Barrel, LyudText::Get(!Has(TEXT("watering_can")) ? TEXT("garden.prompt.no_can")
				: (bIce ? TEXT("garden.prompt.barrel_ice") : TEXT("garden.prompt.fill"))));
			return;
		}
	}
	// a cell of his bed
	const int32 Cell = Model->Bed.bRented ? CellInView(Eye, Dir) : -1;
	if (Cell < 0)
	{
		SetPrompt(EPrompt::None, FText::GetEmpty());
		return;
	}
	PromptCell = Cell;
	float Coldest, Hottest;
	Soon(Coldest, Hottest);
	const LyudGarden::FCell& C = Model->Bed.Cells[(size_t)Cell];
	const FString Crop = LyudText::Get(TEXT("garden.crop.") + Str(C.Crop)).ToString();
	const TCHAR* Key = nullptr;
	switch (Model->Need(Cell, Coldest, Hottest))
	{
	case LyudGarden::ENeed::Dig: Key = Has(TEXT("spade")) ? TEXT("garden.prompt.dig") : TEXT("garden.prompt.no_spade"); break;
	case LyudGarden::ENeed::Sow:
	{
		bool bAny = false;
		for (const LyudGarden::FCrop& K : Model->Data.Crops)
		{
			bAny |= Inv && Inv->Count(FName(*Str(K.Seed))) > 0;
		}
		Key = bAny ? TEXT("garden.prompt.sow") : TEXT("garden.prompt.no_seeds");
		break;
	}
	case LyudGarden::ENeed::Water:
		Key = !Has(TEXT("watering_can")) ? TEXT("garden.prompt.no_can") : (Model->CanLeft <= 0 ? TEXT("garden.prompt.can_empty") : TEXT("garden.prompt.water"));
		break;
	case LyudGarden::ENeed::Weed: Key = TEXT("garden.prompt.weed"); break;
	case LyudGarden::ENeed::Cover: Key = TEXT("garden.prompt.cover"); break;
	case LyudGarden::ENeed::Uncover: Key = TEXT("garden.prompt.uncover"); break;
	case LyudGarden::ENeed::Pick: Key = TEXT("garden.prompt.pick"); break;
	case LyudGarden::ENeed::Clear: Key = TEXT("garden.prompt.clear"); break;
	default: break;
	}
	if (!Key && Has(TEXT("garden_compost")) && C.Rich < 0.3f && C.State == LyudGarden::ECell::Growing)
	{
		Key = TEXT("garden.prompt.compost");
	}
	if (Key)
	{
		SetPrompt(EPrompt::Cell, LyudText::Get(Key, { FStringFormatArg(Crop) }));
		return;
	}
	// nothing to do: how it is, in words
	FString Soil = LyudText::Get(C.Water > 0.6f ? TEXT("garden.soil.wet") : (C.Water > 0.3f ? TEXT("garden.soil.damp") : TEXT("garden.soil.dry"))).ToString();
	if (C.Weeds > 0.6f)
	{
		Soil += TEXT(" ") + LyudText::Get(TEXT("garden.weeds.many")).ToString();
	}
	else if (C.Weeds > 0.25f)
	{
		Soil += TEXT(" ") + LyudText::Get(TEXT("garden.weeds.some")).ToString();
	}
	if (Model->Bed.bCovered)
	{
		Soil += TEXT(" ") + LyudText::Get(TEXT("garden.cover.down")).ToString();
	}
	const int32 St = LyudGarden::Stage(C);
	const FString Stage = LyudText::Get(St < 0 ? TEXT("garden.stage.0") : *FString::Printf(TEXT("garden.stage.%d"), St)).ToString();
	SetPrompt(EPrompt::Cell, LyudText::Get(TEXT("garden.prompt.look"), { FStringFormatArg(
		LyudText::Get(TEXT("garden.look.crop"), { FStringFormatArg(Crop), FStringFormatArg(Stage), FStringFormatArg(Soil) }).ToString()) }));
	PromptCell = -1;     // E does nothing here
}

int32 ULyudGarden::SellValue(int32* Things) const
{
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	int32 Sum = 0, N = 0;
	for (const TPair<FString, TArray<int32>>& KV : SellPrices)
	{
		const int32 Count = Inv ? Inv->Count(FName(*KV.Key)) : 0;
		if (Count <= 0 || KV.Value.Num() < 3)
		{
			continue;
		}
		const int32 Q = FMath::Clamp(Model->LastQuality(Std(KV.Key)), 1, 3);
		Sum += Count * KV.Value[Q - 1];
		N += Count;
	}
	if (Things)
	{
		*Things = N;
	}
	return Sum;
}

void ULyudGarden::Act(APlayerController* PC)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	const FText NeighbourName = LyudText::Get(TEXT("npc.") + NeighbourKey + TEXT(".name"));
	switch (Prompt)
	{
	case EPrompt::Board:
		if (!Model->Bed.bRented)
		{
			if (Inv && Inv->Spend(Model->Data.R.Rent))
			{
				Model->Rent(NowHours());
				Toast(LyudText::Get(TEXT("garden.toast.rented")));
				Note(TEXT("garden.diary.rented"));
				ULyudQuests::Notify(this, TEXT("do"), TEXT("garden_rent"));
				ShowCells();
			}
			else
			{
				Toast(LyudText::Get(TEXT("garden.toast.no_money"), { FStringFormatArg(Model->Data.R.Rent) }));
			}
		}
		else
		{
			OpenSow(-1);     // the notice, read on the page
		}
		break;
	case EPrompt::Barrel:
		if (Inv && Inv->Count(TEXT("watering_can")) > 0 && PromptText.ToString() == LyudText::Get(TEXT("garden.prompt.fill")).ToString())
		{
			Model->Fill();
			Toast(LyudText::Get(TEXT("garden.toast.filled")));
		}
		break;
	case EPrompt::Neighbour:
		if (Inv && Model->JamQuality == 0 && Inv->Count(TEXT("strawberries")) >= JamNeed && Inv->Remove(TEXT("strawberries"), JamNeed))
		{
			Model->JamQuality = FMath::Clamp(Model->LastQuality("strawberries"), 1, 3);
			Model->JamAt = NowHours();
			Toast(LyudText::Get(TEXT("garden.toast.jam_given"), { FStringFormatArg(NeighbourName.ToString()) }));
			NeighbourSay(Pick(TEXT("line.garden.jam.ask"), 2, LastAdvice, (uint32)Today()));
		}
		break;
	case EPrompt::Scales:
	{
		if (!Inv)
		{
			break;
		}
		const FDateTime Now = UGameCalendar::NowIn(GetWorld());
		const int32 Weekday = ((int32)Now.GetDayOfWeek() + 7) % 7;
		if (Now.GetHour() < MarketHours.X || Now.GetHour() >= MarketHours.Y)
		{
			break;
		}
		if (ContestDay < 0 && Weekday < 5 && Inv->Count(TEXT("jam_strawberry")) > 0)
		{
			Inv->Remove(TEXT("jam_strawberry"));
			ContestDay = Today() + (ContestWeekday - Weekday);
			ContestQuality = FMath::Clamp(Model->JamJarQuality, 1, 3);
			Toast(LyudText::Get(TEXT("garden.toast.contest_entered")));
			ULyudQuests::Notify(this, TEXT("do"), TEXT("garden_contest"));
			break;
		}
		int32 Things = 0;
		const int32 Roubles = SellValue(&Things);
		if (Things > 0)
		{
			for (const TPair<FString, TArray<int32>>& KV : SellPrices)
			{
				const int32 Count = Inv->Count(FName(*KV.Key));
				if (Count > 0)
				{
					Inv->Remove(FName(*KV.Key), Count);
				}
			}
			Inv->Earn(Roubles);
			Toast(LyudText::Get(TEXT("garden.toast.sold"), { FStringFormatArg(Roubles) }));
			ULyudQuests::Notify(this, TEXT("do"), TEXT("garden_sell"));
		}
		break;
	}
	case EPrompt::Cell:
		if (PromptCell >= 0)
		{
			ActCell(PromptCell);
		}
		break;
	default:
		break;
	}
	PromptClock = 0.f;
	(void)PC;
}

void ULyudGarden::ActCell(int32 Cell)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv)
	{
		return;
	}
	float Coldest, Hottest;
	Soon(Coldest, Hottest);
	const LyudGarden::FCell& C = Model->Bed.Cells[(size_t)Cell];
	const FString Crop = LyudText::Get(TEXT("garden.crop.") + Str(C.Crop)).ToString();
	bool bDone = false;
	switch (Model->Need(Cell, Coldest, Hottest))
	{
	case LyudGarden::ENeed::Dig:
		if (Inv->Count(TEXT("spade")) > 0 && Model->Dig(Cell))
		{
			Toast(LyudText::Get(TEXT("garden.toast.dug")));
			bDone = true;
		}
		break;
	case LyudGarden::ENeed::Sow:
		OpenSow(Cell);
		return;
	case LyudGarden::ENeed::Water:
		if (Inv->Count(TEXT("watering_can")) > 0 && Model->Water(Cell))
		{
			// the can's rest goes on the other thirsty cells
			Model->WaterAll();
			Toast(LyudText::Get(TEXT("garden.toast.watered")));
			bDone = true;
		}
		break;
	case LyudGarden::ENeed::Weed:
		if (Model->Weed(Cell))
		{
			Toast(LyudText::Get(TEXT("garden.toast.weeded")));
			bDone = true;
		}
		break;
	case LyudGarden::ENeed::Cover:
		Model->SetCover(true);
		Toast(LyudText::Get(TEXT("garden.toast.covered")));
		bDone = true;
		break;
	case LyudGarden::ENeed::Uncover:
		Model->SetCover(false);
		Toast(LyudText::Get(TEXT("garden.toast.uncovered")));
		bDone = true;
		break;
	case LyudGarden::ENeed::Clear:
		if (Model->Clear(Cell))
		{
			Toast(LyudText::Get(TEXT("garden.toast.cleared")));
			bDone = true;
		}
		break;
	case LyudGarden::ENeed::Pick:
	{
		LyudGarden::FHarvest H;
		if (Model->Pick(Cell, H))
		{
			const FName Item(*Str(H.Item));
			Inv->Add(Item, H.Count);
			Toast(LyudText::Get(TEXT("garden.toast.picked"), { FStringFormatArg(ULyudInventory::ItemName(Item).ToString()), FStringFormatArg(H.Count) }));
			ULyudQuests::Notify(this, TEXT("get"), Str(H.Item), H.Count);
			ULyudQuests::Notify(this, TEXT("do"), TEXT("garden_pick"));
			if (!Harvested.Contains(Str(H.Item)))
			{
				Harvested.Add(Str(H.Item));
				Note(TEXT("garden.diary.first_harvest"), { ULyudInventory::ItemName(Item).ToString() });
			}
			bDone = true;
			if (NeighbourBody && H.Quality >= 3)
			{
				NeighbourSay(Pick(TEXT("line.garden.praise"), 2, LastAdvice, (uint32)Cell + (uint32)Today()));
			}
		}
		break;
	}
	default:
		if (Inv->Count(TEXT("garden_compost")) > 0 && Model->Compost(Cell) && Inv->Remove(TEXT("garden_compost")))
		{
			Toast(LyudText::Get(TEXT("garden.toast.compost")));
			bDone = true;
		}
		break;
	}
	if (bDone)
	{
		ShowCells();
		ShowPlants(bNear);
	}
}

void ULyudGarden::OpenSow(int32 Cell)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Viewport || !Inv)
	{
		return;
	}
	SowCell = Cell;
	SowItems.Reset();
	FString Body;
	FText Title, Keys;
	if (Cell < 0)
	{
		// the committee's notice at the gate
		Title = LyudText::Get(TEXT("garden.board.title"));
		Body = LyudText::Get(TEXT("garden.board.text"), { FStringFormatArg(Model->Data.R.Rent) }).ToString();
		Keys = LyudText::Get(TEXT("garden.sow.keys"));
	}
	else
	{
		for (const LyudGarden::FCrop& K : Model->Data.Crops)
		{
			const FName Item(*Str(K.Seed));
			const int32 N = Inv->Count(Item);
			if (N > 0 && SowItems.Num() < PageSize)
			{
				SowItems.Add(Item);
				Body += FString::Printf(TEXT("%d. %s (%d)\n"), SowItems.Num(), *ULyudInventory::ItemName(Item).ToString(), N);
			}
		}
		if (SowItems.Num() == 0)
		{
			Toast(LyudText::Get(TEXT("garden.sow.none")));
			return;
		}
		Title = LyudText::Get(TEXT("garden.sow.title"));
		Keys = LyudText::Get(TEXT("garden.sow.keys"));
	}
	if (!Page)
	{
		Page = SNew(SLyudCityPage);
		Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 61);
	}
	Page->SetScreen(false);
	Page->SetPage(Title, FText::FromString(Body.TrimEnd()), Keys);
	SetPrompt(EPrompt::None, FText::GetEmpty());
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (PC && !MoveBlockedBy.IsValid())
	{
		PC->SetIgnoreMoveInput(true);
		MoveBlockedBy = PC;
	}
	PageFrame = GFrameCounter;
}

void ULyudGarden::TickSow(APlayerController* PC)
{
	UWorld* World = GetWorld();
	const ULyudInventoryUI* UI = World ? ULyudInventoryUI::Get(World) : nullptr;
	if (!World || !PC || (UI && UI->IsOpen()))
	{
		CloseSow();
		return;
	}
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if ((Menu && Menu->IsOpen()) || GFrameCounter == PageFrame)
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::SpaceBar) || (SowCell < 0 && PC->WasInputKeyJustPressed(EKeys::E)))
	{
		CloseSow();
		return;
	}
	static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	for (int32 I = 0; I < SowItems.Num() && I < PageSize; ++I)
	{
		if (!PC->WasInputKeyJustPressed(Digits[I]))
		{
			continue;
		}
		ULyudInventory* Inv = ULyudInventory::Get(World);
		const int32 Cell = SowCell;
		const FName Item = SowItems[I];
		if (Inv && Inv->Count(Item) > 0 && Model->Sow(Cell, Std(Item.ToString()), NowHours()) && Inv->Remove(Item))
		{
			const FString Crop = LyudText::Get(TEXT("garden.crop.") + Str(Model->Bed.Cells[(size_t)Cell].Crop)).ToString();
			Toast(LyudText::Get(TEXT("garden.toast.sown"), { FStringFormatArg(Crop) }));
			ULyudQuests::Notify(this, TEXT("use"), Item.ToString());
			ShowCells();
		}
		CloseSow();
		return;
	}
}

void ULyudGarden::CloseSow()
{
	if (Page)
	{
		UWorld* World = GetWorld();
		if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Page.ToSharedRef());
		}
		Page.Reset();
	}
	if (APlayerController* PC = MoveBlockedBy.Get())
	{
		PC->SetIgnoreMoveInput(false);
	}
	MoveBlockedBy.Reset();
	SowCell = -1;
	SowItems.Reset();
}

int32 ULyudGarden::HarvestQuality(const FString& Item) const
{
	return Model ? FMath::Clamp(Model->LastQuality(Std(Item)), 1, 3) : 2;
}

void ULyudGarden::OwnJam(int32 Quality)
{
	if (Model)
	{
		Model->JamJarQuality = FMath::Clamp(Quality, 1, 3);
	}
}

void ULyudGarden::Toast(const FText& Text) const
{
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->Toast(Text);
	}
}

void ULyudGarden::Note(const FString& Key, const TArray<FString>& Args) const
{
	if (ULyudQuests* Quests = ULyudQuests::Get(this))
	{
		Quests->AddNote(Key, Args);
	}
}

// --- the neighbour, the jam, the market's contest -----------------------------------------------------------------------------

void ULyudGarden::TickNeighbour(float DeltaTime)
{
	UWorld* World = GetWorld();
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	if (!World || !Life || NeighbourKey.IsEmpty())
	{
		return;
	}
	if (NeighbourId < 0)
	{
		NeighbourId = Life->FindPerson(NeighbourKey);
	}
	// she is at her bed on her mornings, if she is indoors and awake then (not out elsewhere), in no more than a drizzle,
	// and only while he is about (her body is taken over from the life for that time)
	const FDateTime Now = UGameCalendar::NowIn(World);
	const float Hour = (float)Now.GetHour() + (float)Now.GetMinute() / 60.f;
	const int32 Weekday = ((int32)Now.GetDayOfWeek() + 7) % 7;
	int32 Activity = -1;
	bool bDoing = false;
	EPersonPose Pose = EPersonPose::Hidden;
	FVector Where = FVector::ZeroVector;
	const bool bKnown = NeighbourId >= 0 && Life->GetActivityState(NeighbourId, Activity, bDoing, Pose, Where);
	const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(World);
	const bool bDry = !Weather || Weather->GetModelState().PrecipRate < 1.5f;
	const bool bWant = bNear && bKnown && NeighbourDays.Contains(Weekday) && Hour >= NeighbourHours.X && Hour < NeighbourHours.Y && bDry
		&& (NeighbourBody || Pose == EPersonPose::Hidden);
	if (bWant && !NeighbourBody)
	{
		const FString ModelName = Life->ShownModel(NeighbourId);
		if (ModelName.IsEmpty())
		{
			return;
		}
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		NeighbourBody = World->SpawnActor<ANpcBody>(NeighbourStand, FRotator(0.f, NeighbourYaw, 0.f), SP);
		if (!NeighbourBody || !NeighbourBody->Setup(ModelName, FString()))
		{
			if (NeighbourBody)
			{
				NeighbourBody->Destroy();
			}
			NeighbourBody = nullptr;
			return;
		}
		Life->SetPresentedExternally(NeighbourId, true);
		NeighbourBody->Drive(NeighbourStand, NeighbourYaw, EPersonPose::Shovel, 1, 0.f, 0.f, DeltaTime, true);
		AdviceClock = 4.f;
		LastAdvice.Reset();
		NeighbourSay(Pick(TEXT("line.garden.hello"), 3, LastAdvice, (uint32)Today()));
	}
	else if (!bWant && NeighbourBody)
	{
		NeighbourBody->Destroy();
		NeighbourBody = nullptr;
		Life->SetPresentedExternally(NeighbourId, false);
	}
}

void ULyudGarden::NeighbourSay(const FString& Key)
{
	ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this);
	if (Talk && NeighbourBody && Talk->SayLine(NeighbourBody, Key, NeighbourId))
	{
		LastAdvice = Key;
	}
}

void ULyudGarden::Advise()
{
	const uint32 Seed = (uint32)Today() * 31u + (uint32)(NowHours() * 4.0);
	const float Day = UGameCalendar::DayOfYear(UGameCalendar::NowIn(GetWorld()));
	if (Model->Bed.bRented)
	{
		float Coldest, Hottest;
		Soon(Coldest, Hottest);
		bool bCrops = false;
		for (const LyudGarden::FCell& C : Model->Bed.Cells)
		{
			bCrops |= C.State == LyudGarden::ECell::Growing || C.State == LyudGarden::ECell::Ripe;
		}
		if (bCrops && !Model->Bed.bCovered && Coldest < 1.f)
		{
			NeighbourSay(Pick(TEXT("line.garden.frost"), 2, LastAdvice, Seed));
			return;
		}
		// the bed's worst need, in her order
		static const LyudGarden::ENeed Order[] = { LyudGarden::ENeed::Pick, LyudGarden::ENeed::Cover, LyudGarden::ENeed::Uncover,
			LyudGarden::ENeed::Water, LyudGarden::ENeed::Weed, LyudGarden::ENeed::Clear, LyudGarden::ENeed::Sow, LyudGarden::ENeed::Dig };
		for (LyudGarden::ENeed N : Order)
		{
			for (int32 I = 0; I < (int32)Model->Bed.Cells.size(); ++I)
			{
				if (Model->Need(I, Coldest, Hottest) == N && (Hash(Seed) % 3u) != 0u)
				{
					NeighbourSay(Pick(FString(TEXT("line.garden.need.")) + Str(LyudGarden::NeedWord(N)), 2, LastAdvice, Seed));
					return;
				}
			}
		}
	}
	NeighbourSay(Pick(FString::Printf(TEXT("line.garden.season.%d"), LyudGarden::Season(Day)), 3, LastAdvice, Seed));
}

void ULyudGarden::TickJam()
{
	if (Model->JamQuality > 0 && NowHours() - Model->JamAt >= JamHours)
	{
		Model->JamJarQuality = Model->JamQuality;
		Model->JamQuality = 0;
		++JamJars;
	}
}

void ULyudGarden::TickContest()
{
	if (ContestDay < 0)
	{
		return;
	}
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	const int32 Day = Today();
	if (Day < ContestDay || (Day == ContestDay && Now.GetHour() < ContestHour))
	{
		return;
	}
	// the jury tastes: the better the strawberries were kept, the likelier a place (the honey seller has her own opinions)
	const float Roll = (float)(Hash((uint32)ContestDay * 2654435761u) % 1000u) / 1000.f;
	int32 Place = 0;
	if (ContestQuality >= 3)
	{
		Place = Roll < 0.5f ? 1 : (Roll < 0.85f ? 2 : 3);
	}
	else if (ContestQuality == 2)
	{
		Place = Roll < 0.3f ? 2 : (Roll < 0.7f ? 3 : 0);
	}
	else
	{
		Place = Roll < 0.25f ? 3 : 0;
	}
	const int32 Prize = Place > 0 && Prizes.IsValidIndex(Place - 1) ? Prizes[Place - 1] : 0;
	if (ULyudLetters* Letters = ULyudLetters::Get(this))
	{
		Letters->PostTownLetter(FString::Printf(TEXT("garden_contest_%d"), ContestDay), TEXT("garden.letter.from"),
			Place > 0 ? FString::Printf(TEXT("garden.letter.contest.%d"), Place) : FString(TEXT("garden.letter.contest.none")),
			{ FString::FromInt(Prize) });
	}
	if (Prize > 0)
	{
		if (ULyudInventory* Inv = ULyudInventory::Get(this))
		{
			Inv->Earn(Prize);
		}
	}
	Note(TEXT("garden.diary.contest"), { Place > 0 ? FString::Printf(TEXT("@garden.place.%d"), Place) : FString(TEXT("@garden.place.none")) });
	ULyudQuests::Notify(this, TEXT("do"), Place > 0 ? TEXT("garden_contest_place") : TEXT("garden_contest_done"));
	ContestDay = -1;
	ContestQuality = 0;
}

void ULyudGarden::OnFriendGreeted(int32 PersonId, int32 Level)
{
	if (!bLoaded || !Model->Bed.bRented || Level < 2 || PersonId < 0)
	{
		return;
	}
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	FPersonInfo Info;
	if (!Life || !Life->GetPerson(PersonId, Info))
	{
		return;
	}
	const int32 Day = Today();
	const int32* Last = SeedsGiven.Find(Info.Key);
	// a friend gives a packet now and then: at most once a week each, on about one greeting in three
	if ((Last && Day - *Last < 7) || Hash((uint32)Day * 7919u + (uint32)PersonId) % 3u != 0u)
	{
		return;
	}
	SeedsGiven.Add(Info.Key, Day);
	const FName Item(Seeds[Hash((uint32)PersonId + (uint32)Day) % UE_ARRAY_COUNT(Seeds)]);
	if (ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		Inv->Add(Item);
	}
	Toast(LyudText::Get(TEXT("garden.toast.friend_seeds"), { FStringFormatArg(Life->GetDisplayName(PersonId).ToString()),
		FStringFormatArg(ULyudInventory::ItemName(Item).ToString()) }));
	if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
	{
		Talk->SayLine(Info.Actor.Get(), Pick(TEXT("line.garden.gift"), 2, FString(), (uint32)Day + (uint32)PersonId), PersonId);
	}
}

// --- console and developer mode ------------------------------------------------------------------------------------------------

FString ULyudGarden::Report() const
{
	if (!bLoaded)
	{
		return TEXT("Garden: not loaded");
	}
	FString S = FString::Printf(TEXT("Garden: %s, film %s, can %d, jam %d (jars %d), contest day %d\n"), Model->Bed.bRented ? TEXT("rented") : TEXT("not rented"),
		Model->Bed.bCovered ? TEXT("down") : TEXT("up"), Model->CanLeft, Model->JamQuality, JamJars, ContestDay);
	float Coldest = 0.f, Hottest = 0.f;
	Soon(Coldest, Hottest);
	for (int32 I = 0; I < (int32)Model->Bed.Cells.size(); ++I)
	{
		const LyudGarden::FCell& C = Model->Bed.Cells[(size_t)I];
		S += FString::Printf(TEXT("  %d %-8s %-10s growth %.2f water %.2f weeds %.2f hurt %.2f care %.2f rich %.2f need %s\n"), I,
			*Str(LyudGarden::CellWord(C.State)), *Str(C.Crop), C.Growth, C.Water, C.Weeds, C.Hurt, C.Care, C.Rich,
			*Str(LyudGarden::NeedWord(Model->Need(I, Coldest, Hottest))));
	}
	return S;
}

void ULyudGarden::DevGrow(float Hours)
{
	if (!bLoaded)
	{
		return;
	}
	const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(this);
	LyudGarden::FAir A;
	if (Weather)
	{
		const LyudWeather::FWeatherState& S = Weather->GetModelState();
		A.TempC = S.TempC;
		A.Cloud = S.CloudCover;
		A.Wind = S.WindSpeed;
		A.SunElev = 30.f;
	}
	for (float T = 0.f; T < Hours; T += 1.f)
	{
		Model->Step(A, FMath::Min(1.f, Hours - T));
	}
	ShowCells();
	ShowPlants(bNear);
}

void ULyudGarden::DevKit()
{
	if (!bLoaded)
	{
		return;
	}
	Model->Rent(NowHours());
	Model->Fill();
	if (ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		for (const TCHAR* Item : { TEXT("spade"), TEXT("watering_can"), TEXT("garden_compost") })
		{
			if (Inv->Count(FName(Item)) == 0)
			{
				Inv->Add(FName(Item));
			}
		}
		for (const LyudGarden::FCrop& C : Model->Data.Crops)
		{
			Inv->Add(FName(*Str(C.Seed)), 2);
		}
	}
	ShowCells();
}

bool ULyudGarden::BedStand(FTransform& Out) const
{
	if (!bLoaded || HeroBed < 0)
	{
		return false;
	}
	const FBedPlace& P = Beds[HeroBed];
	// on the path seaward of the bed, facing it
	const FVector Front = FRotator(0.f, P.Yaw, 0.f).Vector();
	Out = FTransform(FRotator(0.f, P.Yaw + 180.f, 0.f), P.At + Front * 160.0 + FVector(0, 0, 100.0));
	return true;
}

namespace LyudGardenConsole
{
	ULyudGarden* Of(UWorld* World) { return World ? World->GetSubsystem<ULyudGarden>() : nullptr; }

	FAutoConsoleCommandWithWorldAndArgs GReport(TEXT("lyud.Garden.Report"), TEXT("The hero's allotment bed: every cell's state and need."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudGarden* G = Of(World)) { UE_LOG(LogLyudGarden, Display, TEXT("%s"), *G->Report()); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GGrow(TEXT("lyud.Garden.Grow"), TEXT("lyud.Garden.Grow <hours>: the bed grows that many hours in the weather of now."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudGarden* G = Of(World)) { G->DevGrow(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 24.f); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GKit(TEXT("lyud.Garden.Kit"), TEXT("Rents the bed and gives a spade, a full can, compost and seeds."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudGarden* G = Of(World)) { G->DevKit(); }
		}));
}

#if LYUD_GARDEN_DEV
namespace LyudGardenDev
{
	using namespace LyudDev;
	FAutoAction GKitAction(TEXT("garden.kit"), TEXT("garden"), TEXT("dev.action.garden.kit"),
		[](UWorld* W) { if (ULyudGarden* G = LyudGardenConsole::Of(W)) { G->DevKit(); } });
	FAutoAction GGrowAction(TEXT("garden.grow_day"), TEXT("garden"), TEXT("dev.action.garden.grow_day"),
		[](UWorld* W) { if (ULyudGarden* G = LyudGardenConsole::Of(W)) { G->DevGrow(24.f); } });
	FAutoAction GReportAction(TEXT("garden.report"), TEXT("garden"), TEXT("dev.action.garden.report"),
		[](UWorld* W) { if (ULyudGarden* G = LyudGardenConsole::Of(W)) { UE_LOG(LogLyudGarden, Display, TEXT("%s"), *G->Report()); } });
	FAutoTeleport GBedTp(TEXT("garden_bed"), TEXT("garden"), TEXT("dev.tp.garden_bed"), [](UWorld* W, FTransform& Out)
	{
		const ULyudGarden* G = LyudGardenConsole::Of(W);
		return G && G->BedStand(Out);
	});
}
#endif
