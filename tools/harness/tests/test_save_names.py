"""The Xbox's save game containers (port/linux/src/xbox_xapi.c): a save is found by the name in its SaveMeta.xbx,
whatever its directory, and the name is read from this build's format and the Xbox's ("Name=" lines, as build
1749's own profile has it), so renaming 1749's profile deletes its old directory instead of listing it twice."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run  # noqa: E402

SOURCE = "port/linux/src/xbox_xapi.c"

CASES = ["dump-name-read", "xbox-name-read", "own-name-read", "rename-dump-profile", "dump-profile-exists",
         "unknown-name"]

# the code under test, in its order in the source
FUNCTIONS = ("save_name_hash", "save_data_directory", "save_directory_for", "save_read_name", "save_names_equal",
             "save_find_directory", "XCreateSaveGame", "delete_tree", "XDeleteSaveGame")

# a fault in xbox_xapi.c, the function it is in, and the case that must catch it
NEGATIVE_CONTROLS = {
    "found-by-hash-only": (("\tsave_data_directory(root, data_directory, sizeof(data_directory));\n\tsnprintf(pattern",
                            "\treturn FALSE;\n\tsave_data_directory(root, data_directory, sizeof(data_directory));\n"
                            "\tsnprintf(pattern"), "save_find_directory", "rename-dump-profile"),
    "dump-line-not-read": (("else if (read >= 5 && !memcmp(bytes, \"Name=\", 5))", "else if (0)"),
                           "save_read_name", "dump-name-read"),
    "xbox-mark-not-read": (("\t\tindex = 2;\n\t\tkeyed = TRUE;", "\t\tindex = 2;"), "save_read_name",
                           "xbox-name-read"),
    "own-name-keyed": (("\tBOOL keyed = FALSE;", "\tBOOL keyed = TRUE;"), "save_read_name", "own-name-read"),
    "any-name-found": (("save_names_equal(candidate_name, name) &&", ""), "save_find_directory", "unknown-name"),
    "find-left-open": (("\tCloseHandle(find);\n\treturn found;", "\treturn found;"), "save_find_directory",
                       "dump-profile-exists"),
}


def generated(fault=None, faulty_function=None):
    source = read(SOURCE)
    text = '#define SAVE_DATA_DIRECTORY "UDATA"\n#define SAVE_META_FILE "SaveMeta.xbx"\n'
    for name in FUNCTIONS:
        code = function(source, name)
        if fault and name == faulty_function:
            code = mutated(code, *fault)
        text += code + "\n"
    return (("under_test.inc", text),)


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("save_names", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, faulty_function, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("save_names", generated(fault, faulty_function)), case)
    assert status == CHECK_FAILED, f"{SOURCE} with a fault ({control}) passed '{case}': the test cannot see it"
