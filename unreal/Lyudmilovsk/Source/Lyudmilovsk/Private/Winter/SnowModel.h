// Winter's snow and sea ice, in plain C++ (no Unreal types) so it can be run and checked outside the engine
// (tools/winter/run.sh). ULyudWinterSubsystem wraps it; docs/winter.md explains the rules.
//
// The weather engine says how much snow lies on open level ground (FWeatherState::SnowDepthCm) and how fast it falls;
// this file says where it lies:
//   - FCover: the world as the snow sees it, a 2 m grid of the ground's height and the highest thing over it (roofs,
//     canopies, kiosks), read by traces in the game. Under a roof there is no snow on the ground.
//   - FDriftBasis: how sheltered every cell is from each of 8 winds (Winstral's Sx: the steepest rise of the ground and
//     of what stands on it within 100 m upwind). Built once; cheap to weigh afterwards.
//   - FSnowField: the snow's depth on a 1 m grid: the level depth moved by the winter's blowing snow from the exposed
//     places (hill brows, the windward sides of houses) into the sheltered ones (lee slopes, behind houses, forest
//     edges, the cliff's brow) up to ~1 m, shed from steep slopes, thinner under the forest, none under roofs, and cut
//     out of the paths that people shovel, the shovelled snow heaped in berms beside them.
//   - FSnowPaths: the paths (the walk grid's paved cells and the doorsteps), cut into patches of ~12 m that somebody
//     clears one at a time (FPatch::Progress sweeps along the patch).
//   - FIceMask: where the sea is frozen: the distance of every sea cell from water shallower than ShallowM (the shore at
//     the tide now, the railway's bank, the island); the ice keeps OpenStripM (10-15 m, ragged) away from it.
//
// Frame: world centimetres (X, Y as Unreal; Z up). Wind bearings are compass degrees the wind comes from, north = +Y,
// east = -X (as ALyudSky). The noises are mirrored in HLSL by tools/winter/winter_hlsl.py: keep them in step.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace LyudWinter
{
	/** Runs Body(0..N-1), in parallel where the caller can (ParallelFor in the game, a plain loop in tests). */
	using FParallel = std::function<void(int N, const std::function<void(int)>& Body)>;
	void Serial(int N, const std::function<void(int)>& Body);

	/** Cell kinds of FCover. */
	enum ECell : uint8_t { CellNone = 0, CellOpen = 1, CellRoof = 2, CellForest = 3 };

	/** Something tall enough to walk under counts as a roof (cm over the ground). */
	constexpr float RoofMinCm = 180.f;
	/** What a forest's canopy counts for in the shelter (cm): crowns let the wind through. */
	constexpr float ForestShelterCm = 700.f;

	struct FCover
	{
		int W = 0, H = 0;
		double X0 = 0.0, Y0 = 0.0; // world cm of cell (0, 0)'s centre
		float Cell = 200.f;        // cm
		std::vector<float> Ground; // cm (the landscape or the bed); NoGround where the trace found none
		std::vector<float> Top;    // cm, the highest surface over the cell (Ground when nothing stands there)
		std::vector<uint8_t> Kind; // ECell
		/** 0 the wild (forest, fields) .. 255 the town (within ~50 m of a roof, fading out by ~110 m): its snow is kept low. */
		std::vector<uint8_t> Urban;
		static constexpr float NoGround = -1.0e9f;

		void Init(int InW, int InH, double InX0, double InY0, float InCell);
		bool Valid() const { return W > 0 && H > 0 && (int)Ground.size() == W * H; }
		int Index(int X, int Y) const { return Y * W + X; }
		/** Kind from the heights: none (no ground or under SeaZ + 30 cm), roof, open; forest cells (marked before) stay. */
		void Classify(float SeaZ);
		/** Urban from the distance to the nearest roof. */
		void ComputeUrban(const FParallel& Par);
		/** Drops lone roofs (lamp posts, tree trunks, a pole's cap): a roof cell needs 3 of its 8 neighbours roofed. */
		void CleanRoofs();
		/** Bilinear ground height (cm); NoGround outside. */
		float GroundAt(double X, double Y) const;
		uint8_t KindAt(double X, double Y) const;
		/** Marks the cells inside a rectangle (world cm) as forest (open ground with a canopy). */
		void MarkForest(double MinX, double MinY, double MaxX, double MaxY);
		/** Stable hash of the heights (for caches). */
		uint32_t Hash() const;
		bool Save(const std::string& Path) const;
		bool Load(const std::string& Path);
	};

	/** Shelter of every 2 m cell from 8 winds (N, NE, E, SE, S, SW, W, NW from), degrees. */
	struct FDriftBasis
	{
		int W = 0, H = 0;
		std::vector<int8_t> Sx[8];
		/** The other side: a tall wall within 4 m downwind (the scoop the wind digs on the windward side), 0..255. */
		std::vector<uint8_t> Scoop[8];
		std::vector<uint8_t> SlopeDeg;
		void Build(const FCover& C, const FParallel& Par);
		bool Valid() const { return W > 0 && (int)SlopeDeg.size() == W * H; }
	};

	/** What the weather gives the snow now. */
	struct FSnowInputs
	{
		float LevelCm = 0.f;        // undisturbed level depth (FWeatherState::SnowDepthCm)
		float SnowfallCmPerHour = 0.f;
		float Density = 150.f;      // kg/m3
		/** 0 level snow .. 1 this winter's blowing snow has built the drifts fully. */
		float Transport = 0.f;
		/** How the drifting winds were spread over the 8 directions (sums to 1). */
		float Rose[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f };
		/** The deepest snow in the wild (forest, lee slopes) and in the town (people walk there: no more than this). */
		float MaxDepthM = 1.5f;
		float MaxTownM = 0.15f;
		/** The forest keeps more of what falls (no wind takes it, the sun and the thaw reach it less). */
		float ForestGain = 2.2f;
	};

	/** A stretch of path somebody clears at once. */
	struct FPatch
	{
		double X = 0.0, Y = 0.0;              // world cm, the middle
		int MinX = 0, MinY = 0, MaxX = 0, MaxY = 0; // field cells, inclusive
		int Cells = 0;
		bool bAlongX = true;                  // the sweep runs along X (else Y)
		float FreshCm = 0.f;                  // snow on it now (fallen since it was cleared)
		float RemovedCm = 0.f;                // what was shovelled off it this winter (the berms beside it)
		float Progress = 1.f;                 // 0..1 how far the clearing has swept (1 = done)
		float FreshBefore = 0.f;              // the snow being cleared (what the unswept part still has)
		double SnowSinceHours = -1.0;         // when snow started to lie on it (for the auto-clear)
		int Claimed = -1;                     // a person clearing it (-1 nobody)
	};

	struct FSnowPaths
	{
		std::vector<int32_t> Patch;    // per field cell: patch index, -1 not a path
		std::vector<int32_t> Near;     // the nearest path's patch within BermReachM (-1 none)
		std::vector<uint8_t> NearDist; // its distance, 0.1 m units
		std::vector<FPatch> Patches;
		static constexpr float BermReachM = 2.6f;
		static constexpr float PatchM = 12.f;
		bool Valid() const { return !Patch.empty(); }
		/** Cuts the path cells (1 = path) of a field-sized mask into patches and finds the berm cells beside them. */
		void Build(int W, int H, double X0, double Y0, float Cell, const std::vector<uint8_t>& Mask);
		/** The patch at a world point (cm), -1 if none. */
		int PatchAt(double X, double Y, int W, int H, double X0, double Y0, float Cell) const;
	};

	/** The 1 m snow field. */
	class FSnowField
	{
	public:
		int W = 0, H = 0;
		double X0 = 0.0, Y0 = 0.0;
		float Cell = 100.f;
		/** 1 = open sky over the cell at 1 m (finer than the cover's roofs; built from the cover when not given). */
		std::vector<uint8_t> Open;
		/** Snow depth, m. */
		std::vector<float> Depth;
		/** The drift factor at 2 m (cover grid): how much of the level depth lies there once drifted (mean 1). */
		std::vector<float> Factor;

		/** Field grid over the cover (Cell cm), Open from the cover's roofs unless OpenFine (same size) is given. */
		void Setup(const FCover& C, float InCell, const std::vector<uint8_t>* OpenFine = nullptr);
		/** Weighs the basis by the inputs' rose into Factor (mean 1 over the open ground, Transport blended in). */
		void UpdateFactor(const FCover& C, const FDriftBasis& B, const FSnowInputs& In, const FParallel& Par) { ComputeFactor(C, B, In, Par, Factor); }
		/** The same into Out (a worker thread computes it while the field is in use). */
		static void ComputeFactor(const FCover& C, const FDriftBasis& B, const FSnowInputs& In, const FParallel& Par, std::vector<float>& Out);
		/** Writes Depth over the field's rectangle [X0..X1] x [Y0..Y1] (cells, inclusive), paths and berms included. */
		void Compose(const FCover& C, const FSnowInputs& In, const FSnowPaths* P, int RX0, int RY0, int RX1, int RY1, const FParallel& Par);
		void ComposeAll(const FCover& C, const FSnowInputs& In, const FSnowPaths* P, const FParallel& Par) { Compose(C, In, P, 0, 0, W - 1, H - 1, Par); }
		/** The undisturbed (drifted, unshovelled) depth at a cell, m. */
		float DriftedM(const FCover& C, const FSnowInputs& In, int X, int Y) const;
		float DepthAt(double X, double Y) const;
		bool Valid() const { return W > 0 && (int)Depth.size() == W * H; }
		/** The deepest the snow may lie at a world point: the wild's MaxDepthM, the town's MaxTownM, blended by Urban. */
		float CapAt(const FCover& C, const FSnowInputs& In, double WX, double WY) const;
	private:
		float FactorAt(const FCover& C, double WX, double WY) const;
	};

	/** Steps the paths' snow by Hours: fresh snow on them, compaction with the level depth, the auto-clear far from the
	 * camera (DueHours after the snow began to lie), Progress of patches being cleared stays as the caller set it.
	 * Returns the patches that changed (their snow differs by > 0.5 cm). */
	void StepPaths(FSnowPaths& P, const FSnowInputs& In, float PrevLevelCm, double NowHours, float Hours, double CamX, double CamY,
	               float AutoClearHours, float AutoClearNearM, std::vector<int>& Changed);
	/** After a jump of the clock: the paths as people keep them (clear unless it is snowing now, the berms full). */
	void SettlePaths(FSnowPaths& P, const FSnowInputs& In, double NowHours);

	/** The sea ice: distance from the shallows on a 3 m grid over the coast, with the noises that make its edge and floes. */
	struct FIceParams
	{
		float SeaZ = 0.f;          // still water now (with the tide), cm
		float ShallowM = 0.4f;     // water shallower than this is the shore's (no ice keeps there)
		float OpenStripM = 10.f;   // the open water along the shore and the bank (then 4 m of pancakes)
		float StripNoiseM = 3.f;   // how ragged that edge is (+-)
		float Cover = 1.f;         // 0..1 how much of the open sea is frozen (FWeatherState::SeaIceCover)
		float ThicknessCm = 0.f;   // level ice (FWeatherState::SeaIceCm)
		uint32_t Seed = 1947;
		/** The railway's line beyond the grid: world Y (cm) and X range; a lead of OpenStripM is kept open along it. */
		double RailY = 0.0, RailX0 = 0.0, RailX1 = -1.0;
		float RailHalfWidthM = 4.f;
		/** Where the floes drift to (cm), from the winds of the freeze. */
		double DriftX = 0.0, DriftY = 0.0;
	};

	class FIceMask
	{
	public:
		int W = 0, H = 0;
		double X0 = 0.0, Y0 = 0.0;
		float Cell = 300.f;
		std::vector<float> Bed;      // cm, the ground under the water (FCover::NoGround = deep open sea)
		std::vector<float> Dist;     // m from the shallows now
		float BuiltSeaZ = -1.0e9f;

		void Init(int InW, int InH, double InX0, double InY0, float InCell);
		/** Recomputes Dist for the water level now (cheap: an exact distance transform). */
		void Update(float SeaZ, float ShallowM, const FParallel& Par) { ComputeDist(SeaZ, ShallowM, Par, Dist); BuiltSeaZ = SeaZ; }
		/** The same into Out (a worker computes it while Dist is in use). */
		void ComputeDist(float SeaZ, float ShallowM, const FParallel& Par, std::vector<float>& Out) const;
		/** Distance from the shallows at a world point (m), large off the grid (except near the rail line, see IceAt). */
		float DistAt(double X, double Y) const;
		/** 0..1 how frozen the sea is at a world point: the same formula the ice material draws (winter_hlsl.py). */
		float IceAt(double X, double Y, const FIceParams& P) const;
		/** Can somebody stand on the ice there (frozen, thick enough)? */
		bool Walkable(double X, double Y, const FIceParams& P, float MinCm = 8.f) const;
		bool Valid() const { return W > 0 && (int)Dist.size() == W * H; }
	};

	/** Exact squared Euclidean distance transform (Felzenszwalb) of a W x H grid: In 0 = seed, else +inf. In place. */
	void DistanceTransform(std::vector<float>& G, int W, int H, const FParallel& Par);

	// the shared noises (winter_hlsl.py has the same)
	float Hash01(int32_t X, int32_t Y, uint32_t Seed);
	/** Value noise, smooth, ~1 per cell of Scale m: 0..1. */
	float ValueNoise(double XM, double YM, double ScaleM, uint32_t Seed);
	/** Three octaves of ValueNoise, 0..1. */
	float Fbm(double XM, double YM, double ScaleM, uint32_t Seed);

	/** The world's wind bearing (compass, from) as a unit vector pointing upwind (toward where it comes from), world X/Y. */
	void UpwindDir(float FromDeg, double& OutX, double& OutY);
	/** Adds a wind's snow-carrying power to an 8-way rose (bearing from, speed m/s); returns the power added. */
	float AddToRose(float Rose[8], float FromDeg, float SpeedMS, float Weight);
}
