// Topi: the dune land south-west of the town, in plain C++ (no Unreal types) so tools/topi/run.sh can check and
// draw it outside the engine. Metres, the map's frame (docs/world_map.md): X west (the sea), Y north, Z up over the
// club's floor. The land is endless: every function works for any x, y.
//
// What is here: the shape of the sand (Height: a great dune that has buried the end of the upland and the cliff where
// Topi meets the map's landscape, then dunes of five sizes under one wind, a beach, the bed of the river Ural), the
// river's line, the one path to it, and what the place does to the hero's mind (Pressure). docs/topi.md.
#pragma once

#include <cstdint>
#include <vector>

namespace LyudTopi
{
	struct FVec2
	{
		double X = 0.0, Y = 0.0;
	};

	/** The map's own ground (tools/world_map.py W.height) in the strip where Topi's sand lies over the landscape:
	 *  written by tools/topi.py into Content/Topi/topi_edge.tbl. */
	struct FEdgeTable
	{
		double X0 = 0.0, Y0 = 0.0, Cell = 6.0;
		int NX = 0, NY = 0;
		std::vector<float> Z;

		bool Valid() const { return NX > 1 && NY > 1 && Z.size() == size_t(NX) * size_t(NY); }
		/** Bilinear, clamped to the table's edge. */
		double At(double X, double Y) const;
		/** Reads the file's bytes (magic "TOPE", int32 version, NX, NY, float X0, Y0, Cell, then NX * NY floats by rows of Y). */
		bool Read(const uint8_t* Data, size_t Size);
	};

	/** The sea's datum (world_map.SEA_Z). */
	constexpr double SeaZ = -43.0;
	/** No sand north of this Y at all; south of FullY the landscape is over and all ground is Topi's. */
	constexpr double NorthY = -730.0;
	constexpr double FullY = -900.0;
	/** The river's water level, and how wide the water is (m). */
	constexpr double RiverZ = -36.0;
	constexpr double RiverHalfWidth = 20.0;
	constexpr double RiverDeep = 2.6;

	struct FSample
	{
		/** Ground height, m. */
		double Z = 0.0;
		/** 0..1: how much of Topi is here (0: only the hidden skirt under the map's own ground). */
		double Region = 0.0;
		/** Marram grass density 0..1 (the fore dunes and Topi's edge; none in the deep). */
		double Grass = 0.0;
		/** 0..1: the river's bed and wet banks. */
		double River = 0.0;
		/** Metres into Topi (south of the line where it starts). */
		double Depth = 0.0;
	};

	class FModel
	{
	public:
		FEdgeTable Edge;
		/** Call after filling Edge. */
		void Prepare();

		/** Water line X at Y (follows the map's coast in the north, swings west in the south). */
		static double ShoreX(double Y);
		/** 0 north of the sand's edge .. 1 where Topi is whole. */
		static double Region(double X, double Y);
		/** Metres into Topi. */
		static double Depth(double X, double Y);

		/** Ground height. Cell: the spacing it will be drawn at, m (dunes too small to show at it are left out as their
		 *  mean, so far ground costs little and does not alias); 0 = everything. */
		double Height(double X, double Y, double Cell = 0.0) const;
		void Sample(double X, double Y, double Cell, FSample& Out) const;

		/** The river Ural: it runs west toward the sea along Y = RiverY(X) and sinks into the sand before the coast. */
		static double RiverY(double X);
		/** Signed metres from the river's middle line (positive: south of it). */
		static double RiverOffset(double X, double Y);
		/** 0..1: how much river there is at X (0 near the coast, where it has sunk into the sand). */
		static double RiverStrength(double X);
		/** Water depth at a point at night, m (0 outside the water). */
		double RiverDepth(double X, double Y) const;
		/** Which way and how fast the water runs at a point, m/s (zero outside the water). */
		static FVec2 RiverFlow(double X, double Y);

		/** The one way through the dunes, from the beach to the river's bank. */
		static const std::vector<FVec2>& Path();
		/** Metres from the path; Along: metres walked along it to the nearest point. */
		static double PathDistance(double X, double Y, double* Along = nullptr);
		static double PathLength();
		/** The point Along metres down the path. */
		static FVec2 PathPoint(double Along);

		/** How hard the place presses on the hero's mind here, 0..1 (1: he cannot bear it and turns back). By day it
		 *  grows with every step into Topi; at night only away from the path, and never by the river where the path ends. */
		static double Pressure(double X, double Y, bool bNight);
		/** Where to walk to get out from here (unit vector): back along the path if near it, else straight north. */
		static FVec2 WayOut(double X, double Y);

	private:
		/** The great dune's crest over the map's ground at X (Prepare). */
		double RidgeTop(double X) const;
		std::vector<float> RidgeTops;
	};
}
