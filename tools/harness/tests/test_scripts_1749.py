"""Build 1749's engine globals that this build has as functions (custom_edition_scripts.c): a10.map's cryo bay sets
and reads player0_look_invert_pitch, which become calls of player0_look_invert_pitch and
player0_look_pitch_is_inverted; the reader's name is written over an inert global's, and nothing changes for Custom
Edition maps."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, constant, enum_with, function, mutated, read, run, structure  # noqa: E402

SOURCE = "port/linux/game/custom_edition_scripts.c"

CASES = ["set-made-call", "set-value-used", "read-made-call", "read-without-room", "name-still-read", "array-full",
         "custom-edition-unchanged"]

# the conversion's functions, in their order in the source
FUNCTIONS = ("script_string_get", "missing_value", "node_make_inert", "build_1749_global_get", "string_name_bytes",
             "node_text_read", "string_read", "build_1749_name_offset", "node_new", "build_1749_global_set_convert",
             "build_1749_global_read_convert", "engine_global_missing", "function_call_convert",
             "engine_global_convert", "scripts_convert")

# a fault in custom_edition_scripts.c, the function it is in, and the case that must catch it
NEGATIVE_CONTROLS = {
    "global-node-kept": (("\tglobal->datum_header = 0;\n", ""), "build_1749_global_set_convert", "set-made-call"),
    "set-value-ignored": (("|| function->return_type != call->type ", ""), "build_1749_global_set_convert",
                          "set-value-used"),
    "count-not-raised": (("\tsyntax->count++;\n", ""), "node_new", "read-made-call"),
    "name-left-as-the-global": (("build_1749_name_offset(conversion, entry->reader)",
                                 "build_1749_name_offset(conversion, entry->name)"),
                                "build_1749_global_read_convert", "read-made-call"),
    "name-written-while-read": (("||\n\t\t\tstring_read(conversion, first, first + (long)strlen(string))", ""),
                                "build_1749_name_offset", "name-still-read"),
    "past-the-array": (("if (absolute_index >= syntax->maximum_count || !identifier)", "if (!identifier)"),
                       "node_new", "array-full"),
    "custom-edition-too": (("if (!conversion->build_1749 || !name)", "if (!name)"), "build_1749_global_get",
                           "custom-edition-unchanged"),
}


def generated(fault=None, faulty_function=None):
    hs = read("source/hs/hs.h")
    source = read(SOURCE)
    types = enum_with(hs, "_hs_unparsed = 0") + "\n"
    types += "#define hs_type_valid(type) ((type)>=_hs_type_void && (type)<NUMBER_OF_HS_TYPES)\n"
    types += structure(read("source/hs/hs_scenario_definitions.h"), "hs_syntax_node") + "\n"
    types += structure(read("source/memory/data.h"), "data_array") + "\n"
    types += structure(hs, "hs_function_definition") + "\n"

    text = enum_with(source, "_hs_syntax_node_primitive_bit") + "\n"
    for name in ("HS_EXTERNAL_GLOBAL_DESIGNATOR_BIT", "MAXIMUM_MISSING_NAME_MESSAGES", "MAXIMUM_INERT_NAMES",
                 "STRING_CONSOLE_BYTES"):
        text += f"#define {name} {constant(source, name)}\n"
    text += structure(source, "scripts_conversion") + "\n"
    text += structure(source, "build_1749_global") + "\n"
    start = source.index("static struct build_1749_global const build_1749_globals[] =")
    text += source[start:source.index("};", start) + 2] + "\n"
    for name in FUNCTIONS:
        code = function(source, name)
        if fault and name == faulty_function:
            code = mutated(code, *fault)
        text += code + "\n"
    return (("types.inc", types), ("under_test.inc", text))


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("scripts_1749", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, faulty_function, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("scripts_1749", generated(fault, faulty_function)), case)
    assert status == CHECK_FAILED, f"{SOURCE} with a fault ({control}) passed '{case}': the test cannot see it"
