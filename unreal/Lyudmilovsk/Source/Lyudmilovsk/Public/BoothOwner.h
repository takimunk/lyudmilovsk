#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyudInventory.h"
#include "BoothOwner.generated.h"

class UAnimSequence;
class UBoxComponent;
class USkeletalMeshComponent;

/**
 * The club owner in his booth. He sits at his computer; when the player steps up to the booth
 * window (CallZone) he stands up, walks over to the window and keeps looking at the player; when
 * the player leaves, he walks back and sits down again. The actor sits at the window; Body moves.
 * The mesh faces +Y at yaw 0 (MeshYawOffset turns it to face +X like the rest of the logic).
 */
UCLASS()
class LYUDMILOVSK_API ABoothOwner : public AActor
{
	GENERATED_BODY()

public:
	ABoothOwner();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Owner")
	TObjectPtr<USkeletalMeshComponent> Body;

	/** Where the player stands to call him (in the hall, in front of the window). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Owner")
	TObjectPtr<UBoxComponent> CallZone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner")
	FVector SeatLocation = FVector::ZeroVector;

	/** Facing while seated (degrees, +X = 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner")
	float SeatYaw = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner")
	FVector WindowLocation = FVector::ZeroVector;

	/** Facing at the window toward the hall; he turns from it toward the player within LookLimit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner")
	float WindowYaw = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner")
	float LookLimit = 70.f;

	/** Added to the facing to orient the mesh (meshes face +Y at yaw 0 -> -90). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner")
	float MeshYawOffset = -90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner")
	float WalkSpeed = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner")
	float TurnSpeed = 180.f;

	/** Standing up ends this far forward of the chair point; sitting down starts there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner")
	float StandOffset = 12.f;

	/** Seconds the player must be gone before he heads back to his computer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner")
	float ReturnDelay = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner|Anim")
	TObjectPtr<UAnimSequence> SitAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner|Anim")
	TObjectPtr<UAnimSequence> StandUpAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner|Anim")
	TObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner|Anim")
	TObjectPtr<UAnimSequence> StandAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner|Anim")
	TObjectPtr<UAnimSequence> SitDownAnim;

	/** Standing at the window, ready to talk. */
	bool IsAtWindow() const { return State == EState::AtWindow; }

	/** What he sells at the window (T): bird feed, one bag of Count handfuls for Price roubles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner|Trade")
	int32 FeedPrice = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner|Trade")
	int32 FeedCount = 5;

	/** Things to wear he sells too (the game screen's inventory puts them on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner|Trade")
	int32 HeadLampPrice = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owner|Trade")
	int32 BinocularsPrice = 60;

	/** What he sells, as the trade screen lists it. */
	TArray<FLyudShopGood> Goods() const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	enum class EState : uint8 { Seated, StandingUp, ToWindow, AtWindow, ToSeat, SittingDown };

	void Enter(EState NewState);
	void Play(UAnimSequence* Anim, bool bLoop);
	bool PlayerAtWindow() const;
	bool TurnTo(float Yaw, float DeltaSeconds);
	bool WalkTo(const FVector& Target, float DeltaSeconds);
	void SetPose(const FVector& Loc, float Yaw);
	void TickTrade();

	bool bTradePrompt = false;

	EState State = EState::Seated;
	float StateTime = 0.f;
	float AwayTime = 0.f;
	FVector Pos = FVector::ZeroVector;
	FVector StandSpot() const;
	float Facing = 0.f;
};
