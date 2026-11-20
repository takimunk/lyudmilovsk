#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudLetters.generated.h"

class AHeroCharacter;
class ALyudGoodsItem;
class APlayerController;
class SLyudCityPage;
class UStaticMeshComponent;
namespace LyudLetters { class FModel; struct FFacts; struct FTime; struct FAnswer; }

/**
 * A street postbox (blue, the town's) or the club's own mailbox (dark green, on the wall by the door), built from the
 * engine's shapes in code. Spawned and placed by ULyudLetters; nothing else to set up. NotPlaceable.
 */
UCLASS(NotPlaceable)
class LYUDMILOVSK_API ALyudPostbox : public AActor
{
	GENERATED_BODY()

public:
	ALyudPostbox();

	/** Builds the look: the street box (bMailbox false, blue, on its own foot) or the club's mailbox (dark green; bOnWall: its back
	 *  against a wall, origin at the back's middle; else on a post, origin at the post's foot). Local X out of its front. */
	void Build(bool bMailbox, bool bOnWall);

	/** The point the hero looks at to use it (the slot), world cm. */
	FVector UsePoint() const;
	bool IsMailbox() const { return bIsMailbox; }

private:
	UStaticMeshComponent* Part(class UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FLinearColor& Color, const FRotator& Rot = FRotator::ZeroRotator);
	bool bIsMailbox = false;
};

/**
 * The town's post (docs/letters.md): the hero writes postcards to the people he knows (choose the addressee, the picture, two
 * warm sentences) and drops them in a street postbox; they are carried the next post day, the people answer, thank him in
 * the street, put his card on their shelf. Letters come to the club's mailbox: replies, thanks after quests, invitations to
 * the holidays, and the long correspondence with Tonya, a lighthouse keeper's daughter far north, whose questions he answers
 * from what he really did (birds met, fish caught, places found, birds befriended). Every letter read goes into the diary.
 *
 * The rules are the engine-free LyudLetters::FModel (Private/Letters/LettersModel.*, tools/lettersim/run.sh); the data is
 * Content/Letters/letters.json and the words Content/Localization/letters (tools/letters/build_letters.py). This subsystem
 * steps the model once a real second with the calendar and the hero's facts, places the boxes near the hero (a trace finds
 * the ground and a free spot), offers them with the prompt (a tight cone: E never also runs a city thing next to it), and
 * runs the choosers and the letters on SLyudCityPage (keys polled, never consumed: 1-9, A/D, E, Esc). Saved as "Letters".
 *
 * Console: lyud.Letters.Report, lyud.Letters.Deliver, lyud.Letters.PenNext, lyud.Letters.Mail (the mailbox wherever the hero
 * is), lyud.Letters.Post <to> [picture] (a card without the chooser), lyud.Letters.KnowAll. Developer mode: group "letters".
 */
UCLASS()
class LYUDMILOVSK_API ULyudLetters : public UTickableWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudLetters();
	virtual ~ULyudLetters() override;

	static ULyudLetters* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudLetters, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bLoaded; }

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

	/** A box is offered on the prompt line now (ULyudCityThings picks nothing meanwhile: one E, one thing). */
	bool IsOffering() const { return OfferedWeak.IsValid(); }
	/** A chooser or a letter is up (it has the keys). */
	bool HasModeUp() const { return Mode != EMode::None; }

	// --- console and developer mode
	FString Report() const;
	void DevDeliverAll();
	void DevPenNext();
	void DevKnowAll();
	/** Opens the club's mailbox wherever the hero is. */
	void OpenMailAnywhere();
	/** Sends a card at once (no chooser, no item needed): picture empty = the first. False: unknown addressee or picture. */
	bool DevPost(const FString& To, const FString& Picture);
	/** Where to stand to use the club's mailbox / the postbox nearest the hero (false: not placed yet). */
	bool MailboxStand(FTransform& Out) const;
	bool NearestPostboxStand(const FVector& From, FTransform& Out) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	/** The model's state as a flat map ("Letters"). */
	UPROPERTY(SaveGame) TMap<FString, FString> Saved;

private:
	enum class EMode : uint8 { None, Menu, CardTo, CardPicture, CardOpening, CardWish, CardPreview, Answer, AnswerPreview, Reading, Reread };

	/** Somebody the hero can write to: a person of the town's life (Key, PersonId) or a correspondent of the open homes. */
	struct FAddressee
	{
		FString Id;          // the model's addressee: a person's key (t_*, sorokin_oleg) or a correspondent's id (h_holm)
		int32 PersonId = -1;
		FText Name;
		FText Address;
		FString Group;       // kid, teen, adult, elder (for the townsfolk's replies)
	};

	struct FBoxSlot
	{
		FString Id;
		FVector Anchor = FVector::ZeroVector;
		float Yaw = 0.f;
		bool bMailbox = false;
		bool bTried = false;
		TWeakObjectPtr<ALyudPostbox> Actor;
	};

	void Load();
	void StartOver();
	void ApplySaved();
	LyudLetters::FTime Now() const;
	void FillFacts(LyudLetters::FFacts& Out) const;
	void StepPost();

	// boxes
	void PlaceNear(const FVector& Hero);
	bool PlaceStreetBox(FBoxSlot& Slot);
	bool PlaceMailbox(FBoxSlot& Slot);
	ALyudPostbox* PickInView(const AHeroCharacter* Hero, const APlayerController* PC) const;
	void SetPrompt(ALyudPostbox* Box);
	void Refuse(const FText& Why);

	// the people round the hero
	void TickPeople(const AHeroCharacter* Hero);
	void PlaceShelfCards(const FVector& Hero);

	// the choosers and the letters
	void Use(ALyudPostbox* Box);
	void OpenMenu();
	void BeginCard();
	void BuildAddressees(TArray<FAddressee>& Out) const;
	void ShowList(const FText& Title, const TArray<FText>& Entries, const FText& Keys, const FText& Head = FText::GetEmpty());
	void ShowMode();
	void Choose(int32 Entry);
	void Back();
	void PostTheCard();
	void PostTheAnswer();
	void OpenMailbox();
	void ShowLetterPage();
	void FinishLetter();
	void OpenPage();
	void ClosePage();
	void EndMode();
	void TickMode(AHeroCharacter* Hero, APlayerController* PC);

	// words
	FText Fill(const FString& Key, const TArray<FString>& Args) const;
	FText FromName(const FString& From) const;
	FText AddressOf(const FString& AddresseeId, int32 PersonId) const;
	void DiaryLetter(int32 Letter);

	TPimplPtr<LyudLetters::FModel> Model;
	bool bLoaded = false;
	/** A save restored before the data was read: applied when it is. */
	bool bSavedPending = false;

	TArray<FBoxSlot> Boxes;
	TArray<FString> ThanksLines;
	/** The box whose prompt shows (null none). */
	TWeakObjectPtr<ALyudPostbox> OfferedWeak;
	float StepClock = 0.f;
	float PickClock = 0.f;
	float PeopleClock = 0.f;
	float PlaceClock = 0.f;

	/** The cards standing on the correspondents' shelves (by correspondent index; null: none placed yet). */
	UPROPERTY(Transient) TArray<TObjectPtr<ALyudGoodsItem>> ShelfCards;
	/** "Not now" on the prompt line until then (world seconds). */
	double RefusedUntil = 0.0;
	/** The story's people the hero has talked with (their keys, lower case): refreshed when the chooser opens. */
	TSet<FString> StoryKnown;

	// the mode up
	EMode Mode = EMode::None;
	TSharedPtr<SLyudCityPage> Page;
	uint64 ModeFrame = 0;
	int32 Sel = 0;
	int32 NumEntries = 0;
	TWeakObjectPtr<APlayerController> MoveBlockedBy;
	/** What the chooser is building. */
	TArray<FAddressee> Addressees;
	int32 CardTo = -1;
	int32 CardPicture = -1;
	int32 CardOpening = -1;
	int32 CardWish = -1;
	TArray<FString> AnswerKinds;
	TArray<FString> AnswerFacts;
	int32 AnswerSel = -1;
	/** The menu's entries now (what each does). */
	TArray<FName> MenuActions;
	/** Reading: the letters to show (model indexes), the one shown, its pages, the page shown. */
	TArray<int32> ReadQueue;
	int32 ReadAt = 0;
	TArray<FText> ReadPages;
	int32 ReadPage = 0;
	TArray<int32> RereadList;
	bool bKnowAll = false;
};
