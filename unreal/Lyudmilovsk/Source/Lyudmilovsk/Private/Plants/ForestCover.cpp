#include "Plants/ForestCover.h"

#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogForestCover, Log, All);

namespace
{
	/** The map's frame (docs/world_map.md): world Y cm = 790 + map Y * 100. */
	constexpr double CoverFaceY = 790.0;

	struct FCoverGrid
	{
		int32 Nx = 0;
		int32 Ny = 0;
		float X0 = 0.f;
		float Y0 = 0.f;
		float Step = 1.f;
		TArray<uint8> Cells;
		bool bLoaded = false;
	};

	FCoverGrid ReadGrid()
	{
		FCoverGrid G;
		const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("World"), TEXT("forest_cover.fcv"));
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
		{
			UE_LOG(LogForestCover, Log, TEXT("no %s: every forest tree is kept"), *Path);
			return G;
		}
		constexpr int32 Header = 4 + 4 * 5;
		if (Bytes.Num() < Header || FMemory::Memcmp(Bytes.GetData(), "FCV1", 4) != 0)
		{
			UE_LOG(LogForestCover, Warning, TEXT("%s: not an FCV1 file, ignored"), *Path);
			return G;
		}
		int32 InNx = 0, InNy = 0;
		float InX0 = 0.f, InY0 = 0.f, InStep = 0.f;
		FMemory::Memcpy(&InNx, Bytes.GetData() + 4, 4);
		FMemory::Memcpy(&InNy, Bytes.GetData() + 8, 4);
		FMemory::Memcpy(&InX0, Bytes.GetData() + 12, 4);
		FMemory::Memcpy(&InY0, Bytes.GetData() + 16, 4);
		FMemory::Memcpy(&InStep, Bytes.GetData() + 20, 4);
		if (InNx <= 1 || InNy <= 1 || InStep <= 0.f || (int64)Bytes.Num() < (int64)Header + (int64)InNx * InNy)
		{
			UE_LOG(LogForestCover, Warning, TEXT("%s: bad header (%d x %d, step %.2f, %d bytes), ignored"), *Path, InNx, InNy, InStep, Bytes.Num());
			return G;
		}
		G.Nx = InNx;
		G.Ny = InNy;
		G.X0 = InX0;
		G.Y0 = InY0;
		G.Step = InStep;
		G.Cells.Append(Bytes.GetData() + Header, InNx * InNy);
		G.bLoaded = true;
		UE_LOG(LogForestCover, Log, TEXT("%s: %d x %d cells of %.1f m from map (%.0f, %.0f)"), *Path, InNx, InNy, InStep, InX0, InY0);
		return G;
	}

	const FCoverGrid& Grid()
	{
		// a function-local static: initialised once, thread-safe (C++11)
		static const FCoverGrid G = ReadGrid();
		return G;
	}
}

namespace ForestCover
{
	float At(const FVector& WorldLocation)
	{
		const FCoverGrid& G = Grid();
		if (!G.bLoaded) return 1.f;
		const double MapX = WorldLocation.X / 100.0;
		const double MapY = (WorldLocation.Y - CoverFaceY) / 100.0;
		const double U = (MapX - G.X0) / G.Step;
		const double V = (MapY - G.Y0) / G.Step;
		if (U < 0.0 || V < 0.0 || U > G.Nx - 1 || V > G.Ny - 1) return 1.f;
		const int32 I = FMath::Min(FMath::FloorToInt32(U), G.Nx - 2);
		const int32 J = FMath::Min(FMath::FloorToInt32(V), G.Ny - 2);
		const float Fu = (float)(U - I), Fv = (float)(V - J);
		const uint8* Row0 = G.Cells.GetData() + (int64)J * G.Nx;
		const uint8* Row1 = Row0 + G.Nx;
		const float A = FMath::Lerp((float)Row0[I], (float)Row0[I + 1], Fu);
		const float B = FMath::Lerp((float)Row1[I], (float)Row1[I + 1], Fu);
		return FMath::Lerp(A, B, Fv) / 255.f;
	}

	bool IsLoaded()
	{
		return Grid().bLoaded;
	}

	FString Describe()
	{
		const FCoverGrid& G = Grid();
		return G.bLoaded ? FString::Printf(TEXT("forest_cover.fcv %d x %d @ %.0f m"), G.Nx, G.Ny, G.Step) : FString(TEXT("no forest_cover.fcv"));
	}
}
