"""The list of kinds: name -> (builder function, {variants, collision, notes}); order = manifest order."""
import props
import props_tents as tents
import props_camp as camp
import props_signs as signs
import props_bridge as bridge

KINDS = {
    "tent_dome": (tents.tent_dome, {"variants": [0, 1, 2], "collision": "box", "body": [2.1, 2.1, 1.3]}),
    "tent_ridge": (tents.tent_ridge, {"variants": [0, 1], "collision": "box", "body": [2.0, 2.8, 1.5]}),
    "tent_tunnel": (tents.tent_tunnel, {"variants": [0], "collision": "box", "body": [1.6, 4.0, 1.2]}),
    "fire_ring": (props.fire_ring, {"variants": [0], "collision": "box"}),
    "camp_table": (camp.camp_table, {"variants": [0], "collision": "exact"}),
    "food_pole": (camp.food_pole, {"variants": [0], "collision": "box", "body": [1.9, 0.3, 5.0]}),
    "clothes_line": (camp.clothes_line, {"variants": [0], "collision": "none", "body": [5.2, 0.1, 1.9]}),
    "outhouse": (camp.outhouse, {"variants": [0], "collision": "exact"}),
    "log_bench": (props.log_bench, {"variants": [0, 1], "collision": "exact"}),
    "tarp_shelter": (tents.tarp_shelter, {"variants": [0], "collision": "none", "body": [4.0, 3.0, 2.55]}),
    "woodpile": (props.woodpile, {"variants": [0], "collision": "box"}),
    "board_camp": (signs.board_camp, {"variants": [0], "collision": "exact"}),
    "board_reserve": (signs.board_reserve, {"variants": [0], "collision": "exact"}),
    "barrier": (signs.barrier, {"variants": [0], "collision": "exact"}),
    "sign_hot_ground": (signs.sign_hot_ground, {"variants": [0], "collision": "exact"}),
    "sign_post": (signs.sign_post, {"variants": [0], "collision": "exact"}),
    "log_bridge": (bridge.log_bridge, {"variants": [0, 1, 2], "collision": "box"}),
    "spring_deck": (signs.spring_deck, {"variants": [0], "collision": "exact"}),
}
