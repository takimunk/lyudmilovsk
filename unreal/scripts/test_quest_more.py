"""Offscreen check of the side quests after the first four (docs/quests.md), with the game's real systems where a script can:
  1. Dev.QA quests: every quest forgotten and played through by its real events (DEVQA quest <id> done / STUCK)
  2. the chains (and no skipping: the grove before Sorokin's talk leaves waxwing on find; a feeder visit after
     waxwing_after leaves autumn_count before its talk): waxwing_after's end begins autumn_count (stage kit) and Sorokin's next talk is autumn_*, not waxwing_journal;
     waxwing's `prepare` done begins overdue_journal; awakening's end begins owner_supper
  3. first_catch begins when fisherman_cheese ends; neither the reward's fish_herring nor a fish given by hand finishes
     `catch`, a fish_caught does
  4. Aslan at his tent begins tent_neighbours by a real talk; eating from the hand (corn cob, pie: lyud.Food.Hold + the mouse held) and at the diner (pancakes, soup: lyud.Diner.Serve,
     lyud.Diner.Eat, bites) sends `eat <item>`: tent_neighbours, lab_lunch
  5. night_signal (have, wear the head lamp, reach the upper station), soda_science (four sodas drunk by Inventory.Use), magpie_key, painter_views,
     hot_corn (bring to Korneev_ivan, then a real talk with Guseva at her cart), sparrow_hello (the girl hears a sparrow:
     Quest.BirdCall HouseSparrow kid_girl, her three lines, bird_call HouseSparrow.Kid_girl)
  6. who is drawn where: Aya at 17:00, Lange at 14:00, the yard_evening people at 18:15, Kid_girl and the club's doorstep
QUESTMORE lines with Quest.Dump's QUESTTEST state between the steps.

  sh unreal/scripts/test_quest_more.sh
"""
import unreal

state = {"t": 0.0, "step": 0, "shots": 0}
PLAN = []
T = [12.0]


def at(cmd, label=None, gap=0.6):
    PLAN.append((T[0], cmd, label))
    T[0] += gap


def dump(label):
    at("Quest.Dump", label)


def go(person, m=1.5, gap=3.0):
    # twice: the second time their body is drawn (the first may land before it is)
    at("Quest.GoToPerson %s %s" % (person, m), None, gap)
    at("Quest.GoToPerson %s %s" % (person, m), None, 2.0)


def talk(n, gap=1.1):
    for _ in range(n):
        at("Npc.Talk", None, gap)


# 1. every quest by its real events
at("Dev.QA quests", None, 40.0)
# 2. chains
at("Quest.Reset", None, 1.0)
at("Quest.Start waxwing")
at("Bird.Hero -420 1780", None, 4.0)
dump("grove_before_talk")
# waxwing finished, then waxwing_after's talk: its reward begins autumn_count, which settles past kit (a feeder in the pocket)
at("Quest.Reset", None, 1.0)
at("Inventory.Give feeder 1")
at("Quest.SetStage waxwing done", None, 1.0)
at("Quest.Event talk Sorokin_oleg", None, 1.5)
dump("chain_after_waxwing_after")
at("Quest.Event feeder_visit GreatTit", None, 1.0)
dump("chain_feeder_visit_before_talk")
at("lyud.Birds.Time 10", None, 2.5)
go("sorokin_oleg", 1.3)
at("Npc.Where")
talk(8)
dump("chain_sorokin_talked")
at("Quest.Event feeder_fill_at feed_peanuts@academ", None, 1.0)
dump("chain_autumn_post")
at("Quest.Reset", None, 1.0)
at("Quest.Start waxwing")
at("Quest.Event talk Sorokin_oleg")
at("Inventory.Give bird_guide 1")
at("Inventory.Give feed_rowan 5")
at("Inventory.Give feeder 1", None, 1.0)
dump("chain_after_prepare")
at("Quest.Reset", None, 1.0)
at("Dev.Quest awakening play", None, 4.0)
dump("chain_after_awakening")
# 3. first_catch after fisherman_cheese
at("Quest.Reset", None, 1.0)
at("Dev.Quest fisherman_cheese play", None, 4.0)
dump("catch_begun")
at("Inventory.Give fishing_rod 1", None, 1.0)
at("Quest.Event talk Lange_pyotr", None, 1.0)
dump("catch_with_herring")
at("Inventory.Give fish_flounder 1", None, 1.0)
dump("catch_flounder_given")
at("Quest.Event fish_caught fish_flounder", None, 1.0)
dump("catch_fish_caught")
# 4. eating: from the hand, then at the diner
at("Quest.Reset", None, 1.0)
at("lyud.Birds.Time 12", None, 2.5)
go("khuazhev_aslan", 1.5)
at("Npc.Where")
at("shot showui", "aslan_tent_12h")
talk(9)
dump("tent_taste")
for item in ("corn_cob", "pie"):
    at("lyud.Food.Hold %s" % item, None, 1.5)
    for _ in range(6):
        at("lyud.Hand.Press 1", None, 5.0)
        at("lyud.Hand.Press 0", None, 0.8)
    dump("tent_ate_" + item)
at("Dev.Tp cable_lower", None, 4.0)
at("lyud.Diner.Serve diner_pancakes", None, 1.0)
at("lyud.Diner.State", None, 0.5)
at("lyud.Diner.Eat", None, 2.0)
for _ in range(14):
    at("lyud.Bite.Take big", None, 2.0)
at("lyud.Diner.State", None, 0.5)
dump("tent_ate_pancakes")
at("lyud.Birds.Time 12", None, 2.5)
go("khuazhev_aslan", 1.5)
talk(6)
dump("tent_told")
at("Quest.Start lab_lunch")
at("Quest.Event talk Li_vika", None, 1.0)
at("Dev.Quest lab_lunch eat", None, 6.0)
at("lyud.Diner.Eat", None, 2.0)
at("shot showui", "diner_soup", 1.0)
for _ in range(14):
    at("lyud.Bite.Take big", None, 2.0)
dump("lab_ate_soup")
# 5. the others by events (and two by real talks)
at("Quest.Reset", None, 1.0)
at("Quest.Start night_signal")
at("Quest.Event talk Valiev_rustam", None, 1.0)
at("Inventory.Wear head_lamp", None, 1.0)
at("Dev.Tp cable_upper", None, 4.0)
dump("night_signal")
at("Quest.Start soda_science")
at("Quest.Event talk Zotova_galina")
for d in ("drink_baikal", "drink_dyushes", "drink_buratino", "drink_tarkhun"):
    at("Inventory.Give %s 1" % d)
    at("Inventory.Use %s" % d, None, 1.0)
    dump("soda_" + d)
at("Quest.Event talk Zotova_galina", None, 1.0)
dump("soda_told")
at("Quest.Start magpie_key")
at("Quest.Event talk Belov_misha")
at("Quest.Event corvid_find BottleCap")
at("Quest.Event corvid_store_dug Coin")
at("Quest.Event corvid_gift_taken Coin", None, 1.0)
dump("magpie_found")
at("Quest.Event talk Belov_misha", None, 1.0)
dump("magpie_told")
at("Quest.Start painter_views")
at("Quest.Event talk Nishimura_aya")
for p in ("promenade", "starlings", "rowan_grove"):
    at("Dev.Tp %s" % p, None, 4.0)
dump("painter_looked")
at("Quest.Start hot_corn")
at("Quest.Event talk Guseva_valentina")
at("Inventory.Give corn_cob 1", None, 1.0)
dump("corn_bought")
at("Quest.Event talk Korneev_ivan", None, 1.0)
dump("corn_carried")
at("lyud.Birds.Time 12", None, 2.5)
go("guseva_valentina", 1.3)
at("Npc.Where")
at("shot showui", "guseva_cart")
talk(6)
dump("corn_told")
at("Quest.Start sparrow_hello")
at("Quest.Event talk Kid_girl")
at("Quest.Event bird_tamed HouseSparrow", None, 1.0)
at("lyud.Birds.Time 15", None, 2.5)
go("kid_girl", 5)
at("Quest.Scenes")
at("Quest.BirdCall HouseSparrow kid_girl", None, 1.0)
at("shot showui", "kid_girl_hears", 9.0)
dump("sparrow_called")
# the club's doorstep at 12 (Aslan's tent beside it)
at("lyud.Birds.Time 12", None, 2.5)
at("Dev.Tp club_door", None, 3.0)
at("shot showui", "club_door_12h")
# 6. who is drawn where
at("Dev.Tp club_door", None, 3.0)
at("Dev.Where")
at("Quest.Scenes", "kid_from_doorstep")
for h, people in ((17, ["nishimura_aya"]), (14, ["lange_pyotr"]),
                  (18.25, ["zotova_galina", "li_vika", "valiev_rustam", "belov_misha", "korneev_ivan", "korneeva_nina"])):
    at("lyud.Birds.Time %s" % h, None, 2.5)
    for p in people:
        go(p, 3)
        if p in ("nishimura_aya", "lange_pyotr", "zotova_galina"):
            at("shot showui", "%s_%sh" % (p, int(h)))


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def tick(dt):
    w = world()
    if not w:
        return
    state["t"] += min(dt, 0.25)
    while state["step"] < len(PLAN) and state["t"] > PLAN[state["step"]][0]:
        _, cmd, label = PLAN[state["step"]]
        state["step"] += 1
        unreal.log_warning("QUESTMORE %s %s" % (cmd, label or ""))
        if cmd.startswith("shot"):
            unreal.log_warning("QUESTMORE shot %02d %s" % (state["shots"], label))
            state["shots"] += 1
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 2.0:
        unreal.log_warning("QUESTMORE finished")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
