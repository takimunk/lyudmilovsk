#include "UI/LyudDiaryBook.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "UI/LyudDiaryStyle.h"

namespace
{
	struct FMeshData
	{
		TArray<FVector3f> Pos, Nrm;
		TArray<FVector2f> UV;
		TArray<int32> Tris;
	};

	UStaticMesh* BuildMesh(FName Name, const FMeshData& D)
	{
		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
		VUV.SetNumChannels(1);
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = Name;
		TArray<FVertexInstanceID> Inst;
		for (int32 I = 0; I < D.Pos.Num(); ++I)
		{
			const FVertexID V = MD.CreateVertex();
			VPos[V] = D.Pos[I];
			const FVertexInstanceID VI = MD.CreateVertexInstance(V);
			const FVector3f N = D.Nrm[I].GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
			VNrm[VI] = N;
			const FVector3f Ref = FMath::Abs(N.Z) < 0.9f ? FVector3f::UpVector : FVector3f::XAxisVector;
			VTan[VI] = FVector3f::CrossProduct(Ref, N).GetSafeNormal(UE_SMALL_NUMBER, FVector3f::XAxisVector);
			VSign[VI] = 1.f;
			VUV.Set(VI, 0, D.UV[I]);
			Inst.Add(VI);
		}
		for (int32 T = 0; T + 2 < D.Tris.Num(); T += 3)
		{
			MD.CreateTriangle(PG, { Inst[D.Tris[T]], Inst[D.Tris[T + 1]], Inst[D.Tris[T + 2]] });
		}
		UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), Name, RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, Name));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		Mesh->AddToRoot();
		return Mesh;
	}

	/**
	 * A box with rounded edges, HalfExtent each way, edges of Radius: the six faces of a cube subdivided N x N, every point
	 * pulled in to the inner box and pushed out again by the radius along its offset (so the faces stay flat and the edges
	 * and corners are round). Planar UVs from the face it came from.
	 */
	FMeshData RoundedBox(const FVector3f& HalfExtent, float Radius, int32 N = 6)
	{
		FMeshData D;
		const FVector3f Inner = HalfExtent - FVector3f(Radius);
		static const FVector3f Axes[6][3] = {
			{ FVector3f(1, 0, 0), FVector3f(0, 1, 0), FVector3f(0, 0, 1) }, { FVector3f(-1, 0, 0), FVector3f(0, -1, 0), FVector3f(0, 0, 1) },
			{ FVector3f(0, 1, 0), FVector3f(-1, 0, 0), FVector3f(0, 0, 1) }, { FVector3f(0, -1, 0), FVector3f(1, 0, 0), FVector3f(0, 0, 1) },
			{ FVector3f(0, 0, 1), FVector3f(0, 1, 0), FVector3f(-1, 0, 0) }, { FVector3f(0, 0, -1), FVector3f(0, 1, 0), FVector3f(1, 0, 0) } };
		for (int32 F = 0; F < 6; ++F)
		{
			const FVector3f Nf = Axes[F][0], U = Axes[F][1], V = Axes[F][2];
			const int32 Base = D.Pos.Num();
			for (int32 J = 0; J <= N; ++J)
			{
				for (int32 I = 0; I <= N; ++I)
				{
					// points bunched toward the edges, where the rounding is
					auto Spread = [](float T) { const float S = T * 2.f - 1.f; return FMath::Sign(S) * FMath::Pow(FMath::Abs(S), 0.6f); };
					const FVector3f Cube = Nf + U * Spread(float(I) / N) + V * Spread(float(J) / N);
					const FVector3f P = Cube * HalfExtent;
					const FVector3f Clamped(FMath::Clamp(P.X, -Inner.X, Inner.X), FMath::Clamp(P.Y, -Inner.Y, Inner.Y), FMath::Clamp(P.Z, -Inner.Z, Inner.Z));
					FVector3f Off = P - Clamped;
					const FVector3f Normal = Off.SizeSquared() > 1e-8f ? Off.GetSafeNormal() : Nf;
					D.Pos.Add(Clamped + Normal * Radius);
					D.Nrm.Add(Normal);
					D.UV.Add(FVector2f(float(I) / N, 1.f - float(J) / N));
				}
			}
			for (int32 J = 0; J < N; ++J)
			{
				for (int32 I = 0; I < N; ++I)
				{
					const int32 A = Base + J * (N + 1) + I, B = A + 1, C = A + (N + 1), E = C + 1;
					// counter-clockwise seen from outside (Unreal's front faces are clockwise from the camera: (a, c, b))
					D.Tris.Append({ A, C, B, B, C, E });
				}
			}
		}
		return D;
	}

	/** A flat rectangle facing +X: Y from Y0 to Y1, Z from -H/2 to H/2; U from U0 to U1 along Y, V from 0 at the top. */
	FMeshData Quad(float Y0, float Y1, float H, float U0, float U1)
	{
		FMeshData D;
		D.Pos = { FVector3f(0, Y0, H * 0.5f), FVector3f(0, Y1, H * 0.5f), FVector3f(0, Y1, -H * 0.5f), FVector3f(0, Y0, -H * 0.5f) };
		D.Nrm = { FVector3f(1, 0, 0), FVector3f(1, 0, 0), FVector3f(1, 0, 0), FVector3f(1, 0, 0) };
		D.UV = { FVector2f(U0, 0), FVector2f(U1, 0), FVector2f(U1, 1), FVector2f(U0, 1) };
		// wound like the rounded box's faces, or the quad is culled and the block's lit edge shows through
		D.Tris = { 0, 1, 2, 0, 2, 3 };
		return D;
	}

	struct FBookMeshes
	{
		UStaticMesh* BoardMesh = nullptr;
		UStaticMesh* BlockMesh = nullptr;
		UStaticMesh* TabMesh = nullptr;
		UStaticMesh* LeftQuad = nullptr;
		UStaticMesh* RightQuad = nullptr;
	};

	const FBookMeshes& Meshes()
	{
		static FBookMeshes M;
		if (M.BoardMesh)
		{
			return M;
		}
		using B = ALyudDiaryBook;
		M.BoardMesh = BuildMesh(TEXT("Lyud_DiaryBoard"), RoundedBox(FVector3f(B::Board * 0.5f, (B::PageW + B::Overhang) * 0.5f, (B::PageH + 2.f * B::Overhang) * 0.5f), 0.12f));
		M.BlockMesh = BuildMesh(TEXT("Lyud_DiaryBlock"), RoundedBox(FVector3f(B::Block * 0.5f, B::PageW * 0.5f, B::PageH * 0.5f), 0.08f, 3));
		M.TabMesh = BuildMesh(TEXT("Lyud_DiaryTab"), RoundedBox(FVector3f(0.04f, 0.75f, 2.3f), 0.035f, 3));
		// facing the reader, the book's +Y is on his left: the left page runs to +Y, the right one to -Y
		M.LeftQuad = BuildMesh(TEXT("Lyud_DiaryPageL"), Quad(0.f, B::PageW, B::PageH, 0.5f, 0.f));
		M.RightQuad = BuildMesh(TEXT("Lyud_DiaryPageR"), Quad(-B::PageW, 0.f, B::PageH, 1.f, 0.5f));
		return M;
	}

	/** Where index tab K sits up the fore-edge (its centre, cm from the middle), top first. */
	float TabZ(int32 K)
	{
		return 11.2f - K * 5.6f;
	}

	UMaterialInterface* Basic()
	{
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
}

ALyudDiaryBook::ALyudDiaryBook()
{
	PrimaryActorTick.bCanEverTick = false;
	Spine = CreateDefaultSubobject<USceneComponent>(TEXT("Spine"));
	SetRootComponent(Spine);
	LeftHinge = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHinge"));
	LeftHinge->SetupAttachment(Spine);
	SetActorEnableCollision(false);
}

void ALyudDiaryBook::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

UStaticMeshComponent* ALyudDiaryBook::AddPart(USceneComponent* Parent, const FName Name, UStaticMesh* Mesh, const FVector& At, UMaterialInterface* Material)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, Name);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(Parent);
	C->SetRelativeLocation(At);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(true);
	C->bCastDynamicShadow = true;
	C->SetMaterial(0, Material);
	C->RegisterComponent();
	Parts.Add(C);
	return C;
}

void ALyudDiaryBook::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	const FBookMeshes& M = Meshes();
	UMaterialInterface* Base = Basic();
	auto Tinted = [this, Base](const FLinearColor& C)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("Color"), C);
		return MID;
	};
	// oxblood leatherette, cream paper edges
	UMaterialInstanceDynamic* Leather = Tinted(FLinearColor(0.16f, 0.035f, 0.028f));
	UMaterialInstanceDynamic* Edges = Tinted(FLinearColor(0.74f, 0.69f, 0.58f));
	UMaterialInterface* PageBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UI/Diary/M_DiaryPage.M_DiaryPage"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	PageMaterial = UMaterialInstanceDynamic::Create(PageBase ? PageBase : Base, this);
	if (!PageBase)
	{
		PageMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.86f, 0.82f, 0.72f));
	}

	for (const bool bLeft : { false, true })
	{
		USceneComponent* Half = bLeft ? LeftHinge.Get() : Spine.Get();
		const float S = bLeft ? 1.f : -1.f;
		// the pages behind the open surface, the board behind them (overhanging at the fore-edge, top and bottom)
		AddPart(Half, bLeft ? TEXT("BlockL") : TEXT("BlockR"), M.BlockMesh, FVector(-Block * 0.5f, S * PageW * 0.5f, 0.f), Edges);
		AddPart(Half, bLeft ? TEXT("BoardL") : TEXT("BoardR"), M.BoardMesh, FVector(-Block - Board * 0.5f, S * (PageW + Overhang) * 0.5f, 0.f), Leather);
		UStaticMeshComponent* Page = AddPart(Half, bLeft ? TEXT("PageL") : TEXT("PageR"), bLeft ? M.LeftQuad : M.RightQuad, FVector(0.02f, 0.f, 0.f), PageMaterial);
		Page->SetCastShadow(false);
		(bLeft ? LeftPage : RightPage) = Page;
		TArray<TObjectPtr<UStaticMeshComponent>>& Tabs = bLeft ? TabsLeft : TabsRight;
		for (int32 K = 0; K < NumTabs; ++K)
		{
			UStaticMeshComponent* T = AddPart(Half, *FString::Printf(TEXT("Tab%s%d"), bLeft ? TEXT("L") : TEXT("R"), K), M.TabMesh,
				FVector(-0.06f, S * (PageW + 0.55f), TabZ(K)), Tinted(LyudDiary::TabColor(static_cast<LyudDiary::ESection>(K))));
			Tabs.Add(T);
		}
	}
	SetCover(Cover);
	SetTabs(0);
}

void ALyudDiaryBook::SetCover(float Degrees)
{
	Cover = FMath::Clamp(Degrees, 0.f, 180.f);
	// turning by -Cover swings the front board toward the reader as it opens
	LeftHinge->SetRelativeRotation(FRotator(0.f, -Cover, 0.f));
}

void ALyudDiaryBook::SetPages(UTexture* Spread)
{
	if (PageMaterial)
	{
		PageMaterial->SetTextureParameterValue(TEXT("Page"), Spread);
		PageMaterial->SetScalarParameterValue(TEXT("HasPage"), Spread ? 1.f : 0.f);
	}
}

void ALyudDiaryBook::SetPageBrightness(float B)
{
	if (PageMaterial)
	{
		PageMaterial->SetScalarParameterValue(TEXT("Bright"), B);
	}
}

void ALyudDiaryBook::SetTabs(int32 SectionsOnLeft)
{
	for (int32 K = 0; K < NumTabs; ++K)
	{
		const bool bLeft = K < SectionsOnLeft;
		if (TabsLeft.IsValidIndex(K) && TabsRight.IsValidIndex(K))
		{
			TabsLeft[K]->SetVisibility(bLeft);
			TabsRight[K]->SetVisibility(!bLeft);
		}
	}
}

void ALyudDiaryBook::SpreadCorners(FVector Out[4]) const
{
	const FTransform T = GetActorTransform();
	Out[0] = T.TransformPosition(FVector(0.f, PageW, PageH * 0.5f));
	Out[1] = T.TransformPosition(FVector(0.f, -PageW, PageH * 0.5f));
	Out[2] = T.TransformPosition(FVector(0.f, -PageW, -PageH * 0.5f));
	Out[3] = T.TransformPosition(FVector(0.f, PageW, -PageH * 0.5f));
}

FReachShape ALyudDiaryBook::Shape() const
{
	// the right half always; the left half where its hinge has put it
	FReachShape S;
	const float T = Block + Board;
	auto Half = [&S, T](const FTransform& Frame, float Side)
	{
		FReachPrim P;
		P.Kind = FReachPrim::EKind::Box;
		P.Extent = FVector(T * 0.5f, (PageW + Overhang) * 0.5f, PageH * 0.5f + Overhang);
		P.Xf = FTransform(FVector(-T * 0.5f, Side * (PageW + Overhang) * 0.5f, 0.f)) * Frame;
		P.Xf.SetScale3D(FVector::OneVector);
		S.Prims.Add(P);
	};
	Half(GetActorTransform(), -1.f);
	Half(LeftHinge->GetComponentTransform(), 1.f);
	return S;
}

void ALyudDiaryBook::HoldFrame(FVector& Palm, FVector& Finger, FVector& Normal) const
{
	// under the bottom edge: below the spine when open, below the middle of the shut book when shut
	const float Open = 1.f - Cover / 180.f;
	const float T = Block + Board;
	const FVector Local(FMath::Lerp(0.f, -T * 0.5f, Open), FMath::Lerp(-PageW * 0.5f, 0.f, Open), -PageH * 0.5f - Overhang - 1.3f);
	const FTransform X = GetActorTransform();
	Palm = X.TransformPosition(Local);
	Normal = X.TransformVectorNoScale(FVector::UpVector);
	Finger = X.TransformVectorNoScale(-FVector::ForwardVector);
}
