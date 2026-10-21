// The outdoor ground's natural detail, in plain C++ (no Unreal types) so it can also be built and
// looked at outside the engine. Input: the map's macro shape sampled on a 1 m grid by
// tools/terrain.py. Output: the final heights, surface layer weights and the grass masks.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace LyudTerrain
{
	enum EKind : uint8_t { Land = 0, Beach = 1, Seabed = 2 };

	/** Surface layers, in the landscape material's order. */
	enum ELayer : int { Grass = 0, Meadow, Dirt, Mud, Sand, Rock, Forest, NumLayers };
	extern const char* const LayerNames[NumLayers];

	struct FInputs
	{
		int NX = 0, NY = 0;
		double X0 = 0, Y0 = 0;                   // map metres of vertex (0, 0); one vertex per metre
		uint32_t Seed = 1;
		float SeaZ = -43.f;
		std::vector<float> Height;               // macro shape, metres
		std::vector<uint8_t> Protect;            // 255 = keep the macro height exactly (paths, the club)
		std::vector<uint8_t> Kind;               // EKind
		std::vector<uint8_t> Forest;             // 255 = forest floor
		std::vector<uint8_t> Hole;               // 255 = quad cut out of the landscape (the portal)
		std::vector<uint8_t> Cliff;              // 255 = on the face of the coastal bluff (the map's own hint; slope adds to it)
		std::vector<uint8_t> RaiseOnly;          // 255 = the detail may raise the ground here, never lower it (over the corridor)
		std::vector<std::vector<std::pair<float, float>>> Tracks;   // dirt tracks, map metres
	};

	struct FOutputs
	{
		std::vector<float> Height;               // metres
		std::vector<uint8_t> Weights[NumLayers]; // 0..255, summing to 255 per vertex
		std::vector<uint8_t> GrassDensity;       // meadow grass, 0..255
		std::vector<uint8_t> DuneGrass;          // marram / lyme grass on the sand, 0..255
		std::vector<uint8_t> Wetness;            // where water runs and stands, 0..255
		std::string Report;
	};

	/** Reads terrain_in.json/.bin (tools/terrain.py). Empty string on success, else the error. */
	std::string ReadInputs(const std::string& Dir, FInputs& Out);
	void Generate(const FInputs& In, FOutputs& Out);
	/** Writes terrain_out.bin: the final heights as float32 on the same grid (tools/terrain.py ground_z). */
	bool WriteHeights(const std::string& Dir, const FOutputs& Out);
}
