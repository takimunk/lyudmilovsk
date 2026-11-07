#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudSave.h"
#include "LyudRestaurant.generated.h"

struct FStreamableHandle;
class ALyudDoorBase;
class ALyudRestaurant;
class ANpcBody;
class AStaticMeshActor;
class SLyudRestMenu;
class UMaterialInstanceDynamic;
class UAudioComponent;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** One dish of the menu (Content/Restaurant/menu.json) with its tableware (Content/Restaurant/tableware.json dishes.<dish>). */
struct FLyudRestDishDef
{
	FName Id;            // the inventory item: its name, description and icon
	FName Dish;          // the meshes' name: SM_RF_<Dish>_<k>, SM_RF_Load_<Dish>_S|B, SM_RF_<Dish>_Fill
	FName Section;
	int32 Price = 0;
	float Cook = 60.f;
	float SmallBite = 0.08f, BigBite = 0.16f;
	bool bFree = false;
	bool bFestival = false;
	/** The weekday (0 Monday .. 6 Sunday) it is the daily special, -1 always on the menu. */
	int32 Special = -1;
	/** From tableware.json: the vessel mesh, where the food sits over its origin (cm), the utensil, the bite stages. */
	FName Vessel;
	float FoodZ = 1.4f;
	FName Fill;
	FName Utensil = TEXT("fork");     // fork, spoon, teaspoon, chopsticks, knife_fork, fingers, cup
	FName LoadS, LoadB;
	int32 Stages = 5;
	/** A liquid's fill: its width at level t (the bowl's walls slope), and the soups' level at each bite stage (cm, 0..1). */
	TArray<FVector2D> FillXY;
	TArray<float> StageFill;
	/** Cups: radius and half height of the vessel (cm; 0 = from the mesh). A coffee cup stands on a saucer SaucerZ cm high. */
	float CupRadius = 0.f, CupHalfHeight = 0.f;
	FName Saucer;
	float SaucerZ = 0.f;
	bool IsCup() const { return Utensil == TEXT("cup"); }
};

/**
 * A dish of the restaurant on a table (or on the waiter's tray): its vessel, the food as bite-stage meshes (a fill disc that goes
 * down for soups and drinks). On the hero's table, looked at from his chair, the left mouse button eats it through the shared bite
 * mechanic (ULyudBiteSubsystem: fork, spoon, chopsticks, the fingers; a cup to the lips; a knife in the left hand for the steak).
 * Root at the vessel's origin (cups: their middle, for the held-cup pose). NotPlaceable.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALyudRestDish : public AActor
{
	GENERATED_BODY()

public:
	ALyudRestDish();

	void Init(ALyudRestaurant* InOwner, const FLyudRestDishDef& InDef, float InLeft);
	void SetLeft(float InLeft);
	float GetLeft() const { return Left; }
	const FLyudRestDishDef& GetDef() const { return Def; }
	bool IsEmpty() const { return Left <= 0.f; }
	bool IsEating() const { return bEating; }
	/** On the hero's table: looked at and eaten with LMB (props of the townspeople's tables never tick). */
	void SetEdible(bool bOn);
	/** Where it stands on the table (not in the hand). */
	void SetRest(const FTransform& T) { Rest = T; SetActorTransform(T); }
	const FTransform& RestTransform() const { return Rest; }
	/** The rest's offset of the vessel's bottom from the root (cups hang from their middle). */
	float BottomZ() const { return Bottom; }
	float Radius() const { return VesselRadius; }
	float SecondsEmpty() const { return EmptyFor; }
	/** The cup's rest over the table: the saucer's height (cm). */
	float SaucerZ() const { return Def.SaucerZ; }
	/** The order it was cooked for (ALyudRestaurant's stable order id), -1 for none. */
	int32 OrderId = -1;
	bool StartEating();

	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Vessel;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> VesselGlass;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Under;      // a saucer, a plate under a bowl
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Food;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Fill;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> StageMeshes;
	/** The saucer left on the table while the cup is at the lips. */
	UPROPERTY(Transient) TObjectPtr<AStaticMeshActor> SaucerLeft;

	TWeakObjectPtr<ALyudRestaurant> Owner;
	FLyudRestDishDef Def;
	float Left = 1.f;
	FTransform Rest;
	float Bottom = 0.f;
	float VesselRadius = 12.f;
	float VesselHalfHeight = 1.f;
	float FillScaleZ = 1.f;
	bool bEating = false;
	bool bEdible = false;
	bool bPrompted = false;
	float EmptyFor = 0.f;

	void ShowPrompt(bool bOn);
	void OnMealEnd(float InLeft);
};

/**
 * Ресторан «Янтарь» on the city plan's plot P-RESTAURANT over the promenade (docs/restaurant.md): a round glass pavilion under a
 * concrete disc roof (blender/restaurant/building.py). Placed by unreal/scripts/import_restaurant.py at the hall's floor centre, the
 * layout in Content/Restaurant/layout.json (the building's frame, metres: +x to the sea, -y the main door).
 *
 * The hero sits down at a free table (E at a chair; the body sits, the eyes come down to a seated height), opens the menu card on the
 * table (LMB), orders (the money is taken; bread and water come first, free), and each dish is cooked for its own time and carried
 * from the kitchen pass on a tray by a waiter (a resident on their shift: UPeopleLifeSubsystem::StaffOnShift("restaurant", "waiter"),
 * drawn here while serving; before the town life's staff exist, a stand-in), who sets it down at his place. He eats seated, bite by
 * bite (ALyudRestDish). Empty plates are cleared away; the bill is brought on asking. Open while a waiter is on shift (or by the hours
 * in restaurant.json), lit inside then, the soffit and the sign at night; locked when closed and nobody is inside.
 *
 * Console: lyud.Restaurant.State, .Open 1|0|-1, .Sit [table], .Stand, .Menu, .Order <id> [..], .Bill, .Eat, .WaitScale <x>,
 * lyud.Restaurant 0|1 (hidden, the benchmark's A/B), lyud.Restaurant.Festival 0|1.
 */
UCLASS()
class LYUDMILOVSK_API ALyudRestaurant : public AActor, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ALyudRestaurant();

	static ALyudRestaurant* Find(const UObject* WorldContext);
	static const TArray<FLyudRestDishDef>& Menu();
	static const FLyudRestDishDef* FindDish(FName Id);
	static UStaticMesh* RestMesh(FName Name);
	/** Festival day (the holidays thread's subsystem when it exists, or lyud.Restaurant.Festival 1). */
	static bool IsFestival(const UObject* WorldContext);
	/** The holidays' own dishes of today (ULyudFestivalSubsystem::IsFestivalDish, by name): shown in the festival section too. */
	static bool IsFestivalDish(const UObject* WorldContext, FName Id);
	/** In the menu card's Section today. */
	static bool IsInSectionToday(const UObject* WorldContext, const FLyudRestDishDef& D, FName Section);
	/** The price today: the catch of the day is cheaper after the hero sold fish at the kitchen door (the fishing's kitchen "yantar"). */
	int32 PriceToday(const FLyudRestDishDef& D) const;
	bool IsCatchOfDay(const FLyudRestDishDef& D) const;
	/** On the menu today: not a festival dish on an ordinary day, a daily special only on its weekday (the game's calendar). */
	static bool IsOnMenuToday(const UObject* WorldContext, const FLyudRestDishDef& D);

	/** The building's frame (metres: +x to the sea, y across, z over the floor) to the world (cm) and back. */
	FVector ToWorld(const FVector& M) const;
	FVector ToLocal(const FVector& W) const;
	FRotator YawToWorld(float LocalYawDeg) const;

	bool IsOpen() const { return bOpen; }
	bool IsHeroInside() const;
	bool IsHeroSeated() const;
	int32 HeroTable() const { return HeroTableIx; }

	/** Sits the hero down on seat K (any free hero seat when -1); false when it is taken or he can't. */
	bool SitHero(int32 Seat = -1);
	void StandHero();
	void OpenMenu();
	void CloseMenu();
	bool IsMenuOpen() const { return Menu_.IsValid(); }
	/** Orders these dishes for the hero's table: pays for them (false and nothing ordered when he can't). */
	/** Why an order was refused (PlaceOrder). */
	enum class EOrderResult : uint8 { Ok, Closed, NotSeated, NoMoney, KitchenClosing, Unknown };
	/** The last 20 game minutes before closing: the kitchen takes no new orders. */
	bool IsKitchenClosing() const { return bKitchenClosing; }
	EOrderResult PlaceOrder(const TMap<FName, int32>& Items);
	void AskBill();
	/** The hero's table's dish nearest the look (or the first): starts eating it. */
	bool EatNearest(FName Id = NAME_None);
	/** Eats Id up at once (tests: the waiter then clears it). */
	bool FinishDish(FName Id);
	/** Turns the hero's look to a point of the building's frame (tests). */
	void LookAtLocal(const FVector& M);
	int32 TabTotal() const { return Tab; }
	bool IsBillAsked() const { return bBillAsked; }
	/** Orders waiting in the kitchen or being carried (the hero's only, or the guests' too). */
	int32 PendingCount(bool bHeroOnly = true) const;
	FString StateReport() const;
	void DishGone(ALyudRestDish* Dish);

	/** -1 by the staff and hours, 0 closed, 1 open (checks). */
	int32 ForceOpen = -1;
	float WaitScale = 1.f;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaTime) override;

	/** The save keeps what the hero paid for and wasn't served yet: on loading it is given back (the visit itself isn't kept). */
	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;
	UPROPERTY(SaveGame)
	int32 SavedRefund = 0;
	/** The game day (of the year) the hero sold fish to this kitchen: the catch of the day then. */
	UPROPERTY(SaveGame)
	int32 CatchDay = -1;

private:
	struct FSeat
	{
		FVector At;          // local metres: the floor under the seat's front edge
		float Yaw = 0.f;     // local deg, the way the sitter faces
		float Height = 0.46f;
		FVector Place;       // local metres: the plate's spot on the table
		int32 Table = -1;
	};
	struct FTable
	{
		FName Id;
		FVector At;
		bool bHero = false;
		TArray<int32> Seats;
		FVector ServeAt;
		FName Node;
		TWeakObjectPtr<AStaticMeshActor> MenuCard;
	};
	struct FOrder
	{
		/** Stable while the order lives (Orders' indices shift as orders go). */
		int32 Id = 0;
		FName Item;
		float Due = 0.f;     // world seconds when it is ready at the pass
		int32 Seat = -1;
		enum class EState : uint8 { Cooking, Ready, Carried, Served } State = EState::Cooking;
		/** Ready: the plate standing on the pass's shelf (Shelf), until the waiter takes it. */
		TWeakObjectPtr<ALyudRestDish> Dish;
		int32 Shelf = -1;
		/** For a townsperson at their table (not the hero's tab). */
		bool bGuest = false;
	};
	enum class EWaiter : uint8 { Off, Idle, ToPass, Pick, ToTable, SetDown, Collect, ToPassBack, Drop, ToHome };
	enum class EJob : uint8 { None, Serve, Clear, Bill };

	// layout
	bool bLayout = false;
	float HallR = 9.85f, HallTop = 4.f, ChordX = -8.4f;
	FVector KitchenMin, KitchenMax;
	TArray<FSeat> Seats;
	TArray<FTable> Tables;
	TArray<FVector> PassShelf;
	FVector PassStand, WaiterHome, CookAt, BackDoorOut, BackDoorIn;
	float PassYaw = 0.f, HomeYaw = 0.f, CookYaw = 0.f;
	TMap<FName, FVector2D> Nodes;
	TMap<FName, TArray<FName>> Edges;
	TArray<TWeakObjectPtr<ALyudDoorBase>> Doors;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> InsideLights;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> OutsideLights;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMIDs;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> SignMIDs;
	/** The localized plaque on the menu stand by the main door (its picture swapped with the language). */
	UPROPERTY(Transient) TObjectPtr<AStaticMeshActor> Plaque;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PlaqueMID;
	FVector MenuSpot = FVector::ZeroVector;

	// state
	bool bOpen = false, bLit = false, bNight = false, bLocked = false;
	bool bKitchenClosing = false;
	/** Fish sold at the kitchen door (the fishing's ULyudFishingState KitchenSold, read by name), the game day it last happened. */
	int32 FishSeenTaken = -1;
	int32 FishSeenDay = -2;
	/** Updates to wait after play start or a load (the fishing's state is restored a tick later) before counting sales. */
	int32 FishSettle = 2;
	/** The hero at the back door with fish (the fishing's kitchen point): the cook comes to the door. */
	bool bCookAtDoor = false;
	void UpdateFishing();
	/** The game time since which everything for locking up has held (nobody left inside, staff gone). */
	FDateTime LockReadySince;
	bool bLockReady = false;
	bool AnyoneOfLifeInside() const;
	float StateEvery = 0.f;
	float PropsEvery = 0.f;

	// the hero's visit
	int32 HeroSeatIx = -1, HeroTableIx = -1;
	float SatAt = 0.f;
	bool bFreeBrought = false;
	bool bBillAsked = false, bBillServed = false;
	int32 Tab = 0;
	float AwayFor = 0.f;
	bool bSitPrompt = false;
	FString Prompted;
	TArray<FOrder> Orders;
	int32 NextOrderId = 0;
	FOrder& AddOrder(FName Item, int32 Seat, float Due, bool bGuest);
	FOrder* FindOrder(int32 Id) { return Orders.FindByPredicate([Id](const FOrder& O) { return O.Id == Id; }); }
	UPROPERTY(Transient) TArray<TObjectPtr<ALyudRestDish>> HeroDishes;
	TSharedPtr<SLyudRestMenu> Menu_;

	// the waiter
	EWaiter Waiter = EWaiter::Off;
	EJob Job = EJob::None;
	int32 WaiterPerson = -1;
	float WaiterT = 0.f;
	FVector WaiterPos = FVector::ZeroVector;     // world, the floor
	float WaiterYaw = 0.f;
	TArray<FVector> Path;                        // world points still to walk
	int32 SetIndex = 0;
	FTransform SetFrom;
	UPROPERTY(Transient) TObjectPtr<ANpcBody> WaiterBody;
	UPROPERTY(Transient) TObjectPtr<AStaticMeshActor> Tray;
	UPROPERTY(Transient) TObjectPtr<AStaticMeshActor> BillFolder;
	UPROPERTY(Transient) TArray<TObjectPtr<ALyudRestDish>> OnTray;
	TArray<int32> OnTrayOrders;
	FString WaiterMesh;

	// the kitchen (LyudRestaurantKitchen.cpp): the cook at the stove with the pan, the bell on the pass, the room's sounds
	int32 CookPerson = -1;
	FString CookMesh;
	float CookT = 0.f;
	float BellT = -1.f;
	UPROPERTY(Transient) TObjectPtr<ANpcBody> CookBody;
	UPROPERTY(Transient) TObjectPtr<AStaticMeshActor> Pan;
	UPROPERTY(Transient) TObjectPtr<AStaticMeshActor> PanFood;
	UPROPERTY(Transient) TArray<TObjectPtr<AStaticMeshActor>> KitchenProps;
	UPROPERTY(Transient) TObjectPtr<AStaticMeshActor> BellPlunger;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> RoomLoop;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> MusicLoop;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> KitchenLoop;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> SizzleLoop;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> SoundSpots;
	FVector BellAt;
	void TickKitchen(float DeltaTime);
	void ReleaseCook();
	void SetLoops(bool bOpenNow, bool bCooking);
	/** A plate is ready: onto the pass's shelf, the bell rung. */
	void PutOnPass(FOrder& Order);
	void Effect(FName Pool, const FVector& Where, float Volume = 1.f) const;
	AStaticMeshActor* SpawnProp(FName Mesh, const FTransform& At);

	// cached once (BeginPlay) and refreshed on a timer, never per frame
	FString WaiterStandinMesh, CookStandinMesh;
	float ClearAfterS = 25.f, WaiterSpeed = 125.f, TableResetS = 90.f, BreadAfterS = 12.f;
	FName PlaceId = TEXT("restaurant");
	float StaffEvery = 0.f;
	int32 StaffWaiter = -1, StaffCook = -1;
	FString StaffWaiterMesh, StaffCookMesh;
	void RefreshStaff();
	/** The camera within reach of seeing the restaurant (bodies posed, hands solved only then). */
	bool bNear = false;
	float StateAge = 0.f;
	EWaiter LastWaiter = EWaiter::Off;
	/** Every mesh of /Game/Restaurant/Meshes, loaded in the background at BeginPlay and held here (RestMesh looks them up). */
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> MeshRefs;
	TSharedPtr<FStreamableHandle> Preload;
	void PreloadMeshes();

	// the knife in the left hand while the steak is eaten
	UPROPERTY(Transient) TObjectPtr<AStaticMeshActor> Knife;

	// the townspeople's dishes (props at their places)
	struct FProp
	{
		int32 Seat = -1;
		int32 Person = -1;
		TWeakObjectPtr<ALyudRestDish> Dish;
		/** LyudRestaurantGuests.cpp: reading the menu card, ordered, served and eating. */
		float SatAt = 0.f;
		bool bOrdered = false;
		float BiteT = 0.f;
		TWeakObjectPtr<AStaticMeshActor> Card;
		TWeakObjectPtr<AStaticMeshActor> Fork;
		/** Their drink (the main is Dish). */
		TWeakObjectPtr<ALyudRestDish> Cup;
		/** Their body now (refreshed every 2 s with the seats, not looked up each frame). */
		TWeakObjectPtr<ANpcBody> Body;
	};
	TArray<FProp> Props;
	/** The table the waiter's job is for. */
	int32 JobTable = -1;
	void TickGuests(float DeltaTime);
	/** bPaid: they got up and paid (coins); false when the visit is just dropped (EndPlay, a load). */
	void GuestLeft(FProp& P, bool bPaid);
	/** Staff of the place on shift that the restaurant hides (all but the waiter and the cook it draws). */
	TSet<int32> HiddenStaff;
	void RefundNextTick();

	void LoadLayout();
	void FindParts();
	void SpawnMenuCards();
	void UpdateState();
	void ApplyLights();
	void ApplySigns();
	FString SignLanguage;

	void TickHero(float DeltaTime);
	void TickKnife();
	void TickProps();
	void ResetHeroTable();
	int32 SeatNearHero(float MaxCm) const;
	bool IsSeatTaken(int32 Seat) const;
	FVector StandOffsetFor(int32 Seat) const;
	ALyudRestDish* SpawnDish(FName Item, const FTransform& At, float Left);
	FTransform PlaceTransform(int32 Seat, int32 Slot, const FLyudRestDishDef& Def) const;
	void Toast(const FText& Text) const;
	void SetPrompt(const FString& Key, const TArray<FStringFormatArg>& Args = TArray<FStringFormatArg>());

	// the waiter (LyudRestaurantWaiter.cpp)
	void TickWaiter(float DeltaTime);
	bool EnsureWaiter();
	void ReleaseWaiter();
	void PlanPath(const FVector& ToLocalM);
	bool WalkPath(float DeltaTime);
	void PoseWaiter(float DeltaTime, bool bWalking);
	FTransform TrayWorld() const;
	void StartJob();
	FName NearestNode(const FVector2D& P) const;
};
