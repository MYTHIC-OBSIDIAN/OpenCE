"""MCC maps in custom_maps: their package's path (translate_custom_mcc) and
their entries after the Custom Edition maps (custom_mcc_index); MCC's shared
resources in the mods folder (translate_shared_mcc)."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run


def generated(fault=None):
    code = '\n'.join((function(read('port/linux/src/xbox_files.c'), 'translate_custom_mcc'),
                      function(read('port/linux/src/xbox_files.c'), 'translate_shared_mcc'),
                      function(read('port/linux/game/menu_functions.c'), 'custom_mcc_index')))
    if fault:
        code = mutated(code, *fault)
    return (('under_test.inc', code),)


@pytest.mark.parametrize('case', ['paths', 'shared', 'lists'])
def test_case(case):
    status, output = run(build('custom_maps_mcc', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('fault,case', [
    # (custom_maps' own bitmaps.map given to a loose MCC map)
    (('\t\t\t\treturn 0;\n\t\t}\n\t}\n\tif (custom_maps[0])', '\t\t\t\t;\n\t\t}\n\t}\n\tif (custom_maps[0])'), 'paths'),
    # (the MCC maps counted from the wrong row)
    (('(short)(entry - custom_edition_count)', '(short)(entry - custom_edition_count + 1)'), 'lists'),
    # (the package folder's name dropped)
    (('snprintf(folder + length, sizeof(folder) - length, "/%s", local_name);', ';'), 'paths'),
    # (the shared sets left in mcc_maps\\_shared)
    (('snprintf(resolved, size, "%s/%s", platform_data_root(), local_name);',
      'snprintf(resolved, size, "%s/mcc_maps/_shared", platform_data_root());'), 'shared'),
    # (Steam's MCC install not looked in)
    (('\tif (steam_workshop_resource_path(cursor - 8, platform_data_root(), resolved, size))',
      '\tif (0)'), 'shared'),
])
def test_negative_control(fault, case):
    status, output = run(build('custom_maps_mcc', generated(fault)), case)
    assert status == CHECK_FAILED, output
