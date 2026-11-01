// Runs the terrain generator outside the engine on terrain_in.* and writes the sea bed's look for a quick check:
//   bedgen <dir with terrain_in.*> <out dir>  ->  <out>/bed_depth.pgm (shaded relief of the bed, the land white),
//   <out>/bed_stone.pgm (stone cover), <out>/terrain_out.bin, and a line per depth band.
#include "TerrainGen.h"

#include <cmath>
#include <cstdio>
#include <string>

int main(int Argc, char** Argv)
{
	if (Argc < 3) { std::fprintf(stderr, "bedgen <in dir> <out dir>\n"); return 2; }
	LyudTerrain::FInputs In;
	const std::string Err = LyudTerrain::ReadInputs(Argv[1], In);
	if (!Err.empty()) { std::fprintf(stderr, "%s\n", Err.c_str()); return 1; }
	LyudTerrain::FOutputs Out;
	LyudTerrain::Generate(In, Out);
	std::printf("%s\n", Out.Report.c_str());
	const std::string Dir = Argv[2];
	LyudTerrain::WriteHeights(Dir, Out);
	const int NX = In.NX, NY = In.NY;
	// the sea part of the map only: X from the beach out (map X grows to the sea)
	int XA = NX, XB = 0;
	for (int Y = 0; Y < NY; ++Y)
		for (int X = 0; X < NX; ++X)
			if (In.Kind[size_t(Y) * NX + X] == LyudTerrain::Seabed) { XA = std::min(XA, X); XB = std::max(XB, X); }
	XA = std::max(XA - 30, 0);
	const int W = XB - XA + 1, H = NY;
	FILE* D = std::fopen((Dir + "/bed_depth.pgm").c_str(), "wb");
	FILE* S = std::fopen((Dir + "/bed_stone.pgm").c_str(), "wb");
	std::fprintf(D, "P5 %d %d 255\n", W, H);
	std::fprintf(S, "P5 %d %d 255\n", W, H);
	long Band[20] = {};
	for (int Y = NY - 1; Y >= 0; --Y)
		for (int X = XA; X <= XB; ++X)
		{
			const size_t I = size_t(Y) * NX + X;
			const float Z = Out.Height[I], Depth = In.SeaZ - Z;
			unsigned char V = 255, St = 0;
			if (In.Kind[I] == LyudTerrain::Seabed)
			{
				const float GX = Out.Height[I + (X + 1 < NX ? 1 : 0)] - Out.Height[I - (X > 0 ? 1 : 0)];
				const float GY = Out.Height[I + (Y + 1 < NY ? NX : 0)] - Out.Height[I - (Y > 0 ? NX : 0)];
				const float Shade = 0.5f + 1.6f * (GX * 0.6f - GY * 0.8f);
				const float Base = 1.f - std::min(Depth / 16.f, 1.f);
				V = (unsigned char)std::lround(std::max(0.f, std::min(1.f, 0.15f + 0.55f * Base + 0.3f * (Shade - 0.5f) * 2.f)) * 254.f);
				St = Out.SeaStone[I];
				Band[std::min(19, std::max(0, int(Depth)))]++;
			}
			std::fputc(V, D);
			std::fputc(St, S);
		}
	std::fclose(D);
	std::fclose(S);
	for (int B = 0; B < 20; ++B) if (Band[B]) std::printf("depth %2d-%2d m: %ld m2\n", B, B + 1, Band[B]);
	return 0;
}
