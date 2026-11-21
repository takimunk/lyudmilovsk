// The signature behaviours of each species: the table, when a habit fits, its path, and the hooks (BirdHabit.h,
// docs/bird_habits.md).
#include "BirdHabit.h"

#include "BirdCorvid.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace LyudBirds
{
	// (named, and always written out: Unreal's unity builds put this file together with BirdSim.cpp, whose helpers of the
	// same names sit in an anonymous namespace)
	namespace HabitDetail
	{
		constexpr float Pi = 3.14159265f;
		float Clamp(float V, float A, float B) { return V < A ? A : (V > B ? B : V); }
		float Smooth(float T) { T = Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
		float Lerp(float A, float B, float T) { return A + (B - A) * T; }
		V3 Lerp3(const V3& A, const V3& B, float T) { return A + (B - A) * T; }
		float Dice(uint32_t& S)
		{
			S ^= S << 13;
			S ^= S >> 17;
			S ^= S << 5;
			return (S & 0xFFFFFF) / 16777216.f;
		}
		constexpr uint32_t A(EAct X) { return 1u << (int)X; }
		constexpr uint32_t H(EHabitat X) { return 1u << (int)X; }

		const uint32_t Sit = A(EAct::Perch) | A(EAct::Preen);
		const uint32_t Feed = A(EAct::Peck) | A(EAct::Forage);
		const uint32_t Stand = Sit | Feed;
		const uint32_t Afloat = A(EAct::Swim);
		const uint32_t Hawking = A(EAct::Hawk);
		const uint32_t Soaring = A(EAct::Soar);

		const uint32_t Twigs = H(EHabitat::Garden) | H(EHabitat::Broadleaf) | H(EHabitat::Conifer);
		const uint32_t Heights = H(EHabitat::Roof) | H(EHabitat::Tower) | H(EHabitat::Wire);
		const uint32_t Lawn = H(EHabitat::Meadow) | H(EHabitat::Garden);
		const uint32_t Shore = H(EHabitat::Beach) | H(EHabitat::Reed);
		const uint32_t Town = H(EHabitat::Street) | H(EHabitat::Garden) | H(EHabitat::Meadow);

		struct FBuild
		{
			FHabitDef D;
			FBuild(EHabit Id, const char* Name, const char* Species, EBirdFamily Family, EHabitMech Mech)
			{
				D.Id = Id;
				D.Name = Name;
				D.Species = Species;
				D.Family = Family;
				D.Mech = Mech;
			}
			FBuild& At(uint32_t Acts, uint32_t Habitats = 0) { D.Acts = Acts; D.Habitats = Habitats; return *this; }
			FBuild& Need(uint32_t N) { D.Needs |= N; return *this; }
			FBuild& Hours(EHabitHours X) { D.Hours = X; return *this; }
			FBuild& Days(float From, float To) { D.DoyFrom = From; D.DoyTo = To; return *this; }
			FBuild& Wind(float Min, float Max) { D.WindMin = Min; D.WindMax = Max; return *this; }
			FBuild& Len(float Min, float Max, int Beats = 0) { D.LenMin = Min; D.LenMax = Max; D.Beats = Beats; return *this; }
			FBuild& Often(float Rate, int PerDay, float Gap) { D.Rate = Rate; D.PerDay = PerDay; D.Gap = Gap; return *this; }
			FBuild& Room(float M) { D.Room = M; return *this; }
			FBuild& Reach(float M, float Speed = 0.f) { D.Reach = M; D.Speed = Speed; return *this; }
			FBuild& Base(EAct X) { D.Base = X; return *this; }
			FBuild& Clip(const char* X) { D.Clip = X; return *this; }
			FBuild& From(EHabitFrom F, int Kind, int Arg = -1) { D.From = F; D.FromKind = Kind; D.FromArg = Arg; return *this; }
			operator FHabitDef() const { return D; }
		};

		/** Item filter of a mapped corvid task: any shiny thing. */
		constexpr int AnyShiny = -2;

		std::vector<FHabitDef> Build()
		{
			using F = EBirdFamily;
			using M = EHabitMech;
			using E = EHabit;
			const EAct Keep = EAct::Num;
			std::vector<FHabitDef> T;
			T.push_back(FHabitDef());       // None
			// ---- the crow family ----
			T.push_back(FBuild(E::RookParliament, "rook_parliament", "Rook", F::Crows, M::Still).At(Stand, H(EHabitat::Meadow))
				.Need(NeedGathering).Hours(EHabitHours::Evening).Len(4.f, 7.f, 2).Often(2.f, 6, 300.f).Room(0.6f).Clip("Call"));
			T.push_back(FBuild(E::RookProbe, "rook_probe", "Rook", F::Crows, M::Still).At(Feed, Lawn).Need(NeedFeeding | NeedGround)
				.Len(3.f, 5.f, 3).Often(1.5f, 6, 420.f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::CrowNutDrop, "crow_nut_drop", "HoodedCrow", F::Crows, M::Mapped).From(EHabitFrom::Corvid, (int)ETask::Crack).Clip(""));
			T.push_back(FBuild(E::CrowRoofSlide, "crow_roof_slide", "HoodedCrow", F::Crows, M::Still).At(Sit, H(EHabitat::Roof)).Need(NeedFrost)
				.Days(320.f, 75.f).Len(3.f, 4.5f, 1).Often(1.f, 3, 900.f).Room(2.f).Clip("Perch"));
			T.push_back(FBuild(E::JackdawAllopreen, "jackdaw_allopreen", "Jackdaw", F::Crows, M::Mapped).From(EHabitFrom::Corvid, (int)ETask::Preen).Clip("Preen"));
			T.push_back(FBuild(E::JackdawChimney, "jackdaw_chimney", "Jackdaw", F::Crows, M::Still).At(Sit, H(EHabitat::Roof) | H(EHabitat::Tower))
				.Len(4.f, 7.f, 1).Often(1.f, 4, 600.f).Clip("Look"));
			T.push_back(FBuild(E::MagpieHideShiny, "magpie_hide_shiny", "Magpie", F::Crows, M::Mapped).From(EHabitFrom::Corvid, (int)ETask::Cache, AnyShiny).Clip("Peck"));
			T.push_back(FBuild(E::MagpieTailPull, "magpie_tail_pull", "Magpie", F::Crows, M::Mapped).From(EHabitFrom::Corvid, (int)ETask::Tease).Clip("Peck"));
			T.push_back(FBuild(E::RavenTumble, "raven_tumble", "Raven", F::Crows, M::Mapped).From(EHabitFrom::Corvid, (int)ETask::Play).Clip(""));
			T.push_back(FBuild(E::RavenBow, "raven_bow", "Raven", F::Crows, M::Still)
				.At(Sit, H(EHabitat::Cliff) | H(EHabitat::Tower) | H(EHabitat::Roof) | H(EHabitat::Conifer) | H(EHabitat::Broadleaf))
				.Len(3.f, 5.f, 2).Often(1.f, 4, 600.f).Clip("Call"));
			T.push_back(FBuild(E::JayAcornCache, "jay_acorn_cache", "Jay", F::Crows, M::Mapped).From(EHabitFrom::Corvid, (int)ETask::Cache, (int)EItem::Acorn).Clip("Peck"));
			T.push_back(FBuild(E::JayAnting, "jay_anting", "Jay", F::Crows, M::Still).At(Stand, H(EHabitat::ForestFloor) | H(EHabitat::Meadow))
				.Need(NeedDry | NeedSun | NeedGround).Days(135.f, 260.f).Len(8.f, 15.f, 0).Often(1.f, 3, 900.f).Room(0.8f).Base(EAct::Preen).Clip("Preen"));
			T.push_back(FBuild(E::NutcrackerAnvil, "nutcracker_anvil", "Nutcracker", F::Crows, M::Still)
				.At(Stand, H(EHabitat::Conifer) | H(EHabitat::ForestFloor) | H(EHabitat::Broadleaf))
				.Len(5.f, 9.f, 5).Often(1.5f, 5, 420.f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::NutcrackerStore, "nutcracker_store", "Nutcracker", F::Crows, M::Mapped).From(EHabitFrom::Corvid, (int)ETask::Cache).Clip("Peck"));
			T.push_back(FBuild(E::CarrionDunk, "carrion_dunk", "CarrionCrow", F::Crows, M::Still).At(Stand, Shore).Need(NeedGround)
				.Len(4.f, 6.f, 2).Often(1.f, 3, 900.f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::CarrionStrut, "carrion_strut", "CarrionCrow", F::Crows, M::Run)
				.At(Stand, H(EHabitat::Meadow) | H(EHabitat::Beach) | H(EHabitat::Street) | H(EHabitat::Garden)).Need(NeedGround)
				.Len(3.f, 5.f, 2).Often(1.f, 4, 600.f).Reach(2.f, 1.3f).Clip("Hop"));
			// ---- pigeons and the birds of the air ----
			T.push_back(FBuild(E::PigeonBowCoo, "pigeon_bow_coo", "FeralPigeon", F::Wings, M::Pair).At(Stand, Town | H(EHabitat::Roof))
				.Days(45.f, 300.f).Len(6.f, 10.f, 4).Often(2.f, 6, 300.f).Room(0.6f).Clip("Peck"));
			T.push_back(FBuild(E::PigeonSunbathe, "pigeon_sunbathe", "FeralPigeon", F::Wings, M::Still).At(Sit, H(EHabitat::Roof) | H(EHabitat::Street))
				.Need(NeedSun | NeedDry | NeedResting).Hours(EHabitHours::Midday).Len(10.f, 20.f, 0).Often(1.f, 3, 900.f).Room(0.5f).Base(EAct::Preen).Clip("Preen"));
			T.push_back(FBuild(E::WoodPigeonDisplay, "wood_pigeon_display", "WoodPigeon", F::Wings, M::Sally).At(Sit, Twigs | Heights)
				.Days(60.f, 270.f).Wind(0.f, 9.f).Len(6.f, 8.f, 1).Often(1.f, 4, 600.f).Reach(22.f).Clip(""));
			T.push_back(FBuild(E::WoodPigeonDrink, "wood_pigeon_drink", "WoodPigeon", F::Wings, M::Still)
				.At(Stand, Town | H(EHabitat::ForestFloor) | H(EHabitat::Beach)).Need(NeedGround | NeedWet)
				.Len(4.f, 6.f, 0).Often(1.5f, 4, 600.f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::SwiftScreamingParty, "swift_screaming_party", "CommonSwift", F::Wings, M::Swoop).At(Hawking)
				.Hours(EHabitHours::Evening).Days(130.f, 232.f).Wind(0.f, 10.f).Len(4.f, 8.f, 1).Often(1.5f, 6, 300.f).Reach(90.f).Clip(""));
			T.push_back(FBuild(E::SwallowDrink, "swallow_drink", "BarnSwallow", F::Wings, M::Swoop).At(Hawking).Need(NeedWater)
				.Wind(0.f, 9.f).Len(3.f, 8.f, 1).Often(1.5f, 6, 300.f).Reach(120.f).Clip(""));
			T.push_back(FBuild(E::SwallowWirePreen, "swallow_wire_stretch", "BarnSwallow", F::Wings, M::Still).At(Sit, H(EHabitat::Wire))
				.Len(3.f, 5.f, 0).Often(1.f, 4, 600.f).Base(EAct::Preen).Clip("Preen"));
			T.push_back(FBuild(E::MartinBankFlutter, "martin_bank_flutter", "SandMartin", F::Wings, M::Swoop).At(Hawking)
				.Days(120.f, 240.f).Wind(0.f, 11.f).Len(4.f, 9.f, 1).Often(1.5f, 6, 300.f).Reach(120.f).Clip(""));
			// ---- gulls and the water's birds ----
			T.push_back(FBuild(E::GullSnatch, "gull_snatch", "HerringGull", F::Water, M::Sally).At(Stand).Need(NeedMorsel)
				.Wind(0.f, 14.f).Len(4.f, 8.f, 1).Often(4.f, 4, 900.f).Reach(45.f).Clip(""));
			T.push_back(FBuild(E::GullLongCall, "gull_long_call", "HerringGull", F::Water, M::Mapped).From(EHabitFrom::Voice, (int)EVoice::LongCall).Len(3.5f, 3.5f, 0).Clip("Call"));
			T.push_back(FBuild(E::BlackHeadedSquabble, "black_headed_squabble", "BlackHeadedGull", F::Water, M::Pair)
				.At(Stand, H(EHabitat::Beach) | H(EHabitat::Meadow) | H(EHabitat::Street)).Need(NeedGround)
				.Len(3.f, 5.f, 2).Often(1.5f, 5, 420.f).Room(0.5f).Clip("Call"));
			T.push_back(FBuild(E::CommonGullPaddle, "common_gull_paddle", "CommonGull", F::Water, M::Still).At(Feed, H(EHabitat::Beach) | H(EHabitat::Meadow))
				.Need(NeedFeeding | NeedGround).Len(5.f, 8.f, 6).Often(1.5f, 5, 420.f).Base(EAct::Peck).Clip("Walk"));
			T.push_back(FBuild(E::CormorantWings, "cormorant_wings", "Cormorant", F::Water, M::Mapped).From(EHabitFrom::Act, (int)EAct::Preen).Clip("WingsOut"));
			T.push_back(FBuild(E::CormorantDive, "cormorant_dive", "Cormorant", F::Water, M::Mapped).From(EHabitFrom::Act, (int)EAct::Peck).Len(1.2f, 1.2f, 1).Clip("Peck"));
			T.push_back(FBuild(E::SwanBusk, "swan_busk", "MuteSwan", F::Water, M::Still).At(Stand | Afloat).Need(NeedPlayerNear)
				.Len(6.f, 10.f, 1).Often(3.f, 4, 600.f).Base(Keep).Clip("WingsOut"));
			T.push_back(FBuild(E::SwanUpend, "swan_upend", "MuteSwan", F::Water, M::Mapped).From(EHabitFrom::Act, (int)EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::HeronStalk, "heron_stalk", "GreyHeron", F::Water, M::Still).At(Stand, Shore | H(EHabitat::Sea) | H(EHabitat::Meadow))
				.Need(NeedFeeding).Len(8.f, 14.f, 1).Often(1.5f, 6, 300.f).Room(1.f).Base(EAct::Peck).Clip("Look"));
			T.push_back(FBuild(E::HeronSunDelta, "heron_sun_delta", "GreyHeron", F::Water, M::Still).At(Stand).Need(NeedSun | NeedResting)
				.Hours(EHabitHours::Morning).Len(15.f, 30.f, 0).Often(1.f, 2, 1200.f).Room(1.2f).Base(EAct::Preen).Clip("WingsOut"));
			T.push_back(FBuild(E::AlbatrossSeaRun, "albatross_sea_run", "BlackBrowedAlbatross", F::Water, M::Mapped).From(EHabitFrom::Act, (int)EAct::Takeoff).Len(2.5f, 2.5f, 3).Clip(""));
			T.push_back(FBuild(E::AlbatrossShear, "albatross_shear", "BlackBrowedAlbatross", F::Water, M::Mapped).From(EHabitFrom::Act, (int)EAct::Soar).Wind(9.f, 99.f).Len(8.f, 8.f, 0).Clip(""));
			// ---- the garden's small birds ----
			T.push_back(FBuild(E::TitHammer, "tit_hammer", "GreatTit", F::Garden, M::Still).At(Sit | A(EAct::Peck), Twigs).Need(NeedFeeding)
				.Len(3.f, 6.f, 6).Often(2.f, 6, 300.f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::TitHang, "tit_hang", "GreatTit", F::Garden, M::Still).At(Sit | A(EAct::Peck), Twigs).Need(NeedFeeding)
				.Len(3.f, 6.f, 2).Often(2.f, 8, 240.f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::BlueTitAcrobat, "blue_tit_acrobat", "BlueTit", F::Garden, M::Still).At(Sit | A(EAct::Peck), Twigs | H(EHabitat::Reed)).Need(NeedFeeding)
				.Len(3.f, 6.f, 2).Often(2.f, 6, 300.f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::GoldfinchCling, "goldfinch_cling", "Goldfinch", F::Garden, M::Still).At(Stand, Lawn).Need(NeedFeeding)
				.Days(180.f, 330.f).Len(5.f, 9.f, 3).Often(1.5f, 5, 420.f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::ChaffinchSally, "chaffinch_sally", "Chaffinch", F::Garden, M::Sally).At(Sit, Twigs).Need(NeedDry)
				.Days(120.f, 260.f).Wind(0.f, 8.f).Len(2.f, 3.f, 1).Often(1.5f, 5, 420.f).Reach(4.f).Clip(""));
			T.push_back(FBuild(E::BullfinchBudNip, "bullfinch_bud_nip", "Bullfinch", F::Garden, M::Still).At(Sit | A(EAct::Peck), Twigs)
				.Days(330.f, 120.f).Len(6.f, 10.f, 4).Often(1.5f, 5, 420.f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::SparrowDustBath, "sparrow_dust_bath", "HouseSparrow", F::Garden, M::Still).At(Stand, Town).Need(NeedDry | NeedGround)
				.Days(100.f, 290.f).Len(6.f, 12.f, 3).Often(2.f, 5, 400.f).Room(0.4f).Base(EAct::Preen).Clip("Preen"));
			T.push_back(FBuild(E::TreeSparrowPuddle, "tree_sparrow_puddle", "TreeSparrow", F::Garden, M::Still).At(Stand, Town).Need(NeedWet | NeedGround)
				.Len(5.f, 9.f, 3).Often(2.f, 5, 400.f).Room(0.4f).Base(EAct::Preen).Clip("Preen"));
			// ---- thrushes and the other singers ----
			T.push_back(FBuild(E::StarlingMimic, "starling_mimic", "Starling", F::Singers, M::Still).At(Sit, Heights | H(EHabitat::Broadleaf))
				.Len(5.f, 9.f, 2).Often(1.5f, 6, 400.f).Clip("Call"));
			T.push_back(FBuild(E::StarlingGape, "starling_gape", "Starling", F::Singers, M::Still).At(Feed, Lawn).Need(NeedFeeding | NeedGround)
				.Len(3.f, 5.f, 3).Often(1.5f, 6, 400.f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::ThrushAnvil, "thrush_anvil", "SongThrush", F::Singers, M::Still)
				.At(Stand, Town | H(EHabitat::ForestFloor)).Need(NeedFeeding | NeedGround)
				.Days(90.f, 300.f).Len(5.f, 8.f, 5).Often(1.5f, 5, 420.f).Room(0.3f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::FieldfareGuard, "fieldfare_guard", "Fieldfare", F::Singers, M::Sally).At(Stand, Twigs | H(EHabitat::Meadow)).Need(NeedRival)
				.Days(280.f, 90.f).Len(3.f, 5.f, 1).Often(3.f, 6, 240.f).Reach(8.f).Clip(""));
			T.push_back(FBuild(E::BlackbirdLeafToss, "blackbird_leaf_toss", "Blackbird", F::Singers, M::Still)
				.At(Feed, H(EHabitat::ForestFloor) | H(EHabitat::Garden)).Need(NeedFeeding | NeedGround)
				.Len(4.f, 7.f, 4).Often(2.5f, 8, 240.f).Room(0.4f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::BlackbirdWormTug, "blackbird_worm_tug", "Blackbird", F::Singers, M::Still).At(Feed, Lawn).Need(NeedFeeding | NeedGround)
				.Hours(EHabitHours::Morning).Len(5.f, 8.f, 1).Often(1.5f, 4, 600.f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::RobinBob, "robin_bob", "Robin", F::Singers, M::Still).At(Stand).Need(NeedPlayerNear)
				.Len(2.f, 3.f, 2).Often(4.f, 8, 180.f).Clip("Look"));
			T.push_back(FBuild(E::RobinPounce, "robin_pounce", "Robin", F::Singers, M::Sally).At(Sit, Twigs)
				.Wind(0.f, 10.f).Len(3.f, 5.f, 1).Often(1.5f, 6, 300.f).Reach(5.f).Clip(""));
			T.push_back(FBuild(E::WagtailDash, "wagtail_dash", "WhiteWagtail", F::Singers, M::Run)
				.At(Feed, H(EHabitat::Beach) | H(EHabitat::Street) | H(EHabitat::Meadow)).Need(NeedFeeding | NeedGround)
				.Len(3.f, 6.f, 3).Often(3.f, 8, 200.f).Reach(3.5f, 2.6f).Clip("Walk"));
			T.push_back(FBuild(E::WagtailLeap, "wagtail_leap", "WhiteWagtail", F::Singers, M::Still)
				.At(Feed, H(EHabitat::Beach) | H(EHabitat::Street) | H(EHabitat::Meadow)).Need(NeedGround)
				.Days(100.f, 280.f).Len(1.5f, 2.f, 1).Often(1.5f, 5, 400.f).Room(0.5f).Clip("Takeoff"));
			T.push_back(FBuild(E::WaxwingGulp, "waxwing_gulp", "Waxwing", F::Singers, M::Still).At(Sit | A(EAct::Peck), Twigs).Need(NeedFeeding)
				.Days(290.f, 100.f).Len(4.f, 7.f, 4).Often(2.f, 6, 300.f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::WaxwingPass, "waxwing_pass", "Waxwing", F::Singers, M::Pair).At(Sit | A(EAct::Peck), Twigs)
				.Days(290.f, 100.f).Len(4.f, 6.f, 2).Often(1.5f, 4, 600.f).Clip("Peck"));
			// ---- the birds of prey ----
			T.push_back(FBuild(E::SparrowhawkDash, "sparrowhawk_dash", "Sparrowhawk", F::Raptors, M::Mapped).From(EHabitFrom::BirdEvent, (int)EEvent::ChaseStart).Len(6.f, 6.f, 0).Clip(""));
			T.push_back(FBuild(E::SparrowhawkPluck, "sparrowhawk_pluck", "Sparrowhawk", F::Raptors, M::Still).At(Sit, Twigs | H(EHabitat::Roof))
				.Len(8.f, 14.f, 5).Often(1.f, 3, 900.f).Base(EAct::Peck).Clip("Peck"));
			T.push_back(FBuild(E::PeregrineStoop, "peregrine_stoop", "Peregrine", F::Raptors, M::Mapped).From(EHabitFrom::BirdEvent, (int)EEvent::ChaseStart).Len(8.f, 8.f, 0).Clip(""));
			T.push_back(FBuild(E::PeregrineBob, "peregrine_bob", "Peregrine", F::Raptors, M::Still).At(Sit, H(EHabitat::Tower) | H(EHabitat::Cliff) | H(EHabitat::Roof))
				.Len(3.f, 5.f, 3).Often(1.5f, 5, 420.f).Clip("Look"));
			T.push_back(FBuild(E::SeaEagleSnatch, "sea_eagle_snatch", "WhiteTailedEagle", F::Raptors, M::Swoop).At(Soaring).Need(NeedWater)
				.Wind(0.f, 14.f).Len(8.f, 20.f, 1).Often(1.f, 3, 900.f).Reach(250.f).Clip(""));
			T.push_back(FBuild(E::GoldenEagleDance, "golden_eagle_dance", "GoldenEagle", F::Raptors, M::Dance).At(Soaring)
				.Days(20.f, 150.f).Len(14.f, 20.f, 3).Often(1.f, 3, 900.f).Clip(""));
			T.push_back(FBuild(E::BuzzardHang, "buzzard_hang", "CommonBuzzard", F::Raptors, M::Hang).At(Soaring)
				.Wind(5.f, 16.f).Len(6.f, 12.f, 0).Often(1.5f, 4, 600.f).Clip(""));
			return T;
		}

		struct FIndex
		{
			std::vector<FHabitDef> Table;
			std::vector<std::vector<int>> BySpecies;
			std::vector<EBirdFamily> Family;
			std::vector<int> None;
			size_t SpeciesN = 0;
		};

		FIndex& Index()
		{
			static FIndex X;
			if (X.Table.empty())
			{
				X.Table = Build();
				std::sort(X.Table.begin(), X.Table.end(), [](const FHabitDef& L, const FHabitDef& R) { return (int)L.Id < (int)R.Id; });
			}
			const std::vector<FSpecies>& Sp = SpeciesTable();
			if (X.SpeciesN != Sp.size())
			{
				X.SpeciesN = Sp.size();
				X.BySpecies.assign(Sp.size(), {});
				X.Family.assign(Sp.size(), EBirdFamily::Num);
				for (int I = 1; I < (int)X.Table.size(); I++)
				{
					const int S = FindSpecies(X.Table[I].Species);
					if (S >= 0)
					{
						X.BySpecies[S].push_back(I);
						X.Family[S] = X.Table[I].Family;
					}
				}
			}
			return X;
		}

		bool InDays(float Doy, float From, float To)
		{
			return From <= To ? (Doy >= From && Doy <= To) : (Doy >= From || Doy <= To);
		}
	}

	const char* HabitName(EHabit H)
	{
		const std::vector<FHabitDef>& T = HabitTable();
		return (int)H > 0 && (int)H < (int)T.size() ? T[(int)H].Name : "none";
	}

	EHabit FindHabit(const std::string& Name)
	{
		for (const FHabitDef& D : HabitTable())
		{
			if (D.Id != EHabit::None && Name == D.Name)
			{
				return D.Id;
			}
		}
		return EHabit::None;
	}

	const char* FamilyName(EBirdFamily F)
	{
		static const char* Names[] = { "crows", "wings", "water", "garden", "singers", "raptors", "none" };
		return Names[std::min((int)F, (int)EBirdFamily::Num)];
	}

	const std::vector<FHabitDef>& HabitTable() { return HabitDetail::Index().Table; }
	const FHabitDef& HabitDef(EHabit H) { return HabitTable()[(int)H < NumHabits ? (int)H : 0]; }

	const std::vector<int>& HabitsOf(int Species)
	{
		HabitDetail::FIndex& X = HabitDetail::Index();
		return Species >= 0 && Species < (int)X.BySpecies.size() ? X.BySpecies[Species] : X.None;
	}

	EBirdFamily FamilyOf(int Species)
	{
		HabitDetail::FIndex& X = HabitDetail::Index();
		return Species >= 0 && Species < (int)X.Family.size() ? X.Family[Species] : EBirdFamily::Num;
	}

	FHabits::FHabits()
	{
		for (double& L : LastStart)
		{
			L = -1e9;
		}
	}

	int FHabits::Find(int Bird) const
	{
		for (int I = 0; I < (int)Active.size(); I++)
		{
			if (Active[I].Bird == Bird)
			{
				return I;
			}
		}
		return -1;
	}

	const FHabitRun* FHabits::Of(int Bird) const
	{
		const int I = Find(Bird);
		return I >= 0 ? &Active[I] : nullptr;
	}

	void FHabits::Emit(EHabitEvent K, EHabit H, const FBird& B, int Arg)
	{
		Events.push_back({ K, H, B.Id, B.Pos, Arg });
	}

	// --- where a habit can be done --------------------------------------------------------------------

	/** Nothing between A and B: no wall (the game's line trace) and never under the ground. */
	bool FHabits::Clear(const FWorld& W, const V3& A, const V3& B) const
	{
		if (W.Sight && !W.Sight(A, B))
		{
			return false;
		}
		const float L = (B - A).Len();
		const int N = std::max(1, (int)(L / 2.f));
		for (int I = 1; I < N; I++)
		{
			const V3 P = HabitDetail::Lerp3(A, B, (float)I / N);
			if (P.Z < std::max(W.GroundZ(P.X, P.Y), W.SeaZ) - 0.05f)
			{
				return false;
			}
		}
		return true;
	}

	/** Room metres free ahead of P (at Yaw, or facing the other way: then OutYaw) and to both sides. */
	bool FHabits::RoomAt(const FWorld& W, const V3& P, float Yaw, float Room, float* OutYaw) const
	{
		if (OutYaw)
		{
			*OutYaw = Yaw;
		}
		if (Room <= 0.f || !W.Sight)
		{
			return true;
		}
		const V3 Eye = P + V3(0.f, 0.f, 0.12f);
		const auto Free = [&](float A, float M) { return W.Sight(Eye, Eye + V3(std::cos(A), std::sin(A), 0.f) * M); };
		if (!Free(Yaw + HabitDetail::Pi * 0.5f, Room * 0.7f) || !Free(Yaw - HabitDetail::Pi * 0.5f, Room * 0.7f))
		{
			return false;
		}
		if (Free(Yaw, Room))
		{
			return true;
		}
		if (Free(Yaw + HabitDetail::Pi, Room))
		{
			if (OutYaw)
			{
				*OutYaw = Yaw + HabitDetail::Pi;
			}
			return true;
		}
		return false;
	}

	/** The ground at (X, Y) for a bird standing at Near: false when it is a step up or down (a kerb is fine, a ledge is not). */
	bool FHabits::GroundAt(const FWorld& W, const V3& Near, float X, float Y, float& Z) const
	{
		if (!(W.Probe && W.Probe(X, Y, Z)))
		{
			Z = Near.Z + (W.GroundZ(X, Y) - W.GroundZ(Near.X, Near.Y));
		}
		return std::fabs(Z - Near.Z) < 0.35f && Z > W.SeaZ - 0.02f;
	}

	/** A calm bird within R of B that has no habit: of its own species (bSame) or a rival of another, no bigger than it. */
	int FHabits::Partner(const FWorld& W, const FBird& B, float R, bool bSame) const
	{
		int Best = -1;
		float BestD = R;
		const FSpecies& S = SpeciesTable()[B.Species];
		W.ForNear(B.Pos, R, [&](int Id)
		{
			const FBird& O = W.Birds[Id];
			if (Id == B.Id || O.bGone || O.Habit != 0 || IsAirborne(O.Act) || O.Act == EAct::Sleep || O.Act == EAct::Claimed || O.Alert > 0.3f)
			{
				return;
			}
			if (bSame ? O.Species != B.Species : (O.Species == B.Species || SpeciesTable()[O.Species].Length > S.Length * 1.15f || O.Site != B.Site))
			{
				return;
			}
			const float D = (O.Pos - B.Pos).Len();
			if (D < BestD)
			{
				BestD = D;
				Best = Id;
			}
		});
		return Best;
	}

	/** Water to dip to within R of P: a water the game named, else the sea a little off the shore. */
	bool FHabits::WaterNear(const FWorld& W, const V3& P, float R, uint32_t& Rng, V3& Out) const
	{
		float BestD = R;
		bool bFound = false;
		for (const V3& Wt : Waters)
		{
			const float D = (Wt - P).Len2D();
			if (D < BestD && D > 6.f)
			{
				BestD = D;
				Out = Wt;
				bFound = true;
			}
		}
		if (bFound)
		{
			return true;
		}
		for (int Try = 0; Try < 10; Try++)
		{
			const float A = HabitDetail::Dice(Rng) * 2.f * HabitDetail::Pi, D = R * (0.2f + 0.8f * HabitDetail::Dice(Rng));
			const float X = P.X + std::cos(A) * D, Y = P.Y + std::sin(A) * D;
			const float Off = W.Offshore(X, Y);
			if (Off > 4.f && Off < 80.f)
			{
				Out = V3(X, Y, W.SeaZ);
				return true;
			}
		}
		return false;
	}

	bool FHabits::Fits(FWorld& W, const FBird& B, const FHabitDef& D, bool bFree) const
	{
		if (D.Id == EHabit::None || !((D.Acts >> (int)B.Act) & 1u) || B.bGone || B.Delay > 0.f)
		{
			return false;
		}
		const bool bCalm = (D.Needs & NeedPlayerNear) ? B.Alert < 0.5f : (B.Alert < 0.25f && !B.bWatching && B.Wary <= 0.f);
		if (!bCalm || B.Id == W.Summoned || B.Id == W.Approacher)
		{
			return false;
		}
		const bool bAir = IsAirborne(B.Act);
		const FGroup& G = W.Groups[B.Group];
		if (!bAir)
		{
			if (B.Site < 0)
			{
				return false;
			}
			const FSite& Site = W.Sites[B.Site];
			if (D.Habitats && !((D.Habitats >> (int)Site.Kind) & 1u))
			{
				return false;
			}
			if ((D.Needs & NeedGround) && (!IsGroundHabitat(Site.Kind) || Site.Kind == EHabitat::Sea))
			{
				return false;
			}
			const bool bFeeder = (Site.Flags & SiteFeeder) != 0;
			if (((D.Needs & NeedFeeder) && !bFeeder) || ((D.Needs & NeedNoFeeder) && bFeeder))
			{
				return false;
			}
		}
		if (((D.Needs & NeedFeeding) && G.Plan != 1) || ((D.Needs & NeedResting) && G.Plan != 0 && G.Plan != 5) || ((D.Needs & NeedGathering) && G.Plan != 3))
		{
			return false;
		}
		if (D.Needs & NeedPlayerNear)
		{
			const float Dist = (W.Player.Feet - B.Pos).Len();
			if (!W.Player.bPresent || W.Player.bClaimedBird || !B.bNoticed || Dist < 3.f || Dist > 25.f)
			{
				return false;
			}
		}
		if (bFree)
		{
			return true;
		}
		// the hour, the season, the weather
		if (W.Hour < W.Sunrise - 0.2f || W.Hour > W.Sunset + 0.3f)
		{
			return false;
		}
		if ((D.Hours == EHabitHours::Morning && W.Hour > W.Sunrise + 3.5f) || (D.Hours == EHabitHours::Midday && (W.Hour < 11.f || W.Hour > 15.5f))
			|| (D.Hours == EHabitHours::Evening && W.Hour < W.Sunset - 2.5f))
		{
			return false;
		}
		if (!HabitDetail::InDays(W.DayOfYear, D.DoyFrom, D.DoyTo))
		{
			return false;
		}
		const float Wind = W.Wind.Len2D();
		if (Wind < D.WindMin || Wind > D.WindMax)
		{
			return false;
		}
		if (((D.Needs & NeedDry) && (W.Rain > 0.1f || Wet > 0.15f)) || ((D.Needs & NeedWet) && Wet < 0.3f)
			|| ((D.Needs & NeedSun) && (W.Rain > 0.1f || W.TempC < 14.f)) || ((D.Needs & NeedFrost) && W.TempC > -0.5f)
			|| ((D.Needs & NeedLowTide) && W.SeaZ > W.MeanSeaZ - 0.15f))
		{
			return false;
		}
		return true;
	}

	/** The habit's path and partner from where the bird is; false = it can't be done here (a wall, no room, nobody near). */
	bool FHabits::Plan(FWorld& W, FBird& B, const FHabitDef& D, FHabitRun& R) const
	{
		const FSpecies& S = SpeciesTable()[B.Species];
		uint32_t Rng = (uint32_t)(B.Id * 2654435761u) ^ (uint32_t)(W.Time * 1000.0) ^ 0x5bd1e995u;
		Rng |= 1u;
		R.Home = R.From = R.To = R.Exit = B.Pos;
		R.HomeYaw = B.Yaw;
		R.MySpot = B.Spot;
		R.MySite = B.Site;
		R.Was = B.Act;
		R.Len = D.LenMin + (D.LenMax - D.LenMin) * HabitDetail::Dice(Rng);
		const float Speed = D.Speed > 0.f ? D.Speed : S.Cruise;
		switch (D.Mech)
		{
		case EHabitMech::Still:
		{
			float Yaw = B.Yaw;
			if (D.Needs & NeedPlayerNear)
			{
				Yaw = std::atan2(W.Player.Feet.Y - B.Pos.Y, W.Player.Feet.X - B.Pos.X);
				if (!Clear(W, B.Pos + V3(0.f, 0.f, 0.15f), W.Player.Eye))
				{
					return false;
				}
			}
			if (!RoomAt(W, B.Pos, Yaw, D.Room, &Yaw))
			{
				return false;
			}
			R.HomeYaw = Yaw;
			return true;
		}
		case EHabitMech::Pair:
		{
			R.Other = Partner(W, B, 3.f, true);
			if (R.Other < 0)
			{
				return false;
			}
			const FBird& O = W.Birds[R.Other];
			if (std::fabs(O.Pos.Z - B.Pos.Z) > 0.4f || !Clear(W, B.Pos + V3(0.f, 0.f, 0.1f), O.Pos + V3(0.f, 0.f, 0.1f)))
			{
				return false;
			}
			const V3 To = (O.Pos - B.Pos).Flat();
			const float L = To.Len2D();
			const float Stand = std::max(0.12f, S.Length * 0.9f);
			R.To = L > Stand ? O.Pos - To * (Stand / L) : B.Pos;
			R.HomeYaw = L > 0.01f ? std::atan2(To.Y, To.X) : B.Yaw;
			return RoomAt(W, O.Pos, R.HomeYaw, D.Room, nullptr);
		}
		case EHabitMech::Run:
		{
			// the first run; the next ones are picked as it goes (Advance)
			for (int Try = 0; Try < 6; Try++)
			{
				const float A = B.Yaw + (HabitDetail::Dice(Rng) - 0.5f) * 2.4f, L = D.Reach * (0.45f + 0.55f * HabitDetail::Dice(Rng));
				V3 To = B.Pos + V3(std::cos(A), std::sin(A), 0.f) * L;
				if (GroundAt(W, B.Pos, To.X, To.Y, To.Z) && Clear(W, B.Pos + V3(0.f, 0.f, 0.1f), To + V3(0.f, 0.f, 0.1f)))
				{
					R.To = To;
					return true;
				}
			}
			return false;
		}
		case EHabitMech::Sally:
		{
			const V3 Eye = B.Pos + V3(0.f, 0.f, 0.1f);
			if (D.Id == EHabit::GullSnatch)
			{
				float BestD = D.Reach;
				for (const FMorsel& M : Morsels)
				{
					const float Dist = (M.Pos - B.Pos).Len();
					if (Dist < BestD && Dist > 3.f && Clear(W, Eye, M.Pos + V3(0.f, 0.f, 0.3f)))
					{
						BestD = Dist;
						R.Other = M.Id;
						R.To = M.Pos + V3(0.f, 0.f, 0.12f);
					}
				}
				return R.Other >= 0;
			}
			if (D.Id == EHabit::FieldfareGuard)
			{
				R.Other = Partner(W, B, D.Reach, false);
				if (R.Other < 0 || (W.Birds[R.Other].Pos - B.Pos).Len() < 0.6f)
				{
					return false;
				}
				R.To = W.Birds[R.Other].Pos + V3(0.f, 0.f, 0.05f);
				return Clear(W, Eye, R.To);
			}
			for (int Try = 0; Try < 6; Try++)
			{
				const float A = B.Yaw + (HabitDetail::Dice(Rng) - 0.5f) * 3.f, L = D.Reach * (0.5f + 0.5f * HabitDetail::Dice(Rng));
				V3 To = B.Pos + V3(std::cos(A), std::sin(A), 0.f) * L;
				if (D.Id == EHabit::RobinPounce)
				{
					// down to the ground under its perch, a few steps out
					float Z;
					if (!(W.Probe && W.Probe(To.X, To.Y, Z)))
					{
						Z = W.GroundZ(To.X, To.Y);
					}
					if (Z < W.SeaZ - 0.02f || B.Pos.Z - Z < 0.4f || B.Pos.Z - Z > 7.f)
					{
						continue;
					}
					To.Z = Z + 0.02f;
				}
				else if (D.Id == EHabit::WoodPigeonDisplay)
				{
					To.Z = B.Pos.Z + L * 0.55f;
					if (To.Z < W.TopZ(To.X, To.Y) + 2.f)
					{
						continue;
					}
				}
				else
				{
					To.Z = B.Pos.Z + 0.4f + 1.2f * HabitDetail::Dice(Rng);
				}
				if (Clear(W, Eye, To + V3(0.f, 0.f, 0.1f)))
				{
					R.To = To;
					return true;
				}
			}
			return false;
		}
		case EHabitMech::Swoop:
		{
			V3 To;
			if (D.Needs & NeedWater)
			{
				if (!WaterNear(W, B.Pos, D.Reach, Rng, To))
				{
					return false;
				}
				To.Z += D.Id == EHabit::SeaEagleSnatch ? 0.35f : 0.06f;
			}
			else
			{
				// the nearest roof or tower (swifts) or the cliff's face (martins) within reach
				const EHabitat Want = D.Id == EHabit::MartinBankFlutter ? EHabitat::Cliff : EHabitat::Roof;
				float BestD = D.Reach;
				int Best = -1;
				for (int I = 0; I < (int)W.Sites.size(); I++)
				{
					const FSite& Site = W.Sites[I];
					if (Site.Kind != Want && !(Want == EHabitat::Roof && Site.Kind == EHabitat::Tower))
					{
						continue;
					}
					const float Dist = (Site.Center() - B.Pos).Len2D();
					if (Dist < BestD && Dist > 12.f)
					{
						BestD = Dist;
						Best = I;
					}
				}
				if (Best < 0)
				{
					return false;
				}
				const FSite& Site = W.Sites[Best];
				To = Site.Center();
				if (Want == EHabitat::Cliff)
				{
					// out from the face: toward the lowest ground round it
					V3 Out(0.f, 0.f, 0.f);
					float Low = 1e9f;
					for (int K = 0; K < 8; K++)
					{
						const float A = K * HabitDetail::Pi / 4.f;
						const float Z = W.GroundZ(To.X + std::cos(A) * 8.f, To.Y + std::sin(A) * 8.f);
						if (Z < Low)
						{
							Low = Z;
							Out = V3(std::cos(A), std::sin(A), 0.f);
						}
					}
					To += Out * 1.6f;
				}
				else
				{
					To.Z = std::max(To.Z, W.TopZ(To.X, To.Y)) + 2.f;     // over the ridge, never into the wall under it
				}
			}
			if ((To - B.Pos).Len2D() < 8.f)
			{
				return false;
			}
			// the way out mirrors the way in (so the lowest point of the curve is the point itself), turned if a wall is there
			const V3 In = (To - B.Pos).Flat();
			for (int Try = 0; Try < 5; Try++)
			{
				static const float Turn[5] = { 0.f, 0.7f, -0.7f, 1.4f, -1.4f };
				const float C = std::cos(Turn[Try]), Sn = std::sin(Turn[Try]);
				V3 Exit = To + V3(In.X * C - In.Y * Sn, In.X * Sn + In.Y * C, 0.f);
				Exit.Z = B.Pos.Z;
				if (Exit.Z < std::max(W.TopZ(Exit.X, Exit.Y), W.SeaZ) + 1.5f)
				{
					continue;
				}
				if (Clear(W, B.Pos, To + V3(0.f, 0.f, 0.3f)) && Clear(W, To + V3(0.f, 0.f, 0.3f), Exit))
				{
					R.To = To;
					R.Exit = Exit;
					R.StepLen = ((To - B.Pos).Len() + (Exit - To).Len()) / std::max(Speed, 3.f);
					R.Len = R.StepLen + (D.Id == EHabit::MartinBankFlutter ? 1.5f : 0.f);
					return true;
				}
			}
			return false;
		}
		case EHabitMech::Hang:
			return B.Pos.Z > std::max(W.TopZ(B.Pos.X, B.Pos.Y), W.SeaZ) + 8.f;
		case EHabitMech::Dance:
		{
			// three dives and climbs along its heading: the air under the whole of it must be free
			const float Fwd = S.Cruise * 0.9f * R.Len;
			float Floor = -1e9f;
			for (int K = 0; K <= 6; K++)
			{
				const V3 P = B.Pos + V3(std::cos(B.Yaw), std::sin(B.Yaw), 0.f) * (Fwd * K / 6.f);
				Floor = std::max({ Floor, W.TopZ(P.X, P.Y), W.SeaZ });
			}
			const float Depth = std::min(28.f, B.Pos.Z - Floor - 15.f);
			if (Depth < 8.f)
			{
				return false;
			}
			R.Exit = V3(Depth, 0.f, 0.f);       // (the depth of the dives rides in Exit.X)
			return true;
		}
		default:
			return true;
		}
	}

	// --- starting and ending ---------------------------------------------------------------------------

	bool FHabits::Begin(FWorld& W, FBird& B, const FHabitDef& D, bool bMapped, float Len)
	{
		FHabitRun R;
		R.Bird = B.Id;
		R.Habit = D.Id;
		R.bMapped = bMapped;
		if (bMapped)
		{
			R.Home = R.From = R.To = B.Pos;
			R.Was = B.Act;
			R.Len = Len;
			R.Mine = B.Act;
		}
		else
		{
			if (!Plan(W, B, D, R))
			{
				return false;
			}
			switch (D.Mech)
			{
			case EHabitMech::Still:
				if (D.Base != EAct::Num || B.Act == EAct::Forage)
				{
					W.SetAct(B, D.Base != EAct::Num ? D.Base : EAct::Peck, 1e6f);
				}
				else
				{
					B.ActLen = 1e6f;
				}
				B.Vel = V3();
				B.Yaw = R.HomeYaw;
				break;
			case EHabitMech::Pair:
			{
				// it sidles up to the other one, which waits
				R.bMoves = true;
				W.SetAct(B, EAct::Forage, 1e6f);
				FBird& O = W.Birds[R.Other];
				FHabitRun P;
				P.Bird = O.Id;
				P.Habit = D.Id;
				P.bSecond = true;
				P.Other = B.Id;
				P.Home = P.From = P.To = O.Pos;
				P.Was = O.Act;
				P.MySpot = O.Spot;
				P.MySite = O.Site;
				P.Len = 1e6f;
				W.SetAct(O, EAct::Perch, 1e6f);
				O.Vel = V3();
				P.Mine = O.Act;
				O.Habit = (uint8_t)D.Id;
				O.HabitT = 0.f;
				O.bHabitMoves = false;
				Active.push_back(P);
				break;
			}
			case EHabitMech::Run:
				R.bMoves = true;
				R.From = B.Pos;
				W.SetAct(B, EAct::Forage, 1e6f);
				break;
			case EHabitMech::Sally:
				R.bMoves = true;
				R.From = B.Pos;
				R.StepLen = std::max(0.25f, (R.To - R.From).Len() / std::max(D.Id == EHabit::GullSnatch ? SpeciesTable()[B.Species].Sprint : (D.Speed > 0.f ? D.Speed : SpeciesTable()[B.Species].Cruise * 0.6f), 2.5f));
				W.SetAct(B, EAct::Fly, 1e6f);
				break;
			case EHabitMech::Swoop:
			case EHabitMech::Hang:
			case EHabitMech::Dance:
				R.bMoves = true;
				R.From = B.Pos;
				break;
			default:
				break;
			}
			R.Mine = B.Act;
			Today[(int)D.Id]++;
			LastStart[(int)D.Id] = W.Time;
		}
		B.Habit = (uint8_t)D.Id;
		B.HabitT = 0.f;
		B.bHabitMoves = R.bMoves;
		B.bLook = false;
		Count[(int)D.Id]++;
		Active.push_back(R);
		Emit(EHabitEvent::Start, D.Id, B);
		return true;
	}

	void FHabits::End(FWorld& W, FHabitRun& R, bool bAbort)
	{
		if (R.Bird < 0)
		{
			return;
		}
		FBird& B = W.Birds[R.Bird];
		const FHabitDef& D = HabitDef(R.Habit);
		const int Other = R.Other;
		const bool bPair = D.Mech == EHabitMech::Pair && !R.bMapped;
		Emit(EHabitEvent::End, R.Habit, B);
		B.Habit = 0;
		B.bHabitMoves = false;
		B.HabitT = 0.f;
		B.HabitCool = R.bMapped ? 20.f : (bAbort ? 30.f : 90.f + 150.f * ((B.Id * 37 + (int)R.Habit * 11) % 100) / 100.f);
		const bool bMine = B.Act == R.Mine;
		const EHabitMech Mech = R.bMapped ? EHabitMech::Mapped : D.Mech;
		R.Bird = -1;
		if (bMine && !R.bMapped)
		{
			switch (Mech)
			{
			case EHabitMech::Still:
				B.ActLen = 0.f;
				if (!bAbort)
				{
					W.Settle(B);
				}
				break;
			case EHabitMech::Pair:
			case EHabitMech::Run:
				B.Spot = B.Pos;
				B.Vel = V3();
				if (bAbort)
				{
					W.SetAct(B, EAct::Perch, 0.f);
				}
				else
				{
					W.Settle(B);
				}
				break;
			case EHabitMech::Sally:
				B.Site = R.MySite;
				B.Spot = R.MySpot;
				if (bAbort)
				{
					W.SetAct(B, EAct::Fly, 0.f);      // the bird life flies it home from wherever it is
				}
				else
				{
					B.Pos = R.Home;
					B.Yaw = R.HomeYaw;
					W.Settle(B);
				}
				break;
			default:
				break;                               // on the wing: its act went on all the while
			}
		}
		// the two of a pair end together
		if (bPair && Other >= 0)
		{
			const int I = Find(Other);
			if (I >= 0 && Active[I].Habit == D.Id)
			{
				Active[I].Other = -1;
				End(W, Active[I], bAbort);
			}
		}
	}

	bool FHabits::Start(FWorld& W, FBird& B, EHabit H, bool bFree)
	{
		const FHabitDef& D = HabitDef(H);
		if (D.Id == EHabit::None || B.Habit != 0 || FindSpecies(D.Species) != B.Species)
		{
			return false;
		}
		if (D.From != EHabitFrom::Dice)
		{
			return Begin(W, B, D, true, D.LenMax);
		}
		return Fits(W, B, D, bFree) && Begin(W, B, D, false, 0.f);
	}

	void FHabits::Resettle(FWorld& W)
	{
		for (int I = 0; I < (int)Active.size(); I++)
		{
			End(W, Active[I], true);
		}
		Active.clear();
	}

	// --- the hooks -------------------------------------------------------------------------------------

	bool FHabits::Busy(FWorld& W, FBird& B, float Dt)
	{
		if (B.Habit == 0)
		{
			B.HabitCool = std::max(0.f, B.HabitCool - Dt);
			if (B.Tier <= 1 && B.HabitCool <= 0.f)
			{
				MapActs(W, B);
			}
			return false;
		}
		const int I = Find(B.Id);
		if (I < 0)
		{
			B.Habit = 0;
			B.bHabitMoves = false;
			return false;
		}
		FHabitRun& R = Active[I];
		if (R.bMapped)
		{
			return false;                // the bird life goes on doing it; Advance sees when it stops
		}
		if (B.Act != R.Mine || B.Alert > 0.5f || B.bWatching || B.Delay > 0.f)
		{
			End(W, R, true);
			return false;
		}
		return true;
	}

	bool FHabits::Consider(FWorld& W, FBird& B, float Dt)
	{
		if (B.Tier > 1 || B.Habit != 0)
		{
			return false;
		}
		Considered++;
		if (B.HabitCool > 0.f)
		{
			return false;
		}
		const std::vector<int>& Mine = HabitsOf(B.Species);
		if (Mine.empty())
		{
			return false;
		}
		const FSpecies& S = SpeciesTable()[B.Species];
		if ((B.Pos - W.Camera).Len() > ShowDist + 120.f * S.Length)
		{
			return false;
		}
		int Running = 0;
		for (const FHabitRun& R : Active)
		{
			Running += R.Bird >= 0 && !R.bMapped && !R.bSecond;
		}
		if (Running >= MaxActive)
		{
			return false;
		}
		for (int Di : Mine)
		{
			const FHabitDef& D = HabitTable()[Di];
			if (D.From != EHabitFrom::Dice || Today[(int)D.Id] >= D.PerDay || W.Time - LastStart[(int)D.Id] < D.Gap)
			{
				continue;
			}
			if (!Fits(W, B, D, false))
			{
				continue;
			}
			if (HabitDetail::Dice(Rng) >= 1.f - std::exp(-D.Rate * Dt / 60.f))
			{
				continue;
			}
			if (Begin(W, B, D, false, 0.f))
			{
				// a screaming party is several swifts: the ones hunting next to it come along
				if (D.Id == EHabit::SwiftScreamingParty)
				{
					int More = 3;
					for (int Id : W.Groups[B.Group].Members)
					{
						FBird& O = W.Birds[Id];
						if (More > 0 && Id != B.Id && O.Habit == 0 && O.Act == EAct::Hawk && (O.Pos - B.Pos).Len() < 45.f && Begin(W, O, D, false, 0.f))
						{
							More--;
							Today[(int)D.Id]--;
						}
					}
				}
				return true;
			}
			B.HabitCool = 5.f;          // no room here: not every think
		}
		return false;
	}

	/** Things the bird life does that are a habit: seen on a near bird, the habit is announced while it lasts. */
	void FHabits::MapActs(FWorld& W, FBird& B)
	{
		for (int Di : HabitsOf(B.Species))
		{
			const FHabitDef& D = HabitTable()[Di];
			if (D.From == EHabitFrom::Corvid)
			{
				const FCorvids* C = W.Corvids();
				const FCorvid* M = C ? C->Mind(B.Id) : nullptr;
				if (!M || (int)M->Task != D.FromKind)
				{
					continue;
				}
				if (D.FromArg != -1)
				{
					const int Item = M->Held >= 0 ? M->Held : M->Item;
					if (Item < 0 || Item >= (int)C->Items.size())
					{
						continue;
					}
					const EItem K = C->Items[Item].Kind;
					if (D.FromArg == HabitDetail::AnyShiny ? !ItemKind(K).bShiny : (int)K != D.FromArg)
					{
						continue;
					}
				}
				Begin(W, B, D, true, 1e6f);
				return;
			}
			if (D.From != EHabitFrom::Act || (int)B.Act != D.FromKind)
			{
				continue;
			}
			const bool bSea = B.Site >= 0 && W.Sites[B.Site].Kind == EHabitat::Sea;
			bool bNow = false;
			float Len = D.LenMax;
			switch (D.Id)
			{
			case EHabit::CormorantWings:
				bNow = !bSea && B.ActLen >= 60.f && B.ActTime < B.ActLen - 5.f;
				Len = B.ActLen - B.ActTime;
				break;
			case EHabit::CormorantDive:
				bNow = bSea && B.ActLen >= 10.f && B.ActTime < 1.f;
				break;
			case EHabit::SwanUpend:
				bNow = bSea && B.ActTime < B.ActLen - 0.5f;
				Len = std::max(1.f, B.ActLen - B.ActTime);
				break;
			case EHabit::AlbatrossSeaRun:
				bNow = B.Pos.Z < W.SeaZ + 2.5f && W.GroundZ(B.Pos.X, B.Pos.Y) <= W.SeaZ + 0.2f;
				break;
			case EHabit::AlbatrossShear:
				bNow = W.Wind.Len2D() >= D.WindMin;
				break;
			default:
				break;
			}
			if (bNow && Begin(W, B, D, true, Len))
			{
				return;
			}
		}
	}

	/** The bird life's events that are a habit (a hawk's attack, a gull's long call), and the beats of a corvid's task. */
	void FHabits::MapEvents(FWorld& W)
	{
		if (W.Events.size() < Processed)
		{
			Processed = 0;
		}
		for (; Processed < W.Events.size(); Processed++)
		{
			const FEvent E = W.Events[Processed];
			if (E.Bird < 0 || E.Bird >= (int)W.Birds.size())
			{
				continue;
			}
			FBird& B = W.Birds[E.Bird];
			if (B.Tier > 1 || B.Habit != 0)
			{
				continue;
			}
			for (int Di : HabitsOf(B.Species))
			{
				const FHabitDef& D = HabitTable()[Di];
				const bool bEvent = D.From == EHabitFrom::BirdEvent && (int)E.Kind == D.FromKind;
				const bool bVoice = D.From == EHabitFrom::Voice && (E.Kind == EEvent::Call || E.Kind == EEvent::Song) && (int)E.Voice == D.FromKind;
				if (bEvent || bVoice)
				{
					Begin(W, B, D, true, bEvent ? std::max(D.LenMax, B.ActLen) : D.LenMax);
					break;
				}
			}
		}
		if (const FCorvids* C = W.Corvids())
		{
			for (const FCorvidEvent& E : C->Events)
			{
				if (E.Bird >= 0 && E.Bird < (int)W.Birds.size() && W.Birds[E.Bird].Habit != 0 && (E.Kind == ECorvidEvent::Crack || E.Kind == ECorvidEvent::Cache
					|| E.Kind == ECorvidEvent::Tease || E.Kind == ECorvidEvent::Play || E.Kind == ECorvidEvent::Drop))
				{
					const int I = Find(E.Bird);
					if (I >= 0 && Active[I].bMapped)
					{
						Emit(EHabitEvent::Beat, Active[I].Habit, W.Birds[E.Bird], Active[I].Beat++);
					}
				}
			}
		}
	}

	void FHabits::Advance(FWorld& W, FHabitRun& R, float Dt)
	{
		FBird& B = W.Birds[R.Bird];
		const FHabitDef& D = HabitDef(R.Habit);
		const FSpecies& S = SpeciesTable()[B.Species];
		R.T += Dt;
		B.HabitT = R.T;
		if (R.bMapped)
		{
			// while the bird life goes on doing it
			bool bStill = R.T < R.Len;
			if (D.From == EHabitFrom::Corvid)
			{
				const FCorvid* M = W.Corvids() ? W.Corvids()->Mind(B.Id) : nullptr;
				bStill = M && (int)M->Task == D.FromKind;
			}
			else if (D.From == EHabitFrom::Act && D.Id != EHabit::CormorantDive && D.Id != EHabit::AlbatrossSeaRun)
			{
				bStill = bStill && (int)B.Act == D.FromKind;
			}
			else if (D.From == EHabitFrom::BirdEvent)
			{
				bStill = B.Act == EAct::Chase;
			}
			if (D.From != EHabitFrom::Corvid && D.Beats > 0 && R.Beat < D.Beats && R.T >= R.Len * (R.Beat + 1) / (D.Beats + 1))
			{
				Emit(EHabitEvent::Beat, R.Habit, B, R.Beat++);
			}
			if (!bStill || B.Act == EAct::Claimed || B.bGone)
			{
				End(W, R, false);
			}
			return;
		}
		if (B.Act != R.Mine || B.bGone)
		{
			End(W, R, true);             // something else took the bird (a fright, its flock leaving)
			return;
		}
		if (R.bSecond)
		{
			return;                      // its partner's run ends it
		}
		const auto Beats = [&](float T, float Len)
		{
			while (D.Beats > 0 && R.Beat < D.Beats && T >= Len * (R.Beat + 1) / (D.Beats + 1))
			{
				Emit(EHabitEvent::Beat, R.Habit, B, R.Beat++);
			}
		};
		const auto MoveTo = [&](const V3& P)
		{
			B.Vel = (P - B.Pos) * (1.f / std::max(Dt, 1e-4f));
			B.Pos = P;
			if (B.Vel.Len2D() > 0.2f)
			{
				B.Yaw = std::atan2(B.Vel.Y, B.Vel.X);
			}
		};
		B.Phase += Dt * (IsAirborne(B.Act) ? (S.Wingbeat > 0.f ? S.Wingbeat : 6.f) : 2.f);
		switch (D.Mech)
		{
		case EHabitMech::Still:
			Beats(R.T, R.Len);
			if (D.Id == EHabit::SwanBusk && B.Act == EAct::Swim && W.Player.bPresent)
			{
				// the surge: at the intruder in jerks, wings arched
				const V3 To = (W.Player.Feet - B.Pos).Flat();
				const float L = To.Len2D();
				B.Yaw = std::atan2(To.Y, To.X);
				B.Vel = L > 8.f ? To * (0.5f / L) : V3();
			}
			if (R.T >= R.Len)
			{
				End(W, R, false);
			}
			break;
		case EHabitMech::Pair:
			if (R.Step == 0)
			{
				// sidling up
				const V3 To = R.To - B.Pos;
				const float L = To.Len(), V = std::max(0.25f, S.Gait * 0.7f);
				if (L < V * Dt + 0.02f || R.T > 8.f)
				{
					MoveTo(R.To);
					B.Vel = V3();
					B.Yaw = R.HomeYaw;
					W.SetAct(B, D.Base, 1e6f);
					R.Mine = B.Act;
					R.bMoves = false;
					B.bHabitMoves = false;
					R.Step = 1;
					R.StepT = R.T;
					if (R.Other >= 0)
					{
						FBird& O = W.Birds[R.Other];
						O.Yaw = R.HomeYaw + HabitDetail::Pi;
					}
				}
				else
				{
					MoveTo(B.Pos + To * (V * Dt / L));
				}
			}
			else
			{
				Beats(R.T - R.StepT, R.Len);
				if (R.T - R.StepT >= R.Len)
				{
					End(W, R, false);
				}
			}
			break;
		case EHabitMech::Run:
			if ((R.Step & 1) == 0)
			{
				const V3 To = R.To - B.Pos;
				const float L = To.Len(), V = D.Speed > 0.f ? D.Speed : S.Gait * 2.5f;
				if (L < V * Dt + 0.02f)
				{
					MoveTo(R.To);
					B.Vel = V3();
					Emit(EHabitEvent::Beat, R.Habit, B, R.Beat++);
					R.Step++;
					R.StepT = 0.f;
					R.StepLen = 0.4f + 0.5f * ((B.Id * 13 + R.Step * 7) % 10) / 10.f;
				}
				else
				{
					MoveTo(B.Pos + To * (V * Dt / L));
				}
			}
			else
			{
				R.StepT += Dt;
				B.Vel = V3();
				if (R.StepT >= R.StepLen)
				{
					bool bNext = false;
					if (R.Beat < std::max(1, D.Beats))
					{
						uint32_t Rng = ((uint32_t)(B.Id * 2654435761u) ^ (uint32_t)(W.Time * 977.0)) | 1u;
						const FSite* Site = B.Site >= 0 ? &W.Sites[B.Site] : nullptr;
						for (int Try = 0; Try < 6 && !bNext; Try++)
						{
							const float A = B.Yaw + (HabitDetail::Dice(Rng) - 0.5f) * 2.8f, L = D.Reach * (0.45f + 0.55f * HabitDetail::Dice(Rng));
							V3 To = B.Pos + V3(std::cos(A), std::sin(A), 0.f) * L;
							if (Site && (To - Site->Center()).Len2D() > Site->Radius + (Site->B - Site->A).Len() * 0.5f + 1.5f)
							{
								continue;
							}
							if (GroundAt(W, B.Pos, To.X, To.Y, To.Z) && Clear(W, B.Pos + V3(0.f, 0.f, 0.1f), To + V3(0.f, 0.f, 0.1f)))
							{
								R.To = To;
								bNext = true;
							}
						}
					}
					if (bNext)
					{
						R.Step++;
					}
					else
					{
						End(W, R, false);
					}
				}
			}
			break;
		case EHabitMech::Sally:
		{
			R.StepT += Dt;
			const float U = HabitDetail::Smooth(R.StepT / R.StepLen);
			const float Arc = std::sin(U * HabitDetail::Pi) * std::min(0.6f, (R.To - R.From).Len() * 0.08f);
			if (R.Step == 0)
			{
				MoveTo(HabitDetail::Lerp3(R.From, R.To, U) + V3(0.f, 0.f, D.Id == EHabit::RobinPounce ? 0.f : Arc));
				if (R.StepT >= R.StepLen)
				{
					Emit(EHabitEvent::Beat, R.Habit, B, D.Id == EHabit::GullSnatch ? R.Other : R.Beat);
					R.Beat++;
					float Pause = 0.f;
					if (D.Id == EHabit::RobinPounce)
					{
						Pause = 1.2f;
						W.SetAct(B, EAct::Peck, 1e6f);
						R.Mine = B.Act;
						B.Vel = V3();
					}
					else if (D.Id == EHabit::GullSnatch)
					{
						for (int M = 0; M < (int)Morsels.size(); M++)
						{
							if (Morsels[M].Id == R.Other)
							{
								Morsels.erase(Morsels.begin() + M);
								break;
							}
						}
					}
					else if (D.Id == EHabit::FieldfareGuard && R.Other >= 0)
					{
						// the other bird gives way: off to another twig of the place
						FBird& O = W.Birds[R.Other];
						if (!IsAirborne(O.Act) && O.Act != EAct::Claimed && O.Habit == 0 && O.Site >= 0 && (O.Pos - B.Pos).Len() < 1.5f)
						{
							W.FlyTo(O, O.Site, W.SpotFor(O, O.Site));
							W.Events.push_back({ EEvent::Supplant, O.Id, O.Pos });
						}
					}
					R.Step = 1;
					R.StepT = 0.f;
					R.Exit = V3(Pause, 0.f, 0.f);
				}
			}
			else if (R.Step == 1)
			{
				if (R.StepT >= R.Exit.X)
				{
					if (B.Act != EAct::Fly)
					{
						W.SetAct(B, EAct::Fly, 1e6f);
						R.Mine = B.Act;
					}
					R.Step = 2;
					R.StepT = 0.f;
				}
			}
			else
			{
				MoveTo(HabitDetail::Lerp3(R.To, R.Home, U) + V3(0.f, 0.f, Arc));
				if (R.StepT >= R.StepLen)
				{
					End(W, R, false);
				}
			}
			break;
		}
		case EHabitMech::Swoop:
		{
			// one curve through the three points; the martin flutters a moment at the bank
			const float Hover = R.Len - R.StepLen;
			float U = R.T / R.StepLen;
			if (Hover > 0.f)
			{
				const float Half = R.StepLen * 0.5f;
				U = R.T < Half ? R.T / R.StepLen : (R.T < Half + Hover ? 0.5f : (R.T - Hover) / R.StepLen);
			}
			U = HabitDetail::Clamp(U, 0.f, 1.f);
			const float L0 = 2.f * (U - 0.5f) * (U - 1.f), L1 = -4.f * U * (U - 1.f), L2 = 2.f * U * (U - 0.5f);
			V3 P = R.From * L0 + R.To * L1 + R.Exit * L2;
			P.Z = std::max(P.Z, std::max(W.GroundZ(P.X, P.Y), W.SeaZ) + 0.04f);
			MoveTo(P);
			if (R.Beat == 0 && U >= 0.5f)
			{
				Emit(EHabitEvent::Beat, R.Habit, B, R.Beat++);
			}
			if (R.T >= R.Len)
			{
				End(W, R, false);
			}
			break;
		}
		case EHabitMech::Hang:
		{
			// head to the wind, held over one spot, the wings working a little
			const float Wy = std::atan2(-W.Wind.Y, -W.Wind.X);
			const V3 P = R.Home + V3(std::sin(R.T * 1.3f) * 0.4f, std::cos(R.T * 0.9f) * 0.4f, std::sin(R.T * 1.7f) * 0.3f);
			MoveTo(P);
			B.Yaw = Wy;
			if (R.T >= R.Len)
			{
				B.Vel = V3(std::cos(Wy), std::sin(Wy), 0.f) * (S.Cruise * 0.4f);
				End(W, R, false);
			}
			break;
		}
		case EHabitMech::Dance:
		{
			const float Period = R.Len / 3.f, Depth = R.Exit.X;
			const V3 Fwd(std::cos(R.HomeYaw), std::sin(R.HomeYaw), 0.f);
			V3 P = R.Home + Fwd * (S.Cruise * 0.9f * R.T);
			P.Z = R.Home.Z - Depth * 0.5f * (1.f - std::cos(2.f * HabitDetail::Pi * R.T / Period));
			MoveTo(P);
			if (R.Beat < 3 && R.T >= Period * (R.Beat + 0.5f))
			{
				Emit(EHabitEvent::Beat, R.Habit, B, R.Beat++);
			}
			if (R.T >= R.Len)
			{
				B.FlyFrom = B.Pos;           // it circles on from here
				End(W, R, false);
			}
			break;
		}
		default:
			break;
		}
	}

	void FHabits::Step(FWorld& W, float Dt)
	{
		// the day's budget, and how wet the ground is
		const int Today_ = (int)W.DayOfYear;
		if (Today_ != Day)
		{
			Day = Today_;
			for (int& T : Today)
			{
				T = 0;
			}
		}
		if (LastHour >= 0.f)
		{
			float DH = W.Hour - LastHour;
			if (DH < -12.f)
			{
				DH += 24.f;
			}
			if (DH > 0.f && DH < 1.f)
			{
				Wet = HabitDetail::Clamp(Wet + (W.Rain > 0.2f ? DH * 3.f : -DH / (W.TempC > 18.f ? 1.5f : 3.f)), 0.f, 1.f);
			}
		}
		LastHour = W.Hour;
		MapEvents(W);
		for (int I = 0; I < (int)Active.size(); I++)
		{
			if (Active[I].Bird >= 0)
			{
				Advance(W, Active[I], Dt);
			}
		}
		Active.erase(std::remove_if(Active.begin(), Active.end(), [](const FHabitRun& R) { return R.Bird < 0; }), Active.end());
	}

	std::string FHabits::Report(const FWorld& W) const
	{
		std::string Out;
		char Buf[160];
		for (int I = 1; I < NumHabits; I++)
		{
			if (Count[I] > 0)
			{
				std::snprintf(Buf, sizeof(Buf), "%s%s %d", Out.empty() ? "" : ", ", HabitName((EHabit)I), Count[I]);
				Out += Buf;
			}
		}
		std::snprintf(Buf, sizeof(Buf), " | running %d, looked at %ld birds, wet %.2f", (int)Active.size(), Considered, Wet);
		return "habits: " + (Out.empty() ? std::string("none yet") : Out) + Buf;
	}
}
