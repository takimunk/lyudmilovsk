"""Offscreen check of the diary's birds section (docs/diary.md, Birds) and of calling a befriended bird to the hand.
Run by test_bird_diary.sh. BIRDDIARY lines in the log; screenshots (shot showui: the diary is Slate) in this order:

  birds_empty      the birds section before anything is met (B / Screen.Open birds)
  birds_1, birds_2 a few species met, three of them friends (Birds.Meet ...), the first spread and the next
  birds_en, birds_ja
  call_1..call_5   a friend called (Birds.Call 0): the empty palm, the bird flying in, landing, the view going into it
  birds_loaded     the book after a save and a load
"""
import unreal

state = {"t": 0.0, "step": 0}
DOOR = "BugItGo -193 1500 150 0 90 0"

PLAN = [
    (12.0, DOOR),
    (14.0, "Birds.Book"),
    (14.5, "Screen.Open birds"),
    (17.0, "SHOT birds_empty"),
    (17.5, "Screen.Open none"),
    (19.0, "Birds.Meet HerringGull"),
    (19.1, "Birds.Meet MuteSwan"),
    (19.2, "Birds.Meet Chaffinch x female"),
    (19.3, "Birds.Meet Robin"),
    (19.4, "Birds.Meet Peregrine"),
    (19.5, "Birds.Meet FeralPigeon tame"),
    (19.6, "Birds.Meet HouseSparrow tame"),
    (19.7, "Birds.Meet GreatTit tame"),
    (19.8, "Birds.Meet Blackbird"),
    (20.0, "Birds.Book"),
    (20.5, "Screen.Open birds"),
    (23.0, "SHOT birds_1"),
    (23.5, "Screen.Turn 1"),
    (25.0, "SHOT birds_2"),
    (25.5, "lyud.Language en"),
    (25.6, "Screen.Turn -1"),
    (27.5, "SHOT birds_en"),
    (28.0, "lyud.Language ja"),
    (30.0, "SHOT birds_ja"),
    (30.5, "lyud.Language ru"),
    (31.0, "Screen.Open none"),
    (33.0, "Birds.Call 0"),
    (34.5, "SHOT call_1"),
    (38.0, "SHOT call_2"),
    (41.0, "SHOT call_3"),
    (44.0, "SHOT call_4"),
    (48.0, "SHOT call_5"),
    (55.0, "Bird.Return"),
    (60.0, "Birds.Book"),
    (61.0, "lyud.Save birddiary"),
    (63.0, "Birds.Forget"),
    (64.0, "lyud.Load 0"),
    (78.0, "Birds.Book"),
    (79.0, "Screen.Open birds"),
    (82.0, "SHOT birds_loaded"),
]


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
        t, cmd = PLAN[state["step"]]
        state["step"] += 1
        unreal.log_warning("BIRDDIARY " + cmd)
        if cmd.startswith("SHOT "):
            cmd = "shot showui"
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 2.0:
        unreal.log_warning("BIRDDIARY done")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
