#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudDiner.generated.h"

class ALyudDiner;
class ALyudDoorBase;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** How a diner dish is served and eaten (Content/Food/food.json "foods".<item> with "plated": true; docs/diner.md). */
struct FLyudDinerDishDef
{
	FName Item;
	/** fork, spoon (from the dish on the counter), cup (the vessel comes to the lips), hand (the food is picked up: ULyudHandUse). */
	FName Utensil = TEXT("fork");
	/** plate, bowl, mug, tea_glass. */
	FName Dish = TEXT("plate");
	/** SM_Diner_<Food> on the plate, SM_Diner_<Fill> the liquid column in the vessel (scaled in Z as it is drunk). */
	FName Food;
	FName Fill;
	/** What rides on the fork/spoon for a small and a big bite (SM_Diner_<Load>). */
	FName LoadSmall;
	FName LoadBig;
	FLinearColor FillTint = FLinearColor::Transparent;
	float SmallBite = 0.08f;
	float BigBite = 0.18f;
};

/**
 * A dish served at the diner: a plate (or bowl) on the counter with its food, or a mug / a glass in a holder with its drink. Looked at
 * from close by, the left mouse button eats it (ULyudBiteSubsystem: a fork or a spoon from the plate, the cup to the lips; the burger
 * is picked up and bitten in the hand: ULyudHandUse). Eaten up, the empty dish stays a while and is cleared away.
 * Root at the vessel's middle (a cup's centre, a plate's bottom), so the bite's held-cup pose holds it right. NotPlaceable.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALyudDinerDish : public AActor
{
	GENERATED_BODY()

public:
	ALyudDinerDish();

	void Init(ALyudDiner* InDiner, const FLyudDinerDishDef& InDef, float InLeft, const FTransform& InRest);
	void SetLeft(float InLeft);
	float GetLeft() const { return Left; }
	FName GetItem() const { return Def.Item; }
	bool IsEmpty() const { return Left <= 0.f; }
	bool IsCup() const { return Def.Utensil == TEXT("cup"); }
	/** Where it stands when not in the hand (the counter, the coffee table's tray). */
	const FTransform& RestTransform() const { return Rest; }
	void SetRest(const FTransform& T) { Rest = T; SetActorTransform(T); }
	/** Not to be eaten yet (a mug being poured into). */
	void SetReady(bool bIn) { bReady = bIn; }
	/** The fill shows (coffee being poured): 0..1 of the vessel. */
	void SetPoured(float Share);
	/** Starts eating it (the left button went down while the hero looks at it). */
	bool StartEating();
	float HalfHeight() const;
	float Radius() const;

	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Vessel;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> VesselGlass;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Food;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Fill;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FillMID;

	TWeakObjectPtr<ALyudDiner> Diner;
	FLyudDinerDishDef Def;
	float Left = 1.f;
	FTransform Rest;
	bool bEating = false;
	bool bReady = true;
	bool bPrompted = false;
	float EmptyFor = 0.f;
	float FoodZ = 0.f;

	void ShowPrompt(bool bOn);
	void OnMealEnd(float InLeft);
};

/**
 * The diner by the lower station (docs/diner.md): a 1940 Sterling Streamliner lunch car (blender/diner/building.py). Placed by
 * unreal/scripts/import_diner.py at the car's origin (its floor), its layout in Content/Diner/layout.json (the car's frame, metres).
 *
 * Open while one of its waitresses is on her shift (people.json duties work_diner; counters.json sells the menu over the counter):
 * the lamps burn, the doors are unlocked. When the last of them has gone home the lamps go out and the doors are locked (not
 * while the hero is still inside: they lock behind him). What is bought at the counter is not handed over but served on a plate
 * at the counter in front of the hero a few seconds later (ALyudDinerDish). Self service: E at the coffee table pours a mug of
 * coffee from the urn (the hand on the tap's lever), the mug then drunk like any cup.
 *
 * Seats: every booth and nose-bench place and every stool (layout.json "hero_seats", blender/diner/seating.py). Looking at a free
 * one from close by, E sits the hero down on it (AHeroCharacter::SitOn: the body seated, the eyes at a sitter's height, the view
 * turned to the table); E, a step or Space gets him up again, out to the aisle. A dish bought while he sits at a booth is served
 * on his table; at a stool on the counter in front of him. Polled, never consumed: E still reaches doors and the coffee urn.
 *
 * Console: lyud.Diner.State, lyud.Diner.Serve <item>, lyud.Diner.Coffee, lyud.Diner.Open 1|0|-1 (force open/closed/by staff),
 * lyud.Diner.Sit <seat>|-1 (sit on a seat / get up), lyud.Diner.Use (what E does at a seat), lyud.Diner.Seats.
 */
UCLASS()
class LYUDMILOVSK_API ALyudDiner : public AActor
{
	GENERATED_BODY()

public:
	ALyudDiner();

	static ALyudDiner* Find(const UObject* WorldContext);
	static bool FindDishDef(FName Item, FLyudDinerDishDef& Out);
	static UStaticMesh* DinerMesh(FName Name);

	/** The car's frame (metres: x along it, y across, -y the street front, z over the floor) to the world (cm) and back. */
	FVector ToWorld(const FVector& M) const;
	FVector ToLocal(const FVector& W) const;
	FRotator YawToWorld(float LocalYawDeg) const;

	bool IsOpen() const { return bOpen; }
	bool IsHeroInside() const;
	/** Puts a dish of Item on the counter in front of the hero (the nearest free serving place); null when it isn't a diner dish. */
	ALyudDinerDish* Serve(FName Item);
	/** Starts pouring a mug at the coffee table (E there does it); false if a pour runs or the hero can't pay. */
	bool PourCoffee();
	void DishGone(ALyudDinerDish* Dish);

	/** What E does about seats: gets the hero up if he sits, else sits him on the free seat he looks at. False: nothing to do. */
	bool UseSeat();
	/** Sits the hero on seat Index of the layout's hero seats (false: no such seat or it's taken); -1 gets him up. */
	bool SitHero(int32 Index);
	int32 NumSeats() const { return Seats.Num(); }
	/** The seat the hero sits on, -1 none. */
	int32 HeroSeatIndex() const { return HeroSeat; }
	/** Seat Index's description for the checks: kind, the cushion's world point, its yaw. */
	FString DescribeSeat(int32 Index) const;

	/** -1 by the staff, 0 closed, 1 open (checks). */
	int32 ForceOpen = -1;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaTime) override;

	static constexpr int32 CoffeePrice = 8;

private:
	struct FServing
	{
		FVector Plate;          // local metres, on the counter (or a booth's table)
		FVector Stool;
		TWeakObjectPtr<ALyudDinerDish> Dish;
		bool bTable = false;    // a booth's or the nose's table: only for the hero sitting at it
	};
	/** A seat the hero can take (layout.json "hero_seats"). */
	struct FSeat
	{
		FName Kind;             // booth, bench, stool
		FVector At;             // the floor under the cushion's front edge (a stool's: its foot rail), local metres
		float Yaw = 0.f;        // the way the sitter faces, local degrees
		float SeatZ = 0.46f;    // the cushion's top over At, metres
		FVector Exit;           // the floor point getting up leaves him on (local metres), found at load
		int32 Serving = -1;     // where his plate goes (Serving index), -1 none
	};
	struct FPending
	{
		FName Item;
		int32 Spot = -1;
		float Due = 0.f;
	};
	enum class EPour : uint8 { None, ToTray, Carry, Lever, Pour, Back };

	TArray<FServing> Serving;
	TArray<FPending> Pending;
	TArray<FSeat> Seats;
	int32 HeroSeat = -1;
	float SeatPollIn = 0.f;
	float StandPromptFor = 0.f;
	int32 SeatLooked = -1;
	/** The prompt key of the seats (the coffee's is the actor itself). */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> SeatPromptKey;
	TArray<FString> Staff;
	TArray<FVector> InsideBox;          // x0 x1 / y0 y1 / z0 z1 (local metres): the car, then the kitchen
	FVector CoffeeTap, CoffeeCup, CoffeeMugs, CoffeeStand;
	TArray<TWeakObjectPtr<ALyudDoorBase>> Doors;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> InsideLights;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> OutsideLights;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMIDs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ALyudDinerDish>> Dishes;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Stream;

	bool bOpen = false;
	bool bLit = false;
	bool bNight = false;
	bool bLocked = false;
	bool bLayout = false;
	float StateEvery = 0.f;
	FString SignLanguage;
	bool bCoffeePrompt = false;

	EPour Pour = EPour::None;
	float PourT = 0.f;
	TWeakObjectPtr<ALyudDinerDish> PourMug;

	void LoadLayout();
	void FindParts();
	void UpdateState();
	void ApplyOpen(bool bInOpen);
	void ApplyLights();
	void ApplySigns();
	void OnBought(FName Item, int32 Count);
	int32 FreeSpotNear(const FVector& WorldPos) const;
	/** Where the next dish goes: the hero's own table when he sits at one and it's free, else the counter's nearest free place. */
	int32 SpotForHero() const;
	void LoadSeats(const TSharedPtr<FJsonObject>& L);
	FTransform SeatFrameWorld(const FSeat& S) const;
	FVector SeatCushionWorld(const FSeat& S) const;
	int32 SeatInView() const;
	bool SeatTaken(int32 Index) const;
	void TickSeats(float DeltaTime);
	ALyudDinerDish* SpawnDish(FName Item, const FTransform& At, float Left);
	void TickCoffee(float DeltaTime);
	void PoseHandAt(const FVector& Palm, const FVector& Finger, const FVector& Normal, float Curl, const struct FReachShape* Shape);
};
