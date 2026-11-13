"""Network co-op's respawns and cutscenes follow split screen co-op's."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def source(path):
    return (ROOT / path).read_text(encoding="utf-8")


def function(text, name):
    match = re.search(r"\n[^\n;]*\b" + re.escape(name) + r"\(\n?[^;{]*\)\n\{", text)
    assert match, name
    depth, index = 0, text.index("{", match.start())
    for end in range(index, len(text)):
        depth += {"{": 1, "}": -1}.get(text[end], 0)
        if depth == 0:
            return text[match.start():end + 1]
    raise AssertionError(name)


def test_scripts_see_network_coop_as_cooperative():
    # the stock campaign's scripts stage player1 in cutscenes only when
    # game_is_cooperative: network co-op is, as split screen is
    hs = source("source/hs/hs.c")
    assert "HS_EVALUATE_RETURN_BOOLEAN_NO_ARGUMENTS(game_is_cooperative_evaluate, hs_port_game_is_cooperative)" in hs
    assert "return game_is_cooperative() || network_coop_active();" in function(hs, "hs_port_game_is_cooperative")


def test_client_hud_names_the_respawn_failure():
    coop = source("port/linux/game/network_coop.c")
    host = function(coop, "host_presentation")
    client = function(coop, "client_presentation_apply")
    assert "players_get_respawn_failure()" in host
    assert "<< PRESENTATION_RESPAWN_FAILURE_SHIFT" in host
    assert "presentation->skip_voters >> PRESENTATION_RESPAWN_FAILURE_SHIFT" in client
    assert "players_port_set_respawn_failure(" in client
    assert "respawn_failure < NUMBER_OF_RESPAWN_FAILURES ? respawn_failure : 0" in client
    # the count keeps the low bits; the message is the size it was
    assert "#define PRESENTATION_SKIP_VOTERS_MASK 0x0F" in coop
    assert "skip_vote.voters = presentation->skip_voters & PRESENTATION_SKIP_VOTERS_MASK;" in coop
    hud = source("source/interface/hud.c")
    failures = hud[hud.index("enum player_respawn_failure"):]
    failures = failures[:failures.index("};")]
    assert failures.count(",") - 1 == 5, "NUMBER_OF_RESPAWN_FAILURES follows hud.c's"


def test_respawn_is_split_screens_beside_the_partner():
    players = source("source/game/players.c")
    respawn = function(players, "players_respawn_coop")
    assert "players_coop_spawn_beside_index = respawn_unit_index;" in respawn
    assert "players_coop_spawn_beside_index = NONE;" in respawn
    assert "player_place_beside_teammate(iterator.datum_index);" in respawn
    # the death timer and its test are split screen's for the host too
    main = source("source/main/main.c")
    assert "players_respawn_coop())" in function(main, "main_respawn_private")


def test_client_sees_the_hosts_view_until_control_returns():
    coop = source("port/linux/game/network_coop.c")
    wanted = function(coop, "client_host_view_wanted")
    # a cinematic, the scripts' camera, or the controls taken: until the
    # players have them back
    assert "_presentation_cinematic_bit" in wanted
    assert "presentation->camera_scripted" in wanted
    assert "presentation->input_disabled" in wanted
    record = function(coop, "client_view_record")
    assert "coop_presentation.view_active = valid && host_view;" in record
    apply = function(coop, "client_presentation_apply")
    assert "client_view_record(presentation, host_view);" in apply
    # the host's own first person: through its unit's eyes, which is not drawn
    assert "client_host_eyes_unit(&presentation->camera_position)" in apply
    assert "scripted_camera_set_first_person(eyes_unit);" in apply
    eyes = function(coop, "client_host_eyes_unit")
    assert "player->local_player_index != NONE" in eyes
    assert "unit_get_camera_position(" in eyes


def test_events_start_on_their_presentations_tick():
    coop = source("port/linux/game/network_coop.c")
    handle = function(coop, "network_coop_handle_events")
    assert "client_event_arrived(&events[index], host_time);" in handle
    assert "client_apply_event(" not in handle
    arrived = function(coop, "client_event_arrived")
    assert "host_time > coop_presentation.played_time" in arrived
    play = function(coop, "client_play_presentation")
    # after the presentation of the ticks played
    assert play.index("client_presentation_apply(presentation);") < play.index("client_play_pending_events(FALSE);")
    pending = function(coop, "client_play_pending_events")
    assert "client_pending_events.host_times[index] <= coop_presentation.played_time" in pending
    # a host gone quiet plays what was held
    assert "client_play_pending_events(TRUE);" in function(coop, "network_coop_client_tick")
    distributed = source("port/linux/game/network_distributed.c")
    assert "network_coop_handle_events(entries, header.count, header.game_time);" in distributed
