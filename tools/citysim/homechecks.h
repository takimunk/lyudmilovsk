// Things to open and pick up in the homes (docs/city_interiors.md "Things to open and pick up") on the real data: wardrobes
// and cupboards say what is inside, the kettle gives tea once a day, each home's photo is picked up and then turned over
// (its back is the second line), the note on the kitchen table is the same all day and another the next day. Included by
// citytest.cpp (its CHECK, ReadFile).

inline void HomeTouchChecks(const std::string& Path)
{
	std::string Text, Error, Ru;
	if (!ReadFile(Path, Text))
	{
		return;
	}
	std::printf("things to open and pick up in the homes (real data)\n");
	FModel M;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	ReadFile("unreal/Lyudmilovsk/Content/Localization/city/strings_ru.json", Ru);
	auto Has = [&Ru](const std::string& Key) { return !Key.empty() && Ru.find("\"" + Key + "\"") != std::string::npos; };
	int Wardrobes = 0, Cupboards = 0, Photos = 0, Notes = 0, Silent = 0;
	for (int I = 0; I < (int)M.Things.size(); ++I)
	{
		const std::string What = M.Things[(size_t)I].Prop("what");
		const FAct* A = M.ActOf(I);
		if (What == "wardrobe" || What == "cupboard" || What == "photo" || What == "note")
		{
			Wardrobes += What == "wardrobe";
			Cupboards += What == "cupboard";
			Photos += What == "photo";
			Notes += What == "note";
			Silent += A && !A->Lines.empty() ? 0 : 1;
		}
	}
	CHECK(Wardrobes >= 3 && Cupboards >= 4 && Photos >= 4 && Notes >= 4 && Silent == 0,
		"%d wardrobes, %d cupboards, %d photos, %d notes in the homes, each with its lines (%d without)", Wardrobes, Cupboards, Photos, Notes, Silent);
	FWhen W;
	W.Day = 1000;
	W.Since = 3;
	W.Hour = 14.f;
	W.Weekday = 2;
	W.MonthDay = 1015;
	W.Weather = WeatherClear;
	for (const char* Home : { "P-T1H2/flat", "P-T4H1/flat", "P-SERPENTINE/bauer", "P-UNITE/tul" })
	{
		const int Photo = M.FindThing(std::string(Home) + "_photo");
		const int Note = M.FindThing(std::string(Home) + "_note");
		if (Photo < 0 || Note < 0)
		{
			CHECK(false, "%s has a photo and a note", Home);
			continue;
		}
		// the photo: picked up (the front), turned over (the back), then the front again
		const std::string Front = M.NextLine(Photo, W), Back = M.NextLine(Photo, W), Again = M.NextLine(Photo, W);
		CHECK(Has(Front) && Has(Back) && Front != Back && Again == Front && Back.find(".2") == Back.size() - 2,
			"%s: the photo's front, then its back, then the front again (%s, %s)", Home, Front.c_str(), Back.c_str());
		// the note: one all day, another tomorrow, five different ones in five days
		const std::string Today = M.NextLine(Note, W), Still = M.NextLine(Note, W);
		std::set<std::string> Week;
		FWhen D = W;
		for (int K = 0; K < 5; ++K)
		{
			D.Day = W.Day + 1 + K;
			D.Since = W.Since + 1 + K;
			D.Weekday = (W.Weekday + 1 + K) % 7;
			Week.insert(M.NextLine(Note, D));
		}
		CHECK(Has(Today) && Today == Still && Week.size() == 5, "%s: the note is the same all day (%s) and another each day (%d in 5 days)", Home,
			Today.c_str(), (int)Week.size());
	}
	// the kettle where there was none: tea, once a day
	const int Kettle = M.FindThing("P-T4H1/flat_kettle");
	const FAct* KA = Kettle >= 0 ? M.ActOf(Kettle) : nullptr;
	CHECK(KA && KA->Daily == 1 && KA->Give.size() == 1 && KA->Give[0].first == "tea", "Lev Markovich's kettle gives a tea once a day");
	// the wardrobe's words follow the weather: a cold day finds the winter coat at the front
	const int Wardrobe = M.FindThing("P-SERPENTINE/bauer_wardrobe");
	FWhen Cold = W;
	Cold.Weather = WeatherCold;
	CHECK(Wardrobe >= 0 && M.NextLine(Wardrobe, Cold) == "city.wardrobe.cold.1", "on a cold day the wardrobe opens on the winter coat");
}

// The same things in the club rooms and the sanatorium (tools/city/content/x_room_touch.json): the radio amateurs' and
// the model makers' kettles give a tea once a day, the dance, glider and guest photos turn over, the radio club's, the
// reading room's and the sanatorium guest's notes change each day.
inline void RoomTouchChecks(const std::string& Path)
{
	std::string Text, Error, Ru;
	if (!ReadFile(Path, Text))
	{
		return;
	}
	std::printf("things to pick up in the club rooms and the sanatorium (real data)\n");
	FModel M;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	ReadFile("unreal/Lyudmilovsk/Content/Localization/city/strings_ru.json", Ru);
	auto Has = [&Ru](const std::string& Key) { return !Key.empty() && Ru.find("\"" + Key + "\"") != std::string::npos; };
	FWhen W;
	W.Day = 1000;
	W.Since = 3;
	W.Hour = 14.f;
	W.Weekday = 2;
	W.MonthDay = 1015;
	W.Weather = WeatherClear;
	for (const char* Id : { "P-CLUB2/dance_photo", "P-CLUB3/workshop_photo", "P-SANATORIUM/guest_photo" })
	{
		const int Photo = M.FindThing(Id);
		if (Photo < 0)
		{
			CHECK(false, "%s is there", Id);
			continue;
		}
		const std::string Front = M.NextLine(Photo, W), Back = M.NextLine(Photo, W), Again = M.NextLine(Photo, W);
		CHECK(Has(Front) && Has(Back) && Front != Back && Again == Front && Front.find("city.room_photo.") == 0,
			"%s: the front, then the back, then the front again (%s, %s)", Id, Front.c_str(), Back.c_str());
	}
	for (const char* Id : { "P-CLUB2/radio_note", "P-CLUB3/cart_note", "P-SANATORIUM/guest_note" })
	{
		const int Note = M.FindThing(Id);
		if (Note < 0)
		{
			CHECK(false, "%s is there", Id);
			continue;
		}
		const std::string Today = M.NextLine(Note, W), Still = M.NextLine(Note, W);
		std::set<std::string> Week;
		FWhen D = W;
		for (int K = 0; K < 5; ++K)
		{
			D.Day = W.Day + 1 + K;
			D.Since = W.Since + 1 + K;
			D.Weekday = (W.Weekday + 1 + K) % 7;
			Week.insert(M.NextLine(Note, D));
		}
		CHECK(Has(Today) && Today == Still && Week.size() == 5 && Today.find("city.room_note.") == 0,
			"%s: one note all day (%s), another each day (%d in 5 days)", Id, Today.c_str(), (int)Week.size());
	}
	for (const char* Id : { "P-CLUB2/radio_kettle", "P-CLUB3/workshop_kettle" })
	{
		const int Kettle = M.FindThing(Id);
		const FAct* A = Kettle >= 0 ? M.ActOf(Kettle) : nullptr;
		CHECK(A && A->Daily == 1 && A->Give.size() == 1 && A->Give[0].first == "tea" && Has(Kettle >= 0 ? M.NextLine(Kettle, W) : ""),
			"%s gives a tea once a day, with its own words", Id);
	}
	// the radio club's kettle at night: someone was on the air until midnight
	FWhen Night = W;
	Night.Hour = 23.5f;
	const int RK = M.FindThing("P-CLUB2/radio_kettle");
	CHECK(RK >= 0 && M.NextLine(RK, Night) == "city.room_kettle.radio.night.1", "at night the radio club's kettle is still warm");
}
