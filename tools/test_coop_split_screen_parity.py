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


def test_client_sees_the_scripts_camera_as_split_screen_does():
    coop = source("port/linux/game/network_coop.c")
    wanted = function(coop, "client_host_view_wanted")
    # the host's view only while its scripts hold the camera, as split screen
    # shows every player the scripts' camera; else each player's own
    assert "return presentation->camera_scripted != 0;" in wanted
    assert "input_disabled" not in wanted
    assert "CAMERA_LOCKED" not in wanted
    record = function(coop, "client_view_record")
    assert "coop_presentation.view_active = valid && host_view;" in record
    apply = function(coop, "client_presentation_apply")
    assert "client_view_record(presentation, host_view);" in apply
    # the scripts' first-person camera: through the named unit's eyes here
    assert "scripted_camera_set_first_person(eyes_unit);" in apply
    assert "client_host_eyes_unit" not in coop


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


def test_opening_cutscene_starts_where_solos_does():
    # a level's first cutscene camera, before the players have had their
    # controls, is taken at once (solo's observer takes its first command so),
    # not the end of a move from the player's eyes
    scripting = source("source/camera/camera_scripting.c")
    camera_set = function(scripting, "scripted_camera_set")
    assert "if (network_coop_opening_camera())\n\t\tobserver_port_cut_to_next_command();" in camera_set
    assert camera_set.index("network_coop_opening_camera()") < camera_set.index("observer_update(0.0001f);")
    observer = source("source/camera/observer.c")
    assert "observer_get(local_player_index)->first_command = FALSE;" in function(observer, "observer_port_cut_to_next_command")
    coop = source("port/linux/game/network_coop.c")
    opening = function(coop, "network_coop_opening_camera")
    assert "coop_opening.camera_set || coop_opening.control_given" in opening
    update = function(coop, "coop_opening_update")
    assert "player_input_enabled() && !cinematic_in_progress() && !*director_camera_scripted" in update
    assert "coop_opening_update();" in function(coop, "network_coop_host_tick")
    assert "csmemset(&coop_opening, 0, sizeof(coop_opening));" in function(coop, "network_coop_new_game")


def test_lifepod_camera_lock_reaches_the_client():
    coop = source("port/linux/game/network_coop.c")
    host = function(coop, "host_presentation")
    assert "player_control_port_camera_control_enabled() ? 0 : PRESENTATION_CAMERA_LOCKED_BIT" in host
    apply = function(coop, "client_presentation_apply")
    assert "scripted_player_control_set_camera_control((presentation->skip_voters & PRESENTATION_CAMERA_LOCKED_BIT) == 0);" in apply
    # the respawn failure's three bits, below the lock's
    assert "PRESENTATION_RESPAWN_FAILURE_MASK" in apply
    assert "#define PRESENTATION_CAMERA_LOCKED_BIT 0x80" in coop
    assert "#define PRESENTATION_RESPAWN_FAILURE_MASK 0x07" in coop


def test_cutscene_copies_follow_every_host_move():
    objects = source("port/linux/game/network_objects.c")
    follows = function(objects, "distributed_object_follows_host")
    assert "network_coop_client_cutscene()" in follows
    states = function(objects, "network_objects_handle_states")
    assert "if (distributed_object_follows_host(state->object_index) && !own_vehicle)\n\t\t{\n\t\t\ttolerance = 0.0f;" in states


def test_vehicle_hatches_and_custom_animations_are_resent():
    coop = source("port/linux/game/network_coop.c")
    hatches = function(coop, "host_unit_hatches")
    # the host's vehicle as it is, not only as its scripts last set it
    assert "case _unit_state_opening:\n\t\treturn UNIT_HATCHES_OPEN;" in hatches
    assert "return UNIT_HATCHES_IDLE;" in hatches
    looks = function(coop, "host_send_object_looks")
    assert "short hatches = host_unit_hatches(iterator.index) | host_unit_powered(iterator.index);" in looks
    assert "look->pad = hatches;" in looks
    client = function(coop, "network_coop_handle_object_looks")
    assert "boolean ridden = client_vehicle_ridden_here(object_index);" in client
    assert "if (ridden)\n\t\t\t\tcontinue;" in client
    assert "unit_port_animation_idle(object_index);" in client
    # custom animations again every refresh, left be where already played
    resync = function(coop, "host_resync_unit_animations")
    assert "if (!host_resend.refresh)" in resync
    assert "_coop_event_unit_animation_resync" in resync
    apply = function(coop, "client_apply_unit_animation_resync")
    assert "unit->object.animation.state.index == event->value" in apply
    assert "unit_port_play_user_animation(event->object_index, event->tag_index, event->value, FALSE, event->frame);" in apply
    assert "host_resync_unit_animations();" in function(coop, "network_coop_host_tick")


def test_host_names_the_eyes_it_looks_through():
    coop = source("port/linux/game/network_coop.c")
    eyes = function(coop, "host_camera_eyes_unit")
    assert "scripted_camera_object_is_first_person_camera(unit_index)" in eyes
    assert "_director_perspective_first_person" in eyes
    host = function(coop, "host_presentation")
    assert "(eyes_unit_index != NONE ? PRESENTATION_EYES_BIT : 0)" in host
    record = function(coop, "client_view_record")
    assert "unit_get_camera_position(presentation->camera_object_index, &view.position);" in record
    assert "skip_vote.votes = presentation->skip_votes & PRESENTATION_SKIP_VOTES_MASK;" in coop


def test_a_copy_its_name_does_not_lead_to_is_found_by_index():
    # The Maw's banshee_cinematic: the host's looks (its canopy) reached no copy
    coop = source("port/linux/game/network_coop.c")
    find = function(coop, "object_find")
    assert "long named_index = object_index_from_name_index(name_index);" in find
    assert "object = object_try_and_get_and_verify_type(object_index, type_mask);\n\t\treturn object && object->definition_index == definition_index" in find


def test_a_client_gets_out_of_a_seat_by_its_exit_animation():
    # out of a10's cryotube and down a Pelican's ramp as the host's copy did,
    # not dropped where it sat
    objects = source("port/linux/game/network_objects.c")
    seat = function(objects, "network_objects_set_seat")
    assert "unit->unit.animation.state == _unit_state_exiting_seat || unit_port_play_seat_exit(unit_index)" in seat
    assert seat.index("unit_port_play_seat_exit(unit_index)") < seat.index("unit_exit_seat_end(unit_index);")
    units = source("source/units/units.c")
    play = function(units, "unit_port_play_seat_exit")
    assert "unit->unit.animation.state = _unit_state_exiting_seat;" in play
    # without the AI's word of it, or the vehicle's opening: the host's give them
    assert "ai_handle_exit_vehicle" not in play
    assert "unit_open(" not in play


def test_the_camera_moves_evenly_when_a_datagram_is_late():
    # two ticks behind the newest, so a late datagram is in before its tick;
    # a tick with nothing newer holds the view rather than drawing the last
    # tick's move again
    coop = source("port/linux/game/network_coop.c")
    assert "PRESENTATION_DELAY_TICKS = 2," in coop
    play = function(coop, "client_play_presentation")
    assert "else\n\t{\n\t\tclient_view_hold();\n\t\treturn;\n\t}" in play
    hold = function(coop, "client_view_hold")
    assert "client_view_record(&coop_presentation.view_source, coop_presentation.view_source_host_view);" in hold
    record = function(coop, "client_view_record")
    assert "coop_presentation.view_source = *presentation;" in record


def test_a_client_starts_getting_out_when_the_host_does():
    # the host's copy's exit begins with the rider still seated: the state
    # says so, and the client's copy starts the seat's exit animation then
    distributed = source("port/linux/game/network_distributed.c")
    assert "_distributed_unit_exiting_seat_bit," in distributed
    assert "SET_FLAG(state->unit_flags, _distributed_unit_exiting_seat_bit, state->vehicle_index != NONE &&\n\t\t\tunit->unit.animation.state == _unit_state_exiting_seat);" in distributed
    assert "TEST_FLAG(state->unit_flags, _distributed_unit_exiting_seat_bit) &&\n\t\t\tunit->unit.animation.state != _unit_state_exiting_seat)" in distributed
    assert "#define HALO_PORT_NETWORK_VERSION 52" in source("port/linux/include/halo_port_limits.h")


def test_a_vehicle_a_recording_flies_is_powered_on_the_client_too():
    # The Maw's banshee_cinematic: recording_play makes it actively controlled
    # on the host, its seat powers up and its canards turn red; the client's
    # copy, moved by the host's states, stayed blue at rest
    coop = source("port/linux/game/network_coop.c")
    powered = function(coop, "host_unit_powered")
    assert "unit->object.type == _object_type_vehicle && TEST_FLAG(unit->unit.flags, _unit_actively_controlled_bit)" in powered
    looks = function(coop, "host_send_object_looks")
    assert "short hatches = host_unit_hatches(iterator.index) | host_unit_powered(iterator.index);" in looks
    client = function(coop, "network_coop_handle_object_looks")
    assert "powered = (look->pad & UNIT_LOOK_POWERED) != 0;" in client
    assert "SET_FLAG(vehicle->unit.flags, _unit_actively_controlled_bit," in client
    assert client.index("if (ridden)\n\t\t\t\tcontinue;") < client.index("SET_FLAG(vehicle->unit.flags, _unit_actively_controlled_bit,")
    # the hatches stay below the powered bit
    assert "#define UNIT_LOOK_HATCHES_MASK 0x00FF" in coop
    assert "#define UNIT_LOOK_POWERED 0x0100" in coop
    units = source("source/units/units.c")
    assert "unit->unit.driver_object_index!=NONE || TEST_FLAG(unit->unit.flags, _unit_actively_controlled_bit)" in units


def test_a_character_the_scripts_hold_still_is_held_still_on_the_client():
    # unit_suspended on a cutscene's character (no player's): the client's
    # copy, not held, fell or was pushed from where it was put, out of sight
    coop = source("port/linux/game/network_coop.c")
    powered = function(coop, "host_unit_powered")
    assert "unit->unit.player_index == NONE && TEST_FLAG(unit->unit.flags, _unit_suspended_bit)" in powered
    assert "flags |= UNIT_LOOK_SUSPENDED;" in powered
    assert "#define UNIT_LOOK_SUSPENDED 0x0200" in coop
    client = function(coop, "network_coop_handle_object_looks")
    assert "boolean suspended = (look->pad & UNIT_LOOK_SUSPENDED) != 0;" in client
    assert "SET_FLAG(unit->unit.flags, _unit_suspended_bit, suspended);" in client
    assert "unit && unit->unit.player_index == NONE &&" in client


def test_scenery_animations_reach_a_copy_made_since():
    # a script's scenery is made on a client a moment after the host (the
    # object names), or before a late joiner came: its animation is held for
    # it and resent every refresh (The Maw's first flyby)
    coop = source("port/linux/game/network_coop.c")
    resync = function(coop, "host_resync_scenery_animations")
    assert "if (!host_resend.refresh)" in resync
    assert "event_new(_coop_event_scenery_animation_resync)" in resync
    assert "host_resync_scenery_animations();" in function(coop, "network_coop_host_tick")
    apply = function(coop, "client_apply_scenery_animation")
    assert "if (!client_start_scenery_animation(event, 0, resync))\n\t\tclient_hold_scenery_animation(event);" in apply
    start = function(coop, "client_start_scenery_animation")
    assert "scenery->object.object.animation.state.index == event->value" in start
    assert "frame = event->frame + frames_since;" in start
    assert "client_play_held_scenery_animations();" in function(coop, "network_coop_client_tick")
    events = function(coop, "client_apply_event")
    assert "case _coop_event_scenery_animation_resync:\n\t\tclient_apply_scenery_animation(event, TRUE);" in events
