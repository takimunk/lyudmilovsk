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
	// Snow, Scoria and Sulfur are the North's (docs/north.md): firn of the summer snowfields, loose volcanic cinder,
	// ground bleached and crusted by the fumaroles' steam. They have no photos of their own: the landscape material
	// draws them from the Sand, Dirt and Mud photos (import_terrain.py ALIAS).
	enum ELayer : int { Grass = 0, Meadow, Dirt, Mud, Sand, Rock, Forest, Snow, Scoria, Sulfur, NumLayers };
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
		std::vector<uint8_t> SeaKeep;            // 255 = plain sand bed here, no reef or stones (the railway's bank, the hatch); may be empty
		// the North (tools/north.py); all four empty in inputs written before it
		std::vector<uint8_t> Snow;               // 255 = a summer snowfield lies here
		std::vector<uint8_t> Scoria;             // 255 = loose cinder (the cone and its ash apron)
		std::vector<uint8_t> Lava;               // 255 = on the blocky lava flow
		std::vector<uint8_t> Sulfur;             // 255 = ground altered by the fumaroles' steam
		std::vector<uint8_t> Gravel;             // the river's gravel plain: ~140 = dry bars, 255 = the channels' beds
		std::vector<std::vector<std::pair<float, float>>> Tracks;   // dirt tracks, map metres
	};

	struct FOutputs
	{
		std::vector<float> Height;               // metres
		std::vector<uint8_t> Weights[NumLayers]; // 0..255, summing to 255 per vertex
		std::vector<uint8_t> GrassDensity;       // meadow grass, 0..255
		std::vector<uint8_t> DuneGrass;          // marram / lyme grass on the sand, 0..255
		std::vector<uint8_t> Wetness;            // where water runs and stands, 0..255
		std::vector<uint8_t> ForestFloor;        // forest floor the ground cover grows on (no track, rock, sand), 0..255
		std::vector<uint8_t> SeaStone;           // on the sea bed: stone cover (moraine reefs, boulder and cobble fields), 0..255
		std::string Report;
	};

	/** Reads terrain_in.json/.bin (tools/terrain.py). Empty string on success, else the error. */
	std::string ReadInputs(const std::string& Dir, FInputs& Out);
	void Generate(const FInputs& In, FOutputs& Out);

	/**
	 * The sea bed's natural detail at a map point (metres) whose macro bed lies Depth m under the datum: wave ripples
	 * and megaripples on the sand, sand waves out deep, moraine reefs (ridges of boulders 1-3 m high, rough with stones)
	 * past the drop-off, boulder and cobble fields; Keep 0..1 keeps the bed plain. Returns the height to add (m);
	 * OutStone: stone cover 0..1, OutMud: silt in the deep hollows 0..1. Deterministic in (Seed, X, Y).
	 */
	float SeabedDetail(uint32_t Seed, double X, double Y, float Depth, float Keep, float& OutStone, float& OutMud);
	/** Writes terrain_out.bin: the final heights as float32 on the same grid (tools/terrain.py ground_z). */
	bool WriteHeights(const std::string& Dir, const FOutputs& Out);
}
