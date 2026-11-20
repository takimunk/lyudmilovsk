// Checks of the film camera's model (Private/Photo/PhotoModel.cpp): the lens, the film, the studio, the darkroom,
// the album, the stamps and the save line.
//   photo_test
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Photo/PhotoModel.h"

#include <cmath>
#include <cstdio>

static int GChecks = 0, GFailed = 0;

static void Expect(bool bOk, const char* What)
{
	++GChecks;
	if (!bOk)
	{
		++GFailed;
		std::printf("FAIL %s\n", What);
	}
}

static bool Near(double A, double B, double Eps = 0.0006)
{
	return std::fabs(A - B) <= Eps;
}

static PhotoModel::FShot Frame(double Hours, const char* Place, float Sharp)
{
	PhotoModel::FShot Shot;
	Shot.Hours = Hours;
	Shot.Place = Place;
	Shot.Sharp = Sharp;
	return Shot;
}

static PhotoModel::FShot Frame(double Hours, const char* Place, const char* Tag, float Sharp)
{
	PhotoModel::FShot Shot = Frame(Hours, Place, Sharp);
	PhotoModel::FSubject Subject;
	Subject.Tag = Tag;
	Subject.Sharp = Sharp;
	Shot.Subjects.push_back(Subject);
	return Shot;
}

static bool Has(const std::vector<std::string>& List, const char* Item)
{
	int Count = 0;
	for (const std::string& S : List)
	{
		Count += S == Item ? 1 : 0;
	}
	return Count == 1;
}

/** Field by field, with the save line's precision. */
static bool Same(const PhotoModel::FState& A, const PhotoModel::FState& B)
{
	if (A.Rolls().size() != B.Rolls().size() || A.Stamps() != B.Stamps())
	{
		return false;
	}
	for (size_t R = 0; R < A.Rolls().size(); ++R)
	{
		const PhotoModel::FRoll& X = A.Rolls()[R];
		const PhotoModel::FRoll& Y = B.Rolls()[R];
		if (X.Id != Y.Id || X.State != Y.State || !Near(X.ReadyAt, Y.ReadyAt, 0.00006) || !Near(X.Quality, Y.Quality) || X.Shots.size() != Y.Shots.size())
		{
			return false;
		}
		for (size_t I = 0; I < X.Shots.size(); ++I)
		{
			const PhotoModel::FShot& P = X.Shots[I];
			const PhotoModel::FShot& Q = Y.Shots[I];
			if (P.Roll != Q.Roll || P.Frame != Q.Frame || !Near(P.Hours, Q.Hours, 0.00006) || P.Place != Q.Place || !Near(P.Sharp, Q.Sharp) || P.Subjects.size() != Q.Subjects.size())
			{
				return false;
			}
			for (size_t J = 0; J < P.Subjects.size(); ++J)
			{
				if (P.Subjects[J].Tag != Q.Subjects[J].Tag || !Near(P.Subjects[J].Sharp, Q.Subjects[J].Sharp))
				{
					return false;
				}
			}
		}
	}
	return true;
}

/** What must hold whatever the hero did. */
static bool Sound(const PhotoModel::FState& S)
{
	using namespace PhotoModel;
	int Developed = 0, Shots = 0, InCamera = 0, Pocket = 0, Studio = 0;
	for (size_t R = 0; R < S.Rolls().size(); ++R)
	{
		const FRoll& Roll = S.Rolls()[R];
		if (Roll.Id != (int)R + 1 || (int)Roll.Shots.size() > FramesPerFilm)
		{
			return false;
		}
		if (Roll.State == ERollState::InCamera && R + 1 != S.Rolls().size())
		{
			return false;
		}
		if (Roll.State != ERollState::InCamera && Roll.Shots.empty())
		{
			return false;
		}
		if (!(Roll.Quality >= 0.f && Roll.Quality <= 1.f))
		{
			return false;
		}
		for (size_t I = 0; I < Roll.Shots.size(); ++I)
		{
			if (Roll.Shots[I].Frame != (int)I + 1 || Roll.Shots[I].Roll != Roll.Id)
			{
				return false;
			}
		}
		Shots += (int)Roll.Shots.size();
		Developed += Roll.State == ERollState::Developed ? (int)Roll.Shots.size() : 0;
		InCamera += Roll.State == ERollState::InCamera ? 1 : 0;
		Pocket += Roll.State == ERollState::Exposed ? 1 : 0;
		Studio += Roll.State == ERollState::AtStudio ? 1 : 0;
	}
	return (int)S.Album().size() == Developed && S.ShotsTaken() == Shots && InCamera <= 1 && S.HasFilm() == (InCamera == 1)
		&& S.Exposed() == Pocket && S.AtStudio() == Studio && S.FramesLeft() >= 0 && S.FramesLeft() <= FramesPerFilm
		&& (S.HasFilm() || S.FramesLeft() == 0);
}

int main()
{
	using namespace PhotoModel;

	// ---- the lens
	Expect(FocusStops() >= 6, "the ring has a fair number of stops");
	for (int I = 0; I < FocusStops(); ++I)
	{
		Expect(FocusStop(I) > 0.f, "a stop is a distance");
		if (I > 0)
		{
			Expect(FocusStop(I) > FocusStop(I - 1), "the stops go out from near to far");
		}
		Expect(FocusStopFor(FocusStop(I)) == I, "a stop's own distance finds it");
	}
	Expect(FocusStop(FocusStops() - 1) >= 1e8f, "the last stop is infinity");
	Expect(FocusStop(0) < 1.5f, "the first stop is close up");
	Expect(FocusStop(-5) == FocusStop(0) && FocusStop(99) == FocusStop(FocusStops() - 1), "stops are clamped");
	Expect(FocusStopFor(0.f) == 0 && FocusStopFor(-3.f) == 0 && FocusStopFor(0.1f) == 0, "nonsense and too near: the nearest stop");
	Expect(FocusStopFor(1e9f) == FocusStops() - 1 && FocusStopFor(500.f) == FocusStops() - 1, "far away: infinity");
	Expect(Near(FocusStop(FocusStopFor(2.1f)), 2.0, 0.01), "2.1 m sets 2 m");
	Expect(Near(FocusStop(FocusStopFor(1.05f)), 1.0, 0.01), "1.05 m sets 1 m");
	{
		int Last = 0;
		bool bMonotone = true;
		for (float D = 0.3f; D < 200.f; D *= 1.03f)
		{
			const int Stop = FocusStopFor(D);
			bMonotone = bMonotone && Stop >= Last;
			Last = Stop;
		}
		Expect(bMonotone, "a farther subject never sets a nearer stop");
		Expect(Last == FocusStops() - 1, "and ends at infinity");
	}

	// ---- sharpness
	const float Inf = FocusStop(FocusStops() - 1);
	Expect(Sharpness(2.f, 2.f, 0.f, 0.f) == 1.f, "in focus and still: sharp");
	Expect(Sharpness(Inf, Inf, 0.f, 0.f) == 1.f, "the horizon at infinity: sharp");
	for (int I = 0; I < FocusStops(); ++I)
	{
		Expect(Sharpness(FocusStop(I), FocusStop(I), 0.f, 0.f) == 1.f, "every stop is sharp at its own distance");
	}
	Expect(Sharpness(Inf, 30.f, 0.f, 0.f) >= 0.85f, "at infinity a subject at 30 m is sharp");
	Expect(Sharpness(Inf, 6.f, 0.f, 0.f) >= SharpEnough, "at infinity 6 m still counts");
	Expect(Sharpness(Inf, 5.f, 0.f, 0.f) < SharpEnough, "at infinity 5 m does not");
	Expect(Sharpness(Inf, 1.5f, 0.f, 0.f) < 0.1f, "at infinity 1.5 m is a blur");
	Expect(Sharpness(1.f, 30.f, 0.f, 0.f) < 0.1f, "focused at 1 m, 30 m is a blur");
	Expect(Sharpness(2.f, Inf, 0.f, 0.f) < 0.1f, "focused at 2 m, the horizon is a blur");
	Expect(Sharpness(2.f, 2.f, 0.f, 0.f) > Sharpness(3.f, 2.f, 0.f, 0.f) && Sharpness(3.f, 2.f, 0.f, 0.f) > Sharpness(5.f, 2.f, 0.f, 0.f), "the farther the ring from the bird, the softer");
	Expect(Sharpness(Inf, 2.f, 0.f, 0.f) < SharpEnough && Sharpness(10.f, 2.f, 0.f, 0.f) < SharpEnough && Sharpness(0.8f, 2.f, 0.f, 0.f) < SharpEnough, "a bird at 2 m needs the ring near 2 m");
	Expect(Sharpness(FocusStop(FocusStopFor(2.4f)), 2.4f, 0.f, 0.f) >= SharpEnough, "a careful hand gets a subject between stops");
	{
		bool bCareful = true;
		for (float D = 0.8f; D < 300.f; D *= 1.05f)
		{
			bCareful = bCareful && Sharpness(FocusStop(FocusStopFor(D)), D, 0.f, 0.f) >= SharpEnough;
		}
		Expect(bCareful, "from the nearest stop out, the nearest stop is always sharp enough");
	}
	const float Walking = Sharpness(2.f, 2.f, 1.4f, 0.f);
	Expect(Walking < 1.f && Near(Walking, 2.0 / 3.0, 0.05), "walking costs about a third");
	Expect(Walking >= SharpEnough, "a walking shot still counts");
	Expect(Sharpness(2.f, 2.f, 3.5f, 0.f) < SharpEnough && Sharpness(2.f, 2.f, 5.f, 0.f) < 0.05f, "running ruins it");
	Expect(Sharpness(2.f, 2.f, -1.4f, 0.f) == Walking, "backwards is walking too");
	Expect(Sharpness(2.f, 2.f, 0.f, 3.f) < Sharpness(30.f, 30.f, 0.f, 3.f), "a fast subject blurs more up close");
	Expect(Sharpness(2.f, 2.f, 0.f, 8.f) < 0.05f, "a bird flying past the nose is a streak");
	Expect(Sharpness(Inf, 40.f, 0.f, 8.f) >= SharpEnough, "a gull over the bay is caught");
	Expect(Sharpness(3.f, 3.f, 0.f, 1.4f) >= SharpEnough, "a passer-by at 3 m is caught");
	Expect(Sharpness(3.f, 3.f, 0.f, 1.f) > Sharpness(3.f, 3.f, 0.f, 2.f), "the faster, the softer");
	Expect(Sharpness(3.f, 3.f, 0.f, -2.f) == Sharpness(3.f, 3.f, 0.f, 2.f), "either way across the frame");
	{
		bool bInRange = true;
		const float Distances[] = { -1.f, 0.f, 0.05f, 0.8f, 2.f, 7.f, 100.f, 1e9f, 1e30f };
		const float Speeds[] = { -9.f, 0.f, 0.7f, 1.4f, 4.f, 50.f };
		for (const float F : Distances)
		{
			for (const float D : Distances)
			{
				for (const float H : Speeds)
				{
					for (const float V : Speeds)
					{
						const float Q = Sharpness(F, D, H, V);
						bInRange = bInRange && Q >= 0.f && Q <= 1.f;
					}
				}
			}
		}
		Expect(bInRange, "sharpness stays within 0..1 whatever is asked");
		Expect(Sharpness(NAN, 2.f, 0.f, 0.f) >= 0.f && Sharpness(2.f, 2.f, NAN, 0.f) == 0.f, "a NaN is no picture, not a crash");
	}

	// ---- the stamps' table
	Expect(StampFor("bird:Sparrow") == "photo_birds/Sparrow", "bird stamp");
	Expect(StampFor("bird:great_tit") == "photo_birds/great_tit", "bird stamp with an underscore");
	Expect(StampFor("sunset:clear") == "photo_sky/sunset_clear", "sunset stamp");
	Expect(StampFor("aurora") == "photo_sky/aurora", "aurora stamp");
	Expect(StampFor("festival:maslenitsa") == "photo_life/fest_maslenitsa", "festival stamp");
	Expect(StampFor("fish") == "photo_life/fish", "fish stamp");
	Expect(StampFor("maru") == "photo_life/maru", "maru stamp");
	Expect(StampFor("person:olga").empty(), "a person earns no stamp");
	Expect(StampFor("place:pier").empty(), "a place earns no stamp");
	Expect(StampFor("").empty() && StampFor("bird:").empty() && StampFor("sunset:").empty() && StampFor("festival:").empty(), "empty names earn nothing");
	Expect(StampFor("bird").empty() && StampFor("Aurora").empty() && StampFor("fishes").empty() && StampFor("aurora:x").empty() && StampFor("xbird:a").empty(), "near misses earn nothing");

	// ---- the darkroom's sums
	Expect(Baths == 3, "three baths");
	for (int B = 0; B < Baths; ++B)
	{
		const float T = BathIdeal(B);
		Expect(T > 0.f, "a bath takes time");
		Expect(BathQuality(B, T) == 1.f, "the ideal time is perfect");
		Expect(BathQuality(B, T * 1.09f) == 1.f && BathQuality(B, T * 0.91f) == 1.f, "within a tenth is perfect");
		Expect(BathQuality(B, T * 1.3f) < 1.f && BathQuality(B, T * 1.3f) > BathQuality(B, T * 1.6f), "the later, the worse");
		Expect(BathQuality(B, T * 0.7f) < 1.f && BathQuality(B, T * 0.7f) > BathQuality(B, T * 0.4f), "the sooner, the worse");
		Expect(Near(BathQuality(B, T * 1.55f), 0.5, 0.01), "55 % off is half");
		Expect(Near(BathQuality(B, T * 1.3f), BathQuality(B, T * 0.7f), 0.001), "over and under cost the same");
		Expect(BathQuality(B, 0.f) == 0.f && BathQuality(B, T * 2.f) < 0.001f && BathQuality(B, T * 5.f) == 0.f, "none or twice the time is nothing");
		Expect(BathQuality(B, -3.f) == 0.f, "negative time is nothing");
	}
	Expect(BathIdeal(-1) == BathIdeal(0) && BathIdeal(7) == BathIdeal(Baths - 1), "baths are clamped");
	Expect(BathIdeal(0) > BathIdeal(1), "the developer is longer than the stop bath");
	{
		float T[Baths] = { BathIdeal(0), BathIdeal(1), BathIdeal(2) };
		Expect(DarkroomQuality(T) == 1.f, "three good baths: quality 1");
		T[1] = BathIdeal(1) * 1.55f;
		Expect(Near(DarkroomQuality(T), 0.5, 0.01), "the worst bath decides");
		T[0] = BathIdeal(0) * 0.6f;
		Expect(Near(DarkroomQuality(T), 0.5, 0.01), "a better bad bath changes nothing");
		T[2] = BathIdeal(2) * 1.9f;
		Expect(DarkroomQuality(T) < RuinedBelow, "one bath forgotten ruins the film");
		T[2] = 0.f;
		Expect(DarkroomQuality(T) == 0.f, "a bath skipped: nothing");
	}

	// ---- the studio's clock
	Expect(StudioReadyAt(18.0) == 34.0, "handed in at 18:00: ready at 10:00 tomorrow");
	Expect(StudioReadyAt(10.0) == 34.0 && StudioReadyAt(12.5) == 34.0, "handed in in the morning or at noon: tomorrow too");
	Expect(StudioReadyAt(22.0) == 34.0, "at 22:00 there are just twelve hours left: tomorrow");
	Expect(StudioReadyAt(22.5) == 58.0, "after 22:00: a day later");
	Expect(StudioReadyAt(25.0) == 58.0, "past midnight at 01:00: not this morning, the next");
	Expect(StudioReadyAt(33.9) == 58.0, "just before 10:00: the next morning");
	Expect(StudioReadyAt(0.0) == 34.0, "the very first midnight");
	Expect(StudioReadyAt(24.0 * 40 + 15.0) == 24.0 * 41 + 10.0, "day 40");
	{
		bool bFair = true;
		for (double H = 0.0; H < 200.0; H += 0.37)
		{
			const double R = StudioReadyAt(H);
			bFair = bFair && R - H >= 12.0 && R - H < 36.0 && Near(std::fmod(R, 24.0), 10.0, 1e-9);
		}
		Expect(bFair, "always 10:00, twelve to thirty-six hours on");
	}

	// ---- a whole film
	{
		FState S;
		Expect(!S.HasFilm() && S.FramesLeft() == 0 && S.Rolls().empty() && S.Album().empty() && S.Stamps().empty() && S.ShotsTaken() == 0, "a new camera is empty");
		Expect(S.Shoot(Frame(1.0, "pier", 1.f)) == 0, "no film: no frame");
		Expect(!S.Rewind(), "no film: nothing to rewind");
		Expect(S.Exposed() == 0 && S.HandIn(5.0) == 0 && S.AtStudio() == 0 && S.Collect(100.0).Rolls.empty() && S.DevelopSelf(1.f).Rolls.empty(), "nothing to develop");
		Expect(S.Save() == "v1", "an empty state saves as v1");
		Expect(S.LoadFilm(), "a film goes in");
		Expect(!S.LoadFilm(), "a second one does not");
		Expect(S.HasFilm() && S.FramesLeft() == FramesPerFilm, "36 frames to go");
		Expect(S.Rolls().size() == 1 && S.Rolls()[0].Id == 1 && S.Rolls()[0].State == ERollState::InCamera, "roll 1 is in the camera");
		Expect(!S.Rewind(), "an untouched film is not rewound");
		Expect(S.HasFilm() && S.Exposed() == 0 && S.FramesLeft() == FramesPerFilm, "and stays in");
		for (int I = 1; I <= FramesPerFilm; ++I)
		{
			FShot Shot = Frame(8.0 + I * 0.1, "pier", 0.7f);
			Shot.Roll = 77;
			Shot.Frame = 99;
			Expect(S.Shoot(Shot) == I, "frames count from 1");
			Expect(S.FramesLeft() == FramesPerFilm - I, "and the counter runs down");
		}
		Expect(S.Shoot(Frame(20.0, "pier", 1.f)) == 0, "the 37th frame does not exist");
		Expect(S.ShotsTaken() == FramesPerFilm && S.Rolls()[0].Shots.size() == (size_t)FramesPerFilm, "36 kept");
		Expect(S.Rolls()[0].Shots[0].Roll == 1 && S.Rolls()[0].Shots[0].Frame == 1 && S.Rolls()[0].Shots[35].Frame == 36, "the camera numbers the frames itself");
		Expect(!S.LoadFilm(), "a full film is still in the way");
		Expect(S.Album().empty(), "nothing in the album before a development");
		Expect(S.Rewind(), "the full film rewinds");
		Expect(!S.HasFilm() && S.FramesLeft() == 0 && S.Exposed() == 1 && S.Rolls()[0].State == ERollState::Exposed, "and is in the pocket");
		Expect(!S.Rewind() && S.Shoot(Frame(21.0, "", 1.f)) == 0, "the camera is empty again");
		Expect(S.LoadFilm() && S.Rolls().size() == 2 && S.Rolls()[1].Id == 2 && S.FramesLeft() == FramesPerFilm, "the second film is roll 2");
		Expect(S.Shoot(Frame(22.0, "roof", 0.9f)) == 1 && S.Rolls()[1].Shots[0].Roll == 2, "its first frame");
		Expect(S.Rewind() && S.Exposed() == 2 && S.ShotsTaken() == FramesPerFilm + 1, "a film with one frame rewinds too");
		Expect(Sound(S), "the state is sound");
		// sharpness is held to 0..1 as it is kept
		Expect(S.LoadFilm(), "third film");
		FShot Wild = Frame(NAN, "", "maru", 7.f);
		Wild.Subjects[0].Sharp = -2.f;
		Expect(S.Shoot(Wild) == 1, "a wild frame is taken");
		const FShot& Kept = S.Rolls()[2].Shots[0];
		Expect(Kept.Sharp == 1.f && Kept.Subjects[0].Sharp == 0.f && Kept.Hours == 0.0, "and tamed");
	}

	// ---- the studio, the stamps, the album
	{
		FState S;
		S.LoadFilm();
		S.Shoot(Frame(15.0, "park", "bird:Sparrow", 0.9f));
		S.Shoot(Frame(15.1, "park", "bird:Crow", 0.3f));
		S.Shoot(Frame(15.2, "club", "maru", 0.8f));
		FShot Sunset = Frame(17.0, "pier", "sunset:clear", 0.7f);
		FSubject Olga;
		Olga.Tag = "person:olga";
		Olga.Sharp = 0.6f;
		Sunset.Subjects.push_back(Olga);
		FSubject Tower;
		Tower.Tag = "place:tower";
		Tower.Sharp = 0.1f;
		Sunset.Subjects.push_back(Tower);
		S.Shoot(Sunset);
		S.Shoot(Frame(17.5, "", 0.2f));
		Expect(S.HandIn(18.0) == 0, "a film in the camera is not handed in");
		Expect(S.Rewind(), "rewound");
		Expect(S.HandIn(18.0) == 1, "one roll handed in");
		Expect(S.HandIn(18.0) == 0, "and only once");
		Expect(S.Exposed() == 0 && S.AtStudio() == 1 && S.Rolls()[0].State == ERollState::AtStudio && S.Rolls()[0].ReadyAt == 34.0, "it is at the studio till 10:00");
		Expect(S.ReadyAtStudio(23.5) == 0 && S.ReadyAtStudio(33.99) == 0, "not ready the same evening or before ten");
		FDeveloped Early = S.Collect(23.5);
		Expect(Early.Rolls.empty() && Early.Prints == 0 && Early.Stamps.empty() && S.AtStudio() == 1 && S.Album().empty(), "collecting early brings nothing");
		Expect(S.DevelopSelf(1.f).Rolls.empty(), "the darkroom has nothing: the roll is at the studio");
		Expect(S.ReadyAtStudio(34.0) == 1, "ready at 10:00 the next day");
		FDeveloped D = S.Collect(34.0);
		Expect(D.Rolls.size() == 1 && D.Rolls[0] == 1, "roll 1 comes back");
		Expect(D.Prints == 5, "five prints");
		Expect(D.Stamps.size() == 3 && Has(D.Stamps, "photo_birds/Sparrow") && Has(D.Stamps, "photo_life/maru") && Has(D.Stamps, "photo_sky/sunset_clear"), "three stamps");
		Expect(D.Birds.size() == 1 && Has(D.Birds, "Sparrow"), "the sparrow is seen, the blurred crow is not");
		Expect(D.Places.size() == 4 && Has(D.Places, "park") && Has(D.Places, "club") && Has(D.Places, "pier") && Has(D.Places, "tower"), "the places, each once, the blurred tower too");
		Expect(D.Tags.size() == 4 && Has(D.Tags, "bird:Sparrow") && Has(D.Tags, "maru") && Has(D.Tags, "sunset:clear") && Has(D.Tags, "person:olga"), "the sharp tags");
		Expect(!Has(D.Tags, "bird:Crow") && !Has(D.Tags, "place:tower"), "blurred subjects are no tags");
		Expect(S.Stamps().size() == 3 && S.Stamps().count("photo_life/maru") == 1 && S.Stamps().count("photo_birds/Crow") == 0, "the stamps are owned");
		Expect(S.Rolls()[0].State == ERollState::Developed && S.Rolls()[0].Quality == 1.f, "studio prints are even");
		Expect(S.AtStudio() == 0 && S.ReadyAtStudio(99.0) == 0 && S.Collect(99.0).Rolls.empty(), "nothing left at the studio");
		Expect(S.Album().size() == 5 && S.Album()[0]->Frame == 1 && S.Album()[4]->Frame == 5 && S.Album()[3]->Place == "pier", "the album has the roll in order");

		// the second roll: the sparrow again, the crow sharp this time
		S.LoadFilm();
		S.Shoot(Frame(40.0, "park", "bird:Sparrow", 0.95f));
		S.Shoot(Frame(40.1, "yard", "bird:Crow", 0.5f));
		S.Shoot(Frame(40.2, "yard", "bird:Crow", 0.8f));
		S.Shoot(Frame(40.3, "club", "maru", 0.49f));
		S.Rewind();
		Expect(S.Album().size() == 5, "an undeveloped roll is not in the album");
		// handed in at 01:00 of day 2 (hour 49): the 10:00 of that day is too soon
		Expect(S.HandIn(49.0) == 1 && S.Rolls()[1].ReadyAt == 82.0, "handed in past midnight: ready the morning after next");
		Expect(S.ReadyAtStudio(58.0) == 0 && S.ReadyAtStudio(82.0) == 1, "not that morning");
		FDeveloped E = S.Collect(90.0);
		Expect(E.Rolls.size() == 1 && E.Rolls[0] == 2 && E.Prints == 4, "roll 2 comes back");
		Expect(E.Stamps.size() == 1 && E.Stamps[0] == "photo_birds/Crow", "only the crow's stamp is new");
		Expect(E.Birds.size() == 2 && Has(E.Birds, "Sparrow") && Has(E.Birds, "Crow"), "both birds are told to the bird book, each once");
		Expect(E.Tags.size() == 2 && Has(E.Tags, "bird:Sparrow") && Has(E.Tags, "bird:Crow"), "the sharp tags of roll 2");
		Expect(E.Places.size() == 3 && Has(E.Places, "club"), "a blurred frame's place still counts: a view is a view");
		Expect(S.Stamps().size() == 4, "four stamps owned");
		Expect(S.Album().size() == 9 && S.Album()[5]->Roll == 2 && S.Album()[5]->Frame == 1 && S.Album()[8]->Frame == 4, "the album goes on in the order taken");
		{
			bool bOrdered = true;
			const std::vector<const FShot*> Album = S.Album();
			for (size_t I = 1; I < Album.size(); ++I)
			{
				bOrdered = bOrdered && Album[I]->Hours > Album[I - 1]->Hours;
			}
			Expect(bOrdered, "by the hour too");
		}

		// two rolls at once, handed in at different hours
		S.LoadFilm();
		S.Shoot(Frame(100.0, "a", "fish", 0.9f));
		S.Rewind();
		Expect(S.HandIn(100.0) == 1, "roll 3 handed in on day 4");
		S.LoadFilm();
		S.Shoot(Frame(101.0, "b", "fish", 0.9f));
		S.Shoot(Frame(101.0, "b", "aurora", 0.9f));
		S.Rewind();
		S.LoadFilm();
		S.Shoot(Frame(102.0, "c", "festival:ivan_kupala", 0.9f));
		S.Rewind();
		Expect(S.Exposed() == 2 && S.HandIn(125.0) == 2 && S.AtStudio() == 3, "rolls 4 and 5 go a day later, together");
		Expect(S.ReadyAtStudio(130.0) == 1 && S.ReadyAtStudio(154.0) == 3, "one is ready sooner");
		FDeveloped F1 = S.Collect(130.0);
		Expect(F1.Rolls.size() == 1 && F1.Rolls[0] == 3 && F1.Stamps.size() == 1 && F1.Stamps[0] == "photo_life/fish", "roll 3 first");
		FDeveloped F2 = S.Collect(154.0);
		Expect(F2.Rolls.size() == 2 && F2.Rolls[0] == 4 && F2.Rolls[1] == 5 && F2.Prints == 3, "then 4 and 5");
		Expect(F2.Stamps.size() == 2 && Has(F2.Stamps, "photo_sky/aurora") && Has(F2.Stamps, "photo_life/fest_ivan_kupala"), "the fish is not new any more");
		Expect(Has(F2.Tags, "fish") && F2.Birds.empty(), "but it is still a sharp tag");
		Expect(S.Stamps().size() == 7 && S.Album().size() == 13 && Sound(S), "seven stamps, thirteen prints");
	}

	// ---- the hero's own darkroom
	{
		FState S;
		for (int R = 0; R < 4; ++R)
		{
			S.LoadFilm();
			S.Shoot(Frame(10.0 + R, "lab", R == 0 ? "bird:Owl" : (R == 1 ? "maru" : "fish"), 0.9f));
			S.Shoot(Frame(10.5 + R, "lab", 0.4f));
			S.Rewind();
		}
		Expect(S.Exposed() == 4, "four rolls in the pocket");
		FDeveloped Bad = S.DevelopSelf(0.1f);
		Expect(Bad.Rolls.size() == 1 && Bad.Rolls[0] == 1, "the oldest roll goes first");
		Expect(Bad.Prints == 0 && Bad.Stamps.empty() && Bad.Birds.empty() && Bad.Places.empty() && Bad.Tags.empty(), "a ruined film brings nothing");
		Expect(S.Rolls()[0].State == ERollState::Ruined && Near(S.Rolls()[0].Quality, 0.1), "it is ruined, with the quality it got");
		Expect(S.Album().empty() && S.Stamps().empty() && S.Exposed() == 3, "nothing in the album, no owl");
		FDeveloped Pale = S.DevelopSelf(0.6f);
		Expect(Pale.Rolls.size() == 1 && Pale.Rolls[0] == 2 && Pale.Prints == 2, "the next roll comes out");
		Expect(Pale.Stamps.size() == 1 && Pale.Stamps[0] == "photo_life/maru" && Has(Pale.Tags, "maru") && Has(Pale.Places, "lab"), "pale prints count all the same");
		Expect(S.Rolls()[1].State == ERollState::Developed && Near(S.Rolls()[1].Quality, 0.6), "developed at 0.6");
		Expect(S.Album().size() == 2 && S.Album()[0]->Roll == 2, "the album skips the ruined roll");
		FDeveloped Edge = S.DevelopSelf(RuinedBelow);
		Expect(Edge.Prints == 2 && S.Rolls()[2].State == ERollState::Developed && Edge.Stamps.size() == 1, "exactly at the edge it still comes out");
		FDeveloped Over = S.DevelopSelf(3.f);
		Expect(Over.Prints == 2 && S.Rolls()[3].Quality == 1.f && Over.Stamps.empty() && Has(Over.Tags, "fish"), "quality is held to 1; the fish was stamped before");
		Expect(S.DevelopSelf(1.f).Rolls.empty() && S.Exposed() == 0, "the pocket is empty");
		Expect(S.Album().size() == 6 && S.Stamps().size() == 2 && Sound(S), "six prints, two stamps");
		// the darkroom's own sums end in a ruined roll
		S.LoadFilm();
		S.Shoot(Frame(30.0, "lab", "aurora", 1.f));
		S.Rewind();
		const float Clumsy[Baths] = { BathIdeal(0) * 1.95f, BathIdeal(1), BathIdeal(2) };
		FDeveloped Lost = S.DevelopSelf(DarkroomQuality(Clumsy));
		Expect(Lost.Rolls.size() == 1 && Lost.Prints == 0 && S.Rolls()[4].State == ERollState::Ruined && S.Stamps().count("photo_sky/aurora") == 0, "a developer left twice too long loses the aurora");
		S.LoadFilm();
		S.Shoot(Frame(31.0, "lab", "aurora", 1.f));
		S.Rewind();
		const float Steady[Baths] = { BathIdeal(0) * 1.05f, BathIdeal(1) * 0.95f, BathIdeal(2) };
		FDeveloped Good = S.DevelopSelf(DarkroomQuality(Steady));
		Expect(Good.Prints == 1 && S.Rolls()[5].Quality == 1.f && S.Stamps().count("photo_sky/aurora") == 1, "a steady hand gets it the second time");
		Expect(S.DevelopSelf(NAN).Rolls.empty(), "nothing left for a NaN");
		S.LoadFilm();
		S.Shoot(Frame(32.0, "lab", "fish", 1.f));
		S.Rewind();
		Expect(S.DevelopSelf(NAN).Prints == 0 && S.Rolls()[6].State == ERollState::Ruined && S.Rolls()[6].Quality == 0.f, "a NaN quality ruins");
	}

	// ---- the save line
	{
		FState S;
		S.LoadFilm();
		S.Shoot(Frame(15.25, "park", "bird:Sparrow", 0.9f));
		FShot Odd = Frame(16.1234, "a|b;c,d:e=f%g\nh\ti", "bird:Odd|one;two,three:four=five%six\nseven", 0.777f);
		FSubject Empty;
		Empty.Sharp = 0.333f;
		Odd.Subjects.push_back(Empty);
		FSubject Cyr;
		Cyr.Tag = "person:\xD0\x9E\xD0\xBB\xD1\x8C\xD0\xB3\xD0\xB0";
		Cyr.Sharp = 1.f;
		Odd.Subjects.push_back(Cyr);
		S.Shoot(Odd);
		S.Shoot(Frame(16.5, "", 0.f));
		S.Rewind();
		S.DevelopSelf(0.8125f);
		S.LoadFilm();
		S.Shoot(Frame(20.0, "pier", "maru", 0.6f));
		S.Rewind();
		S.DevelopSelf(0.05f);
		S.LoadFilm();
		S.Shoot(Frame(21.0, "pier", "fish", 0.6f));
		S.Rewind();
		S.HandIn(21.5);
		S.LoadFilm();
		S.Shoot(Frame(22.0, "roof", "aurora", 0.6f));
		S.Rewind();
		S.LoadFilm();
		S.Shoot(Frame(23.0, "roof", "sunset:storm", 0.45f));
		Expect(S.Stamps().count("photo_birds/Odd|one;two,three:four=five%six\nseven") == 1, "an odd species makes an odd stamp");
		const std::string Line = S.Save();
		Expect(Line.compare(0, 3, "v1|") == 0, "the line starts with v1|");
		Expect(Line.find('\n') == std::string::npos && Line.find('\t') == std::string::npos && Line.find(';') == std::string::npos, "one line, no control characters, no semicolons");
		FState T;
		Expect(T.Restore(Line), "the save line reads back");
		Expect(T.Save() == Line, "and writes the same line");
		Expect(Same(S, T), "every field");
		Expect(T.Rolls().size() == 5 && T.Rolls()[0].State == ERollState::Developed && T.Rolls()[1].State == ERollState::Ruined && T.Rolls()[2].State == ERollState::AtStudio
			&& T.Rolls()[3].State == ERollState::Exposed && T.Rolls()[4].State == ERollState::InCamera, "all five states");
		Expect(T.Rolls()[0].Shots[1].Place == "a|b;c,d:e=f%g\nh\ti", "the odd place survives");
		Expect(T.Rolls()[0].Shots[1].Subjects.size() == 3 && T.Rolls()[0].Shots[1].Subjects[0].Tag == "bird:Odd|one;two,three:four=five%six\nseven"
			&& T.Rolls()[0].Shots[1].Subjects[1].Tag.empty() && T.Rolls()[0].Shots[1].Subjects[2].Tag == Cyr.Tag, "the odd tags survive");
		Expect(Near(T.Rolls()[0].Shots[1].Hours, 16.1234, 0.00006) && Near(T.Rolls()[0].Shots[1].Sharp, 0.777) && Near(T.Rolls()[0].Quality, 0.8125), "the numbers survive");
		Expect(T.Rolls()[2].ReadyAt == 34.0 && T.Stamps() == S.Stamps() && T.Stamps().size() == 2, "the studio's hour and the stamps");
		Expect(T.HasFilm() && T.FramesLeft() == FramesPerFilm - 1 && T.Exposed() == 1 && T.AtStudio() == 1 && T.Album().size() == 3 && T.ShotsTaken() == 7 && Sound(T), "the restored camera works on");
		// the restored state goes on exactly as the original
		FDeveloped A = S.Collect(40.0);
		FDeveloped B = T.Collect(40.0);
		Expect(A.Stamps == B.Stamps && A.Prints == B.Prints && A.Rolls == B.Rolls && S.Save() == T.Save(), "both collect the same");
		Expect(T.Shoot(Frame(24.0, "roof", 1.f)) == 2 && S.Shoot(Frame(24.0, "roof", 1.f)) == 2 && S.Save() == T.Save(), "both shoot the same frame");

		FState Blank;
		FState FromBlank;
		FromBlank.LoadFilm();
		Expect(FromBlank.Restore(Blank.Save()) && FromBlank.Rolls().empty() && !FromBlank.HasFilm(), "an empty state reads back empty");
		FState Fresh;
		Fresh.LoadFilm();
		FState FromFresh;
		Expect(FromFresh.Restore(Fresh.Save()) && FromFresh.HasFilm() && FromFresh.FramesLeft() == FramesPerFilm && !FromFresh.Rewind(), "an untouched film in the camera reads back");
		Expect(T.Restore("v1|roll=1:c:0.0000:1.000|future=1|shot=1:5.0000:0.500:x:"), "an unknown key is skipped");
		Expect(T.Rolls().size() == 1 && T.Rolls()[0].Shots.size() == 1 && T.Rolls()[0].Shots[0].Place == "x" && T.Stamps().empty(), "and the rest is read, the old state gone");
		Expect(T.Restore("v1|stamps=photo_life/maru,a%7Cb|roll=1:d:0:1|shot=1:5:0.5:p%3aq:maru=1,%3D=0"), "short numbers and small hex letters read");
		Expect(T.Stamps().count("a|b") == 1 && T.Rolls()[0].Shots[0].Place == "p:q" && T.Rolls()[0].Shots[0].Subjects[1].Tag == "=" && T.Album().size() == 1, "as written");

		const char* Garbage[] = {
			"", "v", "v2", "v2|roll=1:c:0.0000:1.000", " v1", "V1", "garbage", "v1|", "v1|junk", "v1||",
			"v1|roll=", "v1|roll=1:c", "v1|roll=1:c:0.0000", "v1|roll=1:c:0.0000:1.000:9",
			"v1|roll=2:c:0.0000:1.000", "v1|roll=0:c:0.0000:1.000", "v1|roll=-1:c:0.0000:1.000", "v1|roll=x:c:0.0000:1.000", "v1|roll=1 :c:0.0000:1.000",
			"v1|roll=1:x:0.0000:1.000", "v1|roll=1:cc:0.0000:1.000", "v1|roll=1::0.0000:1.000",
			"v1|roll=1:c:abc:1.000", "v1|roll=1:c:1e3:1.000", "v1|roll=1:c:0.:1.000", "v1|roll=1:c:.5:1.000", "v1|roll=1:c:0,5:1.000", "v1|roll=1:c::1.000",
			"v1|roll=1:c:0.0000:2.000", "v1|roll=1:c:0.0000:-0.100", "v1|roll=1:c:0.0000:nan", "v1|roll=1:c:0.0000:",
			"v1|roll=1:c:0.0000:1.000|roll=2:c:0.0000:1.000",
			"v1|roll=1:c:0.0000:1.000|roll=1:c:0.0000:1.000",
			"v1|roll=1:e:0.0000:1.000", "v1|roll=1:d:0.0000:1.000", "v1|roll=1:s:34.0000:1.000", "v1|roll=1:r:0.0000:0.100",
			"v1|roll=1:e:0.0000:1.000|roll=2:c:0.0000:1.000",
			"v1|shot=1:5.0000:0.500:x:",
			"v1|roll=1:c:0.0000:1.000|shot=2:5.0000:0.500:x:",
			"v1|roll=1:c:0.0000:1.000|shot=0:5.0000:0.500:x:",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:0.500:x:|shot=1:5.0000:0.500:x:",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:0.500:x:|shot=3:5.0000:0.500:x:",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:0.500:x",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:0.500:x:y:z",
			"v1|roll=1:c:0.0000:1.000|shot=1:five:0.500:x:",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:1.500:x:",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:sharp:x:",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:0.500:x%:",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:0.500:x%4:",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:0.500:x%zz:",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:0.500:x:maru",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:0.500:x:maru=",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:0.500:x:maru=2.000",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:0.500:x:maru=0.5=0.5",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:0.500:x:maru=0.500,",
			"v1|roll=1:c:0.0000:1.000|shot=1:5.0000:0.500:x:ma%ru=0.500",
			"v1|stamps=", "v1|stamps=a,,b", "v1|stamps=a%", "v1|stamps=a%G0",
			"v1|roll=1:c:0.0000:1.000\nv1",
		};
		for (const char* Bad : Garbage)
		{
			FState G;
			G.LoadFilm();
			G.Shoot(Frame(1.0, "x", "maru", 1.f));
			G.Rewind();
			G.DevelopSelf(1.f);
			const bool bRefused = !G.Restore(Bad);
			Expect(bRefused, Bad);
			Expect(G.Rolls().empty() && G.Stamps().empty() && !G.HasFilm() && G.Album().empty() && G.Save() == "v1", "and the state is left empty");
		}
		{
			// a film cannot hold a 37th frame
			std::string Long = "v1|roll=1:c:0.0000:1.000";
			for (int I = 1; I <= FramesPerFilm; ++I)
			{
				Long += "|shot=" + std::to_string(I) + ":1.0000:0.500::";
			}
			FState G;
			Expect(G.Restore(Long) && G.FramesLeft() == 0 && G.Shoot(Frame(2.0, "", 1.f)) == 0, "36 saved frames read back as a full film");
			Expect(!G.Restore(Long + "|shot=37:1.0000:0.500::") && G.Rolls().empty(), "37 are refused");
		}
	}

	// ---- a long random life with the camera
	{
		unsigned Seed = 20261120u;
		const auto Next = [&Seed]() -> unsigned
		{
			Seed = Seed * 1664525u + 1013904223u;
			return Seed >> 8;
		};
		const char* Tags[] = { "bird:Sparrow", "bird:Crow", "bird:great_tit", "bird:Owl", "bird:odd|name,with:all=of%it", "sunset:clear", "sunset:storm", "aurora",
			"festival:maslenitsa", "festival:city_day", "fish", "maru", "person:olga", "person:semyon", "place:tower", "place:pier", "", "junk" };
		const char* Places[] = { "", "pier", "park", "club", "roof", "an odd; place\nwith a newline", "\xD0\xBC\xD0\xBE\xD1\x81\xD1\x82" };
		const int TagCount = (int)(sizeof(Tags) / sizeof(Tags[0]));
		const int PlaceCount = (int)(sizeof(Places) / sizeof(Places[0]));
		FState S;
		double Now = 8.0;
		std::set<std::string> Given;
		int Shots = 0, Loads = 0, Rewinds = 0, Hands = 0, Collected = 0, Selfs = 0, Ruined = 0, Saves = 0, Prints = 0;
		bool bSound = true, bFrames = true, bStampsOnce = true, bStampsKnown = true, bAlbum = true, bFixed = true, bSame = true, bLists = true, bCounts = true;
		const auto Took = [&](const FDeveloped& D)
		{
			Prints += D.Prints;
			for (const std::string& Stamp : D.Stamps)
			{
				bStampsOnce = bStampsOnce && Given.insert(Stamp).second;
			}
			for (const std::string& Bird : D.Birds)
			{
				bLists = bLists && !Bird.empty() && Has(D.Birds, Bird.c_str()) && Has(D.Tags, ("bird:" + Bird).c_str());
			}
			for (const std::string& Tag : D.Tags)
			{
				bLists = bLists && Has(D.Tags, Tag.c_str());
			}
			for (const std::string& Place : D.Places)
			{
				bLists = bLists && !Place.empty() && Has(D.Places, Place.c_str());
			}
		};
		for (int Step = 0; Step < 6000; ++Step)
		{
			Now += (Next() % 100) * 0.013;
			const unsigned Op = Next() % 100;
			if (Op < 62)
			{
				FShot Shot = Frame(Now, Places[Next() % (unsigned)PlaceCount], (Next() % 1001) / 1000.f);
				const int Subjects = (int)(Next() % 4);
				for (int I = 0; I < Subjects; ++I)
				{
					FSubject Subject;
					Subject.Tag = Tags[Next() % (unsigned)TagCount];
					Subject.Sharp = (Next() % 1201) / 1000.f - 0.1f;
					Shot.Subjects.push_back(Subject);
				}
				const int Left = S.FramesLeft();
				const int Got = S.Shoot(Shot);
				bFrames = bFrames && (Left > 0 ? (Got == FramesPerFilm - Left + 1 && S.FramesLeft() == Left - 1) : Got == 0) && Got <= FramesPerFilm;
				Shots += Got > 0 ? 1 : 0;
			}
			else if (Op < 72)
			{
				const bool bHad = S.HasFilm();
				const bool bLoaded = S.LoadFilm();
				bCounts = bCounts && bLoaded == !bHad && S.HasFilm();
				Loads += bLoaded ? 1 : 0;
			}
			else if (Op < 80)
			{
				const int Left = S.FramesLeft();
				const bool bHad = S.HasFilm();
				const int Pocket = S.Exposed();
				const bool bOut = S.Rewind();
				bCounts = bCounts && bOut == (bHad && Left < FramesPerFilm) && S.Exposed() == Pocket + (bOut ? 1 : 0) && S.HasFilm() == (bHad && !bOut);
				Rewinds += bOut ? 1 : 0;
			}
			else if (Op < 85)
			{
				const int Pocket = S.Exposed();
				const int Studio = S.AtStudio();
				const int ReadyBefore = S.ReadyAtStudio(Now);
				const int Count = S.HandIn(Now);
				bCounts = bCounts && Count == Pocket && S.Exposed() == 0 && S.AtStudio() == Studio + Count;
				for (const FRoll& Roll : S.Rolls())
				{
					// whatever is at the studio was promised for some 10:00, and what went in just now is not ready for twelve hours
					bCounts = bCounts && (Roll.State != ERollState::AtStudio || (Near(std::fmod(Roll.ReadyAt, 24.0), 10.0, 0.001) && Roll.ReadyAt < Now + 36.0));
				}
				bCounts = bCounts && S.ReadyAtStudio(Now) == ReadyBefore && S.ReadyAtStudio(Now + 11.99) - ReadyBefore <= Studio - ReadyBefore;
				Hands += Count;
			}
			else if (Op < 91)
			{
				const int Ready = S.ReadyAtStudio(Now);
				const int Studio = S.AtStudio();
				const size_t Album = S.Album().size();
				const FDeveloped D = S.Collect(Now);
				bCounts = bCounts && (int)D.Rolls.size() == Ready && S.AtStudio() == Studio - Ready && S.ReadyAtStudio(Now) == 0 && S.Album().size() == Album + (size_t)D.Prints;
				Collected += Ready;
				Took(D);
			}
			else if (Op < 97)
			{
				const int Pocket = S.Exposed();
				const size_t Album = S.Album().size();
				const size_t Stamps = S.Stamps().size();
				const float Quality = (Next() % 1000) / 999.f;
				const FDeveloped D = S.DevelopSelf(Quality);
				const bool bRuined = Quality < RuinedBelow;
				bCounts = bCounts && (int)D.Rolls.size() == (Pocket > 0 ? 1 : 0) && S.Exposed() == Pocket - (int)D.Rolls.size() && S.Album().size() == Album + (size_t)D.Prints;
				if (Pocket > 0 && bRuined)
				{
					bCounts = bCounts && D.Prints == 0 && D.Stamps.empty() && D.Birds.empty() && D.Places.empty() && D.Tags.empty() && S.Stamps().size() == Stamps
						&& S.Rolls()[(size_t)D.Rolls[0] - 1].State == ERollState::Ruined;
					++Ruined;
				}
				else if (Pocket > 0)
				{
					bCounts = bCounts && D.Prints > 0 && S.Rolls()[(size_t)D.Rolls[0] - 1].State == ERollState::Developed && S.Stamps().size() == Stamps + D.Stamps.size();
				}
				Selfs += (int)D.Rolls.size();
				Took(D);
			}
			else
			{
				const std::string Line = S.Save();
				FState T;
				const bool bRead = T.Restore(Line);
				bFixed = bFixed && bRead && T.Save() == Line && Line.find('\n') == std::string::npos;
				bSame = bSame && Same(S, T) && Sound(T);
				// the game goes on from what was read, as after a load
				S = T;
				++Saves;
			}
			bSound = bSound && Sound(S);
			bStampsKnown = bStampsKnown && S.Stamps() == Given;
			if (Step % 97 == 0)
			{
				int Developed = 0;
				for (const FRoll& Roll : S.Rolls())
				{
					Developed += Roll.State == ERollState::Developed ? (int)Roll.Shots.size() : 0;
				}
				bAlbum = bAlbum && (int)S.Album().size() == Developed && Developed == Prints;
			}
			if (Step % 500 == 499)
			{
				Expect(bSound, "random: the state stays sound");
				Expect(bFrames, "random: frames never pass 36");
				Expect(bStampsOnce && bStampsKnown, "random: a stamp is given once, and the owned ones are those given");
				Expect(bAlbum, "random: the album is the developed rolls' frames");
				Expect(bFixed && bSame, "random: save, restore, save is a fixed point");
				Expect(bLists && bCounts, "random: the counts and the lists add up");
			}
		}
		Expect(S.ShotsTaken() == Shots && (int)S.Rolls().size() == Loads, "random: every shot and every film is kept");
		Expect((int)S.Album().size() == Prints, "random: the prints handed out are the album");
		Expect(Shots > 2000 && Loads > 80 && Rewinds > 80 && Hands > 20 && Collected > 20 && Selfs > 20 && Ruined > 5 && Saves > 100, "random: the run did a bit of everything");
		Expect(Given.size() >= 11, "random: every stamp of the pool was earned");
		for (const std::string& Stamp : Given)
		{
			Expect(Stamp.compare(0, 6, "photo_") == 0 && Stamp.find('/') != std::string::npos, "random: a stamp is set/piece");
		}
	}

	std::printf("photo: %d checks, %d failed\n", GChecks, GFailed);
	return GFailed ? 1 : 0;
}
