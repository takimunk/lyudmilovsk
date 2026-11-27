#include "Home/LyudHeroRoom.h"

#include "Home/HomeModel.h"
#include "Audio/LyudSoundscape.h"
#include "BottleItem.h"
#include "City/CityKit.h"
#include "City/LyudCityThings.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudGoodsItem.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Photo/LyudPhoto.h"
#include "Plants/GameCalendar.h"
#include "Plants/ProceduralPlant.h"
#include "UI/LyudQuests.h"
#if __has_include("Dev/LyudDev.h")
#include "Dev/LyudDev.h"
#define LYUD_HOME_DEV 1
#else
#define LYUD_HOME_DEV 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudHome, Log, All);

namespace LyudHomeImpl
{
	const TCHAR* DataFile = TEXT("Home/home.json");
	const TCHAR* PropMatPath = TEXT("/Game/City/M_CityProp.M_CityProp");
	const TCHAR* PlotPrefix = TEXT("P-HOME/");
	const FName HomeScreen(TEXT("home"));
	/** A print: the engine's cube as a thin card, the goods' label material with the photograph on it. */
	const TCHAR* PrintMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* PrintMatPath = TEXT("/Game/Hall/M_BottleLabel.M_BottleLabel");
	/** The prints the chooser offers at once: the newest of the album. */
	constexpr int32 PrintsOffered = 8;
	/** The plant's age and the box test: a few times a second is plenty. */
	constexpr float Every = 0.5f;
	constexpr double NoteSeconds = 2.6;

	FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	std::string Std(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }

	/** How big an item is, as the item table says. */
	LyudHome::FLook LookOf(const FLyudItemDef& Def)
	{
		LyudHome::FLook L;
		if (!Def.Flavor.IsNone())
		{
			L.Depth = L.Width = Def.Radius * 2.0;
			L.Height = (Def.HalfLength + Def.Radius) * 2.0;
		}
		else if (Def.Look == ELyudItemLook::Auto)
		{
			L.Depth = L.Width = L.Height = Def.Radius * 2.0;
		}
		else
		{
			L.bFlat = Def.Look == ELyudItemLook::Flat;
			L.Depth = Def.Size.X;
			L.Width = Def.Size.Y;
			L.Height = Def.Size.Z;
		}
		return L;
	}

	FLinearColor Hex(const std::string& S, const FLinearColor& Default)
	{
		const FString H = Str(S);
		return H.Len() == 7 && H[0] == TEXT('#') ? FLinearColor::FromSRGBColor(FColor::FromHex(H)) : Default;
	}

	APlayerController* Player(const UWorld* World)
	{
		return World ? World->GetFirstPlayerController() : nullptr;
	}

	/** The cups' metals: gold, silver, bronze. */
	CityKit::FSkin Metal(int32 Tint)
	{
		CityKit::FSkin S;
		S.Color = Tint == 1 ? CityKit::Srgb(196, 198, 204) : Tint == 2 ? CityKit::Srgb(176, 112, 62) : CityKit::Srgb(214, 168, 62);
		S.Rough = 0.28f;
		return S;
	}
}

ULyudHeroRoom::ULyudHeroRoom() = default;
ULyudHeroRoom::~ULyudHeroRoom() = default;

ULyudHeroRoom* ULyudHeroRoom::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudHeroRoom>() : nullptr;
}

void ULyudHeroRoom::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Model = MakePimpl<LyudHome::FModel>();
	bLoaded = LoadData();
	if (!bLoaded)
	{
		return;
	}
	BuildRoom();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("HeroRoom"), this);
		Saves->OnNewGame.AddUObject(this, &ULyudHeroRoom::StartOver);
	}
	BindCity();
}

void ULyudHeroRoom::Deinitialize()
{
	if (ChoiceSlot >= 0)
	{
		EndChoice(false);
	}
	if (ULyudCityThings* City = bCityBound ? ULyudCityThings::Get(this) : nullptr)
	{
		City->OnOpen.RemoveAll(this);
	}
	bCityBound = false;
	if (ULyudSaveSubsystem* Saves = bLoaded ? ULyudSaveSubsystem::Get(this) : nullptr)
	{
		Saves->UnregisterSaveObject(TEXT("HeroRoom"));
		Saves->OnNewGame.RemoveAll(this);
	}
	Super::Deinitialize();
}

bool ULyudHeroRoom::LoadData()
{
	const FString Path = FPaths::ProjectContentDir() / LyudHomeImpl::DataFile;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogLyudHome, Display, TEXT("HeroRoom: no %s: no room"), *Path);
		return false;
	}
	const FTCHARToUTF8 Utf8(*Text);
	std::string Error;
	if (!Model->Load(std::string(Utf8.Get(), Utf8.Length()), Error))
	{
		UE_LOG(LogLyudHome, Warning, TEXT("HeroRoom: %s: %s"), *Path, *LyudHomeImpl::Str(Error));
		return false;
	}
	return true;
}

void ULyudHeroRoom::BindCity()
{
	ULyudCityThings* City = ULyudCityThings::Get(this);
	if (!City || bCityBound)
	{
		return;
	}
	City->OnOpen.AddUObject(this, &ULyudHeroRoom::OnCityOpen);
	bCityBound = true;
}

// ------------------------------------------------------------------ the room in the level

void ULyudHeroRoom::BuildRoom()
{
	UWorld* World = GetWorld();
	if (!World || bBuilt)
	{
		return;
	}
	// the meshes of the import (unreal/scripts/import_city.sh, TYPES=hero_room STEPS=import): without them there is no room
	TArray<UStaticMesh*> Meshes;
	for (const std::string& Path : Model->Meshes)
	{
		const FString P = LyudHomeImpl::Str(Path);
		const FString Name = FPaths::GetBaseFilename(P);
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *(P + TEXT(".") + Name), nullptr, LOAD_NoWarn))
		{
			Meshes.Add(Mesh);
		}
	}
	if (Meshes.Num() < Model->Meshes.size())
	{
		UE_LOG(LogLyudHome, Warning, TEXT("HeroRoom: the room's meshes are not imported (%d of %d): run blender/city/build_city.py -- hero_room and "
			"TYPES=hero_room STEPS=import unreal/scripts/import_city.sh. The hall stays as it was."), Meshes.Num(), (int32)Model->Meshes.size());
		return;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.ObjectFlags |= RF_Transient;
	const FVector Origin(Model->OriginX, Model->OriginY, Model->OriginZ);
	// the mesh's front (Blender -y) is its +Y in Unreal: turned onto the way the front faces, as ALyudCityBuilding stands on
	// a plot (import_city.py place: yaw - 90)
	const FRotator Rot(0.f, (float)Model->Yaw - 90.f, 0.f);
	Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
	if (!Holder)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();
	for (int32 I = 0; I < Meshes.Num(); ++I)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Holder);
		C->SetMobility(EComponentMobility::Static);
		C->SetStaticMesh(Meshes[I]);
		C->SetWorldLocationAndRotation(Origin, Rot);
		// the shell blocks, the detail (furniture's faces; their collision boxes are in the shell) does not
		if (I == 0)
		{
			C->SetCollisionProfileName(TEXT("BlockAll"));
		}
		else
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		C->SetupAttachment(Root);
		C->RegisterComponent();
	}
	for (const LyudHome::FLamp& L : Model->Lamps)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Holder);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetupAttachment(Root);
		Light->SetIntensityUnits(ELightUnits::Lumens);
		Light->SetCastShadows(false);
		Light->SetIntensity((float)L.Lumens);
		Light->SetLightColor(LyudHomeImpl::Hex(L.Colour, FLinearColor(1.f, 0.85f, 0.66f)));
		Light->SetAttenuationRadius((float)L.Radius);
		Light->SetWorldLocation(FVector(L.X, L.Y, L.Z));
		Light->RegisterComponent();
		Lights.Add(Light);
	}
	PropMaterial = LoadObject<UMaterialInterface>(nullptr, LyudHomeImpl::PropMatPath);
	SlotActors.SetNum((int32)Model->Slots.size());
	bBuilt = true;
	CutWall();
	ApplyLight();
	UE_LOG(LogLyudHome, Display, TEXT("HeroRoom: built at (%.0f, %.0f, %.0f), %d places, %d lamps"), Origin.X, Origin.Y, Origin.Z,
		(int32)Model->Slots.size(), Lights.Num());
}

void ULyudHeroRoom::CutWall()
{
	UWorld* World = GetWorld();
	if (!World || !Holder)
	{
		return;
	}
	// The hall's left wall is three meshes of the club's scene (oil paint below, a dark line, whitewash above), found by where
	// they are: thin along X, the wall's whole length along Y. They are hidden and drawn again round the doorway with their own
	// materials. A scene rebuilt with the doorway (blender/scripts/build_scene01_corridor.py) has no such meshes: nothing to do.
	const LyudHome::FModel& M = *Model;
	UMaterialInterface* Paint = nullptr;
	UMaterialInterface* White = nullptr;
	UMaterialInterface* Line = nullptr;
	int32 Hidden = 0;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		UStaticMeshComponent* C = It->GetStaticMeshComponent();
		if (!C || !C->GetStaticMesh() || !C->IsVisible())
		{
			continue;
		}
		const FBox B = C->Bounds.GetBox();
		const FVector S = B.GetSize();
		if (B.Min.X < M.WallX0 - 3.0 || B.Max.X > M.WallX1 + 3.0 || S.Y < (M.WallY1 - M.WallY0) * 0.8 || B.Min.Y < M.WallY0 - 60.0
			|| B.Max.Y > M.WallY1 + 10.0 || B.Min.Z < -5.0 || B.Max.Z > M.WallZ1 + 5.0)
		{
			continue;
		}
		UMaterialInterface* Mat = C->GetMaterial(0);
		if (S.Z < 8.0)
		{
			Line = Mat;
		}
		else if (B.Max.Z <= M.PaintZ + 3.0)
		{
			Paint = Mat;
		}
		else
		{
			White = Mat;
		}
		UE_LOG(LogLyudHome, Display, TEXT("HeroRoom: the hall wall's %s (%s) gives way to the doorway"), *It->GetName(), *C->GetStaticMesh()->GetName());
		C->SetVisibility(false);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		++Hidden;
	}
	if (Hidden == 0)
	{
		UE_LOG(LogLyudHome, Display, TEXT("HeroRoom: no solid hall wall found: the doorway is the scene's own"));
		return;
	}
	// The wall again, in metres: its hall face (the other faces are behind the room's lining and the door's casing), with the
	// scene's own mapping (blender world_uv, 1 m a repeat: u = Blender y, v = z, flipped by glTF) so the paint does not jump.
	const double X = M.WallX1 / 100.0, Xb = M.WallX0 / 100.0;
	const double Y0 = (M.WallY0 - 50.0) / 100.0, Y1 = M.WallY1 / 100.0, D0 = M.DoorY0 / 100.0, D1 = M.DoorY1 / 100.0;
	const double Zp = M.PaintZ / 100.0, Zt = M.WallZ1 / 100.0, Zd = M.DoorZ / 100.0;
	const CityKit::FSkin Skin;
	auto Face = [&Skin](CityKit::FMesh& Mesh, double Fx, double Ya, double Yb, double Za, double Zb)
	{
		auto UV = [](double Y, double Z) { return FVector2f((float)-Y, (float)(1.0 - Z)); };
		// seen from the hall (+X): corners round the face so that it looks to +X
		Mesh.QuadUV(FVector(Fx, Ya, Za), FVector(Fx, Yb, Za), FVector(Fx, Yb, Zb), FVector(Fx, Ya, Zb), UV(Ya, Za), UV(Yb, Za), UV(Yb, Zb), UV(Ya, Zb), Skin);
	};
	auto Solid = [Xb, X](CityKit::FMesh& Mesh, double Ya, double Yb, double Za, double Zb)
	{
		Mesh.Collide(FVector((Xb + X) / 2, (Ya + Yb) / 2, (Za + Zb) / 2), FVector((X - Xb) / 2, (Yb - Ya) / 2, (Zb - Za) / 2));
	};
	CityKit::FMesh PaintMesh, WhiteMesh, LineMesh;
	Face(PaintMesh, X, Y0, D0, 0.0, Zp);
	Face(PaintMesh, X, D1, Y1, 0.0, Zp);
	Face(WhiteMesh, X, Y0, D0, Zp, Zt);
	Face(WhiteMesh, X, D1, Y1, Zp, Zt);
	Face(WhiteMesh, X, D0, D1, Zd, Zt);
	Solid(WhiteMesh, Y0, D0, 0.0, Zt);
	Solid(WhiteMesh, D1, Y1, 0.0, Zt);
	Solid(WhiteMesh, D0, D1, Zd, Zt);
	Face(LineMesh, X + 0.005, Y0, D0 - 0.08, Zp - 0.0175, Zp + 0.0175);
	Face(LineMesh, X + 0.005, D1 + 0.08, Y1, Zp - 0.0175, Zp + 0.0175);
	struct FPiece { CityKit::FMesh* Mesh; UMaterialInterface* Mat; const TCHAR* Name; bool bSolid; };
	const FPiece Pieces[] = { { &WhiteMesh, White ? White : Paint, TEXT("HomeWall_White"), true }, { &PaintMesh, Paint ? Paint : White, TEXT("HomeWall_Paint"), false },
		{ &LineMesh, Line, TEXT("HomeWall_Line"), false } };
	for (const FPiece& Piece : Pieces)
	{
		if (!Piece.Mat)
		{
			continue;
		}
		UStaticMesh* Mesh = Piece.Mesh->Build(Holder, Piece.Name, Piece.bSolid);
		if (!Mesh)
		{
			continue;
		}
		WallMeshes.Add(Mesh);
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Holder);
		C->SetMobility(EComponentMobility::Static);
		C->SetStaticMesh(Mesh);
		C->SetMaterial(0, Piece.Mat);
		if (Piece.bSolid)
		{
			C->SetCollisionProfileName(TEXT("BlockAll"));
		}
		else
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		C->SetupAttachment(Holder->GetRootComponent());
		C->RegisterComponent();
	}
	UE_LOG(LogLyudHome, Display, TEXT("HeroRoom: doorway cut in the hall's wall (%d meshes replaced), Y %.0f..%.0f, %.0f cm high"), Hidden, M.DoorY0, M.DoorY1, M.DoorZ);
}

void ULyudHeroRoom::ApplyLight()
{
	for (UPointLightComponent* Light : Lights)
	{
		if (Light)
		{
			Light->SetVisibility(Model->bLight);
		}
	}
}

// ------------------------------------------------------------------ the save

void ULyudHeroRoom::OnGameSaving_Implementation()
{
	if (!Model || !bLoaded)
	{
		return;
	}
	Saved.Reset();
	std::map<std::string, std::string> Out;
	Model->SaveState(Out);
	for (const auto& KV : Out)
	{
		Saved.Add(LyudHomeImpl::Str(KV.first), LyudHomeImpl::Str(KV.second));
	}
}

void ULyudHeroRoom::OnGameLoaded_Implementation()
{
	if (!Model || !bLoaded)
	{
		return;
	}
	if (ChoiceSlot >= 0)
	{
		EndChoice(false);
	}
	std::map<std::string, std::string> In;
	for (const TPair<FString, FString>& KV : Saved)
	{
		In[LyudHomeImpl::Std(KV.Key)] = LyudHomeImpl::Std(KV.Value);
	}
	Model->LoadState(In);
	// a thing whose place the data no longer has goes back to the pocket
	if (ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		for (const std::string& Id : Model->Lost)
		{
			Inv->Add(FName(*LyudHomeImpl::Str(Id)));
		}
	}
	Model->Lost.clear();
	// another game: its own photographs, and no note left from the last one
	PrintTextures.Reset();
	bMindNote = false;
	MindDay = -1;
	RefreshAll();
}

void ULyudHeroRoom::StartOver()
{
	if (!Model)
	{
		return;
	}
	if (ChoiceSlot >= 0)
	{
		EndChoice(false);
	}
	Model->StartOver();
	PrintTextures.Reset();
	bMindNote = false;
	MindDay = -1;
	RefreshAll();
}

long long ULyudHeroRoom::Today() const
{
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	return (long long)(Now.GetDate().GetTicks() / ETimespan::TicksPerDay);
}

// ------------------------------------------------------------------ what stands there

FText ULyudHeroRoom::NameOf(const FString& What) const
{
	if (What.StartsWith(TEXT("item:")))
	{
		return ULyudInventory::ItemName(FName(*What.Mid(5)));
	}
	int Roll = 0, Frame = 0;
	if (LyudHome::FModel::ParsePrint(LyudHomeImpl::Std(What), Roll, Frame))
	{
		return LyudText::Get(TEXT("city.myroom.print"), { FStringFormatArg(Roll), FStringFormatArg(Frame) });
	}
	const LyudHome::FTrophy* T = Model->FindTrophy(LyudHomeImpl::Std(What));
	return T ? LyudText::Get(LyudHomeImpl::Str(T->Name)) : FText::FromString(What);
}

bool ULyudHeroRoom::Holds(FName Item) const
{
	return bLoaded && Model && !Item.IsNone() && Model->HoldsItem(LyudHomeImpl::Std(Item.ToString()));
}

UStaticMesh* ULyudHeroRoom::TrophyMesh(const FString& Shape, int32 Tint)
{
	const FString Key = FString::Printf(TEXT("%s%d"), *Shape, Tint);
	if (const TObjectPtr<UStaticMesh>* Found = TrophyMeshes.Find(Key))
	{
		return *Found;
	}
	CityKit::FMesh M;
	if (Shape == TEXT("cup"))
	{
		// a small cup on a dark plinth, 24 cm: a foot, a stem, a bowl with a lip, two ear handles
		const CityKit::FSkin Metal = LyudHomeImpl::Metal(Tint);
		CityKit::FSkin Plinth;
		Plinth.Color = CityKit::Srgb(38, 26, 20);
		Plinth.Rough = 0.45f;
		M.Box(FVector(0, 0, 0.02), FVector(0.045, 0.045, 0.02), Plinth);
		const TArray<FVector2D> Profile = { { 0.034, 0.04 }, { 0.03, 0.05 }, { 0.01, 0.062 }, { 0.008, 0.1 }, { 0.022, 0.118 }, { 0.046, 0.15 },
			{ 0.054, 0.2 }, { 0.058, 0.236 }, { 0.052, 0.236 }, { 0.046, 0.2 }, { 0.0, 0.17 } };
		M.Lathe(FVector::ZeroVector, Profile, 14, [&Metal](int32, int32) { return Metal; });
		for (const double Side : { -1.0, 1.0 })
		{
			M.Bar(FVector(0, Side * 0.05, 0.215), FVector(0, Side * 0.082, 0.2), 0.008f, 0.008f, Metal);
			M.Bar(FVector(0, Side * 0.082, 0.2), FVector(0, Side * 0.078, 0.165), 0.008f, 0.008f, Metal);
			M.Bar(FVector(0, Side * 0.078, 0.165), FVector(0, Side * 0.044, 0.148), 0.008f, 0.008f, Metal);
		}
	}
	else
	{
		// the bird diary standing on its edge: a blue cloth cover, the pages' block, a label
		CityKit::FSkin Cover = CityKit::Paint(44, 78, 110, 0.7f);
		CityKit::FSkin Pages = CityKit::Paint(232, 226, 208, 0.85f);
		CityKit::FSkin Label = CityKit::Paint(236, 214, 150, 0.6f);
		M.Box(FVector(0, 0, 0.105), FVector(0.017, 0.075, 0.105), Cover);
		M.Box(FVector(0.0, 0.004, 0.105), FVector(0.013, 0.074, 0.1), Pages);
		M.Box(FVector(0.0175, 0, 0.14), FVector(0.0006, 0.045, 0.028), Label);
	}
	UStaticMesh* Mesh = M.Build(this, FName(*(TEXT("HomeTrophy_") + Key)), false);
	TrophyMeshes.Add(Key, Mesh);
	return Mesh;
}

AActor* ULyudHeroRoom::SpawnTrophy(const FString& What, const FVector& At, float Yaw)
{
	UWorld* World = GetWorld();
	const LyudHome::FTrophy* T = Model->FindTrophy(LyudHomeImpl::Std(What));
	UStaticMesh* Mesh = T ? TrophyMesh(LyudHomeImpl::Str(T->Shape), T->Tint) : nullptr;
	if (!World || !Mesh)
	{
		return nullptr;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.ObjectFlags |= RF_Transient;
	AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(At, FRotator(0.f, Yaw, 0.f), P);
	if (A)
	{
		UStaticMeshComponent* C = A->GetStaticMeshComponent();
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Mesh);
		C->SetMaterial(0, PropMaterial);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	return A;
}

AActor* ULyudHeroRoom::SpawnPrint(const FString& What, int32 Slot)
{
	UWorld* World = GetWorld();
	int Roll = 0, Frame = 0;
	if (!World || !LyudHome::FModel::ParsePrint(LyudHomeImpl::Std(What), Roll, Frame))
	{
		return nullptr;
	}
	if (!PrintMesh)
	{
		PrintMesh = LoadObject<UStaticMesh>(nullptr, LyudHomeImpl::PrintMeshPath);
		PrintMaterial = LoadObject<UMaterialInterface>(nullptr, LyudHomeImpl::PrintMatPath);
	}
	if (!PrintMesh)
	{
		return nullptr;
	}
	// the picture: read from the disk once, then kept (a print whose file is gone stays a white card, and is not asked again)
	UTexture2D* Picture = nullptr;
	if (const TObjectPtr<UTexture2D>* Kept = PrintTextures.Find(What))
	{
		Picture = *Kept;
	}
	else
	{
		ULyudPhoto* Photo = ULyudPhoto::Get(this);
		Picture = Photo ? Photo->LoadPrint(Roll, Frame) : nullptr;
		PrintTextures.Add(What, Picture);
	}
	const LyudHome::FSlot& S = Model->Slots[(size_t)Slot];
	const bool bCard = S.Type == "card";
	// a 3:2 frame: 15 x 10 cm pinned to the board, 30 x 20 enlarged on a nail
	const double Width = bCard ? 15.0 : 30.0, Height = bCard ? 10.0 : 20.0, Thick = bCard ? 0.2 : 0.8;
	const FVector Out = FRotator(0.f, (float)S.Yaw, 0.f).Vector();
	const float Askew = bCard ? (float)((Slot * 37) % 7 - 3) * 1.2f : 0.f;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.ObjectFlags |= RF_Transient;
	AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(FVector(S.X, S.Y, S.Z) + Out * (Thick * 0.5 + 0.4), FRotator(0.f, (float)S.Yaw, Askew), P);
	if (A)
	{
		UStaticMeshComponent* C = A->GetStaticMeshComponent();
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(PrintMesh);
		C->SetWorldScale3D(FVector(Thick, Width, Height) / 100.0);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		if (PrintMaterial)
		{
			// (the component holds the material, the material the picture: nothing is made again until the place changes)
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PrintMaterial, C);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
			if (Picture)
			{
				MID->SetTextureParameterValue(TEXT("Label"), Picture);
			}
			C->SetMaterial(0, MID);
		}
	}
	return A;
}

void ULyudHeroRoom::RefreshSlot(int32 Slot)
{
	if (!bBuilt || !SlotActors.IsValidIndex(Slot))
	{
		return;
	}
	if (AActor* Old = SlotActors[Slot])
	{
		Old->Destroy();
		SlotActors[Slot] = nullptr;
	}
	const LyudHome::FSlot& S = Model->Slots[(size_t)Slot];
	if (S.Type == "pot")
	{
		return;
	}
	const FString What = LyudHomeImpl::Str(Model->At(S.Id));
	if (What.IsEmpty())
	{
		return;
	}
	const FVector At(S.X, S.Y, S.Z);
	const FVector Out = FRotator(0.f, (float)S.Yaw, 0.f).Vector();
	AActor* Made = nullptr;
	if (What.StartsWith(TEXT("print:")))
	{
		Made = SpawnPrint(What, Slot);
	}
	else if (!What.StartsWith(TEXT("item:")))
	{
		Made = SpawnTrophy(What, At, (float)S.Yaw);
	}
	else if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		const FName Id(*What.Mid(5));
		const FLyudItemDef* Def = ULyudInventory::FindDef(Id);
		Made = Def ? UI->SpawnHeld(Id, At) : nullptr;
		if (Made && Def)
		{
			const LyudHome::FLook L = LyudHomeImpl::LookOf(*Def);
			Made->SetActorEnableCollision(false);
			if (S.Type == "shelf")
			{
				// standing on the board; a flat thing leans back a little against the wall
				const bool bLean = L.bFlat;
				const FVector Pos = At + FVector(0, 0, L.Height * 0.5 + 0.3) - Out * (bLean ? 6.0 : 0.0);
				Made->SetActorLocationAndRotation(Pos, FRotator(bLean ? 9.f : 0.f, (float)S.Yaw, 0.f));
			}
			else
			{
				// on the wall: its back to it; a pinned card hangs a little askew, each its own way
				const float Roll = S.Type == "card" ? (float)((Slot * 37) % 7 - 3) * 1.2f : 0.f;
				Made->SetActorLocationAndRotation(At + Out * (L.Depth * 0.5 + 0.4), FRotator(0.f, (float)S.Yaw, Roll));
			}
			if (Made->IsA(ALyudGoodsItem::StaticClass()))
			{
				ALyudGoodsItem::SetShelfStock(Made);
			}
		}
	}
	SlotActors[Slot] = Made;
}

void ULyudHeroRoom::RefreshPlant()
{
	if (!bBuilt)
	{
		return;
	}
	UWorld* World = GetWorld();
	const long long Day = Today();
	const FName Species(*LyudHomeImpl::Str(Model->Plant.Species));
	if (PlantActor && (!Model->HasPlant() || PlantActor->Species != Species))
	{
		PlantActor->Destroy();
		PlantActor = nullptr;
	}
	PlantDayShown = Day;
	if (!Model->HasPlant() || !World)
	{
		return;
	}
	const float Age = (float)Model->PlantAge(Day);
	if (!PlantActor)
	{
		const int32 Pot = Model->FindSlot("pot");
		if (Pot < 0)
		{
			return;
		}
		const LyudHome::FSlot& S = Model->Slots[(size_t)Pot];
		// in the room's own pot (the kit's): the plant brings none, and stands on the soil, not on the ground under it
		const FTransform Where(FRotator::ZeroRotator, FVector(S.X, S.Y, S.Z - 1.5));
		PlantActor = World->SpawnActorDeferred<AProceduralPlant>(AProceduralPlant::StaticClass(), Where, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!PlantActor)
		{
			return;
		}
		PlantActor->Species = Species;
		PlantActor->Seed = 19;
		PlantActor->AgeAtStart = Age;
		PlantActor->bDemoGrowth = false;
		PlantActor->bSnapToGround = false;
		PlantActor->Pot = EPlantPot::None;
		UGameplayStatics::FinishSpawningActor(PlantActor, Where);
	}
	PlantActor->SetDevTime(Age, -1.f);
	// left dry too long its leaves hang (the plant system's withered look, where the species has one; the words say it anyway)
	PlantActor->SetWithered(Model->Droops(Day));
}

void ULyudHeroRoom::RefreshAll()
{
	for (int32 I = 0; I < SlotActors.Num(); ++I)
	{
		RefreshSlot(I);
	}
	RefreshPlant();
	ApplyLight();
}

// ------------------------------------------------------------------ E

void ULyudHeroRoom::OnCityOpen(FName Screen, int32 Thing)
{
	const ULyudCityThings* City = ULyudCityThings::Get(this);
	if (Screen != LyudHomeImpl::HomeScreen || !City)
	{
		return;
	}
	FString Id = City->ThingId(Thing);
	if (Id.RemoveFromStart(LyudHomeImpl::PlotPrefix))
	{
		Use(Id);
	}
}

bool ULyudHeroRoom::Use(const FString& PlaceId)
{
	// not in the frame a choice was made: the E that took it must not open the place again
	if (!bLoaded || ChoiceSlot >= 0 || ChoiceFrame == GFrameCounter)
	{
		return false;
	}
	if (PlaceId == TEXT("switch"))
	{
		ToggleLight();
		Note(LyudText::Get(Model->bLight ? TEXT("city.myroom.light.on") : TEXT("city.myroom.light.off")));
		return true;
	}
	const int32 Slot = Model->FindSlot(LyudHomeImpl::Std(PlaceId));
	if (Slot < 0)
	{
		return false;
	}
	BeginChoice(Slot);
	return true;
}

void ULyudHeroRoom::BeginChoice(int32 Slot)
{
	const LyudHome::FSlot& S = Model->Slots[(size_t)Slot];
	const long long Day = Today();
	Options.Reset();
	if (S.Type == "pot")
	{
		if (!Model->HasPlant())
		{
			for (const std::string& Sp : Model->Species)
			{
				FOption O;
				O.Kind = FOption::PlantIn;
				O.What = LyudHomeImpl::Str(Sp);
				O.Label = LyudText::Get(TEXT("city.myroom.pot.plant"), { FStringFormatArg(LyudText::Get(TEXT("city.myroom.plant.") + O.What).ToString()) });
				Options.Add(O);
			}
		}
		else
		{
			const TPair<FOption::EKind, const TCHAR*> Verbs[] = { { FOption::Water, TEXT("city.myroom.pot.water") }, { FOption::Look, TEXT("city.myroom.pot.look") },
				{ FOption::Uproot, TEXT("city.myroom.pot.uproot") } };
			for (const TPair<FOption::EKind, const TCHAR*>& V : Verbs)
			{
				FOption O;
				O.Kind = V.Key;
				O.Label = LyudText::Get(V.Value);
				Options.Add(O);
			}
			if (Model->WateredToday(Day))
			{
				Options.Swap(0, 1);     // watered today: looking comes first
			}
		}
	}
	else
	{
		const FString Here = LyudHomeImpl::Str(Model->At(S.Id));
		if (!Here.IsEmpty())
		{
			FOption O;
			O.Kind = FOption::Take;
			O.What = Here;
			O.Label = LyudText::Get(Here.StartsWith(TEXT("item:")) ? TEXT("city.myroom.ui.take") : Here.StartsWith(TEXT("print:")) ? TEXT("city.myroom.ui.take_print")
				: TEXT("city.myroom.ui.take_trophy"),
				{ FStringFormatArg(NameOf(Here).ToString()) });
			Options.Add(O);
		}
		auto Offer = [this](const FString& What)
		{
			FOption O;
			O.Kind = FOption::Put;
			O.What = What;
			O.Label = LyudText::Get(TEXT("city.myroom.ui.put"), { FStringFormatArg(NameOf(What).ToString()) });
			Options.Add(O);
		};
		// the trophies he has won and has not put anywhere, then what is in his pockets that goes on such a place
		if (LyudHome::FModel::TrophyFits(S.Type))
		{
			const ULyudCityThings* City = ULyudCityThings::Get(this);
			for (const LyudHome::FTrophy& T : Model->Trophies)
			{
				const bool bHas = T.Stamp.empty() || (City && City->HasStamp(LyudHomeImpl::Str(T.Stamp)));
				if (bHas && Model->WhereIs(T.Id).empty())
				{
					Offer(LyudHomeImpl::Str(T.Id));
				}
			}
		}
		// his own photographs: the newest prints of the album that hang nowhere yet
		if (LyudHome::FModel::PrintFits(S.Type))
		{
			if (ULyudPhoto* Photo = ULyudPhoto::Get(this))
			{
				TArray<FIntPoint> Prints;
				Photo->NewestPrints(LyudHomeImpl::PrintsOffered + (int32)Model->Slots.size(), Prints);
				int32 Offered = 0;
				for (const FIntPoint& Print : Prints)
				{
					const std::string Id = LyudHome::FModel::PrintId(Print.X, Print.Y);
					if (Offered < LyudHomeImpl::PrintsOffered && Model->WhereIs(Id).empty())
					{
						Offer(LyudHomeImpl::Str(Id));
						++Offered;
					}
				}
			}
		}
		if (const ULyudInventory* Inv = ULyudInventory::Get(this))
		{
			for (const TPair<FName, int32>& Held : Inv->Held())
			{
				const FLyudItemDef* Def = Held.Value > 0 ? ULyudInventory::FindDef(Held.Key) : nullptr;
				if (Def && LyudHome::FModel::Fits(S.Type, LyudHomeImpl::LookOf(*Def)))
				{
					Offer(TEXT("item:") + Held.Key.ToString());
				}
			}
		}
		if (Options.Num() == 0)
		{
			Note(LyudText::Get(TEXT("city.myroom.ui.nothing.") + LyudHomeImpl::Str(S.Type)));
			return;
		}
	}
	ChoiceSlot = Slot;
	ChoiceSel = 0;
	ChoiceFrame = GFrameCounter;
	NoteUntil = 0.0;
	// the keys are the chooser's: nothing else of the game opens over it, and A and D do not walk him off
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetExternalScreen(this);
	}
	if (APlayerController* PC = LyudHomeImpl::Player(GetWorld()))
	{
		PC->SetIgnoreMoveInput(true);
		MoveHeldOn = PC;
	}
	ShowChoice();
}

FString ULyudHeroRoom::ChoiceLine() const
{
	return Options.IsValidIndex(ChoiceSel) && ChoiceSlot >= 0 ? LyudText::Get(TEXT("city.myroom.ui.choose"), { FStringFormatArg(Options[ChoiceSel].Label.ToString()),
		FStringFormatArg(ChoiceSel + 1), FStringFormatArg(Options.Num()) }).ToString() : FString();
}

void ULyudHeroRoom::ShowChoice()
{
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, FText::FromString(ChoiceLine()));
	}
}

void ULyudHeroRoom::EndChoice(bool bDo)
{
	if (ChoiceSlot < 0)
	{
		return;
	}
	const int32 Slot = ChoiceSlot;
	const FOption Option = Options.IsValidIndex(ChoiceSel) ? Options[ChoiceSel] : FOption();
	const bool bValid = Options.IsValidIndex(ChoiceSel);
	ChoiceSlot = -1;
	ChoiceFrame = GFrameCounter;
	Options.Reset();
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (UI)
	{
		UI->SetPrompt(this, FText::GetEmpty());
		UI->SetExternalScreen(nullptr);
	}
	if (APlayerController* PC = MoveHeldOn.Get())
	{
		PC->SetIgnoreMoveInput(false);
	}
	MoveHeldOn.Reset();
	if (bDo && bValid)
	{
		Do(Slot, Option);
	}
}

FText ULyudHeroRoom::TakeBack(int32 Slot)
{
	const LyudHome::FSlot& S = Model->Slots[(size_t)Slot];
	const FString What = LyudHomeImpl::Str(Model->Take(S.Id));
	if (What.IsEmpty())
	{
		return FText::GetEmpty();
	}
	if (What.StartsWith(TEXT("item:")))
	{
		if (ULyudInventory* Inv = ULyudInventory::Get(this))
		{
			Inv->Add(FName(*What.Mid(5)));
		}
	}
	RefreshSlot(Slot);
	return NameOf(What);
}

void ULyudHeroRoom::Do(int32 Slot, const FOption& Option)
{
	const LyudHome::FSlot& S = Model->Slots[(size_t)Slot];
	const long long Day = Today();
	const FVector At(S.X, S.Y, S.Z);
	ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this);
	const FString PlantName = Model->HasPlant() ? LyudText::Get(TEXT("city.myroom.plant.") + LyudHomeImpl::Str(Model->Plant.Species)).ToString() : FString();
	switch (Option.Kind)
	{
	case FOption::Take:
	{
		const bool bItem = Option.What.StartsWith(TEXT("item:"));
		const FText Name = TakeBack(Slot);
		Note(LyudText::Get(bItem ? TEXT("city.myroom.ui.taken") : Option.What.StartsWith(TEXT("print:")) ? TEXT("city.myroom.ui.taken_print")
			: TEXT("city.myroom.ui.taken_trophy"), { FStringFormatArg(Name.ToString()) }));
		break;
	}
	case FOption::Put:
	{
		ULyudInventory* Inv = ULyudInventory::Get(this);
		const bool bItem = Option.What.StartsWith(TEXT("item:"));
		const FName Id(*Option.What.Mid(5));
		if (bItem && (!Inv || Inv->Count(Id) <= 0))
		{
			break;      // it left his pocket meanwhile
		}
		TakeBack(Slot);
		if (!Model->Put(S.Id, LyudHomeImpl::Std(Option.What)))
		{
			break;
		}
		if (bItem)
		{
			Inv->Remove(Id, 1);
		}
		RefreshSlot(Slot);
		if (Sound)
		{
			Sound->PlayEffect(S.Type == "shelf" ? TEXT("rest_clink") : TEXT("diary_page"), At, 0.7f);
		}
		ULyudQuests::Notify(this, TEXT("do"), TEXT("myroom_put"));
		Note(LyudText::Get(TEXT("city.myroom.ui.placed"), { FStringFormatArg(NameOf(Option.What).ToString()) }));
		break;
	}
	case FOption::PlantIn:
		if (Model->PlantIn(LyudHomeImpl::Std(Option.What), Day))
		{
			RefreshPlant();
			ULyudQuests::Notify(this, TEXT("do"), TEXT("myroom_plant"));
			Note(LyudText::Get(TEXT("city.myroom.pot.planted"), { FStringFormatArg(LyudText::Get(TEXT("city.myroom.plant.") + Option.What).ToString()) }));
		}
		break;
	case FOption::Water:
	{
		const bool bHung = Model->Droops(Day);
		if (Model->Water(Day))
		{
			RefreshPlant();
			if (Sound)
			{
				Sound->PlayEffect(TEXT("rest_pour"), At, 0.5f);
			}
			ULyudQuests::Notify(this, TEXT("do"), TEXT("myroom_water"));
			Note(bHung ? LyudText::Get(TEXT("city.myroom.pot.revived"), { FStringFormatArg(PlantName) }) : LyudText::Get(TEXT("city.myroom.pot.watered")));
		}
		else
		{
			Note(LyudText::Get(TEXT("city.myroom.pot.watered_already")));
		}
		break;
	}
	case FOption::Look:
		if (Model->Droops(Day))
		{
			Note(LyudText::Get(TEXT("city.myroom.pot.droops"), { FStringFormatArg(PlantName), FStringFormatArg((int32)Model->DryDays(Day)) }));
			break;
		}
		Note(LyudText::Get(FString::Printf(TEXT("city.myroom.pot.stage.%d"), Model->PlantStage(Day)),
			{ FStringFormatArg(PlantName), FStringFormatArg((int32)(Day - Model->Plant.Day) + 1) }));
		break;
	case FOption::Uproot:
		Model->Uproot();
		RefreshPlant();
		Note(LyudText::Get(TEXT("city.myroom.pot.uprooted")));
		break;
	}
}

void ULyudHeroRoom::ChoiceKey(int32 Delta, bool bConfirm, bool bCancel)
{
	if (ChoiceSlot < 0 || Options.Num() == 0)
	{
		return;
	}
	if (Delta != 0)
	{
		ChoiceSel = ((ChoiceSel + Delta) % Options.Num() + Options.Num()) % Options.Num();
		ShowChoice();
	}
	if (bConfirm || bCancel)
	{
		EndChoice(bConfirm);
	}
}

void ULyudHeroRoom::TickChoice()
{
	UWorld* World = GetWorld();
	APlayerController* PC = LyudHomeImpl::Player(World);
	const AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	const ULyudGameMenuSubsystem* Menu = World ? World->GetSubsystem<ULyudGameMenuSubsystem>() : nullptr;
	if (!PC || !Hero || (Menu && Menu->IsOpen()))
	{
		EndChoice(false);
		return;
	}
	if (GFrameCounter == ChoiceFrame)
	{
		return;     // the E that opened it
	}
	if (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Enter))
	{
		EndChoice(true);
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::S) || PC->WasInputKeyJustPressed(EKeys::W) || PC->WasInputKeyJustPressed(EKeys::Escape)
		|| PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Up) || PC->WasInputKeyJustPressed(EKeys::Down))
	{
		EndChoice(false);
		return;
	}
	const int32 Delta = (PC->WasInputKeyJustPressed(EKeys::D) || PC->WasInputKeyJustPressed(EKeys::Right)) ? 1
		: ((PC->WasInputKeyJustPressed(EKeys::A) || PC->WasInputKeyJustPressed(EKeys::Left)) ? -1 : 0);
	if (Delta != 0)
	{
		ChoiceKey(Delta, false, false);
	}
}

void ULyudHeroRoom::Note(const FText& What)
{
	UE_LOG(LogLyudHome, Display, TEXT("HeroRoom: %s"), *What.ToString());
	UWorld* World = GetWorld();
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!World || !UI)
	{
		return;
	}
	NoteUntil = World->GetTimeSeconds() + LyudHomeImpl::NoteSeconds;
	UI->SetPrompt(this, What);
}

void ULyudHeroRoom::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !bLoaded)
	{
		return;
	}
	if (ChoiceSlot >= 0)
	{
		TickChoice();
		return;
	}
	if (NoteUntil > 0.0 && World->GetTimeSeconds() >= NoteUntil)
	{
		NoteUntil = 0.0;
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
		{
			UI->SetPrompt(this, FText::GetEmpty());
		}
	}
	Clock -= DeltaTime;
	if (Clock > 0.f)
	{
		return;
	}
	Clock = LyudHomeImpl::Every;
	if (!bCityBound)
	{
		BindCity();
	}
	TickAway();
	// the plant grows by the day: its look is set again when the day turns (a sleep, a jump of the clock)
	if (bBuilt && Model->HasPlant() && Today() != PlantDayShown)
	{
		RefreshPlant();
	}
}

void ULyudHeroRoom::TickAway()
{
	if (!Model->HasPlant() && !bMindNote)
	{
		return;
	}
	const APlayerController* PC = LyudHomeImpl::Player(GetWorld());
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	const FVector Where = Pawn->GetActorLocation();
	const bool bInRoom = Model->InBox(Where.X, Where.Y, Where.Z, 20.0);
	const long long Day = Today();
	// the owner comes only while the hero is out, and once a day at most
	if (!bInRoom && Day != MindDay)
	{
		MindDay = Day;
		if (Model->Mind(Day))
		{
			bMindNote = true;
			RefreshPlant();
			UE_LOG(LogLyudHome, Display, TEXT("HeroRoom: the owner watered the plant (dry too long), time %d"), Model->Plant.Minded);
		}
	}
	// the note waits on the pot's saucer until he is back in the room
	if (bMindNote && bInRoom && NoteUntil <= 0.0)
	{
		bMindNote = false;
		if (Model->HasPlant())
		{
			const FString PlantName = LyudText::Get(TEXT("city.myroom.plant.") + LyudHomeImpl::Str(Model->Plant.Species)).ToString();
			Note(LyudText::Get(FString::Printf(TEXT("city.myroom.pot.minded.%d"), 1 + (Model->Plant.Minded + 2) % 3), { FStringFormatArg(PlantName) }));
			ULyudQuests::Notify(this, TEXT("do"), TEXT("myroom_minded"));
		}
	}
}

// ------------------------------------------------------------------ developer mode and the console

void ULyudHeroRoom::ToggleLight()
{
	if (!Model)
	{
		return;
	}
	Model->bLight = !Model->bLight;
	ApplyLight();
	if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
	{
		Sound->PlayEffect(TEXT("mouse_clicks"), FVector(Model->OriginX, Model->OriginY, Model->OriginZ + 130.0), 0.6f);
	}
}

bool ULyudHeroRoom::IsLightOn() const
{
	return Model && Model->bLight;
}

void ULyudHeroRoom::DevClear()
{
	if (!bLoaded)
	{
		return;
	}
	if (ChoiceSlot >= 0)
	{
		EndChoice(false);
	}
	for (int32 I = 0; I < (int32)Model->Slots.size(); ++I)
	{
		TakeBack(I);
	}
	Model->Uproot();
	RefreshPlant();
}

void ULyudHeroRoom::DevFill()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!bLoaded || !Inv)
	{
		return;
	}
	DevClear();
	// a thing for every place, of the kinds the game has: cards, things for the nails, things for the shelves
	// (only things whose pictures exist: the cups on the wall shelf, as the room was drawn)
	const TCHAR* Cards[] = { TEXT("postcard") };
	const TCHAR* Frames[] = { TEXT("record_sea_1"), TEXT("record_night_1"), TEXT("daily_paper") };
	const TCHAR* Shelves[] = { TEXT("cup:cog_chess"), TEXT("cup:tt_cup"), TEXT("shell_box"), TEXT("record_dawn_1"), TEXT("book:birds"), TEXT("amber_charm"),
		TEXT("cup:san_dominoes"), TEXT("shell_box") };
	// and his own photographs, when the album has any: every second card and the last nail
	TArray<FIntPoint> Prints;
	if (ULyudPhoto* Photo = ULyudPhoto::Get(this))
	{
		Photo->NewestPrints(4, Prints);
	}
	int32 Card = 0, Frame = 0, Shelf = 0, Print = 0;
	for (int32 I = 0; I < (int32)Model->Slots.size(); ++I)
	{
		const LyudHome::FSlot& S = Model->Slots[(size_t)I];
		if (Prints.IsValidIndex(Print) && ((S.Type == "card" && I % 2 == 1) || S.Id == "frame_3")
			&& Model->Put(S.Id, LyudHome::FModel::PrintId(Prints[Print].X, Prints[Print].Y)))
		{
			++Print;
			RefreshSlot(I);
			continue;
		}
		const TCHAR* Pick = S.Type == "card" ? Cards[Card++ % UE_ARRAY_COUNT(Cards)] : S.Type == "frame" ? Frames[Frame++ % UE_ARRAY_COUNT(Frames)]
			: S.Type == "shelf" ? Shelves[Shelf++ % UE_ARRAY_COUNT(Shelves)] : nullptr;
		if (!Pick)
		{
			continue;
		}
		FString What(Pick);
		if (!What.Contains(TEXT(":")))
		{
			if (!ULyudInventory::FindDef(FName(Pick)))
			{
				continue;
			}
			What = TEXT("item:") + What;
		}
		if (Model->Put(S.Id, LyudHomeImpl::Std(What)))
		{
			RefreshSlot(I);
		}
	}
	if (!Model->Species.empty())
	{
		Model->PlantIn(Model->Species[0], Today() - 20);
		Model->Plant.Watered = 14;
	}
	RefreshPlant();
	UE_LOG(LogLyudHome, Display, TEXT("HeroRoom: furnished for a look: %d things, the plant %d days old"), Model->NumPlaced(), 20);
}

void ULyudHeroRoom::DevGrow(int32 Days)
{
	if (bLoaded && Model->HasPlant())
	{
		Model->Plant.Day -= Days;
		RefreshPlant();
	}
}

void ULyudHeroRoom::DevDry(int32 Days)
{
	if (bLoaded && Model->HasPlant() && Days > 0)
	{
		// the whole of its life moves back, so the last water is that much older
		Model->Plant.Day -= Days;
		Model->Plant.LastWater = Model->Plant.LastWater >= 0 ? Model->Plant.LastWater - Days : -1;
		MindDay = -1;
		RefreshPlant();
		UE_LOG(LogLyudHome, Display, TEXT("HeroRoom: the plant is %d days dry%s"), (int32)Model->DryDays(Today()), Model->Droops(Today()) ? TEXT(", its leaves hang") : TEXT(""));
	}
}

FString ULyudHeroRoom::Report() const
{
	if (!bLoaded)
	{
		return TEXT("HeroRoom: no data");
	}
	FString Out = FString::Printf(TEXT("HeroRoom: built %d, light %d, %d things"), bBuilt ? 1 : 0, Model->bLight ? 1 : 0, Model->NumPlaced());
	for (const LyudHome::FSlot& S : Model->Slots)
	{
		const std::string& What = Model->At(S.Id);
		if (!What.empty())
		{
			Out += FString::Printf(TEXT("\n  %s: %s"), *LyudHomeImpl::Str(S.Id), *LyudHomeImpl::Str(What));
		}
	}
	if (Model->HasPlant())
	{
		const long long Day = Today();
		Out += FString::Printf(TEXT("\n  pot: %s, day %d, watered %d times, age %.2f, stage %d, dry %d days%s, minded %d times"), *LyudHomeImpl::Str(Model->Plant.Species),
			(int32)(Day - Model->Plant.Day) + 1, Model->Plant.Watered, Model->PlantAge(Day), Model->PlantStage(Day), (int32)Model->DryDays(Day),
			Model->Droops(Day) ? TEXT(" (hangs)") : TEXT(""), Model->Plant.Minded);
	}
	return Out;
}

namespace LyudHomeConsole
{
	ULyudHeroRoom* Of(UWorld* World)
	{
		return World ? World->GetSubsystem<ULyudHeroRoom>() : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GReport(TEXT("lyud.Home.Report"), TEXT("The hero's room: what stands where, the plant, the lamp (to the log)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (const ULyudHeroRoom* Home = Of(World))
			{
				UE_LOG(LogLyudHome, Display, TEXT("%s"), *Home->Report());
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GFill(TEXT("lyud.Home.Fill"), TEXT("The hero's room furnished for a look: a thing on every place, a grown plant."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudHeroRoom* Home = Of(World))
			{
				Home->DevFill();
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GClear(TEXT("lyud.Home.Clear"), TEXT("The hero's room: everything back in the pocket, the pot empty."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudHeroRoom* Home = Of(World))
			{
				Home->DevClear();
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GGrow(TEXT("lyud.Home.Grow"), TEXT("lyud.Home.Grow <days>: the plant in the hero's pot as if planted that much earlier."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudHeroRoom* Home = Of(World))
			{
				Home->DevGrow(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 10);
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GDry(TEXT("lyud.Home.Dry"), TEXT("lyud.Home.Dry <days>: the plant in the hero's pot as if not watered for that long (8 by default)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudHeroRoom* Home = Of(World))
			{
				Home->DevDry(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 8);
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GLight(TEXT("lyud.Home.Light"), TEXT("The lamp of the hero's room on or off."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudHeroRoom* Home = Of(World))
			{
				Home->ToggleLight();
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GUse(TEXT("lyud.Home.Use"),
		TEXT("lyud.Home.Use <place> [next|prev|ok|cancel ...]: E at a place of the hero's room (pin_1, frame_2, shelf_3, top_1, pot, switch), then the chooser's keys."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ULyudHeroRoom* Home = Of(World);
			if (!Home || Args.Num() == 0)
			{
				return;
			}
			if (!Home->Use(Args[0]))
			{
				UE_LOG(LogLyudHome, Warning, TEXT("HeroRoom: no place %s (or the chooser is up)"), *Args[0]);
				return;
			}
			for (int32 I = 1; I < Args.Num(); ++I)
			{
				Home->ChoiceKey(Args[I] == TEXT("next") ? 1 : Args[I] == TEXT("prev") ? -1 : 0, Args[I] == TEXT("ok"), Args[I] == TEXT("cancel"));
			}
			UE_LOG(LogLyudHome, Display, TEXT("HeroRoom: chooser: %s"), Home->HasModeUp() ? *Home->ChoiceLine() : TEXT("(shut)"));
		}));

#if LYUD_HOME_DEV
	LyudDev::FAutoAction GDevFill(TEXT("home.fill"), TEXT("home"), TEXT("dev.action.home.fill"),
		[](UWorld* W) { if (ULyudHeroRoom* H = Of(W)) { H->DevFill(); } });
	LyudDev::FAutoAction GDevClear(TEXT("home.clear"), TEXT("home"), TEXT("dev.action.home.clear"),
		[](UWorld* W) { if (ULyudHeroRoom* H = Of(W)) { H->DevClear(); } });
	LyudDev::FAutoAction GDevGrow(TEXT("home.plant_grow"), TEXT("home"), TEXT("dev.action.home.plant_grow"),
		[](UWorld* W) { if (ULyudHeroRoom* H = Of(W)) { H->DevGrow(10); } });
	LyudDev::FAutoAction GDevDry(TEXT("home.plant_dry"), TEXT("home"), TEXT("dev.action.home.plant_dry"),
		[](UWorld* W) { if (ULyudHeroRoom* H = Of(W)) { H->DevDry(8); } });
	LyudDev::FAutoAction GDevLight(TEXT("home.light"), TEXT("home"), TEXT("dev.action.home.light"),
		[](UWorld* W) { if (ULyudHeroRoom* H = Of(W)) { H->ToggleLight(); } },
		[](UWorld* W) { const ULyudHeroRoom* H = Of(W); return H && H->IsLightOn(); });
#endif
}
