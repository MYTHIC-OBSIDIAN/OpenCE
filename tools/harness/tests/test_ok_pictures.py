"""The settings menu's and Edit Multiplayer Gametype's OK pictures follow
display.intro (menu_functions.c's profile_ok_picture, gametype_ok_picture)."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

NAMES = ('ok_picture_xbox', 'profile_ok_picture', 'gametype_ok_picture')


def generated(fault=None):
    text = read('port/linux/game/menu_functions.c')
    defines = '\n'.join(line for line in text.splitlines()
                        if line.startswith(('#define PROFILE_', '#define GAMETYPE_')))
    code = defines + '\n' + '\n'.join(function(text, name) for name in NAMES)
    if fault:
        code = mutated(code, *fault)
    return (('under_test.inc', code),)


@pytest.mark.parametrize('case', ['profile', 'gametype'])
def test_case(case):
    status, output = run(build('ok_pictures', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('fault,case', [
    (('xbox_platform ? PROFILE_XBOX_OK_FRAME : PROFILE_OK_FRAME', 'PROFILE_OK_FRAME'), 'profile'),
    (('if (frame != PROFILE_OK_FRAME && frame != PROFILE_XBOX_OK_FRAME)\n\t\t\trow_frame = frame;',
      'row_frame = frame;'), 'profile'),
    # (the rows' Xbox pictures whatever PLATFORM is, or never; CONTROLS
    # SETUP's pair left out)
    (('profile_pictures[index][xbox_platform];', 'profile_pictures[index][1];'), 'profile'),
    (('profile_pictures[index][xbox_platform];', 'profile_pictures[index][0];'), 'profile'),
    (('\t\t{ PROFILE_CONTROLS_FRAME, PROFILE_XBOX_CONTROLS_FRAME },\n', ''), 'profile'),
    # (Halo PC's picture whatever PLATFORM is; the Xbox's)
    (('gametype_pictures[index][xbox_platform];', 'gametype_pictures[index][0];'), 'gametype'),
    (('gametype_pictures[index][xbox_platform];', 'gametype_pictures[index][1];'), 'gametype'),
    # (GAME OPTIONS' or CHANGE NAME's pair left out)
    (('\t\t{ GAMETYPE_GAME_FRAME, GAMETYPE_XBOX_GAME_FRAME },\n', ''), 'gametype'),
    (('\t\t{ GAMETYPE_NAME_FRAME, GAMETYPE_XBOX_NAME_FRAME },\n', ''), 'gametype'),
])
def test_negative_control(fault, case):
    status, output = run(build('ok_pictures', generated(fault)), case)
    assert status == CHECK_FAILED, output
