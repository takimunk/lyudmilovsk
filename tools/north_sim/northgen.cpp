// Runs the terrain generator outside the engine on terrain_in.* and draws the North (docs/north.md) for a quick look:
//   northgen <dir with terrain_in.*> <out dir> [y0 metres, default 2000]
//     -> <out>/north_ground.ppm (the surface layers in their colours, shaded by the built relief, north up, west left),
//        <out>/terrain_out.bin, the generator's report and the North's own numbers (slopes, heights, layer cover).
#include "TerrainGen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

int main(int Argc, char** Argv)
{
	if (Argc < 3) { std::fprintf(stderr, "northgen <in dir> <out dir> [y0]\n"); return 2; }
	LyudTerrain::FInputs In;
	const std::string Err = LyudTerrain::ReadInputs(Argv[1], In);
	if (!Err.empty()) { std::fprintf(stderr, "%s\n", Err.c_str()); return 1; }
	LyudTerrain::FOutputs Out;
	LyudTerrain::Generate(In, Out);
	std::printf("%s\n", Out.Report.c_str());
	const std::string Dir = Argv[2];
	LyudTerrain::WriteHeights(Dir, Out);
	const int NX = In.NX, NY = In.NY;
	const int Y0 = std::clamp(int((Argc > 3 ? std::atof(Argv[3]) : 2000.0) - In.Y0), 0, NY - 2);
	// Grass, Meadow, Dirt, Mud, Sand, Rock, Forest, Snow, Scoria, Sulfur
	static const float Col[LyudTerrain::NumLayers][3] = {{0.33f, 0.46f, 0.2f}, {0.5f, 0.5f, 0.27f}, {0.4f, 0.32f, 0.25f}, {0.3f, 0.27f, 0.24f},
		{0.78f, 0.72f, 0.58f}, {0.42f, 0.4f, 0.4f}, {0.24f, 0.3f, 0.17f}, {0.95f, 0.97f, 1.f}, {0.2f, 0.13f, 0.12f}, {0.82f, 0.76f, 0.5f}};
	FILE* F = std::fopen((Dir + "/north_ground.ppm").c_str(), "wb");
	std::fprintf(F, "P6 %d %d 255\n", NX - 2, NY - 1 - Y0);
	double Cover[LyudTerrain::NumLayers] = {};
	float Hi = -1e9f, Steepest = 0.f;
	long Land = 0, Over35 = 0;
	for (int Y = NY - 2; Y >= Y0; --Y)
		for (int X = NX - 2; X >= 1; --X)                       // map X grows west: west on the left
		{
			const size_t I = size_t(Y) * NX + X;
			const float GX = (Out.Height[I + 1] - Out.Height[I - 1]) * 0.5f, GY = (Out.Height[I + NX] - Out.Height[I - (Y > 0 ? NX : 0)]) * 0.5f;
			float C[3] = {0.25f, 0.4f, 0.55f};
			if (In.Kind[I] != LyudTerrain::Seabed)
			{
				C[0] = C[1] = C[2] = 0.f;
				for (int L = 0; L < LyudTerrain::NumLayers; ++L)
					for (int K = 0; K < 3; ++K) C[K] += Col[L][K] * Out.Weights[L][I] / 255.f;
				const float Shade = std::clamp(0.9f + 1.3f * (GX * 0.7f - GY * 0.7f), 0.3f, 1.45f);
				for (float& V : C) V *= Shade;
				if (In.Kind[I] == LyudTerrain::Land && Y - Y0 > 200)
				{
					++Land;
					for (int L = 0; L < LyudTerrain::NumLayers; ++L) Cover[L] += Out.Weights[L][I] / 255.0;
					Hi = std::max(Hi, Out.Height[I]);
					const float Slope = std::atan(std::sqrt(GX * GX + GY * GY)) * 57.2958f;
					Steepest = std::max(Steepest, Slope);
					Over35 += Slope > 35.f;
				}
			}
			for (float V : C) std::fputc(int(std::clamp(V, 0.f, 1.f) * 255.f), F);
		}
	std::fclose(F);
	std::printf("north (from y0 + 200): %ld m2 of land, highest %.1f m, steepest %.0f deg, %.2f%% over 35 deg; cover:", Land, Hi, Steepest,
	            100.0 * Over35 / std::max(Land, 1L));
	for (int L = 0; L < LyudTerrain::NumLayers; ++L) std::printf(" %s %.1f%%", LyudTerrain::LayerNames[L], 100.0 * Cover[L] / std::max(Land, 1L));
	std::printf("\n");
	return 0;
}
