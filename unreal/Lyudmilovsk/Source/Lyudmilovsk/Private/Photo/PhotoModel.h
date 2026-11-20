// The film camera's model (docs/photo.md), in plain C++ so that tools/photosim checks it outside the engine: the lens
// and how sharp a frame comes out, the film of 36 frames, the studio and the hero's own darkroom, the album, the photo
// stamps, and everything the hero shot as one saved line.
#pragma once

#include <set>
#include <string>
#include <vector>

/** The film camera's rules (docs/photo.md): a 36-frame film, what a frame holds, how sharp it came out, the two ways of
 *  developing, the album and the photo goals. Plain C++: tools/photosim checks it outside the engine. */
namespace PhotoModel
{
	constexpr int FramesPerFilm = 36;
	/** The studio's price for developing and printing one film, roubles. */
	constexpr int StudioPrice = 45;
	/** A subject counts (a bird sighting, a stamp) from this sharpness of its frame. */
	constexpr float SharpEnough = 0.5f;

	/** The lens: focus distances the ring clicks through, metres (the last is infinity, 1e9). */
	int FocusStops();
	float FocusStop(int Index);             // clamped
	/** The stop nearest to a distance (what a careful hand would set); nearest in dioptres, as the lens sees it. */
	int FocusStopFor(float DistanceM);
	/**
	 * How sharp a subject at SubjectM comes out with the ring at FocusM, 0..1: depth of field of a 50 mm lens at f/8
	 * in dioptres (|1/focus - 1/subject|, 1 at 0, falling to 0 at about 0.35 1/m, so at infinity everything beyond ~6 m
	 * is sharp and a bird at 2 m needs the ring near 2 m), times the slow shutter's blur: the hero's own speed
	 * (walking 1.4 m/s costs about a third, running ruins it) and the subject's speed across the frame scaled by distance.
	 */
	float Sharpness(float FocusM, float SubjectM, float HeroSpeed, float SubjectSpeed);

	/** One thing in the frame when the shutter fell. Tag: "bird:<Species>", "sunset:<regime>", "aurora", "festival:<id>",
	 *  "fish", "maru", "person:<key>", "place:<id>". */
	struct FSubject
	{
		std::string Tag;
		float Sharp = 0.f;
	};

	struct FShot
	{
		int Roll = 0;
		/** 1..36. */
		int Frame = 0;
		/** Game hours since the first midnight. */
		double Hours = 0.0;
		/** The map place it was taken at ("" when nowhere known). */
		std::string Place;
		/** The whole frame's sharpness (its main subject's, or the scene's at the focus set). */
		float Sharp = 0.f;
		std::vector<FSubject> Subjects;
	};

	enum class ERollState { InCamera, Exposed, AtStudio, Developed, Ruined };

	struct FRoll
	{
		int Id = 0;
		ERollState State = ERollState::InCamera;
		/** AtStudio: the game hour the prints are ready. */
		double ReadyAt = 0.0;
		/** Developed: 1 a studio's even prints, lower a hurried darkroom's (pale or dark prints; under RuinedBelow nothing came out). */
		float Quality = 1.f;
		std::vector<FShot> Shots;
	};

	/** What a development brought, for the game to hand out: stamps "set/piece", bird species to note as seen, places for the quests. */
	struct FDeveloped
	{
		/** Every roll that was developed, a ruined one too (it brings no prints and nothing below). */
		std::vector<int> Rolls;
		int Prints = 0;
		/** New stamps only (each once in a life). */
		std::vector<std::string> Stamps;
		/** Species with a sharp frame on these rolls (each once per call). */
		std::vector<std::string> Birds;
		/** Map places with a frame on these rolls (each once per call; any sharpness: a view is a view): where a frame was
		 *  taken (FShot::Place) and what stood in it ("place:<id>" subjects). */
		std::vector<std::string> Places;
		/** Subject tags with a sharp frame, whole as they were given: "maru", "fish", "bird:X", "person:key" (each once per call). */
		std::vector<std::string> Tags;
	};

	/** The stamp a sharp subject earns, "" for none: "bird:X" -> "photo_birds/X", "sunset:r" -> "photo_sky/sunset_r",
	 *  "aurora" -> "photo_sky/aurora", "festival:f" -> "photo_life/fest_f", "fish" -> "photo_life/fish", "maru" -> "photo_life/maru". */
	std::string StampFor(const std::string& Tag);

	/** The darkroom: three baths, each stopped by hand. Error of each is |t - ideal| / ideal; quality 1 within 10 %, falling to 0 at 100 %; the film's quality is the worst bath's. */
	constexpr float RuinedBelow = 0.25f;
	constexpr int Baths = 3;
	/** Ideal seconds in the developer, the stop bath and the fixer (the short process of the game, not a real one): 12, 4, 8. */
	float BathIdeal(int Bath);
	float BathQuality(int Bath, float Seconds);
	float DarkroomQuality(const float Seconds[Baths]);
	/**
	 * The studio takes films at any hour and has them ready at the first 10:00 that leaves it at least 12 hours: a film
	 * handed in by day or in the evening (up to 22:00) is ready at 10:00 the next morning; one handed in after 22:00 or
	 * past midnight before 10:00 misses that morning's printing and is ready at 10:00 a day later.
	 */
	double StudioReadyAt(double NowHours);

	class FState
	{
	public:
		/** Puts a fresh film in: false when one is in already. The game takes the film item from the pocket. */
		bool LoadFilm();
		bool HasFilm() const;
		/** Frames still unexposed on the film in the camera (0 with none). */
		int FramesLeft() const;
		/** Exposes a frame: fills Roll and Frame of the shot and keeps it (sharpness is held to 0..1); the frame number, or 0 with no film or a full one. */
		int Shoot(FShot Shot);
		/** Winds the film back and takes it out (any number of frames, at least one exposed; an untouched film stays in): true when a roll went to the pocket. */
		bool Rewind();
		/** Rolls in the pocket waiting for a development. */
		int Exposed() const;
		/** All exposed rolls go to the studio; how many (the game charges StudioPrice each). */
		int HandIn(double NowHours);
		int AtStudio() const;
		int ReadyAtStudio(double NowHours) const;
		/** The ready rolls come back as prints. */
		FDeveloped Collect(double NowHours);
		/** The hero's own darkroom: the oldest exposed roll, developed with this quality (under RuinedBelow: ruined, no prints, no stamps). Out.Rolls is empty when there is none. */
		FDeveloped DevelopSelf(float Quality);
		/** Every roll there ever was, oldest first. */
		const std::vector<FRoll>& Rolls() const;
		/** The album: every frame of the developed rolls, in the order taken. The pointers live until the state changes. */
		std::vector<const FShot*> Album() const;
		const std::set<std::string>& Stamps() const;
		/** Every frame ever exposed, developed or not. */
		int ShotsTaken() const;
		/**
		 * One line for the save, and back: "v1|stamps=a,b|roll=id:state:ready:quality|shot=frame:hours:sharp:place:tag=sharp,..."
		 * (a shot belongs to the roll before it). Tags, places and stamps may hold any characters: | , : = %, control
		 * characters and newlines are written as %XX, nothing is refused. Sharpness and quality keep 3 decimals, hours 4.
		 * Restore of garbage returns false and leaves the state empty; a key it does not know is skipped.
		 */
		std::string Save() const;
		bool Restore(const std::string& Line);

	private:
		/** Index of the film in the camera in AllRolls, -1 with none (it can only be the newest roll). */
		int CameraIndex() const;
		int CountOf(ERollState State) const;
		/** Turns a roll into prints (or into a ruined film) and adds what it brought to Out. */
		void Develop(FRoll& Roll, float Quality, FDeveloped& Out);

		std::vector<FRoll> AllRolls;
		std::set<std::string> Owned;
	};
}
