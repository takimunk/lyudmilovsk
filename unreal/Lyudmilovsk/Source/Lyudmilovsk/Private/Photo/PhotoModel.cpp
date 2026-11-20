#include "PhotoModel.h"

#include <cmath>

namespace PhotoModel
{
	namespace PhotoDetail
	{
		/** The ring's clicks, metres; the last is infinity. */
		constexpr int StopCount = 9;
		constexpr float Stops[StopCount] = { 0.8f, 1.f, 1.2f, 1.5f, 2.f, 3.f, 5.f, 10.f, 1e9f };
		/** Dioptres of focus error at which a 50 mm lens at f/8 gives nothing sharp any more. */
		constexpr float FieldDioptres = 0.35f;
		/** The hero's speed, m/s, at which the slow shutter smears everything (walking 1.4 costs a third). */
		constexpr float HeroBlurSpeed = 4.2f;
		/** The subject's speed across the frame, rad/s, at which it is only a streak. */
		constexpr float SubjectBlurRate = 1.5f;
		constexpr float BathSeconds[Baths] = { 12.f, 4.f, 8.f };

		float Clamp01(float Value)
		{
			// a NaN fails both comparisons of a plain clamp: it is nothing sharp
			if (!(Value > 0.f))
			{
				return 0.f;
			}
			return Value > 1.f ? 1.f : Value;
		}

		/** 1/distance with the nonsense (zero, negative, NaN) held at 10 cm. */
		float Dioptres(float DistanceM)
		{
			return 1.f / (DistanceM > 0.1f ? DistanceM : 0.1f);
		}

		bool StartsWith(const std::string& Text, const char* Prefix, std::string& Rest)
		{
			const std::string P(Prefix);
			if (Text.size() <= P.size() || Text.compare(0, P.size(), P) != 0)
			{
				return false;
			}
			Rest = Text.substr(P.size());
			return true;
		}

		void AddOnce(std::vector<std::string>& List, const std::string& Item)
		{
			for (const std::string& Have : List)
			{
				if (Have == Item)
				{
					return;
				}
			}
			List.push_back(Item);
		}

		std::vector<std::string> Split(const std::string& Text, char Sep)
		{
			std::vector<std::string> Out;
			size_t Start = 0;
			for (;;)
			{
				const size_t At = Text.find(Sep, Start);
				if (At == std::string::npos)
				{
					Out.push_back(Text.substr(Start));
					return Out;
				}
				Out.push_back(Text.substr(Start, At - Start));
				Start = At + 1;
			}
		}

		/** The save line's separators and anything that would break a line, as %XX. */
		std::string Escape(const std::string& Text)
		{
			const char* const Hex = "0123456789ABCDEF";
			std::string Out;
			for (const char C : Text)
			{
				const unsigned char U = (unsigned char)C;
				if (U < 0x20 || U == 0x7F || C == '|' || C == ',' || C == ':' || C == '=' || C == ';' || C == '%')
				{
					Out += '%';
					Out += Hex[U >> 4];
					Out += Hex[U & 15];
				}
				else
				{
					Out += C;
				}
			}
			return Out;
		}

		int HexDigit(char C)
		{
			if (C >= '0' && C <= '9')
			{
				return C - '0';
			}
			if (C >= 'A' && C <= 'F')
			{
				return C - 'A' + 10;
			}
			if (C >= 'a' && C <= 'f')
			{
				return C - 'a' + 10;
			}
			return -1;
		}

		bool Unescape(const std::string& Text, std::string& Out)
		{
			Out.clear();
			for (size_t I = 0; I < Text.size(); ++I)
			{
				if (Text[I] != '%')
				{
					Out += Text[I];
					continue;
				}
				if (I + 2 >= Text.size())
				{
					return false;
				}
				const int Hi = HexDigit(Text[I + 1]);
				const int Lo = HexDigit(Text[I + 2]);
				if (Hi < 0 || Lo < 0)
				{
					return false;
				}
				Out += (char)(Hi * 16 + Lo);
				I += 2;
			}
			return true;
		}

		bool ToInt(const std::string& Text, int& Out)
		{
			if (Text.empty() || Text.size() > 9)
			{
				return false;
			}
			int Value = 0;
			for (const char C : Text)
			{
				if (C < '0' || C > '9')
				{
					return false;
				}
				Value = Value * 10 + (C - '0');
			}
			Out = Value;
			return true;
		}

		/** A number with a fixed count of decimals, written by hand: the save must not depend on the machine's locale. */
		std::string Fixed(double Value, int Decimals)
		{
			double Scale = 1.0;
			for (int I = 0; I < Decimals; ++I)
			{
				Scale *= 10.0;
			}
			if (!(Value == Value))
			{
				Value = 0.0;
			}
			Value = Value > 1e9 ? 1e9 : (Value < -1e9 ? -1e9 : Value);
			long long N = std::llround(Value * Scale);
			std::string Out;
			if (N < 0)
			{
				Out += '-';
				N = -N;
			}
			const long long Unit = (long long)Scale;
			Out += std::to_string(N / Unit);
			if (Decimals > 0)
			{
				const std::string Frac = std::to_string(N % Unit);
				Out += '.';
				Out.append((size_t)Decimals - Frac.size(), '0');
				Out += Frac;
			}
			return Out;
		}

		/** Reads what Fixed wrote: [-]digits[.digits]. */
		bool ToNumber(const std::string& Text, double& Out)
		{
			size_t I = (!Text.empty() && Text[0] == '-') ? 1 : 0;
			const bool bNegative = I == 1;
			double Value = 0.0;
			size_t Digits = 0;
			for (; I < Text.size() && Text[I] >= '0' && Text[I] <= '9'; ++I, ++Digits)
			{
				Value = Value * 10.0 + (Text[I] - '0');
			}
			if (Digits == 0 || Digits > 12)
			{
				return false;
			}
			if (I < Text.size())
			{
				if (Text[I] != '.')
				{
					return false;
				}
				++I;
				double Unit = 1.0;
				size_t Fracs = 0;
				for (; I < Text.size() && Text[I] >= '0' && Text[I] <= '9'; ++I, ++Fracs)
				{
					Unit /= 10.0;
					Value += (Text[I] - '0') * Unit;
				}
				if (Fracs == 0 || Fracs > 9 || I != Text.size())
				{
					return false;
				}
			}
			Out = bNegative ? -Value : Value;
			return true;
		}

		/** A sharpness or a quality of the save line: a number within 0..1. */
		bool ToUnit(const std::string& Text, float& Out)
		{
			double Value = 0.0;
			if (!ToNumber(Text, Value) || Value < 0.0 || Value > 1.0)
			{
				return false;
			}
			Out = (float)Value;
			return true;
		}

		char StateLetter(ERollState State)
		{
			switch (State)
			{
			case ERollState::InCamera: return 'c';
			case ERollState::Exposed: return 'e';
			case ERollState::AtStudio: return 's';
			case ERollState::Developed: return 'd';
			default: return 'r';
			}
		}

		bool StateOf(const std::string& Text, ERollState& Out)
		{
			if (Text.size() != 1)
			{
				return false;
			}
			switch (Text[0])
			{
			case 'c': Out = ERollState::InCamera; return true;
			case 'e': Out = ERollState::Exposed; return true;
			case 's': Out = ERollState::AtStudio; return true;
			case 'd': Out = ERollState::Developed; return true;
			case 'r': Out = ERollState::Ruined; return true;
			default: return false;
			}
		}

		/** "frame:hours:sharp:place:tag=sharp,..." of the save line; the frame must be the roll's next one. */
		bool ReadShot(const std::string& Value, FRoll& Roll)
		{
			const std::vector<std::string> F = Split(Value, ':');
			FShot Shot;
			if (F.size() != 5 || !ToInt(F[0], Shot.Frame) || !ToNumber(F[1], Shot.Hours) || !ToUnit(F[2], Shot.Sharp) || !Unescape(F[3], Shot.Place))
			{
				return false;
			}
			if ((int)Roll.Shots.size() >= FramesPerFilm || Shot.Frame != (int)Roll.Shots.size() + 1)
			{
				return false;
			}
			Shot.Roll = Roll.Id;
			if (!F[4].empty())
			{
				for (const std::string& Item : Split(F[4], ','))
				{
					const size_t Eq = Item.find('=');
					FSubject Subject;
					if (Eq == std::string::npos || !Unescape(Item.substr(0, Eq), Subject.Tag) || !ToUnit(Item.substr(Eq + 1), Subject.Sharp))
					{
						return false;
					}
					Shot.Subjects.push_back(Subject);
				}
			}
			Roll.Shots.push_back(Shot);
			return true;
		}
	}

	int FocusStops()
	{
		return PhotoDetail::StopCount;
	}

	float FocusStop(int Index)
	{
		return PhotoDetail::Stops[Index < 0 ? 0 : (Index >= PhotoDetail::StopCount ? PhotoDetail::StopCount - 1 : Index)];
	}

	int FocusStopFor(float DistanceM)
	{
		// what is out of focus is measured in dioptres, so the nearest stop is too: 7 m is nearer to 10 m than to 5 m
		const float Want = PhotoDetail::Dioptres(DistanceM);
		int Best = 0;
		float BestError = 1e9f;
		for (int I = 0; I < PhotoDetail::StopCount; ++I)
		{
			const float Error = std::fabs(PhotoDetail::Dioptres(PhotoDetail::Stops[I]) - Want);
			if (Error < BestError)
			{
				BestError = Error;
				Best = I;
			}
		}
		return Best;
	}

	float Sharpness(float FocusM, float SubjectM, float HeroSpeed, float SubjectSpeed)
	{
		const float Focus = PhotoDetail::Clamp01(1.f - std::fabs(PhotoDetail::Dioptres(FocusM) - PhotoDetail::Dioptres(SubjectM)) / PhotoDetail::FieldDioptres);
		const float Hand = PhotoDetail::Clamp01(1.f - std::fabs(HeroSpeed) / PhotoDetail::HeroBlurSpeed);
		// the same metres a second cross more of the frame up close: a sparrow at arm's length, a gull over the bay
		const float Across = std::fabs(SubjectSpeed) * PhotoDetail::Dioptres(SubjectM > 0.5f ? SubjectM : 0.5f);
		const float Motion = PhotoDetail::Clamp01(1.f - Across / PhotoDetail::SubjectBlurRate);
		return PhotoDetail::Clamp01(Focus * Hand * Motion);
	}

	std::string StampFor(const std::string& Tag)
	{
		std::string Rest;
		if (PhotoDetail::StartsWith(Tag, "bird:", Rest))
		{
			return "photo_birds/" + Rest;
		}
		if (PhotoDetail::StartsWith(Tag, "sunset:", Rest))
		{
			return "photo_sky/sunset_" + Rest;
		}
		if (PhotoDetail::StartsWith(Tag, "festival:", Rest))
		{
			return "photo_life/fest_" + Rest;
		}
		if (Tag == "aurora")
		{
			return "photo_sky/aurora";
		}
		if (Tag == "fish" || Tag == "maru")
		{
			return "photo_life/" + Tag;
		}
		return std::string();
	}

	float BathIdeal(int Bath)
	{
		return PhotoDetail::BathSeconds[Bath < 0 ? 0 : (Bath >= Baths ? Baths - 1 : Bath)];
	}

	float BathQuality(int Bath, float Seconds)
	{
		const float Ideal = BathIdeal(Bath);
		const float Error = std::fabs(Seconds - Ideal) / Ideal;
		// a tenth off either way is a steady hand; from there it pales (or blackens) evenly down to nothing at twice the time or none
		return PhotoDetail::Clamp01(1.f - (Error - 0.1f) / 0.9f);
	}

	float DarkroomQuality(const float Seconds[Baths])
	{
		float Worst = 1.f;
		for (int I = 0; I < Baths; ++I)
		{
			const float Q = BathQuality(I, Seconds[I]);
			Worst = Q < Worst ? Q : Worst;
		}
		return Worst;
	}

	double StudioReadyAt(double NowHours)
	{
		const double Earliest = NowHours + 12.0;
		double Ready = std::floor(Earliest / 24.0) * 24.0 + 10.0;
		if (Ready < Earliest)
		{
			Ready += 24.0;
		}
		return Ready;
	}

	int FState::CameraIndex() const
	{
		return (!AllRolls.empty() && AllRolls.back().State == ERollState::InCamera) ? (int)AllRolls.size() - 1 : -1;
	}

	int FState::CountOf(ERollState State) const
	{
		int Count = 0;
		for (const FRoll& Roll : AllRolls)
		{
			Count += Roll.State == State ? 1 : 0;
		}
		return Count;
	}

	bool FState::LoadFilm()
	{
		if (HasFilm())
		{
			return false;
		}
		FRoll Roll;
		Roll.Id = (int)AllRolls.size() + 1;
		AllRolls.push_back(Roll);
		return true;
	}

	bool FState::HasFilm() const
	{
		return CameraIndex() >= 0;
	}

	int FState::FramesLeft() const
	{
		const int At = CameraIndex();
		return At < 0 ? 0 : FramesPerFilm - (int)AllRolls[(size_t)At].Shots.size();
	}

	int FState::Shoot(FShot Shot)
	{
		if (FramesLeft() <= 0)
		{
			return 0;
		}
		FRoll& Roll = AllRolls.back();
		Shot.Roll = Roll.Id;
		Shot.Frame = (int)Roll.Shots.size() + 1;
		Shot.Sharp = PhotoDetail::Clamp01(Shot.Sharp);
		if (!(Shot.Hours == Shot.Hours))
		{
			Shot.Hours = 0.0;
		}
		for (FSubject& Subject : Shot.Subjects)
		{
			Subject.Sharp = PhotoDetail::Clamp01(Subject.Sharp);
		}
		Roll.Shots.push_back(Shot);
		return Shot.Frame;
	}

	bool FState::Rewind()
	{
		const int At = CameraIndex();
		if (At < 0 || AllRolls[(size_t)At].Shots.empty())
		{
			return false;
		}
		AllRolls[(size_t)At].State = ERollState::Exposed;
		return true;
	}

	int FState::Exposed() const
	{
		return CountOf(ERollState::Exposed);
	}

	int FState::HandIn(double NowHours)
	{
		int Count = 0;
		for (FRoll& Roll : AllRolls)
		{
			if (Roll.State == ERollState::Exposed)
			{
				Roll.State = ERollState::AtStudio;
				Roll.ReadyAt = StudioReadyAt(NowHours);
				++Count;
			}
		}
		return Count;
	}

	int FState::AtStudio() const
	{
		return CountOf(ERollState::AtStudio);
	}

	int FState::ReadyAtStudio(double NowHours) const
	{
		int Count = 0;
		for (const FRoll& Roll : AllRolls)
		{
			Count += (Roll.State == ERollState::AtStudio && Roll.ReadyAt <= NowHours) ? 1 : 0;
		}
		return Count;
	}

	void FState::Develop(FRoll& Roll, float Quality, FDeveloped& Out)
	{
		Out.Rolls.push_back(Roll.Id);
		Roll.Quality = PhotoDetail::Clamp01(Quality);
		if (Roll.Quality < RuinedBelow)
		{
			Roll.State = ERollState::Ruined;
			return;
		}
		Roll.State = ERollState::Developed;
		Out.Prints += (int)Roll.Shots.size();
		for (const FShot& Shot : Roll.Shots)
		{
			if (!Shot.Place.empty())
			{
				PhotoDetail::AddOnce(Out.Places, Shot.Place);
			}
			for (const FSubject& Subject : Shot.Subjects)
			{
				std::string Rest;
				if (PhotoDetail::StartsWith(Subject.Tag, "place:", Rest))
				{
					PhotoDetail::AddOnce(Out.Places, Rest);
				}
				if (Subject.Sharp < SharpEnough)
				{
					continue;
				}
				PhotoDetail::AddOnce(Out.Tags, Subject.Tag);
				// the bird book decides what is new to it: a species is told every time, a stamp only once
				if (PhotoDetail::StartsWith(Subject.Tag, "bird:", Rest))
				{
					PhotoDetail::AddOnce(Out.Birds, Rest);
				}
				const std::string Stamp = StampFor(Subject.Tag);
				if (!Stamp.empty() && Owned.insert(Stamp).second)
				{
					Out.Stamps.push_back(Stamp);
				}
			}
		}
	}

	FDeveloped FState::Collect(double NowHours)
	{
		FDeveloped Out;
		for (FRoll& Roll : AllRolls)
		{
			if (Roll.State == ERollState::AtStudio && Roll.ReadyAt <= NowHours)
			{
				Develop(Roll, 1.f, Out);
			}
		}
		return Out;
	}

	FDeveloped FState::DevelopSelf(float Quality)
	{
		FDeveloped Out;
		for (FRoll& Roll : AllRolls)
		{
			if (Roll.State == ERollState::Exposed)
			{
				Develop(Roll, Quality, Out);
				break;
			}
		}
		return Out;
	}

	const std::vector<FRoll>& FState::Rolls() const
	{
		return AllRolls;
	}

	std::vector<const FShot*> FState::Album() const
	{
		std::vector<const FShot*> Out;
		for (const FRoll& Roll : AllRolls)
		{
			if (Roll.State != ERollState::Developed)
			{
				continue;
			}
			for (const FShot& Shot : Roll.Shots)
			{
				Out.push_back(&Shot);
			}
		}
		return Out;
	}

	const std::set<std::string>& FState::Stamps() const
	{
		return Owned;
	}

	int FState::ShotsTaken() const
	{
		int Count = 0;
		for (const FRoll& Roll : AllRolls)
		{
			Count += (int)Roll.Shots.size();
		}
		return Count;
	}

	std::string FState::Save() const
	{
		std::string Out = "v1";
		if (!Owned.empty())
		{
			Out += "|stamps=";
			bool bFirst = true;
			for (const std::string& Stamp : Owned)
			{
				Out += (bFirst ? "" : ",") + PhotoDetail::Escape(Stamp);
				bFirst = false;
			}
		}
		for (const FRoll& Roll : AllRolls)
		{
			Out += "|roll=" + std::to_string(Roll.Id) + ":" + PhotoDetail::StateLetter(Roll.State) + ":" + PhotoDetail::Fixed(Roll.ReadyAt, 4) + ":" + PhotoDetail::Fixed(Roll.Quality, 3);
			for (const FShot& Shot : Roll.Shots)
			{
				Out += "|shot=" + std::to_string(Shot.Frame) + ":" + PhotoDetail::Fixed(Shot.Hours, 4) + ":" + PhotoDetail::Fixed(Shot.Sharp, 3) + ":" + PhotoDetail::Escape(Shot.Place) + ":";
				bool bFirst = true;
				for (const FSubject& Subject : Shot.Subjects)
				{
					Out += (bFirst ? "" : ",") + PhotoDetail::Escape(Subject.Tag) + "=" + PhotoDetail::Fixed(Subject.Sharp, 3);
					bFirst = false;
				}
			}
		}
		return Out;
	}

	bool FState::Restore(const std::string& Line)
	{
		// whatever goes wrong, the camera is left empty: half a film read from a broken line would be worse than none
		AllRolls.clear();
		Owned.clear();
		FState Next;
		const std::vector<std::string> Parts = PhotoDetail::Split(Line, '|');
		if (Parts[0] != "v1")
		{
			return false;
		}
		for (size_t I = 1; I < Parts.size(); ++I)
		{
			const size_t Eq = Parts[I].find('=');
			if (Eq == std::string::npos)
			{
				return false;
			}
			const std::string Key = Parts[I].substr(0, Eq);
			const std::string Value = Parts[I].substr(Eq + 1);
			if (Key == "stamps")
			{
				for (const std::string& Item : PhotoDetail::Split(Value, ','))
				{
					std::string Stamp;
					if (Item.empty() || !PhotoDetail::Unescape(Item, Stamp))
					{
						return false;
					}
					Next.Owned.insert(Stamp);
				}
			}
			else if (Key == "roll")
			{
				const std::vector<std::string> F = PhotoDetail::Split(Value, ':');
				FRoll Roll;
				// ids count from 1 with no gaps, and a film can only be loaded once the one before it is out
				if (F.size() != 4 || !PhotoDetail::ToInt(F[0], Roll.Id) || Roll.Id != (int)Next.AllRolls.size() + 1 || !PhotoDetail::StateOf(F[1], Roll.State)
					|| !PhotoDetail::ToNumber(F[2], Roll.ReadyAt) || !PhotoDetail::ToUnit(F[3], Roll.Quality) || Next.HasFilm())
				{
					return false;
				}
				Next.AllRolls.push_back(Roll);
			}
			else if (Key == "shot")
			{
				if (Next.AllRolls.empty() || !PhotoDetail::ReadShot(Value, Next.AllRolls.back()))
				{
					return false;
				}
			}
		}
		for (const FRoll& Roll : Next.AllRolls)
		{
			// only the film in the camera may be untouched: Rewind never lets an empty one out
			if (Roll.State != ERollState::InCamera && Roll.Shots.empty())
			{
				return false;
			}
		}
		AllRolls = Next.AllRolls;
		Owned = Next.Owned;
		return true;
	}
}
