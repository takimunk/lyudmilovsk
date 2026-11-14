// Topi: the dune land east of the town, in plain C++ (no Unreal types) so tools/topi/run.sh can check and
// draw it outside the engine. Metres, the map's frame (docs/world_map.md): X west (the sea), Y north, Z up over the
// club's floor. The land is endless: every function works for any x, y.
//
// What is here: the shape of the sand (Height: a great dune that has climbed the map's inland ridge from behind where
// Topi meets the map's landscape, then dunes of five sizes under one wind, the bed of the river Ural), the
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

	/** The map's east edge (world_map.X_MIN): west of it the ground is the map's landscape, east of it all Topi's. */
	constexpr double WestX = -1300.0;
	/** The level the dunes stand on at Topi's edge, m (the map's inland ridge stands ~30 m over it). */
	constexpr double BaseZ = -30.0;
	/** The river's water level, and how wide the water is (m). */
	constexpr double RiverZ = BaseZ - 1.0;
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
		/** Metres into Topi (east of the line where it starts). */
		double Depth = 0.0;
	};

	class FModel
	{
	public:
		FEdgeTable Edge;
		/** Call after filling Edge. */
		void Prepare();

		/** 0 west of the sand's edge .. 1 where Topi is whole. */
		static double Region(double X, double Y);
		/** Metres into Topi (east of the line where it starts). */
		static double Depth(double X, double Y);
		/** Is the point in Topi or within Margin metres of its sand? (Cheap: for early outs.) */
		static bool Near(double X, double Y, double Margin);
		/** Topi's own frame (LX along the edge, LY across it; TopiModel.cpp) <-> the map's. */
		static FVec2 ToLocal(double X, double Y);
		static FVec2 FromLocal(double LX, double LY);

		/** Ground height. Cell: the spacing it will be drawn at, m (dunes too small to show at it are left out as their
		 *  mean, so far ground costs little and does not alias); 0 = everything. */
		double Height(double X, double Y, double Cell = 0.0) const;
		void Sample(double X, double Y, double Cell, FSample& Out) const;

		/** The river Ural runs across Topi ~940 m in, parallel to its edge, flowing south, endless both ways.
		 *  RiverPoint: its middle line S metres downstream of where the path comes out; RiverDirection: the stream's
		 *  unit direction there. */
		static FVec2 RiverPoint(double S);
		static FVec2 RiverDirection(double S);
		/** Signed metres from the river's middle line (positive: on the far side, deeper into Topi). */
		static double RiverOffset(double X, double Y);
		/** Water depth at a point at night, m (0 outside the water). */
		double RiverDepth(double X, double Y) const;
		/** Which way and how fast the water runs at a point, m/s (zero outside the water). */
		static FVec2 RiverFlow(double X, double Y);

		/** The one way through the dunes, from the sand's edge east of the town to the river's bank. */
		static const std::vector<FVec2>& Path();
		/** Metres from the path; Along: metres walked along it to the nearest point. */
		static double PathDistance(double X, double Y, double* Along = nullptr);
		static double PathLength();
		/** The point Along metres down the path. */
		static FVec2 PathPoint(double Along);

		/** How hard the place presses on the hero's mind here, 0..1 (1: he cannot bear it and turns back). By day it
		 *  grows with every step into Topi; at night only away from the path, and never by the river where the path ends. */
		static double Pressure(double X, double Y, bool bNight);
		/** Where to walk to get out from here (unit vector): back along the path if near it, else straight west. */
		static FVec2 WayOut(double X, double Y);

	private:
		/** The great dune's crest over the map's ground at map Y (Prepare). */
		double RidgeTop(double Y) const;
		std::vector<float> RidgeTops;
	};
}
