// The game's flags and named waits (docs/city_interiors.md "Flags"; docs/ship.md): the "flag" condition of lines, topics and
// acts (FModel::SetFlag: the cruise sets "cruise_boarding", "cruise_ev_dinner1"...), an act's "no_flag" words, a link door
// behind a flag (the launch to the motor ship), and "until": "<name>" (the act lasts to the cruise's next event:
// FModel::SetWaitMinutes). Included by citytest.cpp (its CHECK, AtDate).

inline void FlagChecks()
{
	std::printf("flags (the flag condition, no_flag words, a gated link door, named waits)\n");
	const std::string Text = R"({"things": [
  {"id": "P-PIER/launch_gate", "plot": "P-PIER", "kind": "door", "ue": [0, 0, 100], "yaw": 0, "props": {"to": "P-SHIP/gangway"},
   "act": "launch_out", "floors": [{"to": "P-SHIP/gangway"}]},
  {"id": "P-SHIP/gangway", "plot": "P-SHIP", "kind": "door", "ue": [900000, 0, 100], "yaw": 180, "props": {"to": "P-PIER/launch_gate"},
   "act": "gangway_out", "floors": [{"to": "P-PIER/launch_gate"}]},
  {"id": "P-SHIP/board", "plot": "P-SHIP", "kind": "board", "ue": [900400, 0, 150], "yaw": 0, "props": {}, "act": "programme"},
  {"id": "P-SHIP/chair", "plot": "P-SHIP", "kind": "bed", "ue": [900800, 0, 40], "yaw": 0, "props": {}, "act": "deck_chair"},
  {"id": "P-SHIP/bed", "plot": "P-SHIP", "kind": "bed", "ue": [901200, 0, 50], "yaw": 0, "props": {}, "act": "bed_lux"},
  {"id": "P-SHIP/desk", "plot": "P-SHIP", "kind": "counter", "ue": [901600, 0, 100], "yaw": 0, "props": {"role": "purser"}, "act": "desk"}],
 "acts": {
  "launch_out": {"prompt": "p", "any_hour": true, "flag": "cruise_can_board", "no_flag": "launch_words"},
  "launch_words": {"prompt": "p", "any_hour": true, "lines": [
     {"k": "l.laid_up", "flag": "!cruise_season"},
     {"k": "l.away", "flag": ["cruise_season", "!cruise_boarding", "!cruise_port"]},
     {"k": "l.not_today", "flag": ["cruise_port", "!cruise_boarding"]},
     {"k": "l.no_ticket", "flag": ["cruise_boarding", "!cruise_has_ticket"]}]},
  "gangway_out": {"prompt": "p", "any_hour": true, "flag": "cruise_landing", "no_flag": "_closed:g.shut"},
  "_closed:g.shut": {"prompt": "p", "lines": ["g.shut"], "event": "look", "target": "closed", "any_hour": true},
  "programme": {"prompt": "p", "lines": [{"k": "pr.day1", "flag": "cruise_day1"}, {"k": "pr.day2", "flag": "cruise_day2"}, {"k": "pr.port", "flag": ["!cruise_day1", "!cruise_day2"]}]},
  "deck_chair": {"prompt": "p", "pose": "lie", "fade": true, "any_hour": true, "until": "cruise_event", "minutes": 60, "lines": ["dc.1"], "flag": "cruise_aboard"},
  "bed_lux": {"prompt": "p", "pose": "lie", "minutes": 480, "until": 7, "any_hour": true, "lines": ["b.1"], "flag": ["cruise_aboard", "cruise_class_lux"],
              "no_flag": "_closed:b.not_yours"},
  "_closed:b.not_yours": {"prompt": "p", "lines": ["b.not_yours"], "event": "look", "target": "closed", "any_hour": true},
  "desk": {"prompt": "p", "lines": ["d.1"], "topics": ["t.isles", "t.port"]}},
 "topics": {"t.isles": {"k": "t.isles.q", "lines": ["t.isles.1", {"k": "t.isles.2", "flag": "cruise_at_birds"}], "flag": "cruise_underway"},
            "t.port": {"k": "t.port.q", "lines": ["t.port.1"], "flag": "!cruise_underway"}},
 "shops": {}})";
	FModel M;
	std::string Error;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	FAsk Ask;
	Ask.Money = 100;
	Ask.StaffOnShift = [](const std::string&, const std::string&) { return true; };
	const FWhen Now = AtDate(4, 17.f, 100, 1023, 3);
	const int Gate = M.FindThing("P-PIER/launch_gate"), Gangway = M.FindThing("P-SHIP/gangway"), Board = M.FindThing("P-SHIP/board");
	const int Chair = M.FindThing("P-SHIP/chair"), Bed = M.FindThing("P-SHIP/bed"), Desk = M.FindThing("P-SHIP/desk");
	CHECK(Gate >= 0 && Gangway >= 0 && Board >= 0 && Chair >= 0 && Bed >= 0 && Desk >= 0, "the things are there");
	if (Gate < 0 || Gangway < 0 || Board < 0 || Chair < 0 || Bed < 0 || Desk < 0)
	{
		return;
	}
	const FAct& Launch = M.Acts[(size_t)M.Things[(size_t)Gate].Act];
	CHECK(Launch.Flags.size() == 1 && Launch.Flags[0] == "cruise_can_board" && Launch.NoFlagAct == M.FindAct("launch_words")
		&& M.Acts[(size_t)M.FindAct("launch_words")].Lines[1].Flags.size() == 3 && M.Acts[(size_t)M.Things[(size_t)Bed].Act].Flags.size() == 2,
		"flag (a name or a list) and no_flag read");
	CHECK(M.IsLift(Gate) && M.IsLift(Gangway) && M.Things[(size_t)Gate].Floors[0].To == Gangway, "the two doors are a link across the plots");

	// no flag set: winter. The gate only says its words (and no other line of them), nothing is counted as a visit
	const FOutcome Winter = M.Run(Gate, Now, Ask);
	CHECK(Winter.Verdict == EVerdict::Ok && Winter.bClosed && Winter.Line == "l.laid_up" && Winter.NumLines == 1 && !Winter.bFirstVisitToday,
		"no flags: the gate says the ship is laid up (%s, %d lines)", Winter.Line.c_str(), Winter.NumLines);
	CHECK(M.Check(Gate, Now, Ask) == EVerdict::Ok && M.ActIndexNow(Gate, Now, false) == M.FindAct("launch_words")
		&& M.ActIndexNow(Gate, Now, true) == M.FindAct("launch_words"), "open-all-hours does not lift a flag");
	// in season, the ship away
	CHECK(M.SetFlag("cruise_season", true) && !M.SetFlag("cruise_season", true) && M.HasFlag("cruise_season"), "SetFlag says whether it changed");
	CHECK(M.Run(Gate, Now, Ask).Line == "l.away", "in season, the ship at sea: away");
	M.SetFlag("cruise_port", true);
	CHECK(M.Run(Gate, Now, Ask).Line == "l.not_today", "the ship on the roads, no boarding: not today");
	M.SetFlag("cruise_boarding", true);
	const FOutcome NoTicket = M.Run(Gate, Now, Ask);
	CHECK(NoTicket.bClosed && NoTicket.Line == "l.no_ticket", "boarding without a ticket: the ticket's words (%s)", NoTicket.Line.c_str());
	// the ticket bought: the gate's own act runs, and the game takes the link
	M.SetFlag("cruise_has_ticket", true);
	M.SetFlag("cruise_can_board", true);
	const FOutcome Go = M.Run(Gate, Now, Ask);
	CHECK(Go.Verdict == EVerdict::Ok && !Go.bClosed && Go.Act == M.Things[(size_t)Gate].Act && Go.Target == "launch_out", "with the flag the gate is a door");
	// aboard: the gangway is shut until the landing
	const FOutcome Shut = M.Run(Gangway, Now, Ask);
	CHECK(Shut.bClosed && Shut.Line == "g.shut", "the gangway during the voyage: shut, its words");
	M.SetFlag("cruise_boarding", false);
	M.SetFlag("cruise_can_board", false);
	M.SetFlag("cruise_landing", true);
	CHECK(!M.Run(Gangway, Now, Ask).bClosed && M.Run(Gate, Now, Ask).bClosed, "the landing: the gangway opens, the gate is words again");
	M.SetFlag("cruise_landing", false);

	// lines by flags
	CHECK(M.Run(Board, Now, Ask).Line == "pr.port", "no day flag: the port's line");
	M.SetFlag("cruise_day2", true);
	const FOutcome Day2 = M.Run(Board, Now, Ask);
	CHECK(Day2.Line == "pr.day2" && Day2.NumLines == 1, "day 2: its line only (%s, %d)", Day2.Line.c_str(), Day2.NumLines);
	M.SetFlag("cruise_day2", false);

	// an act behind a flag with no words of its own is refused as closed; a list needs all its flags
	CHECK(M.Check(Chair, Now, Ask) == EVerdict::Closed && M.Run(Chair, Now, Ask).Verdict == EVerdict::Closed, "the deck chair ashore: closed");
	M.SetFlag("cruise_aboard", true);
	const FOutcome NotYours = M.Run(Bed, Now, Ask);
	CHECK(NotYours.bClosed && NotYours.Line == "b.not_yours", "aboard in another class: the lux bed says it is not his");
	M.SetFlag("cruise_class_lux", true);
	const FOutcome Mine = M.Run(Bed, AtDate(4, 23.f, 100, 1023, 3), Ask);
	CHECK(!Mine.bClosed && Mine.Line == "b.1" && Mine.Minutes == 480, "in lux: his bed, till 7");

	// the named wait: to the next event while the cruise gives one, else the act's own minutes
	CHECK(M.Acts[(size_t)M.Things[(size_t)Chair].Act].UntilName == "cruise_event" && M.Acts[(size_t)M.Things[(size_t)Chair].Act].Until == -1
		&& M.Acts[(size_t)M.Things[(size_t)Bed].Act].UntilName.empty() && M.Acts[(size_t)M.Things[(size_t)Bed].Act].Until == 7, "until: a name or an hour");
	CHECK(M.Run(Chair, Now, Ask).Minutes == 60, "no wait set: the act's own 60 minutes");
	M.SetWaitMinutes("cruise_event", 25);
	const FOutcome Wait = M.Run(Chair, Now, Ask);
	CHECK(Wait.Verdict == EVerdict::Ok && Wait.Minutes == 25 && Wait.bFade && Wait.Pose == "lie" && Wait.Line == "dc.1", "the deck chair: 25 minutes to the next event");
	M.SetWaitMinutes("cruise_event", 180);
	CHECK(M.Run(Chair, Now, Ask).Minutes == 180 && M.WaitMinutes("cruise_event") == 180, "the wait follows the game");
	M.SetWaitMinutes("cruise_event", 0);
	CHECK(M.Run(Chair, Now, Ask).Minutes == 60 && M.WaitMinutes("cruise_event") == 0, "0 clears it");

	// topics by flags
	std::vector<int> Topics;
	M.TopicsNow(Desk, Now, Topics);
	CHECK(Topics.size() == 1 && M.Topics[(size_t)Topics[0]].Id == "t.port", "not under way: the port's topic only (%d)", (int)Topics.size());
	M.SetFlag("cruise_underway", true);
	M.TopicsNow(Desk, Now, Topics);
	CHECK(Topics.size() == 1 && M.Topics[(size_t)Topics[0]].Id == "t.isles", "under way: the islands' topic");
	const FTopicAnswer Far = M.AskTopic(Topics[0], Now);
	M.SetFlag("cruise_at_birds", true);
	const FTopicAnswer Near = M.AskTopic(Topics[0], Now);
	CHECK(Far.Lines.size() == 1 && Near.Lines.size() == 2, "an answer's line by a flag (%d, %d)", (int)Far.Lines.size(), (int)Near.Lines.size());

	// the flags are the game's, not the hero's: not in the save, kept over a load of it
	std::map<std::string, std::string> Saved;
	M.SaveState(Saved);
	bool bFlagSaved = false;
	for (const auto& P : Saved)
	{
		bFlagSaved = bFlagSaved || P.first.find("cruise_") != std::string::npos || P.second.find("cruise_") != std::string::npos;
	}
	M.LoadState(Saved);
	CHECK(!bFlagSaved && M.HasFlag("cruise_aboard") && M.GetFlags().size() >= 4, "flags are not saved and outlive a load of the save");
	// a line without the set (the static count the data checks use): "!flag" holds, "flag" does not
	FLine Plain, Not;
	Plain.Flags = { "x" };
	Not.Flags = { "!x" };
	CHECK(!Plain.Holds(Now) && Not.Holds(Now), "without flags a flag line fails and a !flag line holds");
}
