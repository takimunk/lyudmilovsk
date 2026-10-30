#include "LyudGoodsItem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "LyudText.h"
#include "LyudInventory.h"
#include "Materials/MaterialInstanceDynamic.h"

ALyudGoodsItem::ALyudGoodsItem()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);
}

UStaticMeshComponent* ALyudGoodsItem::Part(UStaticMesh* Mesh, const FVector& Loc, const FVector& Size, const FLinearColor& Color)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(RootComponent);
	C->SetRelativeLocation(Loc);
	C->SetRelativeScale3D(Size / 100.f);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->RegisterComponent();
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		C->SetMaterial(0, MID);
	}
	return C;
}

void ALyudGoodsItem::Init(FName InItem)
{
	Item = InItem;
	const FLyudItemDef* Def = ULyudInventory::FindDef(Item);
	if (!Def)
	{
		return;
	}
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	const FVector S = Def->Size;
	const FLinearColor C = Def->Tint;
	const FLinearColor Dark = C * 0.55f + FLinearColor(0.02f, 0.02f, 0.02f);
	Half = S * 0.5f;
	bThin = Def->Look == ELyudItemLook::Flat;
	// the front picture: the icon on a thin plate that sits on the front face (a cylinder gets a flat label strip)
	FVector PlateSize(0.25f, S.Y * 0.86f, S.Z * 0.7f);
	float PlateX = Half.X + 0.05f;
	switch (Def->Look)
	{
	case ELyudItemLook::Pack:
		Part(Cube, FVector::ZeroVector, S, C);
		Part(Cube, FVector(0.f, 0.f, Half.Z - 0.6f), FVector(S.X + 0.6f, S.Y + 0.2f, 1.6f), Dark);        // the crimped top
		break;
	case ELyudItemLook::Flat:
		Part(Cube, FVector::ZeroVector, S, C);
		PlateSize = FVector(0.1f, S.Y * 0.92f, S.Z * 0.92f);
		break;
	case ELyudItemLook::Cup:
		Part(Cyl, FVector::ZeroVector, FVector(S.X, S.Y, S.Z), C);
		Part(Cyl, FVector(0.f, 0.f, Half.Z + 0.4f), FVector(S.X + 0.8f, S.Y + 0.8f, 1.f), Dark);          // the lid
		Part(Cyl, FVector(0.f, 0.f, -1.f), FVector(S.X * 0.62f, S.Y * 0.62f, S.Z * 0.5f) * 0.7f, Dark);     // the sleeve's shade under it
		PlateSize = FVector(0.25f, S.Y * 0.45f, S.Z * 0.45f);
		PlateX = Half.X - 0.1f;
		break;
	case ELyudItemLook::Glass:
		Part(Cyl, FVector::ZeroVector, S, FLinearColor(0.7f, 0.35f, 0.08f));
		Part(Cyl, FVector(0.f, 0.f, -0.6f), FVector(S.X + 0.9f, S.Y + 0.9f, S.Z * 0.7f), FLinearColor(0.75f, 0.75f, 0.78f));   // the metal holder
		PlateSize = FVector(0.25f, S.Y * 0.4f, S.Z * 0.4f);
		PlateX = Half.X + 0.35f;
		break;
	case ELyudItemLook::Bottle:
		Part(Cyl, FVector(0.f, 0.f, -S.Z * 0.12f), FVector(S.X, S.Y, S.Z * 0.72f), C);
		Part(Cyl, FVector(0.f, 0.f, S.Z * 0.34f), FVector(S.X * 0.4f, S.Y * 0.4f, S.Z * 0.3f), C);
		Part(Cyl, FVector(0.f, 0.f, Half.Z), FVector(S.X * 0.46f, S.Y * 0.46f, 0.8f), FLinearColor(0.1f, 0.35f, 0.7f));
		PlateSize = FVector(0.25f, S.Y * 0.55f, S.Z * 0.35f);
		PlateX = Half.X - 0.15f;
		break;
	case ELyudItemLook::Cone:
		Part(Cone, FVector(0.f, 0.f, -S.Z * 0.2f), FVector(S.X, S.Y, S.Z * 0.6f), FLinearColor(0.75f, 0.5f, 0.2f));
		Part(Ball, FVector(0.f, 0.f, S.Z * 0.2f), FVector(S.X * 1.1f), C);
		PlateSize = FVector();
		break;
	default:
		Part(Ball, FVector::ZeroVector, S, C);
		PlateSize = FVector();
		break;
	}
	if (PlateSize.X > 0.f)
	{
		UStaticMeshComponent* Plate = Part(Cube, FVector(PlateX, 0.f, Def->Look == ELyudItemLook::Flat ? 0.f : -S.Z * 0.02f), PlateSize, FLinearColor::White);
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Hall/M_BottleLabel.M_BottleLabel")))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			// a printed label when the item has one (the grocery's feeds: T_Label_<id>), else its icon
			if (UTexture2D* Label = LyudText::LoadTexture(TEXT("Hall"), TEXT("T_Label_") + Item.ToString()))
			{
				MID->SetTextureParameterValue(TEXT("Label"), Label);
			}
			else if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
			{
				if (UTexture2D* Tex = UI->IconTexture(Item))
				{
					MID->SetTextureParameterValue(TEXT("Label"), Tex);
				}
			}
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
			Plate->SetMaterial(0, MID);
		}
	}
}

FReachItem ALyudGoodsItem::MakeReachItem() const
{
	FReachItem R;
	R.Actor = const_cast<ALyudGoodsItem*>(this);
	R.Center = GetActorLocation();
	R.Axis = GetActorUpVector();
	R.Front = GetActorForwardVector();
	const float Rad = bThin ? FMath::Max(1.5f, Half.X * 2.f + 1.5f) : FMath::Clamp(FMath::Min(Half.X, Half.Y), 1.5f, 5.f);
	R.Radius = Rad;
	R.HalfLength = FMath::Max(0.f, FMath::Min(Half.Z, 7.f) - Rad);
	R.MouthDistance = R.HalfLength + Rad;
	return R;
}

void ALyudGoodsItem::LieDown(const FVector& Pos, const FVector& Along)
{
	SetActorLocationAndRotation(Pos, FRotationMatrix::MakeFromZX(Along.GetSafeNormal(), FVector::UpVector).Rotator());
}
