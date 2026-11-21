// The telescopes, in plain C++ (no Unreal types) so they can be run and checked outside the engine
// (tools/telescopesim/run.sh). ULyudTelescope wraps it and draws it; docs/telescopes.md explains it.
//
// What is here:
//   - where Venus, Mars, Jupiter and Saturn stand over the coast at a date and hour (Paul Schlyter's low-precision
//     elements, the same method LyudEphemeris uses for the sun and the moon), how big, how bright and how lit they
//     are, how far Saturn's ring is tilted and where Jupiter's four big moons are (Meeus, "Astronomical Algorithms",
//     the low-accuracy method of ch. 44);
//   - the telescope itself: the eyepieces, the mount's slow aim with inertia, the focus knob and how blurred a thing
//     at some distance is;
//   - what the hero is looking at and whether it counts as an observation (in the field, sharp, enough power, dark
//     enough, held for a moment), and the log those observations go into;
//   - the faint field stars (a hash of the equatorial sky, so they turn with it and are the same every night);
//   - small pictures of the planets as the eyepiece shows them.
//
// Angles are degrees. Azimuth is the compass bearing (north through east). "Day" is Schlyter's day number: days since
// 2000 January 0.0 UT (LyudEphemeris::DayNumber gives the same).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace LyudScope
{
	enum class EBody : uint8_t { Moon, Venus, Mars, Jupiter, Saturn, Num };

	/** "moon", "venus", "mars", "jupiter", "saturn". */
	const char* BodyId(EBody Body);

	struct FBody
	{
		double AltDeg = -90.0;
		double AzDeg = 0.0;
		double RaDeg = 0.0;
		double DecDeg = 0.0;
		/** From us and from the sun, AU. */
		double DistanceAu = 1.0;
		double SunDistanceAu = 1.0;
		/** The disk's equatorial diameter, arc seconds. */
		double DiameterArcsec = 0.0;
		/** The lit part of the disk 0..1 and the phase angle (sun - body - us). */
		double Lit = 1.0;
		double PhaseAngleDeg = 0.0;
		/** How far from the sun it stands in the sky. */
		double ElongationDeg = 180.0;
		double Magnitude = 0.0;
		/** How far celestial north is turned from "up" in the eyepiece (counter-clockwise on the sky, toward east). */
		double ParallacticDeg = 0.0;
		/** The position angle of the sun's side (the bright limb's middle), from celestial north through east. */
		double BrightLimbDeg = 0.0;
		/** Saturn: the ring's tilt toward us (0 = edge-on, +: we see its north face). */
		double RingTiltDeg = 0.0;
		/** Jupiter: the turn of its clouds (the Great Red Spot rides it), 0..360. */
		double SpinDeg = 0.0;
		/** Jupiter's Io, Europa, Ganymede and Callisto in its equatorial radii: X toward west, Y toward north; not seen
		 *  while one crosses the disk or is behind it. */
		double MoonX[4] = { 0.0, 0.0, 0.0, 0.0 };
		double MoonY[4] = { 0.0, 0.0, 0.0, 0.0 };
		bool bMoonSeen[4] = { false, false, false, false };
	};

	struct FSky
	{
		double Day = 0.0;
		double LatDeg = 54.96;
		double LonDeg = 20.40;
		/** Local sidereal time. */
		double LstDeg = 0.0;
		double SunAltDeg = -90.0;
		double SunAzDeg = 0.0;
		double SunRaDeg = 0.0;
		double SunDecDeg = 0.0;
		/** The earth - sun distance, AU. */
		double SunDistanceAu = 1.0;
		/** Bodies[Moon] is left empty: the game's moon is ALyudSky's (the engine fills it in, SetMoon). */
		FBody Bodies[static_cast<int>(EBody::Num)];

		FBody& Of(EBody Body) { return Bodies[static_cast<int>(Body)]; }
		const FBody& Of(EBody Body) const { return Bodies[static_cast<int>(Body)]; }
	};

	/** Schlyter's day number of a UT date. */
	double DayNumber(int Year, int Month, int DayOfMonth, double UtHours);

	/** The sky at a moment over a place. */
	FSky Sky(double Day, double LatDeg, double LonDeg);

	/** Puts the game's own moon in (its place, its size across in degrees, its lit part 0..1). */
	void SetMoon(FSky& S, double AltDeg, double AzDeg, double SizeDeg, double Lit);

	/** The angle between two directions given as altitude and azimuth. */
	double Separation(double AltA, double AzA, double AltB, double AzB);

	/** Altitude and azimuth -> right ascension and declination, and back. */
	void ToEquatorial(const FSky& S, double AltDeg, double AzDeg, double& RaDeg, double& DecDeg);
	void ToHorizontal(const FSky& S, double RaDeg, double DecDeg, double& AltDeg, double& AzDeg);

	/** A direction as seen in an eyepiece aimed at (AimAlt, AimAz): tangent-plane X to the right, Y up (the tangent of
	 *  the angle off the middle, so a perspective camera puts it at X * R / tan(Field / 2)). False: behind the view. */
	bool Project(double AimAltDeg, double AimAzDeg, double AltDeg, double AzDeg, double& X, double& Y);

	// ---------------------------------------------------------------- the instrument

	struct FEyepiece
	{
		/** The power as the label says it, and the true field across the eyepiece's circle. */
		int Power = 3;
		double FieldDeg = 15.0;
	};

	const int NumEyepieces = 4;
	/** x3 (the finder, 15 degrees across), x12 (4), x60 (0.8), x400 (0.12: Jupiter a tenth of the field). */
	const FEyepiece& Eyepiece(int Index);

	/** The objective's width, m (the blur of a thing out of focus grows with it). */
	const double ApertureM = 0.08;

	struct FMount
	{
		/** The middle of its swing and how far it turns either way (180: all the way round). */
		double AzCenterDeg = 0.0;
		double AzRangeDeg = 180.0;
		double AltMinDeg = -14.0;
		double AltMaxDeg = 82.0;
	};

	struct FAim
	{
		double AzDeg = 0.0;
		double AltDeg = 2.0;
		/** Degrees a second. */
		double VAz = 0.0;
		double VAlt = 0.0;
	};

	/** One step of the mount: the mouse's move since the last step (counts; right and up positive) pushes the tube, which
	 *  glides on and settles; slower the narrower the field. */
	void StepAim(FAim& Aim, double MouseX, double MouseY, double FieldDeg, double Dt, const FMount& Mount);

	/** Where the knob (0..1) focuses, in dioptres (1 / metres): 0 is infinity (the knob at InfinityKnob), up to 20 m
	 *  at 0; past infinity it goes a little negative (nothing is sharp there). */
	const double InfinityKnob = 0.85;
	double FocusDioptre(double Knob);
	/** The same as a distance, metres (1e7 at and past infinity). */
	double FocusDistanceM(double Knob);
	/** How blurred a thing DistanceM away is (<= 0: the sky), as a share of the field's width. */
	double Blur(double Knob, double DistanceM, double FieldDeg);
	/** Blur below this reads as sharp. */
	const double SharpBlur = 0.006;

	// ---------------------------------------------------------------- what is seen

	enum class ESight : uint8_t
	{
		MoonCrescent, MoonQuarter, MoonGibbous, MoonFull,
		Venus, VenusPhase, Mars, Jupiter, JupiterMoons, Saturn, SaturnRing,
		Sunspots, Aurora, Noctilucent,
		Num
	};

	/** "moon_crescent", "venus", "jupiter_moons"...: the stamp's piece, the note's key, the quest event's target. */
	const char* SightId(ESight Sight);

	struct FLook
	{
		FAim Aim;
		int Eyepiece = 0;
		double Knob = InfinityKnob;
		/** A dark solar filter is on the objective (only the sun shows through it). */
		bool bFilter = false;
		/** The weather's total cloud cover 0..1. */
		double CloudCover = 0.0;
		/** Something solid stands in the way of the middle of the view. */
		bool bBlocked = false;
		/** The rare sky now, 0..1 (what reaches the eye). */
		double Aurora = 0.0;
		double Noctilucent = 0.0;
		/** The sun's disk across, degrees (our red dwarf: 10). */
		double SunSizeDeg = 10.0;
	};

	/** How well a body shows 0..1: above the horizon, the sky dark enough for it, not behind the moon (clouds apart). */
	double Visibility(const FSky& S, EBody Body);

	/** How much the clouds let through toward a direction at a moment 0..1 (gaps drift over; 1 under a clear sky). */
	double CloudGap(double CloudCover, double Day, double AltDeg, double AzDeg);

	struct FTarget
	{
		/** The body nearest the middle of the field (-1: none in it). */
		int Body = -1;
		/** How far off the middle it is, as a share of the field's width. */
		double Off = 0.0;
		/** How well it shows 0..1 (daylight, horizon, cloud). */
		double Visible = 0.0;
		double BlurShare = 0.0;
		/** What looking at it now amounts to (Num: nothing yet: too blurred, too little power, too faint, off the middle). */
		ESight Sight = ESight::Num;
		/** Why not, for the hint line: 0 nothing, 1 focus, 2 more power, 3 too bright a sky, 4 cloud. */
		int Hint = 0;
	};

	FTarget Target(const FSky& S, const FLook& L);

	/** The unfiltered sun is in or near the field: look away at once. */
	bool SunDanger(const FSky& S, const FLook& L);

	/** A steady look: the same sight held in the field for HoldSeconds counts once. */
	const double HoldSeconds = 2.0;
	struct FWatch
	{
		ESight Sight = ESight::Num;
		double Held = 0.0;
		bool bCounted = false;
	};
	/** True on the step the look has been held long enough. */
	bool StepWatch(FWatch& W, const FTarget& T, double Dt);

	// ---------------------------------------------------------------- the log

	struct FLog
	{
		/** Bit per ESight: seen at least once. */
		uint32_t Seen = 0;
		/** The nights Jupiter's moons were drawn on (the quest counts four), and the last of them. */
		int JupiterNights = 0;
		int LastJupiterNight = -1000000;

		bool Has(ESight Sight) const { return (Seen >> static_cast<int>(Sight)) & 1u; }
		int Count() const;
	};

	struct FNote
	{
		ESight Sight = ESight::Num;
		/** The first time ever: a stamp and a line in the diary. */
		bool bFirst = false;
		/** Jupiter's moons on a night they were not drawn on yet: a drawing in the diary, the quest counts it. */
		bool bNewNight = false;
		int JupiterNights = 0;
		/** A sight that carries another with it (the moons: Jupiter; the ring: Saturn; Venus' phase: Venus). */
		ESight Also = ESight::Num;
		bool bAlsoFirst = false;
	};

	/** The night a local moment belongs to (noon to noon), from the local day count (Day + UTC offset / 24). */
	int NightOf(double LocalDay);

	/** Writes a held look into the log. False: nothing new came of it. */
	bool Record(FLog& Log, ESight Sight, double LocalDay, FNote& Out);

	/** Jupiter and its moons as the hero draws them in the diary: "--o---o-O--o-------o" (west on the left as the sky
	 *  shows them with north up: 'O' Jupiter, 'o' a moon that is seen). */
	std::string MoonsDiagram(const FBody& Jupiter);

	// ---------------------------------------------------------------- the field stars

	struct FStar
	{
		/** Tangent-plane place in the eyepiece (Project). */
		float X = 0.f;
		float Y = 0.f;
		float Magnitude = 0.f;
		/** 0 blue-white .. 1 orange. */
		float Warmth = 0.5f;
	};

	/** The faintest star an eyepiece shows on a dark night, and how the twilight takes them away. */
	double LimitMagnitude(int EyepieceIndex, double SunAltDeg);

	/** The stars in the field. They come from a hash of the equatorial sky: the same stars every night, turning with
	 *  the sky, more of them the higher the power. Returns how many were written. */
	int Stars(const FSky& S, double AimAltDeg, double AimAzDeg, double FieldDeg, double Limit, FStar* Out, int Max);

	// ---------------------------------------------------------------- pictures

	struct FImage
	{
		int Size = 0;
		/** Size x Size, RGBA, straight alpha, rows from the top. */
		std::vector<uint8_t> Rgba;
		/** How many pixels the body's equatorial diameter takes in the picture. */
		double DiskPixels = 0.0;
	};

	/** A planet as a small telescope shows it: its own north up, the sun's side to the right (Venus, Mars; the engine
	 *  turns the picture by the bright limb's angle), the equator level (Jupiter with its belts and the spot, Saturn with
	 *  the ring at its tilt). */
	void Draw(EBody Body, const FBody& B, int Size, FImage& Out);

	/** A number that changes when the picture would (so the engine redraws its texture only then). */
	uint32_t DrawKey(EBody Body, const FBody& B);
}
