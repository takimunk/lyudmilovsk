#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include <vector>
#include "PeopleLife.generated.h"

namespace LyudPeople { class FWorld; class FManners; }
class ABoothOwner;
class ANpcBody;
class ALyudBike;
class USkeletalMeshComponent;
struct FLyudBikeRide;

/** How someone looks now (the simulation's poses, Private/People/PeopleSim.h EPose, in the same order). */
UENUM(BlueprintType)
enum class EPersonPose : uint8 { Hidden, Stand, Walk, Talk, Look, Sit, Computer, Eat, Read, Sleep, Work, Shovel };

USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FPersonInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "People") int32 Id = -1;
	/** owner, clerk, streamer, ... : npc.<key>.name in the strings. */
	UPROPERTY(BlueprintReadOnly, Category = "People") FString Key;
	/** The model shown: /Game/Characters/Humans/<Mesh>/SK_<Mesh> (an existing one while their own body isn't made). */
	UPROPERTY(BlueprintReadOnly, Category = "People") FName Mesh;
	/** Their own body's id (people.json body; = Mesh for the club's people): loaded first, and their [<Body>] in npc_dialogue.txt. */
	UPROPERTY(BlueprintReadOnly, Category = "People") FName Body;
	UPROPERTY(BlueprintReadOnly, Category = "People") int32 Age = 0;
	UPROPERTY(BlueprintReadOnly, Category = "People") FString Occupation;
	UPROPERTY(BlueprintReadOnly, Category = "People") FString Household;
	/** Their flat (e.g. Novikova1-2-17), empty if homeless. */
	UPROPERTY(BlueprintReadOnly, Category = "People") FString Home;
	/** What they do (activities.json id) and where (place id). */
	UPROPERTY(BlueprintReadOnly, Category = "People") FString Activity;
	UPROPERTY(BlueprintReadOnly, Category = "People") FString Place;
	/** idle, going, riding, settling, doing, leaving, away, waiting (for the train on a halt's platform). */
	UPROPERTY(BlueprintReadOnly, Category = "People") FString State;
	UPROPERTY(BlueprintReadOnly, Category = "People") EPersonPose Pose = EPersonPose::Stand;
	UPROPERTY(BlueprintReadOnly, Category = "People") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "People") float Yaw = 0.f;
	/** hunger, tired, social, fun, nature, mind, sun: 0 fine .. 1 urgent. */
	UPROPERTY(BlueprintReadOnly, Category = "People") TMap<FName, float> Needs;
	/** Who they talk to now (-1 nobody). */
	UPROPERTY(BlueprintReadOnly, Category = "People") int32 Partner = -1;
	/** What shows them now: their body walking about, the placed club actor at their station, the owner's booth actor. */
	UPROPERTY(BlueprintReadOnly, Category = "People") TObjectPtr<AActor> Actor = nullptr;
};

/** What the save keeps per person (by key). */
USTRUCT()
struct LYUDMILOVSK_API FPersonMemory
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) TArray<float> Needs;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPeopleLifeEvent, int32, PersonId, FName, Event, int32, Other);
/** A flat's (or a shop's) windows light up or go dark: somebody home and awake after dusk (docs/town_life.md). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPeopleFlatLight, FName, FlatId, bool, bLit);

/** How people spend a gathering (SetGathering): talking in groups, facing a stage or the fireworks, seated, about the stalls. */
UENUM(BlueprintType)
enum class EPeopleGatherMode : uint8 { Mingle, Watch, Sit, Fair };

/**
 * The town's people (docs/people.md): everybody who sits in the club has a life: a profile (name, age, occupation,
 * household, temper), needs (food, sleep, company, fun, fresh air, the mind, the sun), a flat of their own in the
 * Academgorodok (reserved for their household and kept with the save) and a day by the game's clock: sleep, school
 * or work, and free time in which they choose for themselves among the activities of Content/PeopleData/activities.json
 * (the club and its computers, the library, walks, benches, the sunset, talking to each other, a meal at home, town).
 * Slots (beds, chairs, computers, benches) are reserved so nobody takes a place somebody else has. They walk the
 * town's walk grid, the Academgorodok's stairs and flats and the club's corridor, ride the cable car between the
 * valley and the club's spur and the sea railway's train (ULyudRailwaySubsystem's timetable) to the sea island and back.
 * Besides the club's people, the Academgorodok's residents (people.json with a body of their own): Laboratory No. 2's
 * staff, the kiosk's assistant, pensioners, a family, a fisherman, a painter, an ornithologist.
 *
 * The simulation is plain C++ (Private/People/PeopleSim.cpp; tools/peoplesim runs it outside the engine). Near the
 * camera a person is an ANpcBody (their MakeHuman model with the shared clips); at their own station in the club the
 * level's placed actor shows them (as before), the owner at his booth is ABoothOwner (as before).
 * Starts by itself in Lvl_Scene01; lyud.People 0 turns it off (the club as it was).
 */
UCLASS()
class LYUDMILOVSK_API UPeopleLifeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UPeopleLifeSubsystem();
	virtual ~UPeopleLifeSubsystem() override;

	static UPeopleLifeSubsystem* Get(const UObject* WorldContext);
	/** For Blueprints and Python: unreal.PeopleLifeSubsystem.get_people_life(world). */
	UFUNCTION(BlueprintPure, Category = "People", meta = (WorldContext = "WorldContext"))
	static UPeopleLifeSubsystem* GetPeopleLife(const UObject* WorldContext) { return Get(WorldContext); }

	UFUNCTION(BlueprintPure, Category = "People")
	bool IsRunning() const { return Sim.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "People")
	int32 NumPeople() const;

	UFUNCTION(BlueprintCallable, Category = "People")
	bool GetPerson(int32 Id, FPersonInfo& Out) const;

	/** By key (owner, clerk, streamer, ...); -1 if there is nobody of that key. */
	UFUNCTION(BlueprintPure, Category = "People")
	int32 FindPerson(const FString& Key) const;

	/** An activity's index (activities.json id), -1 if none: for GetActivityState. */
	int32 FindActivity(const FString& Id) const;

	/** A cheap read for per-frame checks (no strings built, unlike GetPerson): the person's activity index (-1 none), whether
	 *  they are at it ("doing"), their pose and where they are (cm). False if there is no such person. */
	bool GetActivityState(int32 Id, int32& Activity, bool& bDoing, EPersonPose& Pose, FVector& Location) const;

	/** Sends someone to do something now (activities.json id), at a place (its id) or wherever suits them. */
	UFUNCTION(BlueprintCallable, Category = "People")
	bool Order(int32 Id, const FString& Activity, const FString& PlaceId = TEXT(""));

	/** Moves a household into a flat (its id, e.g. Novikova1-2-17); false if it is not free. Kept with the save. */
	UFUNCTION(BlueprintCallable, Category = "People")
	bool SetHome(const FString& Household, const FString& FlatId);

	/** The flats and who lives in them (id -> residents' keys), for the housing view and tests. */
	UFUNCTION(BlueprintCallable, Category = "People")
	TMap<FString, FString> Flats() const;

	/** One line: who does what (console lyud.People.Report). */
	UFUNCTION(BlueprintCallable, Category = "People")
	FString Report() const;

	UFUNCTION(BlueprintCallable, Category = "People")
	FString Describe(int32 Id) const;

	/**
	 * The model a person is shown by (/Game/Characters/Humans/<Model>/SK_<Model>): their own body, else the stand-in
	 * people.json names while it is unmade if that model is nobody else's; "" if none (then they live unseen, never in
	 * somebody else's face). One model, one person: ResolveModels.
	 */
	UFUNCTION(BlueprintPure, Category = "People")
	FString ShownModel(int32 Id) const;

	/** Whose model this is (-1 nobody's). */
	UFUNCTION(BlueprintPure, Category = "People")
	int32 ModelOwner(const FString& Model) const;

	/**
	 * Everybody seen in the world now, whatever shows them (the life's bodies, the club's placed actors, the booth's
	 * owner, any other human model in the level), counted by whose model it is: one line per person seen ("key xN at
	 * ..."), Duplicates = how many people are seen more than once (console lyud.People.Unique).
	 */
	UFUNCTION(BlueprintCallable, Category = "People")
	FString InstanceReport(int32& Duplicates) const;

	/** Start, Arrive, Leave, TalkStart, TalkEnd, Ride, Away, Back. */
	UPROPERTY(BlueprintAssignable, Category = "People") FPeopleLifeEvent OnPersonEvent;

	// --- the town (docs/town_life.md) -------------------------------------------------------------------------

	/** Their name in the game's language (npc.<key>.name: the story's people and the townsfolk alike). */
	UFUNCTION(BlueprintPure, Category = "People|Town")
	FText GetDisplayName(int32 Id) const;

	/** A townsperson (townsfolk.json: generated, shown by a model of the shared pool), not one of the story's people. */
	UFUNCTION(BlueprintPure, Category = "People|Town")
	bool IsTownsperson(int32 Id) const;

	/**
	 * A holiday's phase (ULyudFestivalSubsystem): from FromHour till ToHour (To < From: over midnight) the town is drawn to a
	 * place (its id) or to the places of a kind (square, promenade, fair, beach), by Pull (0..2, the sociable most).
	 * Mode: how they spend it; FacePoint (UE cm, ZeroVector = none): what the watchers face. The same Id replaces it.
	 */
	UFUNCTION(BlueprintCallable, Category = "People|Town")
	void SetGathering(FName Id, FName PlaceOrKind, float FromHour, float ToHour, float Pull, EPeopleGatherMode Mode, FVector FacePoint);
	UFUNCTION(BlueprintCallable, Category = "People|Town")
	void ClearGathering(FName Id);

	/** A public holiday: school, the offices and the clinic shut (activities.json holiday "off"); shops, cafés and the club work. */
	UFUNCTION(BlueprintCallable, Category = "People|Town")
	void SetDayOff(bool bOff);
	UFUNCTION(BlueprintPure, Category = "People|Town")
	bool IsDayOff() const;
	/** A festival night: grown-ups and teens stay up till Hour (from today's 00:00, may pass 24: 26.5 = 02:30 tonight),
	 *  small children till 23 at most, and everyone sleeps in as much (4 h at most); 0 ends it. */
	UFUNCTION(BlueprintCallable, Category = "People|Town")
	void SetStayUpUntil(float Hour);

	/** Who is at work at a place now (town_places.json id: restaurant, P-MARKET, W1-U01, ...), in a role (waiter, cook; None any). */
	UFUNCTION(BlueprintCallable, Category = "People|Town")
	TArray<int32> StaffOnShift(FName PlaceId, FName Role) const;

	/** While true the life doesn't draw them (somebody else's actor shows them: the restaurant's waiter on shift). */
	UFUNCTION(BlueprintCallable, Category = "People|Town")
	void SetPresentedExternally(int32 Id, bool bExternal);

	/** Whether a flat's windows are lit now (a flat id: W3-D017, P-CAPSULE/5-03), and one window (<flat>/w<n>, W3-D017-w1). */
	UFUNCTION(BlueprintPure, Category = "People|Town")
	bool IsFlatLit(FName FlatId) const;
	UFUNCTION(BlueprintPure, Category = "People|Town")
	bool IsWindowLit(FName WindowId) const;

	/** Fired when a flat's windows light up or go dark (and for every flat with windows when the life starts or the clock jumps). */
	UPROPERTY(BlueprintAssignable, Category = "People|Town") FPeopleFlatLight OnFlatLight;

	/** Household -> flat id. */
	UPROPERTY(SaveGame) TMap<FString, FString> Homes;
	UPROPERTY(SaveGame) TMap<FString, FPersonMemory> Memory;
	/** Person key -> on how many days they have greeted the player. */
	UPROPERTY(SaveGame) TMap<FString, int32> MetHero;

	LyudPeople::FWorld* GetSim() const { return Sim.Get(); }
	LyudPeople::FManners* GetManners() const { return Manners.Get(); }

	// --- manners (docs/town_manners.md): who people are and the small things they do with their hands ---
	/** What somebody drawn does now: fold_arms, phone_call, smoke, umbrella, ... (manners.json), none otherwise. */
	UFUNCTION(BlueprintPure, Category = "People|Manners")
	FName GetManner(int32 Id) const;
	/** warmth, nerve, curiosity, tidy: 0..1, the same every game (0.5 if unknown). */
	UFUNCTION(BlueprintPure, Category = "People|Manners")
	float GetTrait(int32 Id, FName Trait) const;
	/** Somebody drawn does this for Seconds over whatever they did (wave, nod, clap, point): false if they aren't drawn or
	 *  their hands are busy. */
	UFUNCTION(BlueprintCallable, Category = "People|Manners")
	bool ReactManner(int32 Id, FName Manner, float Seconds = 2.5f);
	/** On how many days somebody has greeted the player (kept with the save): they come to know him (docs/town_manners.md). */
	UFUNCTION(BlueprintPure, Category = "People|Manners")
	int32 GetMet(int32 Id) const;
	/** They greeted him today (counts once a game day). */
	UFUNCTION(BlueprintCallable, Category = "People|Manners")
	void NoteMet(int32 Id);
	/** Something to see in the sky for Seconds (the fireworks say so with every shell): people out of doors look up, point,
	 *  photograph, and have a word about it (town talk's weather = fireworks). The northern lights and a rainbow
	 *  (ALyudRareSky) are noticed by themselves. Strength 0..1. */
	UFUNCTION(BlueprintCallable, Category = "People|Manners")
	void SetSkyShow(FName What, float Strength = 1.f, float Seconds = 8.f);
	/** aurora, rainbow, fireworks or None: what people look at in the sky now. */
	UFUNCTION(BlueprintPure, Category = "People|Manners")
	FName GetSkyShow() const { return SkyShowWhat; }
	/** Everybody drawn does this one (the console and developer mode; None: as they like). */
	UFUNCTION(BlueprintCallable, Category = "People|Manners")
	void ForceManner(FName Manner);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bStarted || bWantStart; }

private:
	TPimplPtr<LyudPeople::FWorld> Sim;
	TPimplPtr<LyudPeople::FManners> Manners;
	/** The props' meshes by LyudPeople::EProp (null: not made or not loaded yet; such manners aren't picked), the easel. */
	UPROPERTY(Transient) TArray<TObjectPtr<class UStaticMesh>> PropMeshes;
	UPROPERTY(Transient) TObjectPtr<class UStaticMesh> EaselMesh;
	TSharedPtr<struct FStreamableHandle> PropLoad;
	/** Present's scratch: the people drawn this frame. */
	TArray<int32> ShownIds;
	float NoticeTimer = 0.f, PassingTimer = 0.f;
	float MannerClock = 0.f;
	/** The sky's show now (RefreshSkyShow, once a second): what, how much of it draws the eye (people get used to a long
	 *  one), since when; the one somebody set for a while (SetSkyShow). */
	FName SkyShowWhat, TimedShowWhat;
	float SkyShow = 0.f, SkyShowSince = 0.f, SkyShowTimer = 0.f;
	float TimedShow = 0.f, TimedShowUntil = -1.f;
	TWeakObjectPtr<class ALyudRareSky> RareSky;
	int32 RareSkySeek = 0;
	void RefreshSkyShow(float Cloud, bool bSunUp);
	/** Person -> the game day they last met the player on (NoteMet counts a day once). */
	TMap<int32, int32> MetDay;
	void LoadManners();
	void UpdateManners(float DeltaTime);
	/** A strike (ULyudWeatherSubsystem::OnLightning): when its thunder reaches the camera people out of doors start
	 *  (the nervy and the children more), and one of them has a word (OnPersonEvent "thunder"). */
	UFUNCTION() void OnThunder(FVector Location, float DistanceFromViewM);
	float ThunderAt = -1.f, ThunderNear = 0.f;

	// the town: pooled bodies for the townsfolk (nearest first, lyud.People.TownBodies), the window lights
	/** Free townsfolk bodies by model, to be given to whoever of that look comes near. */
	TMap<FString, TArray<TObjectPtr<ANpcBody>>> TownPool;
	/** The parked bodies of townsfolk with a look of their own, oldest first: only so many are kept (ReleaseBody). */
	TArray<TWeakObjectPtr<ANpcBody>> OwnParked;
	/** Townsfolk allowed a body now (the nearest drawn ones), refreshed a few times a second. */
	TBitArray<> TownAllowed;
	TArray<TPair<float, int32>> TownNear;
	float TownPickTimer = 0.f;
	void PickTownBodies(const FVector& Cam, float Draw);
	void ReleaseBody(int32 Id);
	/** Lights: every couple of seconds the flats that changed are told to the window lights (the buildings' and the walls'). */
	float LightTimer = 0.f;
	/** A clock jump asks for every flat to be told again; done once the townsfolk are placed. */
	bool bLightsAll = false;
	std::vector<int> LightChanged;
	TMap<FName, int32> FlatIndex;
	TMap<FName, int32> WindowIndex;
	/** bBuildings: the plot buildings' lights (flat ids "<plot>/<flat>"), else the terrace walls' (W2-D001) */
	struct FLightSink { TWeakObjectPtr<UObject> Object; UFunction* Fn = nullptr; bool bBuildings = false; };
	TArray<FLightSink> LightSinks;
	bool bLightSinksBound = false;
	double LastLightBind = -1e9;
	void SetLightsDriven(bool bDriven);
	void BindLights();
	void UpdateLights(bool bAll);
	void SendFlatLight(const FString& FlatId, bool bOn);
	/** What the festivals asked of the town (SetGathering, SetDayOff, SetStayUpUntil), kept here and given to every life
	 *  that starts: they ask once, often before the life has started (it waits half a second after BeginPlay), and a life
	 *  stopped and started again (lyud.People 0/1) would forget them. */
	struct FWantedGathering { FName PlaceOrKind; float From = 0.f, To = 0.f, Pull = 0.f; EPeopleGatherMode Mode = EPeopleGatherMode::Mingle; FVector Face = FVector::ZeroVector; };
	TMap<FName, FWantedGathering> WantedGatherings;
	bool bWantedDayOff = false;
	/** The festival night's end on the calendar (FDateTime ticks), 0 none. */
	int64 WantedStayUpUntil = 0;
	void ApplyWanted();
	bool bWantStart = false;
	bool bStarted = false;
	bool bLoaded = false;
	float StartDelay = 0.f;
	double LastSeconds = 0.0;
	bool bClockSeen = false;
	int32 SunDay = -1;
	float SaveTimer = 0.f;
	float DialogueTimer = 0.f;
	/** The railway's timetable clock minus the life's (both seconds; the railway's counts whole days of the calendar). */
	double RailOffset = 0.0;
	bool bRailSynced = false;
	/** Train riders: person -> (train, seat). */
	TMap<int32, FIntPoint> TrainSeats;

	/** One body per person, made when first needed. */
	UPROPERTY(Transient) TArray<TObjectPtr<ANpcBody>> Bodies;
	/** Every townsfolk body made (shown or in the pool), kept so GC never takes one. */
	UPROPERTY(Transient) TArray<TObjectPtr<ANpcBody>> TownBodies;
	/** The club's placed actors (the scene's people at their stations), by station tag: the seat's own person's model only. */
	UPROPERTY(Transient) TMap<FString, TObjectPtr<AActor>> ClubActors;
	/** The same actors by slot index (null where a slot has none): Present looks them up without building strings. */
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> ClubSlotActors;
	/** Present's scratch: the placed actors shown this frame (kept to reuse its memory). */
	TSet<AActor*> PlacedShown;
	/** Present's ground trace under each outdoor person: the spot traced (cm) and the ground's height found there; reused
	 *  while they stand still on it (a short while), traced again when they move. */
	struct FGroundCache { FVector At = FVector(FLT_MAX); float Z = 0.f; float Age = 0.f; bool bHit = false; };
	TArray<FGroundCache> GroundCache;
	/** Placed human actors that are a second copy of somebody (their model, not at that person's station): hidden while the life runs. */
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> StrayActors;
	/** The one registry of who is shown by what: the model per person id ("" none) and the person per model. */
	TArray<FString> ShownModels;
	TMap<FString, int32> ModelOwners;
	UPROPERTY(Transient) TObjectPtr<ABoothOwner> Owner;
	TSet<int32> Talkable;

	bool Start();
	void Stop();
	/** The hour, weekday and sun from the game's calendar; true if it jumped (everybody resettled). */
	bool UpdateClock(bool bFirst);
	/** Gives everybody one model nobody else has (ShownModels, ModelOwners); before BindClub and any body. */
	void ResolveModels();
	void BindClub();
	void Present(float DeltaTime);
	void DispatchEvents();
	void SyncMemory(bool bToSave);
	/** The sea railway (ULyudRailwaySubsystem): the timetable for the people's train rides, the halts' real positions. */
	void BindRailway();
	/** The winter (ULyudWinterSubsystem): stretches of snowy path for the shovel activity, and how far they got. */
	void BindWinter();
	float SnowTimer = 0.f;
	void UpdateRailway();
	/** A rider of the train: seated on a seat of the train they board while it is drawn, else hidden. */
	void PresentTrainRider(int32 Id, const FVector& Cam, float Draw, float DeltaTime);
	void ApplySaved();
	ANpcBody* BodyFor(int32 Id);
	void ShowPlaced(AActor* Actor, bool bShow);
};

/**
 * One person's body when it isn't the level's own actor: their MakeHuman model (all on SK_Human_Skeleton) driven by
 * the people's life: walks (the shared walk clip at their speed), stands, talks, sits down and stands up, eats, reads,
 * lies down and sleeps, sits at a computer with their own seated clip. Hidden far from the camera, indoors out of reach.
 */
UCLASS(NotPlaceable, Transient)
class LYUDMILOVSK_API ANpcBody : public AActor
{
	GENERATED_BODY()

public:
	ANpcBody();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "People")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** A blanket over them asleep: draped over the body's bones once they lie still (BuildBlanket). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "People")
	TObjectPtr<class UStaticMeshComponent> Blanket;

	/** A snow pusher (a scraper's blade on a pole with a cross bar) in their hands while they clear a path. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "People") TObjectPtr<class UStaticMeshComponent> PusherBlade;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "People") TObjectPtr<class UStaticMeshComponent> PusherPole;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "People") TObjectPtr<class UStaticMeshComponent> PusherBar;

	/** Loads the model and remembers whose clips to use. False if the model is missing. */
	bool Setup(const FString& MeshName, const FString& ComputerClip);
	/** Pose this frame: where (cm; seated: the floor under the seat, lying: under the mattress's middle), facing (deg; lying:
	 *  the bed's head to foot), the pose, the transition (1 sit down, 2 stand up, 3 lie down, 4 get up), the walking speed
	 *  (cm/s) and the seat's or mattress's height (cm, 0 = the clips' own). */
	void Drive(const FVector& Location, float Yaw, EPersonPose Pose, int32 Transit, float Speed, float SeatHeight, float DeltaTime, bool bSnap);
	/** On a bicycle, getting on or off it, or leading it by the bars (ULyudBikeSubsystem::PresentPerson does this instead of
	 *  Drive while the person has their bike in hand); a null Bike lets go of it. Ground: the feet's point, cm. */
	void DriveBike(ALyudBike* Bike, const FLyudBikeRide& Ride, const FVector& Ground, float Yaw, float Speed, float DeltaTime);
	/** Body size against the clips' body (pelvis height / the clips'). */
	float GetBodyScale() const { return BodyScale; }
	/** Moves the mouth for Seconds whatever the pose: a line said over the head (ULyudTownTalkSubsystem, docs/town_talk.md). */
	void Chatter(float Seconds) { ChatterUntil = Clock + Seconds; }
	/** Their manner now (docs/town_manners.md; LyudPeople::EManner and EProp as numbers), told before Drive: Since seconds
	 *  ago it began, Left to go (negative: no end in sight). PropMesh: what it puts in the hands (null: nothing, or not
	 *  made yet), StandMesh: what stands before them (a painter's easel). Drive does the gesture and holds the prop. */
	void SetManner(uint8 Manner, uint8 Prop, float Since, float Left, bool bLeftHanded, class UStaticMesh* PropMesh, class UStaticMesh* StandMesh);
	/** What they look at for the next Seconds (somebody passing, the player running by), then nothing in particular again. */
	void LookAtPoint(const FVector& Where, float Seconds);
	void LookAtNothing();

	/** What a manner puts in the hands (SM_Prop_*), and what stands before them. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "People") TObjectPtr<class UStaticMeshComponent> MannerProp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "People") TObjectPtr<class UStaticMeshComponent> MannerStand;

	FString MeshName;
	/** Who they are in npc_dialogue.txt (their own body's id, whatever model shows them now). */
	FString TalkId;

private:
	UPROPERTY(Transient) TMap<FName, TObjectPtr<class UAnimSequence>> Clips;
	FString ComputerClip;
	/** The computer loops' style for ComputerClip (ULyudBodyAnimInstance::StyleOfClip), at a computer and at work (Setup). */
	FName ComputerStyle = TEXT("Gamer");
	FName WorkStyle = TEXT("Office");
	FName Playing;
	bool bOnBike = false;
	float Facing = 0.f;
	bool bPlaced = false;
	/** Body size against the body the shared clips are baked on (Guy_a: pelvis height / 93.7 cm). */
	float BodyScale = 1.f;
	/** Standing, looking and talking take turns among a few clips (a nod, folded arms) so nobody loops one gesture. */
	FName Role;
	float Clock = 0.f;
	float ChatterUntil = 0.f;
	float VariantUntil = 0.f;
	uint32 Seed = 0;

	UPROPERTY(Transient) TObjectPtr<class UStaticMesh> BlanketMesh;
	float StillAsleep = 0.f;
	bool bBlanket = false;
	/** Whether the walk clip is the shared one (A_Human_*), worked out once (-1 not yet): the clip never changes. */
	int8 SharedWalk = -1;
	/** The bed: the floor under the mattress's middle, head-to-foot yaw, mattress top (cm over that floor). */
	FVector BedCenter = FVector::ZeroVector;
	float BedYaw = 0.f;
	float BedTop = 50.f;

	uint8 Manner = 0, MannerPropKind = 0;
	float MannerSince = 0.f, MannerLeft = -1.f;
	bool bMannerLeft = false;
	bool bLookGiven = false;
	bool bStandPlaced = false;
	/** The gesture of the manner for this pose (none in a transition, asleep, at a computer, on a bike); true if a clip of
	 *  its own was played for it. Then the prop into the hands. */
	bool DriveManner(EPersonPose Pose, int32 Transit, float Speed);
	void PlaceMannerProp(bool bShow);
	class UAnimSequence* Clip(FName Name);
	/** Lays the pusher between the hands and the ground ahead (or hides it). */
	void PlacePusher(bool bShow);
	void Play(FName Name, bool bLoop, float Rate = 1.f);
	/** A duvet over the sleeper: a height field over the bed shaped by capsules round the bones, from the shoulders to past the feet. */
	void BuildBlanket();
	/** Plays one of the role's variants (the role's own clip most of the time), picking again every 6-12 s. */
	void PlayVaried(FName InRole, std::initializer_list<FName> Variants, float Rate = 1.f);
};
