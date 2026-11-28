// The people in a dinghy: a townsperson's own body sat at the helm (and the cadet's crew ahead of it), playing the
// sailing clips by the boat's state (docs/yacht_school.md "Clips"), the hands on the real tiller extension and sheet,
// a life jacket on the chest. The boat owns these bodies while they sail: the life does not draw the person meanwhile.
#include "LyudDinghyImpl.h"

#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Bikes/LyudBikeRider.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "People/LyudBodyAnim.h"
#include "People/PeopleLife.h"
#include "ReferenceSkeleton.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudSailCrew, Log, All);

namespace
{
	bool DinghyCrewOver(LyudSail::EState S)
	{
		return S == LyudSail::EState::Capsized || S == LyudSail::EState::Turtled || S == LyudSail::EState::Righting;
	}

	/** Sail_<Base>_Port / _Stbd: the body on that side deck. */
	FString DinghyClipName(const TCHAR* Base, int32 Side)
	{
		return FString::Printf(TEXT("Sail_%s_%s"), Base, Side < 0 ? TEXT("Port") : TEXT("Stbd"));
	}

	/** The sitting loop of a seat. Level: 0 helm, 1 half hike, 2 full hike, 3 lull, 4 bailing. */
	const TCHAR* DinghySitClip(int32 BoatClass, int32 Slot, int32 Level)
	{
		if (BoatClass == 0)
		{
			return Level == 4 ? TEXT("Opti_Bail") : TEXT("Opti_Helm");
		}
		if (BoatClass == 1)
		{
			return Slot == 1 ? TEXT("Crew_Jib") : TEXT("Cadet_Helm");
		}
		switch (Level)
		{
		case 1: return TEXT("Hike_Half");
		case 2: return TEXT("Hike_Full");
		case 3: return TEXT("Lull");
		default: return TEXT("Helm");
		}
	}

	/** While the sailing clips are not baked: any seated loop the people have. */
	UAnimSequence* DinghySeatedStandIn()
	{
		static UAnimSequence* Found = nullptr;
		static bool bLooked = false;
		if (!bLooked)
		{
			bLooked = true;
			Found = LyudSailKit::Clip(TEXT("SitIdle"));
			if (!Found)
			{
				Found = LyudSailKit::Clip(TEXT("Sit_Idle"));
			}
			if (!Found)
			{
				Found = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Humans/Owner/Anims/A_Owner_Sit_Idle.A_Owner_Sit_Idle"), nullptr, LOAD_NoWarn | LOAD_Quiet);
				if (Found)
				{
					Found->AddToRoot();
				}
			}
		}
		return Found;
	}

	/** The palms' and fingers' ways at rest on the extension and the sheet, in the PORT clip's space
	 *  (blender/humans/sail_grips.json, the Helm clips' first frame). */
	struct FDinghyHands
	{
		FVector TillerPalm, TillerFingers, SheetPalm, SheetFingers;
	};

	const FDinghyHands& DinghyHands(int32 BoatClass)
	{
		static const FDinghyHands Table[3] = {
			{ FVector(0.35f, 0.77f, -0.53f), FVector(-0.80f, 0.54f, 0.27f), FVector(-0.24f, -0.61f, 0.76f), FVector(-0.56f, 0.72f, 0.40f) },
			{ FVector(0.51f, 0.86f, -0.07f), FVector(-0.65f, 0.44f, 0.62f), FVector(-0.71f, -0.62f, 0.34f), FVector(-0.49f, 0.77f, 0.40f) },
			{ FVector(0.20f, 0.66f, -0.72f), FVector(-0.89f, 0.43f, 0.15f), FVector(-0.59f, -0.46f, 0.66f), FVector(-0.59f, 0.80f, 0.04f) } };
		return Table[FMath::Clamp(BoatClass, 0, 2)];
	}
}

namespace LyudSailKit
{
	UAnimSequence* Clip(const FString& Name)
	{
		static TMap<FString, UAnimSequence*> Cache;
		if (UAnimSequence** Found = Cache.Find(Name))
		{
			return *Found;
		}
		const FString Path = FString::Printf(TEXT("/Game/Characters/Humans/Anims/A_Human_%s.A_Human_%s"), *Name, *Name);
		UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Seq)
		{
			Seq->AddToRoot();
		}
		else
		{
			UE_LOG(LogLyudSailCrew, Display, TEXT("SAIL no clip A_Human_%s yet: a seated stand-in plays"), *Name);
		}
		Cache.Add(Name, Seq);
		return Seq;
	}

	ANpcBody* SpawnBody(AActor* Boat, FName PersonKey, int32& InOutId)
	{
		UWorld* W = Boat ? Boat->GetWorld() : nullptr;
		UPeopleLifeSubsystem* Life = Boat ? UPeopleLifeSubsystem::Get(Boat) : nullptr;
		if (!W || !Life || PersonKey.IsNone())
		{
			return nullptr;
		}
		if (InOutId < 0)
		{
			InOutId = Life->FindPerson(PersonKey.ToString());
		}
		if (InOutId < 0)
		{
			return nullptr;
		}
		Life->SetPresentedExternally(InOutId, true);
		const FString Model = Life->ShownModel(InOutId);
		if (Model.IsEmpty())
		{
			return nullptr;
		}
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ANpcBody* Body = W->SpawnActor<ANpcBody>(Boat->GetActorLocation(), Boat->GetActorRotation(), SP);
		if (!Body || !Body->Setup(Model, FString()))
		{
			UE_LOG(LogLyudSailCrew, Warning, TEXT("SAIL no body for %s (model %s)"), *PersonKey.ToString(), *Model);
			if (Body)
			{
				Body->Destroy();
			}
			return nullptr;
		}
		Body->Tags.Add(TEXT("Sailing"));
		// a boat's crew is seen from afar more than from near: posed less often with the distance
		Body->Mesh->bEnableUpdateRateOptimizations = true;
		Body->Mesh->bAffectDistanceFieldLighting = false;
		Body->Mesh->bAffectDynamicIndirectLighting = false;
		if (ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(Body->Mesh->GetAnimInstance()))
		{
			Anim->SetOnFeet(false);
		}
		return Body;
	}

	void ReleaseBody(AActor* Boat, TWeakObjectPtr<ANpcBody>& Body, int32& Id)
	{
		if (ANpcBody* Was = Body.Get())
		{
			Was->Destroy();
		}
		Body.Reset();
		if (Id >= 0)
		{
			if (UPeopleLifeSubsystem* Life = Boat ? UPeopleLifeSubsystem::Get(Boat) : nullptr)
			{
				Life->SetPresentedExternally(Id, false);
			}
			Id = -1;
		}
	}

	UStaticMeshComponent* PutJacketOn(ANpcBody* Body)
	{
		if (!Body || !Body->Mesh || !Body->Mesh->GetSkeletalMeshAsset())
		{
			return nullptr;
		}
		UStaticMesh* Vest = AssetAt(TEXT("/Game/Characters/Humans/Props/SM_Prop_life_jacket.SM_Prop_life_jacket"));
		if (!Vest)
		{
			Vest = Asset(TEXT("LifeJacket_Worn"));
		}
		static const FName Chest(TEXT("spine_03"));
		const FReferenceSkeleton& Ref = Body->Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
		const int32 Bone = Ref.FindBoneIndex(Chest);
		if (!Vest || Bone == INDEX_NONE)
		{
			return nullptr;
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Body, TEXT("LifeJacket"), RF_Transient);
		NoLumen(C);
		C->SetStaticMesh(Vest);
		C->SetupAttachment(Body->Mesh, Chest);
		C->LightingChannels.bChannel0 = true;
		C->LightingChannels.bChannel1 = true;
		C->RegisterComponent();
		Body->AddInstanceComponent(C);
		// modelled in the body's rest frame about the chest joint: the bone's rest turn is taken back out; a smaller
		// person is the same skeleton made smaller, so the jacket is scaled to them
		const FQuat Rest = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Bone).GetRotation();
		C->SetRelativeLocationAndRotation(FVector::ZeroVector, Rest.Inverse());
		C->SetRelativeScale3D(FVector(Body->GetBodyScale()));
		return C;
	}
}

// ------------------------------------------------------------------ who sits where

void ALyudDinghy::FImpl::CrewSet(ALyudDinghy& D, int32 Slot, FName PersonKey, float AgeYears, bool bLifeJacket)
{
	FLyudSailCrew& C = Crew[Slot];
	if (C.Key != PersonKey)
	{
		CrewDrop(D, Slot, true);
		C.Key = PersonKey;
		if (!PersonKey.IsNone())
		{
			// the person is the boat's from now (also far off, with no body): the life does not stand them on the slip
			if (UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(&D))
			{
				C.PersonId = Life->FindPerson(PersonKey.ToString());
				if (C.PersonId >= 0)
				{
					Life->SetPresentedExternally(C.PersonId, true);
				}
			}
		}
	}
	C.Age = AgeYears;
	C.bJacket = bLifeJacket;
	if (UStaticMeshComponent* Vest = C.Jacket.Get())
	{
		Vest->SetVisibility(bLifeJacket);
	}
	if (Slot == 0 && AgeYears > 0.f && !PersonKey.IsNone())
	{
		// the helm's weight is the boat's righting moment
		B.CrewMass = LyudSail::CrewMassFor(B.Class, AgeYears);
	}
}

void ALyudDinghy::FImpl::CrewDrop(ALyudDinghy& D, int32 Slot, bool bForget)
{
	FLyudSailCrew& C = Crew[Slot];
	if (ANpcBody* Was = C.Body.Get())
	{
		Was->Destroy();
	}
	C.Body.Reset();
	C.Jacket.Reset();
	C.bTried = false;
	C.Play = FLyudSailCrew::EPlay::None;
	C.Left = 0.f;
	C.HandWeight = 0.f;
	C.Level = -1;
	C.bRelSet = false;
	C.bHandsOn = false;
	if (bForget)
	{
		if (C.PersonId >= 0)
		{
			if (UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(&D))
			{
				Life->SetPresentedExternally(C.PersonId, false);
			}
		}
		C.PersonId = -1;
		C.Key = NAME_None;
	}
}

FVector ALyudDinghy::FImpl::TillerHand(const ALyudDinghy& D) const
{
	const LyudSailKit::FClassDef& Def = LyudSailKit::ClassDef(BuiltClass);
	const float RudderDeg = -FMath::RadiansToDegrees(float(B.Out.RudderAngle));
	const float Len = Def.Ext.X - Def.Tiller.X;
	const FVector Stock(Def.Rudder.X, 0.f, Def.Ext.Z);
	const FVector RestTip = Stock + FVector(Len, 0.f, 0.f);
	const FVector Tip = Stock + FRotator(0.f, RudderDeg, 0.f).RotateVector(FVector(Len, 0.f, 0.f));
	if (!Crew[0].Body.IsValid() && !D.bHeroAboard)
	{
		// nobody at the helm: the extension lies along the tiller
		return Tip + (Tip - Stock).GetSafeNormal(1e-3f, FVector::ForwardVector) * Def.ExtLen;
	}
	// the aft hand's rest place of the Helm clip (the port clip's space: +X the body's left, +Y the bow; a body of the
	// class's scale), carried along with most of the tiller's swing
	const float Side = D.bHeroAboard ? (SeatSide >= 0.f ? 1.f : -1.f) : float(SitSide);
	const FVector G = Def.TillerGrip * Def.ClipScale;
	const FVector Rest = Def.BodyOrigin + FVector(G.Y, Side * G.X, G.Z);
	return Rest + (Tip - RestTip) * 0.6f;
}

// ------------------------------------------------------------------ each frame

void ALyudDinghy::FImpl::CrewFrameMove(ALyudDinghy& D, float DeltaTime)
{
	// the crew's frame rides with the hull (they heel with her); capsized it comes level on the water where she lies,
	// which is the frame the fall's end, the swim and the righting clips are posed in
	const bool bOver = DinghyCrewOver(B.State);
	if (bOver)
	{
		// heel + = the mast to starboard = her bottom, and the board, to port
		OverSide = B.Out.Heel >= 0.0 ? -1 : 1;
	}
	CrewOver = FMath::FInterpConstantTo(CrewOver, bOver ? 1.f : 0.f, DeltaTime, 2.5f);
	const FTransform HullW = Hull->GetComponentTransform();
	if (CrewOver <= 0.f)
	{
		CrewFrame->SetWorldLocationAndRotation(HullW.GetLocation(), HullW.GetRotation());
		return;
	}
	const float A = FMath::SmoothStep(0.f, 1.f, CrewOver);
	const FQuat Flat = FRotator(0.f, FMath::RadiansToDegrees(DrawYaw), 0.f).Quaternion();
	const FVector Level(DrawPos.X, DrawPos.Y, SurfaceZ);
	CrewFrame->SetWorldLocationAndRotation(FMath::Lerp(HullW.GetLocation(), Level, A), FQuat::Slerp(HullW.GetRotation(), Flat, A));
}

void ALyudDinghy::FImpl::CrewTick(ALyudDinghy& D, float DeltaTime)
{
	const LyudSail::FOutputs& O = B.Out;
	const bool bOver = DinghyCrewOver(B.State);
	// the side they sit on: opposite the boom. At once on a tack or a gybe, else when the boom has stayed over a moment
	if (!bOver)
	{
		const int32 Want = O.Boom > 0.06 ? -1 : (O.Boom < -0.06 ? 1 : SitSide);
		if (Want != SitSide)
		{
			SideHold += DeltaTime;
			if ((StepEvents & (LyudSail::EvTack | LyudSail::EvGybe)) != 0 || SideHold > 0.5f)
			{
				SitSide = Want;
				SideHold = 0.f;
			}
		}
		else
		{
			SideHold = 0.f;
		}
	}
	if ((D.Lod != ELyudDinghyLod::Near && D.Lod != ELyudDinghyLod::Mid) || !Hull || !CrewFrame)
	{
		return;
	}
	if (Crew[0].Key.IsNone() && Crew[1].Key.IsNone())
	{
		return;
	}
	CrewFrameMove(D, DeltaTime);
	const LyudSailKit::FClassDef& Def = LyudSailKit::ClassDef(BuiltClass);
	const bool bNear = D.Lod == ELyudDinghyLod::Near;
	typedef FLyudSailCrew::EPlay EPlay;
	for (int32 Slot = 0; Slot < 2; ++Slot)
	{
		FLyudSailCrew& C = Crew[Slot];
		if (C.Key.IsNone() || (Slot == 1 && BuiltClass != 1))
		{
			continue;
		}
		ANpcBody* Body = C.Body.Get();
		if (!Body)
		{
			if (C.bTried)
			{
				continue;
			}
			// asked for once per stretch near the camera (a missing model is not loaded again every frame)
			C.bTried = true;
			Body = LyudSailKit::SpawnBody(&D, C.Key, C.PersonId);
			if (!Body)
			{
				continue;
			}
			C.Body = Body;
			Body->AttachToComponent(CrewFrame, FAttachmentTransformRules::KeepRelativeTransform);
			C.Play = EPlay::None;
			C.Level = -1;
			C.Side = SitSide;
			C.bRelSet = false;
			C.HandWeight = 0.f;
			C.bHandsOn = false;
			C.Jacket = LyudSailKit::PutJacketOn(Body);
			if (UStaticMeshComponent* Vest = C.Jacket.Get())
			{
				Vest->SetVisibility(C.bJacket);
			}
		}
		ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(Body->Mesh->GetAnimInstance());
		if (!Anim)
		{
			continue;
		}
		C.Left -= DeltaTime;

		// what they do now
		const bool bWasOver = C.Play == EPlay::Fall || C.Play == EPlay::Swim || C.Play == EPlay::Right;
		EPlay Next = C.Play;
		if (bOver)
		{
			if (!bWasOver)
			{
				Next = EPlay::Fall;
			}
			else if (C.Play == EPlay::Fall && C.Left <= 0.f && Slot == 0 && B.State == LyudSail::EState::Righting)
			{
				// the helm swims round to the board and stands on it; the crew treads water by the hull
				Next = EPlay::Swim;
			}
			else if (C.Play == EPlay::Swim && C.Left <= 0.f)
			{
				Next = EPlay::Right;
			}
		}
		else if (bWasOver)
		{
			Next = EPlay::Climb;
		}
		else if (C.Play == EPlay::None || ((C.Play == EPlay::Climb || C.Play == EPlay::Cross) && C.Left <= 0.f))
		{
			Next = EPlay::Sit;
		}
		else if (C.Play == EPlay::Sit && C.Side != SitSide)
		{
			Next = EPlay::Cross;
		}

		// the sitting pose: the luch's helm leans out by the model's hike (three loops sharing one phase), kneels in the
		// middle in a calm; the optimist's child bails when there is water aboard
		int32 Level = FMath::Max(C.Level, 0);
		if (BuiltClass == 0)
		{
			Level = Last.bBail && O.Water > 0.05 ? 4 : 0;
		}
		else if (BuiltClass == 2)
		{
			const float Out = FMath::Abs(float(O.Hike));
			if (O.TrueWindSpeed < (Level == 3 ? 2.0 : 1.5) && Out < 0.1f)
			{
				Level = 3;
			}
			else
			{
				if (Level == 3)
				{
					Level = 0;
				}
				if (Level == 0 && Out > 0.33f)
				{
					Level = 1;
				}
				else if (Level == 1 && Out < 0.2f)
				{
					Level = 0;
				}
				else if (Level == 1 && Out > 0.8f)
				{
					Level = 2;
				}
				else if (Level == 2 && Out < 0.67f)
				{
					Level = 1;
				}
			}
		}
		else
		{
			Level = 0;
		}

		if (Next != C.Play || (Next == EPlay::Sit && Level != C.Level))
		{
			FString Name;
			bool bLoop = false;
			float Blend = 0.3f, StartAt = 0.f, Length = 0.f;
			int32 Side = SitSide;
			switch (Next)
			{
			case EPlay::Fall:
				Name = DinghyClipName(TEXT("Capsize_Fall"), OverSide);
				Length = 1.5f;
				Side = OverSide;
				Blend = 0.2f;
				break;
			case EPlay::Swim:
				Name = DinghyClipName(TEXT("Swim_ToBoard"), OverSide);
				Length = 3.f;
				Side = OverSide;
				Blend = 0.4f;
				break;
			case EPlay::Right:
				Name = DinghyClipName(TEXT("Right"), OverSide);
				Side = OverSide;
				bLoop = true;
				Blend = 0.2f;
				break;
			case EPlay::Climb:
				Name = DinghyClipName(TEXT("Climb_In"), SitSide);
				Length = 3.2f;
				Blend = 0.4f;
				break;
			case EPlay::Cross:
				// the crossing clips start on the old side and end on the new one. Only the luch's helm and the cadet's
				// crew have them: the others melt from one side's loop into the other's
				if (BuiltClass == 2)
				{
					const bool bGybe = (StepEvents & LyudSail::EvGybe) != 0 || FMath::Abs(O.TrueWindAngle) > 1.9;
					Name = DinghyClipName(bGybe ? TEXT("Gybe") : TEXT("Tack"), C.Side);
					Length = bGybe ? 1.6f : 2.2f;
					Blend = 0.15f;
				}
				else if (BuiltClass == 1 && Slot == 1)
				{
					Name = DinghyClipName(TEXT("Crew_Tack"), C.Side);
					Length = 2.4f;
					Blend = 0.15f;
				}
				else
				{
					Next = EPlay::Sit;
				}
				break;
			default:
				break;
			}
			if (Next == EPlay::Sit)
			{
				// the three hiking loops share an 8 s phase: one melts into the next where it is
				const bool bFamily = BuiltClass == 2 && C.Play == EPlay::Sit && C.Side == SitSide && Level <= 2 && C.Level >= 0 && C.Level <= 2 && C.bSailClips;
				Name = DinghyClipName(DinghySitClip(BuiltClass, Slot, Level), SitSide);
				bLoop = true;
				Blend = bFamily ? 0.6f : (C.Play == EPlay::Sit ? 0.7f : 0.25f);
				StartAt = bFamily ? Anim->CurrentClipPhase() * 8.f : 0.f;
			}
			UAnimSequence* Seq = LyudSailKit::Clip(Name);
			C.bSailClips = Seq != nullptr;
			if (!Seq)
			{
				Seq = DinghySeatedStandIn();
				bLoop = true;
				StartAt = 0.f;
			}
			if (Seq)
			{
				Anim->PlayClip(Seq, bLoop, 1.f, Blend, StartAt);
			}
			// with no clip at all a body would stand in its rest pose in the boat: better nobody
			Body->SetActorHiddenInGame(Seq == nullptr);
			C.Play = Next;
			C.Left = Length;
			C.Side = Side;
			C.Level = Level;
		}

		// where the body is in the crew's frame
		const float Scale = Body->GetBodyScale();
		const float Ahead = Slot == 1 ? Def.CrewAhead : 0.f;
		FVector Loc;
		float Yaw = -90.f;      // the clips face the bow: the mesh's +Y on the boat's +X
		if (!C.bSailClips)
		{
			// the stand-in is a chair's clip (seat top 47 cm, the pelvis at 56.7): on the side deck, facing inboard
			Loc = FVector(Def.HelmSeat.X + Ahead, float(SitSide) * FMath::Abs(Def.HelmSeat.Y), Def.HelmSeat.Z - 56.7f * Scale);
			Yaw = SitSide > 0 ? 180.f : 0.f;
		}
		else if (C.Play == EPlay::Swim || C.Play == EPlay::Right)
		{
			// the capsized frame: where the board leaves the hull, on the water
			Loc = FVector(Def.Board.X, float(OverSide) * Def.HullDraft, 0.f);
		}
		else
		{
			// a body of another size than the clips were posed for sits higher or lower by the deck's height over the sole
			const float Raise = Def.DeckOverSole * (1.f - Scale / Def.ClipScale);
			Loc = Def.BodyOrigin + FVector(Ahead, 0.f, Raise);
		}
		C.Rel = C.bRelSet ? FMath::VInterpTo(C.Rel, Loc, DeltaTime, 5.f) : Loc;
		C.bRelSet = true;
		Body->SetActorRelativeLocation(C.Rel);
		Body->SetActorRelativeRotation(FRotator(0.f, Yaw, 0.f));

		// the hands on the gear while they sit (the crossing and the capsize clips carry the hands themselves)
		const bool bHands = bNear && Slot == 0 && C.bSailClips && C.Play == EPlay::Sit && C.Level != 3 && C.Level != 4;
		C.HandWeight = FMath::FInterpConstantTo(C.HandWeight, bHands ? 1.f : 0.f, DeltaTime, 4.f);
		if (bNear && C.HandWeight > 0.001f)
		{
			CrewHands(D, Slot, Anim, Scale);
			C.bHandsOn = true;
		}
		else if (C.bHandsOn)
		{
			Anim->SetBikeRider(FBikeRiderPose());
			C.bHandsOn = false;
		}
	}
}

void ALyudDinghy::FImpl::CrewHands(ALyudDinghy& D, int32 Slot, ULyudBodyAnimInstance* Anim, float BodyScale)
{
	const FLyudSailCrew& C = Crew[Slot];
	const ANpcBody* Body = C.Body.Get();
	if (!Body || !Anim)
	{
		return;
	}
	const LyudSailKit::FClassDef& Def = LyudSailKit::ClassDef(BuiltClass);
	const FDinghyHands& Rest = DinghyHands(BuiltClass);
	const FTransform HullW = Hull->GetComponentTransform();
	const FTransform MeshW = Body->Mesh->GetComponentTransform();
	const float Side = float(C.Side);
	// the hull's frame -> the body's mesh space; the port clip's directions mirrored for a body on the starboard deck
	auto Point = [&HullW, &MeshW](const FVector& InHull)
	{
		return MeshW.InverseTransformPosition(HullW.TransformPosition(InHull));
	};
	auto Way = [&HullW, &MeshW](const FVector& InHull)
	{
		return MeshW.InverseTransformVectorNoScale(HullW.TransformVectorNoScale(InHull)).GetSafeNormal(1e-4f, FVector::YAxisVector);
	};
	auto FromClip = [Side](const FVector& V)
	{
		return FVector(V.Y, Side * V.X, V.Z);
	};

	FBikeRiderPose P;
	P.Weight = 1.f;
	P.PelvisWeight = 0.f;
	P.TorsoPitch = 0.f;
	P.bAutoLean = false;
	P.HeadUp = 0.f;
	P.Feet[0].Weight = 0.f;
	P.Feet[1].Weight = 0.f;
	// on the port deck the tiller is in the right hand (the aft one) and the sheet in the left; mirrored to starboard
	const int32 TillerIdx = C.Side < 0 ? 1 : 0;
	const int32 SheetIdx = 1 - TillerIdx;

	// the extension: the hand where TillerHand has it (the drawn extension points at the same place)
	{
		const float RudderDeg = -FMath::RadiansToDegrees(float(B.Out.RudderAngle));
		const float Len = Def.Ext.X - Def.Tiller.X;
		const FVector Tip = FVector(Def.Rudder.X, 0.f, Def.Ext.Z) + FRotator(0.f, RudderDeg, 0.f).RotateVector(FVector(Len, 0.f, 0.f));
		const FVector Hand = TillerHand(D);
		FBikeRiderPose::FHand& H = P.Hands[TillerIdx];
		H.Grip = Point(Hand);
		H.Axis = Way((Hand - Tip).GetSafeNormal(1e-3f, FVector::ForwardVector));
		H.Radius = 1.2f;
		H.PalmNormal = Way(FromClip(Rest.TillerPalm));
		H.FingerDir = Way(FromClip(Rest.TillerFingers));
		H.Weight = C.HandWeight;
		H.Close = 1.f;
	}
	// the sheet: the forward hand at its rest place, the rope running to it from the ratchet block
	{
		const FVector Hand = Def.BodyOrigin + FromClip(Def.SheetGrip * Def.ClipScale);
		FBikeRiderPose::FHand& H = P.Hands[SheetIdx];
		H.Grip = Point(Hand);
		H.Axis = Way((Hand - Def.SheetBlock).GetSafeNormal(1e-3f, FVector::UpVector));
		H.Radius = 0.5f;
		H.PalmNormal = Way(FromClip(Rest.SheetPalm));
		H.FingerDir = Way(FromClip(Rest.SheetFingers));
		H.Weight = C.HandWeight;
		H.Close = 1.f;
	}
	Anim->SetBikeRider(P);
}
