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
    assert "network_coop_handle_events(entries, header.count, header.game_time,\n\t\t\t!network_connection_last_read_was_unreliable() && !distributed_handling_batch);" in distributed


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
    look = function(coop, "host_object_look")
    assert "look->pad = (short)(host_unit_hatches(object_index) | host_unit_powered(object_index));" in look
    looks = function(coop, "host_send_object_looks")
    assert "host_object_look(iterator.index, object, &now);" in looks
    assert "changing = csmemcmp(&host_sent_looks[absolute_index].look, &now, sizeof(now)) != 0;" in looks
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
    assert "else\n\t{\n\t\tclient_view_hold();" in play
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
    assert "#define HALO_PORT_NETWORK_VERSION 57" in source("port/linux/include/halo_port_limits.h")


def test_a_vehicle_a_recording_flies_is_powered_on_the_client_too():
    # The Maw's banshee_cinematic: recording_play makes it actively controlled
    # on the host, its seat powers up and its canards turn red; the client's
    # copy, moved by the host's states, stayed blue at rest
    coop = source("port/linux/game/network_coop.c")
    powered = function(coop, "host_unit_powered")
    assert "unit->object.type == _object_type_vehicle && TEST_FLAG(unit->unit.flags, _unit_actively_controlled_bit)" in powered
    look = function(coop, "host_object_look")
    assert "host_unit_powered(object_index)" in look
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
    assert "if (!client_start_scenery_animation(event, 0, resync))\n\t\tclient_hold_event(event);" in apply
    start = function(coop, "client_start_scenery_animation")
    assert "scenery->object.object.animation.state.index == event->value" in start
    assert "frame = event->frame + frames_since +" in start
    assert "client_play_held_events();" in function(coop, "network_coop_client_tick")
    events = function(coop, "client_apply_event")
    assert "case _coop_event_scenery_animation_resync:\n\t\tclient_apply_scenery_animation(event, TRUE);" in events


def test_a_cutscene_character_sits_and_looks_as_the_hosts():
    # a10's crewmen in their chairs (vehicle_load_magic), Cortana's face
    # (unit_set_emotion), stances (unit_set_seat), idles: a biped neither a
    # player's nor an actor's, which nothing else sets on a client
    coop = source("port/linux/game/network_coop.c")
    look = function(coop, "host_object_look")
    assert "unit->object.type != _object_type_biped || unit->unit.player_index != NONE" in look
    assert "unit->unit.actor_index != NONE" in look
    assert "look->emotion_index = unit->unit.animation.emotion_index;" in look
    assert "look->emotion_animation_index = unit->unit.override_emotion_animation_index;" in look
    assert "look->magic_seat_index = unit->unit.magic_seat_index;" in look
    assert "look->vehicle_index = unit->object.parent_object_index;" in look
    assert "look->animation_index = unit->object.animation.state.index;" in look
    apply = function(coop, "client_apply_character_look")
    assert "unit_set_emotion(object_index, (word)(short)look->emotion_index);" in apply
    assert "unit->unit.override_emotion_animation_index = look->emotion_animation_index;" in apply
    assert "unit->unit.magic_seat_index = look->magic_seat_index;" in apply
    assert "network_objects_set_seat(object_index, look->vehicle_index, look->seat_index);" in apply
    assert "unit_port_set_base_animation(object_index, unit->object.animation.animation_graph_index, look->animation_index);" in apply
    client = function(coop, "network_coop_handle_object_looks")
    assert "client_apply_character_look(object_index, character, look);" in client
    units = source("source/units/units.c")
    base = function(units, "unit_port_set_base_animation")
    assert "unit->unit.animation.state == _unit_state_user_animation" in base


def test_an_item_the_scripts_hang_on_an_object_is_shown():
    # c40's Index: a copy of an item the host had attached waits hidden for an
    # inventory; hung by a script, nothing else shows it
    coop = source("port/linux/game/network_coop.c")
    show = function(coop, "client_show_attached_item")
    assert "_item_attached_to_unit_bit" in show
    assert "object_set_visibility(child_index, TRUE);" in show
    attach = function(coop, "client_try_attach")
    assert attach.count("client_show_attached_item(child_index);") == 2


def test_an_event_reaching_a_client_before_its_objects_is_held():
    # the batons and the Index came a moment late: their unit's animation or
    # their attach played before the client had the objects, and was lost
    # until a refresh
    coop = source("port/linux/game/network_coop.c")
    assert "if (!client_start_unit_animation(event, 0))\n\t\tclient_hold_event(event);" in function(coop, "client_apply_unit_animation")
    assert "if (!client_try_attach(event))\n\t\tclient_hold_event(event);" in function(coop, "client_apply_attach")
    held = function(coop, "client_play_held_events")
    assert "done = client_start_unit_animation(event, since);" in held
    assert "done = client_try_attach(event);" in held
    assert "done = client_start_scenery_animation(event, since, FALSE);" in held
    start = function(coop, "client_start_unit_animation")
    assert "frame = event->frame + frames_since +" in start


def test_a_units_weapon_is_in_hand_when_the_hosts_is():
    # a custom animation begun before the host's inventory came held the
    # marine's batons back until it ended
    objects = source("port/linux/game/network_objects.c")
    apply = function(objects, "distributed_client_apply_inventory")
    assert "if (!local)\n\t\t\tunit_port_ready_desired_weapon(inventory->unit_index);" in apply
    units = source("source/units/units.c")
    ready = function(units, "unit_port_ready_desired_weapon")
    assert "unit_ready_desired_weapon(unit_index, TRUE);" in ready
    assert "unit_animation_busy" not in ready


def test_states_play_with_the_camera_through_a_cutscene():
    # Sgt Johnson stepped back and forth: states applied as they came while
    # the camera played two ticks behind
    distributed = source("port/linux/game/network_distributed.c")
    hold = function(distributed, "distributed_hold_state_message")
    assert "!network_coop_client_cutscene()" in hold
    assert "header->game_time <= played_time" in hold
    play = function(distributed, "network_distributed_play_held_states")
    assert "distributed_held_states[index].host_time <= played_time" in play
    apply = function(distributed, "distributed_apply_state_message")
    assert "if (checkpoint_epoch != distributed_checkpoint_epoch)\n\t\treturn;" in apply
    handle = function(distributed, "network_distributed_handle_message")
    # the objects', the actors' and the looks' (a character's seat, face, idle)
    assert handle.count("distributed_hold_state_message(machine_index, &header, entries, header.count,") == 3
    coop = source("port/linux/game/network_coop.c")
    tick = function(coop, "network_coop_client_tick")
    assert tick.index("client_play_presentation();") < tick.index("network_distributed_play_held_states();")


def test_cutscene_animations_start_on_the_hosts_frame():
    # the host's scripts start an animation before its objects step it a
    # frame on in the same tick; every cutscene animation ran a frame behind
    coop = source("port/linux/game/network_coop.c")
    assert "#define EVENT_ANIMATION_FRAMES_STEPPED 1" in coop
    unit = function(coop, "client_start_unit_animation")
    assert "(held_event_kind(event->kind) == event->kind ? EVENT_ANIMATION_FRAMES_STEPPED : 0)" in unit
    scenery = function(coop, "client_start_scenery_animation")
    assert "_coop_event_scenery_animation_resync ? 0 : EVENT_ANIMATION_FRAMES_STEPPED" in scenery


def test_what_a_cutscene_films_is_sent_every_tick():
    # Sgt Johnson, walked by a recording on the host, slowed between states
    # sent every other tick and was pulled on by the next
    objects = source("port/linux/game/network_objects.c")
    period = function(objects, "distributed_host_object_period")
    assert "if (nearest < FAR_OBJECT_DISTANCE * FAR_OBJECT_DISTANCE)\n\t\t\treturn 1;" in period


def test_network_coop_confirmations_are_centred():
    # REVERT TO SAVED's and QUIT's confirmations, split screen's, sat where a
    # half screen's viewport has them
    tags = source("port/linux/game/menu_tags.c")
    rows = function(tags, "pause_coop_rows")
    assert "pause_coop_centre_dialogs(split_rows[row].widget_tag.index, revert, quit, leave, centred, &centred_count, 0);" in rows
    centre = function(tags, "pause_coop_centre_dialog")
    assert "dy = (short)((480 - (top + bottom)) / 2);" in centre
    assert "children[child].vertical_offset = (short)(children[child].vertical_offset + dy);" in centre


def test_the_clients_clock_follows_the_hosts_through_a_cutscene():
    # two of the host's ticks played in one jerked the camera about every
    # seven seconds (Sgt Johnson), and left animations a frame off
    coop = source("port/linux/game/network_coop.c")
    play = function(coop, "client_play_presentation")
    assert "game_time_port_nudge(PRESENTATION_CLOCK_NUDGE);" in play
    assert "game_time_port_nudge(-PRESENTATION_CLOCK_NUDGE);" in play
    assert "lag > PRESENTATION_DELAY_TICKS + (cutscene ? 3 : 1)" in play
    assert "client_shift_animations(1);" in play and "client_shift_animations(-1);" in play
    game_time = source("source/game/game_time.c")
    nudge = function(game_time, "game_time_port_nudge")
    assert "0.95f * tick" in nudge


def test_a_scripted_vehicles_hatches_and_functions_are_the_hosts():
    # The Maw's Banshee: closed by the scripts as it is made, open on a client
    # that made it after; its canards blue for want of the recording's controls
    coop = source("port/linux/game/network_coop.c")
    events = function(coop, "client_apply_event")
    assert "if (!client_start_unit_open(event, 0))\n\t\t\tclient_hold_event(event);" in events
    held = function(coop, "client_play_held_events")
    assert "done = client_start_unit_open(event, since);" in held
    look = function(coop, "host_object_look")
    assert "look->function_values[function] =" in look
    client = function(coop, "network_coop_handle_object_looks")
    assert "client_set_function_values(object_index, powered ? look->function_values : NULL);" in client
    objects = source("source/objects/objects.c")
    assert "network_coop_object_function_values(object_index, object->object.incoming_function_values);" in objects


def test_a_still_camera_is_not_tied_to_a_vehicle_rocking_by_it():
    # Sgt Johnson's close-ups: a still camera tied on the client to a Warthog
    # rocking at rest in the hangar, shaking with it
    coop = source("port/linux/game/network_coop.c")
    get = function(coop, "camera_object_get")
    assert get.count("camera_moves_with(&camera_velocity, object)") == 2
    moves = function(coop, "camera_moves_with")
    assert "magnitude_squared3d(&difference) < speed_squared / 16.0f" in moves


def test_the_scripts_reflection_tint_reaches_the_client():
    # d20's opening: the Chief's visor black and armour flat on the client
    coop = source("port/linux/game/network_coop.c")
    send = function(coop, "host_send_reflection_tint")
    assert "event_new(_coop_event_reflection_tint)" in send
    assert "host_resend.refresh" in send
    assert "host_send_reflection_tint();" in function(coop, "network_coop_host_tick")
    events = function(coop, "client_apply_event")
    assert "rasterizer_model_ambient_reflection_tint(alpha, event->reals[0], event->reals[1], event->reals[2]);" in events


def test_restart_level_restarts_every_machine():
    handlers = source("source/interface/ui_widget_event_handler_functions.c")
    restart = function(handlers, "pause_game_restart_level")
    assert "return network_coop_pause_request(_coop_pause_request_restart);" in restart
    coop = source("port/linux/game/network_coop.c")
    assert "main_port_coop_restart();" in function(coop, "host_pause_request")
    main = source("source/main/main.c")
    assert "network_game_server_port_cooperative_restart();" in main
    server = source("source/networking/network_server_manager.c")
    assert "network_game_server_port_cooperative_won(server->game.map.name);" in function(server, "network_game_server_port_cooperative_restart")
    assert "network_game_server_begin_game_start_countdown(server, 3000);" in function(server, "network_game_server_port_cooperative_restart_pregame")
    tags = source("port/linux/game/menu_tags.c")
    rows = function(tags, "pause_coop_rows")
    assert "widget_runs_function(split_rows[row].widget_tag.index, restart, 0) &&" not in rows


def test_a_vehicle_the_scripts_fly_is_kept_active_on_the_client():
    # The Maw's Banshee, out in space: recording_play keeps its unit from
    # deactivating on the host; the client's copy deactivated outside the
    # level and was never updated (canopy, seat power, canards)
    recorded = source("source/cutscene/recorded_animations.c")
    assert "object_set_automatic_deactivation(unit_index, FALSE);" in recorded
    coop = source("port/linux/game/network_coop.c")
    client = function(coop, "network_coop_handle_object_looks")
    assert "object_set_automatic_deactivation(object_index, FALSE);" in client
    assert "else if (!powered && client_function_values_held(object_index))\n\t\t\t{\n\t\t\t\tobject_set_automatic_deactivation(object_index, TRUE);" in client


def test_a_skipped_cutscenes_audio_stops_on_the_client():
    # either player's skip is the host's revert, which stops every sound
    # there; a client stopped only its last sixteen script lines
    coop = source("port/linux/game/network_coop.c")
    stop = function(coop, "client_stop_script_sounds")
    assert "sound_port_stop_impulses();" in stop
    # every script loop cut at once: stopped as the scripts stop one, a
    # one-shot (the cutscene's music and foley) played on to its end
    assert "game_sound_port_cut_scripted_loops();" in stop
    manager = source("source/sound/sound_manager.c")
    assert "if (sound_get(sound_index)->type == _sound_impulse)\n\t\t\tsound_stop(sound_index);" in function(manager, "sound_port_stop_impulses")
    cut = function(manager, "sound_port_cut_looping")
    assert "if (sound->type != _sound_impulse && sound->source_identifier == looping_sound_index)\n\t\t\tsound_stop(sound_index);" in cut
    assert "datum_delete(looping_sound_data, looping_sound_index);" in cut
    game_sound = source("source/sound/game_sound.c")
    loops = function(game_sound, "game_sound_port_cut_scripted_loops")
    assert loops.index("sound_port_cut_looping(looping_sound_index);") < loops.index("game_looping_sound_delete(looping_sound_index);")
    assert "game_sound_port_stop_scripted_one_shots" not in game_sound


def test_held_items_reach_the_client_with_their_unit():
    # a cutscene arms its cast in a tick: the retried inventories overflowed,
    # and a lost datagram waited a second for the refresh
    objects = source("port/linux/game/network_objects.c")
    assert "MAXIMUM_CLIENT_RETRIED_INVENTORIES = 128," in objects
    assert "INVENTORY_REPEAT_ROUNDS * INVENTORY_INTERVAL_TICKS" in objects


def test_a_reverted_hosts_events_and_inventories_go_reliably():
    # a client drops the new world's datagrams until the checkpoint and the
    # snapshot ahead of them on the stream have come; for a few seconds after
    # a revert (a skip) the host's events (a skipped cutscene's music stop,
    # attachments) and inventories (the weapons its scripts gave) go reliably
    coop = source("port/linux/game/network_coop.c")
    reverted = function(coop, "network_coop_reverted")
    assert reverted.index("network_distributed_reverted();") < reverted.index("host_settling.from = game_time_get();")
    assert "host_settling.until = host_settling.from + REVERT_RELIABLE_TICKS;" in reverted
    settling = function(coop, "network_coop_host_settling")
    assert "now >= host_settling.from && now < host_settling.until" in settling
    send = function(coop, "host_send_events")
    assert send.index("if (network_coop_host_settling())") < send.index("send_to_clients(")
    assert "_distributed_to_clients_reliably);" in send
    handle = function(coop, "network_coop_handle_events")
    assert "if (!later && !reliable)" in handle
    objects = source("port/linux/game/network_objects.c")
    inventories = function(objects, "distributed_host_send_inventories")
    assert "boolean reliably = network_coop_host_settling();" in inventories
    assert "distributed_host_send_inventory_message(machine_index, &message, count, reliably);" in inventories
    message = function(objects, "distributed_host_send_inventory_message")
    assert "distributed_send_to_machine_reliably(machine_index, message, _distributed_message_inventories, count, size);" in message
    distributed = source("port/linux/game/network_distributed.c")
    stale = function(distributed, "distributed_message_stale")
    assert stale.index("header->type == _distributed_message_coop_events &&") < stale.index("latest = &distributed_received_times")


def test_a_scene_traces_each_units_weapon_in_hand_every_tick():
    coop = source("port/linux/game/network_coop.c")
    scene = function(coop, "coop_trace_scene")
    assert scene.index("coop_trace_held(who, time);") < scene.index("if (!scene || time % SCENE_TRACE_TICKS)")
    assert "co-op held (%s) %ld: %s %s %ld has %s %ld in hand%s" in function(coop, "coop_trace_held")


def test_named_objects_reach_a_client_as_they_change():
    # a client makes a named prop when two messages running say so: sent every
    # half second only, a10's hangar marine's batons came up to a second late
    coop = source("port/linux/game/network_coop.c")
    names = function(coop, "host_send_object_names")
    assert "if (csmemcmp(names, &sent, sizeof(sent)) != 0)" in names
    assert "(changed_time == NONE || now - changed_time > 1))" in names
    assert "\thost_send_object_names();\n" in function(coop, "network_coop_host_tick")
