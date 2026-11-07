#include "LyudRestaurantStaff.h"

#include "Engine/World.h"
#include "People/PeopleLife.h"
#include "UObject/UnrealType.h"

namespace LyudRestaurantStaff
{
	UPeopleLifeSubsystem* Life(const UObject* WorldContext)
	{
		return UPeopleLifeSubsystem::Get(WorldContext);
	}

	/** Calls Life's UFUNCTION Name with the arguments set by Fill (the parameters in their order, return value excluded) and hands the
	 *  parameter block to Read afterwards; false when there is no such function. */
	bool Call(UObject* Obj, FName Name, TFunctionRef<void(UFunction*, uint8*, const TArray<FProperty*>&)> Fill,
		TFunctionRef<void(UFunction*, uint8*, FProperty*)> Read)
	{
		// the functions are looked up once per class (UFunctions live as long as their class)
		static TMap<FName, TWeakObjectPtr<UFunction>> Found;
		static TWeakObjectPtr<UClass> FoundFor;
		if (!Obj)
		{
			return false;
		}
		if (FoundFor.Get() != Obj->GetClass())
		{
			Found.Reset();
			FoundFor = Obj->GetClass();
		}
		TWeakObjectPtr<UFunction>* Slot = Found.Find(Name);
		if (!Slot)
		{
			Slot = &Found.Add(Name, Obj->FindFunction(Name));
		}
		UFunction* F = Slot->Get();
		if (!F)
		{
			return false;
		}
		uint8* Parms = (uint8*)FMemory_Alloca(FMath::Max<int32>(F->ParmsSize, 1));
		FMemory::Memzero(Parms, F->ParmsSize);
		TArray<FProperty*> Args;
		FProperty* Ret = nullptr;
		for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Parms);
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				Ret = *It;
			}
			else
			{
				Args.Add(*It);
			}
		}
		Fill(F, Parms, Args);
		Obj->ProcessEvent(F, Parms);
		Read(F, Parms, Ret);
		for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Parms);
		}
		return true;
	}

	TArray<int32> OnShift(const UObject* WorldContext, FName PlaceId, FName Role)
	{
		TArray<int32> Out;
		Call(Life(WorldContext), TEXT("StaffOnShift"),
			[&](UFunction*, uint8* P, const TArray<FProperty*>& Args)
			{
				if (Args.Num() == 2 && CastField<FNameProperty>(Args[0]) && CastField<FNameProperty>(Args[1]))
				{
					*Args[0]->ContainerPtrToValuePtr<FName>(P) = PlaceId;
					*Args[1]->ContainerPtrToValuePtr<FName>(P) = Role;
				}
			},
			[&](UFunction*, uint8* P, FProperty* Ret)
			{
				const FArrayProperty* A = CastField<FArrayProperty>(Ret);
				if (A && CastField<FIntProperty>(A->Inner))
				{
					Out = *A->ContainerPtrToValuePtr<TArray<int32>>(P);
				}
			});
		return Out;
	}

	bool SetPresentedExternally(const UObject* WorldContext, int32 Id, bool bExternal)
	{
		return Call(Life(WorldContext), TEXT("SetPresentedExternally"),
			[&](UFunction*, uint8* P, const TArray<FProperty*>& Args)
			{
				if (Args.Num() == 2 && CastField<FIntProperty>(Args[0]) && CastField<FBoolProperty>(Args[1]))
				{
					*Args[0]->ContainerPtrToValuePtr<int32>(P) = Id;
					CastField<FBoolProperty>(Args[1])->SetPropertyValue_InContainer(P, bExternal);
				}
			},
			[](UFunction*, uint8*, FProperty*) {});
	}

	FText DisplayName(const UObject* WorldContext, int32 Id)
	{
		FText Out;
		Call(Life(WorldContext), TEXT("GetDisplayName"),
			[&](UFunction*, uint8* P, const TArray<FProperty*>& Args)
			{
				if (Args.Num() == 1 && CastField<FIntProperty>(Args[0]))
				{
					*Args[0]->ContainerPtrToValuePtr<int32>(P) = Id;
				}
			},
			[&](UFunction*, uint8* P, FProperty* Ret)
			{
				if (CastField<FTextProperty>(Ret))
				{
					Out = *Ret->ContainerPtrToValuePtr<FText>(P);
				}
			});
		return Out;
	}

	bool IsAvailable(const UObject* WorldContext)
	{
		const UPeopleLifeSubsystem* L = Life(WorldContext);
		return L && L->IsRunning() && L->FindFunction(TEXT("StaffOnShift")) != nullptr;
	}
}
