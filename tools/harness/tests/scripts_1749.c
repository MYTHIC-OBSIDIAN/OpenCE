/*
SCRIPTS_1749.C (test)

The conversion of build 1749's maps' scripts
(port/linux/game/custom_edition_scripts.c) where 1749 had an engine global
that this build has as functions: a10.map's cryo bay sets
player0_look_invert_pitch to try the player's look inverted, and reads it,
where this build calls player0_look_invert_pitch with the value and
player0_look_pitch_is_inverted. A set is made a call in place and its
global's node freed; a read, a call with a new node for its function's name,
whose text is written over the name of an engine global made inert (the
scripts' loading finds functions by name). Custom Edition maps' scripts are
converted as before. test_scripts_1749.py takes the types and structures
(types.inc) and the code (under_test.inc).
*/

/* (the harness's own data arrays are not the game's) */
#define data_array harness_data_array
#include "harness.h"
#undef data_array

#define TAG_STRING_LENGTH 31
#define SHORT_BITS 16
#define NUMBEROF(array) (sizeof(array) / sizeof((array)[0]))
#define _error_silent 0

typedef boolean (*hs_function_parse_proc)(short function_index, long expression_index);
typedef void (*hs_evaluate_procedure)(short function_index, long thread_index, boolean initialize);

#include "types.inc"

/* the log: how many lines */
static int log_lines;
#define error(priority, ...) ((void)(priority), log_lines++)

/* this build's functions (hs.c's, those the scripts here call) */
static boolean hs_macro_function_parse(short function_index, long expression_index)
{
	return function_index >= 0 && expression_index != NONE;
}

enum
{
	FUNCTION_BEGIN,
	FUNCTION_IF,
	FUNCTION_SET,
	FUNCTION_NOT,
	FUNCTION_INVERT_PITCH,
	FUNCTION_PITCH_IS_INVERTED,
	FUNCTION_SLEEP,
	NUMBER_OF_FUNCTIONS
};

static struct hs_function_definition functions[NUMBER_OF_FUNCTIONS] =
{
	{ _hs_passthrough, 0, "begin", NULL, NULL, NULL, NULL, 0, { 0 } },
	{ _hs_passthrough, 0, "if", NULL, NULL, NULL, NULL, 0, { 0 } },
	{ _hs_passthrough, 0, "set", NULL, NULL, NULL, NULL, 0, { 0 } },
	{ _hs_type_boolean, 0, "not", hs_macro_function_parse, NULL, NULL, NULL, 1, { _hs_type_boolean } },
	{ _hs_type_void, 0, "player0_look_invert_pitch", hs_macro_function_parse, NULL, NULL, NULL, 1, { _hs_type_boolean } },
	{ _hs_type_boolean, 0, "player0_look_pitch_is_inverted", hs_macro_function_parse, NULL, NULL, NULL, 0, { 0 } },
	{ _hs_type_void, 0, "sleep", hs_macro_function_parse, NULL, NULL, NULL, 1, { _hs_type_short_integer } },
};

/* this build's engine globals (hs_globals_external.c's): not 1749's two */
static char const *const engine_globals[] = { "cheat_deathless_player", "player0_look_pitch_rate" };

static struct hs_function_definition *hs_function_get(short function_index)
{
	return &functions[function_index];
}

static short hs_find_function_by_name(char const *name)
{
	short function_index;

	for (function_index = 0; function_index < NUMBER_OF_FUNCTIONS; function_index++)
	{
		if (!strcmp(functions[function_index].name, name))
			return function_index;
	}
	return NONE;
}

static short hs_find_global_by_name(char const *name)
{
	short global_index;

	for (global_index = 0; global_index < (short)NUMBEROF(engine_globals); global_index++)
	{
		if (!strcmp(engine_globals[global_index], name))
			return (short)(global_index | 0x8000);
	}
	return NONE;
}

#include "under_test.inc"

/* ---------- a map's scripts */

#define MAXIMUM_NODES 64
#define CONSOLE_BYTES 0x400
#define FIRST_IDENTIFIER ((short)0xE000)

#define PRIMITIVE FLAG(_hs_syntax_node_primitive_bit)
#define GLOBAL FLAG(_hs_syntax_node_global_bit)
#define PERMANENT FLAG(_hs_syntax_node_permanent_bit)

/* the syntax data as the map holds it: the array, its nodes after it */
static struct
{
	struct data_array array;
	struct hs_syntax_node nodes[MAXIMUM_NODES];
} syntax;

/* the string data, and the console's bytes at its end */
static char strings[512 + CONSOLE_BYTES];
static long string_bytes;

static void scripts_begin(short maximum_count)
{
	memset(&syntax, 0, sizeof(syntax));
	strcpy(syntax.array.name, "script node");
	syntax.array.maximum_count = maximum_count;
	syntax.array.size = sizeof(struct hs_syntax_node);
	syntax.array.valid = TRUE;
	syntax.array.signature = 0x64407440;
	syntax.array.next_identifier = FIRST_IDENTIFIER;
	memset(strings, 0, sizeof(strings));
	/* (an empty string first, as the compiler writes) */
	string_bytes = 1;
}

/* a string's offset, written once */
static long string(char const *text)
{
	long offset;

	for (offset = 0; offset < string_bytes; offset += (long)strlen(strings + offset) + 1)
	{
		if (!strcmp(strings + offset, text))
			return offset;
	}
	strcpy(strings + offset, text);
	string_bytes = offset + (long)strlen(text) + 1;
	return offset;
}

static long datum_index(long absolute_index)
{
	return (long)(unsigned short)syntax.nodes[absolute_index].datum_header << 16 | absolute_index;
}

static struct hs_syntax_node *node(long index)
{
	return &syntax.nodes[DATUM_INDEX_TO_ABSOLUTE_INDEX(index)];
}

/* a node after those made, as the compiler made them */
static long node_add(short type, short flags, long string_offset, long data)
{
	long absolute_index = syntax.array.count;
	struct hs_syntax_node *added = &syntax.nodes[absolute_index];

	added->datum_header = syntax.array.next_identifier++;
	added->type = type;
	added->flags = (short)(flags | PERMANENT);
	added->next_node_index = NONE;
	added->string_offset = string_offset;
	added->data = data;
	syntax.array.count++;
	syntax.array.actual_count++;
	return datum_index(absolute_index);
}

/* arguments linked in order */
static void link_arguments(long first, long const *arguments, int count)
{
	int index;
	long previous = first;

	for (index = 0; index < count; index++)
	{
		node(previous)->next_node_index = arguments[index];
		previous = arguments[index];
	}
}

/* (function arguments...): its name node first, by the function's name */
static long call(short function_index, short type, long const *arguments, int count)
{
	long name = node_add(_hs_function_name, PRIMITIVE, string(functions[function_index].name), 0);
	long made = node_add(type, 0, 9999, name);

	node(made)->function_index = function_index;
	node(name)->function_index = function_index;
	link_arguments(name, arguments, count);
	return made;
}

/* a reference to an engine global (1749's index: 1749's designator) */
static long engine_global(char const *name, short type, short designator)
{
	return node_add(type, PRIMITIVE | GLOBAL, string(name), (unsigned short)(designator | 0x8000));
}

static long constant_boolean(boolean value)
{
	long made = node_add(_hs_type_boolean, PRIMITIVE, 0, value);

	node(made)->constant_type = _hs_type_boolean;
	return made;
}

static long constant_string(char const *text)
{
	long made = node_add(_hs_type_string, PRIMITIVE, string(text), 0);

	node(made)->constant_type = _hs_type_string;
	return made;
}

/* the scripts converted, 1749's or Custom Edition's */
static boolean convert(boolean build_1749)
{
	/* (the console's bytes past the strings) */
	long bytes = string_bytes + CONSOLE_BYTES;

	return scripts_convert(build_1749 ? "build 1749" : "custom edition", build_1749, "levels\\a10\\a10",
		&syntax.array, sizeof(syntax.array) + (unsigned long)syntax.array.maximum_count * sizeof(struct hs_syntax_node),
		strings, (unsigned long)bytes, 0x40000000);
}

/* (set player0_look_invert_pitch value), with set of the given type */
static long set_invert_pitch(boolean value, short type, long *global, long *value_node)
{
	long arguments[2];

	arguments[0] = *global = engine_global("player0_look_invert_pitch", _hs_type_boolean, 0x1A0);
	arguments[1] = *value_node = constant_boolean(value);
	return call(FUNCTION_SET, type, arguments, 2);
}

/* (if <read> (sleep 1)) */
static long if_read(long read)
{
	long arguments[2];
	long sleep_argument = node_add(_hs_type_short_integer, PRIMITIVE, 0, 1);

	node(sleep_argument)->constant_type = _hs_type_short_integer;
	arguments[0] = read;
	arguments[1] = call(FUNCTION_SLEEP, _hs_type_void, &sleep_argument, 1);
	return call(FUNCTION_IF, _hs_type_void, arguments, 2);
}

/* (not player0_alternate_player_control): 1749's global this build lacks
in every form */
static long alternate_read(long *global)
{
	*global = engine_global("player0_alternate_player_control", _hs_type_boolean, 0x1A1);
	return call(FUNCTION_NOT, _hs_type_boolean, global, 1);
}

/* whether a read has been made a call of player0_look_pitch_is_inverted:
the reference made a call in place, its name a new node */
static void check_read_made_call(long read, short old_count)
{
	struct hs_syntax_node const *made = node(read);
	struct hs_syntax_node const *name = node(made->data);

	CHECK(made->flags == PERMANENT, "the read's flags %d: not a call's", made->flags);
	CHECK(made->function_index == FUNCTION_PITCH_IS_INVERTED, "the read calls function %d", made->function_index);
	CHECK(made->type == _hs_type_boolean, "the read's type %d changed", made->type);
	CHECK(DATUM_INDEX_TO_ABSOLUTE_INDEX(made->data) >= old_count, "the name node is not a new one");
	CHECK(made->data == datum_index(DATUM_INDEX_TO_ABSOLUTE_INDEX(made->data)), "the name node's index %08lx is not its "
		"datum's", made->data);
	CHECK(name->type == _hs_function_name && name->flags == (PRIMITIVE | PERMANENT), "the name node's type %d flags %d",
		name->type, name->flags);
	CHECK(name->function_index == FUNCTION_PITCH_IS_INVERTED, "the name node names function %d",
		name->function_index);
	CHECK(name->next_node_index == NONE, "the call has arguments");
	CHECK(name->string_offset >= 0 && name->string_offset < string_bytes &&
		!strcmp(strings + name->string_offset, "player0_look_pitch_is_inverted"),
		"the name node's text is not the function's: the scripts' loading finds it by name");
}

/* whether a read has been made inert: false */
static void check_read_inert(long read)
{
	struct hs_syntax_node const *made = node(read);

	CHECK(made->flags == (PRIMITIVE | PERMANENT), "the read's flags %d: not a constant's", made->flags);
	CHECK(made->constant_type == _hs_type_boolean && made->data == FALSE, "the read is not false");
}

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";

	/* (set player0_look_invert_pitch true) is (player0_look_invert_pitch
	true): the set's name node names the function, its argument is the
	value, and the global's node is freed */
	CASE("set-made-call")
	{
		long global, value;
		long made;
		short actual_count;

		scripts_begin(MAXIMUM_NODES);
		made = set_invert_pitch(TRUE, _hs_type_void, &global, &value);
		actual_count = syntax.array.actual_count;
		CHECK(convert(TRUE), "the scripts were refused");
		CHECK(node(made)->function_index == FUNCTION_INVERT_PITCH, "the set calls function %d",
			node(made)->function_index);
		CHECK(node(node(made)->data)->function_index == FUNCTION_INVERT_PITCH, "its name node names function %d",
			node(node(made)->data)->function_index);
		CHECK(!strcmp(strings + node(node(made)->data)->string_offset, "player0_look_invert_pitch"),
			"its name node's text is '%s'", strings + node(node(made)->data)->string_offset);
		CHECK(node(node(made)->data)->next_node_index == value, "its argument is not the value");
		CHECK(node(global)->datum_header == 0, "the global's node, linked no more, is not freed");
		CHECK(syntax.array.actual_count == actual_count - 1, "%d nodes in use, not %d", syntax.array.actual_count,
			actual_count - 1);
		CHECK(syntax.array.first_free_absolute_index == DATUM_INDEX_TO_ABSOLUTE_INDEX(global),
			"the first free node is %d, not the freed one", syntax.array.first_free_absolute_index);
		return 0;
	}

	/* a set whose value is used (of the global's type): this build's
	function returns none, so it is made inert */
	CASE("set-value-used")
	{
		long global, value;
		long made;

		scripts_begin(MAXIMUM_NODES);
		made = set_invert_pitch(TRUE, _hs_type_boolean, &global, &value);
		CHECK(convert(TRUE), "the scripts were refused");
		CHECK(node(made)->flags == (PRIMITIVE | PERMANENT) && node(made)->constant_type == _hs_type_boolean,
			"the set was not made inert");
		CHECK(node(global)->datum_header != 0, "the global's node was freed");
		return 0;
	}

	/* a10's: a read is a call of player0_look_pitch_is_inverted, its name
	written over player0_alternate_player_control's, made inert; a second
	read finds it there */
	CASE("read-made-call")
	{
		long alternate, global, value;
		long read, second_read;
		long alternate_name;
		short count, actual_count, next_identifier;

		scripts_begin(MAXIMUM_NODES);
		if_read(alternate_read(&alternate));
		set_invert_pitch(TRUE, _hs_type_void, &global, &value);
		read = engine_global("player0_look_invert_pitch", _hs_type_boolean, 0x1A0);
		if_read(read);
		second_read = engine_global("player0_look_invert_pitch", _hs_type_boolean, 0x1A0);
		if_read(second_read);
		alternate_name = node(alternate)->string_offset;
		count = syntax.array.count;
		actual_count = syntax.array.actual_count;
		next_identifier = syntax.array.next_identifier;
		CHECK(convert(TRUE), "the scripts were refused");

		check_read_inert(alternate);
		check_read_made_call(read, count);
		check_read_made_call(second_read, count);
		CHECK(node(node(read)->data)->string_offset == alternate_name,
			"the reader's name is not where the inert global's was");
		CHECK(node(node(second_read)->data)->string_offset == alternate_name,
			"the second read's name is not the first's");
		CHECK(node(read)->data != node(second_read)->data, "two reads share a name node");
		CHECK(syntax.array.count == count + 2, "the array's count is %d, not %d", syntax.array.count, count + 2);
		CHECK(syntax.array.actual_count == actual_count + 2 - 1, "%d nodes in use, not %d",
			syntax.array.actual_count, actual_count + 1);
		CHECK(node(node(read)->data)->datum_header == next_identifier, "the new node's identifier is %04x, not "
			"the array's next, %04x", (unsigned short)node(node(read)->data)->datum_header,
			(unsigned short)next_identifier);
		CHECK(syntax.array.next_identifier == (short)(next_identifier + 2), "the array's next identifier is %04x",
			(unsigned short)syntax.array.next_identifier);
		return 0;
	}

	/* without an inert global's name to write its own over, a read is made
	inert, as any missing global is; the strings are as they were */
	CASE("read-without-room")
	{
		long read;
		char before[sizeof(strings)];

		scripts_begin(MAXIMUM_NODES);
		read = engine_global("player0_look_invert_pitch", _hs_type_boolean, 0x1A0);
		if_read(read);
		memcpy(before, strings, sizeof(strings));
		CHECK(convert(TRUE), "the scripts were refused");
		check_read_inert(read);
		CHECK(!memcmp(before, strings, sizeof(strings)), "the strings were changed");
		return 0;
	}

	/* an inert global's name another node still reads (a string with the
	same text) is not written over */
	CASE("name-still-read")
	{
		long alternate;
		long read;
		long text;

		scripts_begin(MAXIMUM_NODES);
		if_read(alternate_read(&alternate));
		text = constant_string("player0_alternate_player_control");
		if_read(text);
		read = engine_global("player0_look_invert_pitch", _hs_type_boolean, 0x1A0);
		if_read(read);
		CHECK(convert(TRUE), "the scripts were refused");
		check_read_inert(read);
		CHECK(!strcmp(strings + node(text)->string_offset, "player0_alternate_player_control"),
			"a string still read was written over: '%s'", strings + node(text)->string_offset);
		return 0;
	}

	/* a full array: no node for a reader's name, so the read is inert */
	CASE("array-full")
	{
		long alternate;
		long read;
		short count;

		scripts_begin(MAXIMUM_NODES);
		if_read(alternate_read(&alternate));
		read = engine_global("player0_look_invert_pitch", _hs_type_boolean, 0x1A0);
		if_read(read);
		syntax.array.maximum_count = count = syntax.array.count;
		CHECK(convert(TRUE), "the scripts were refused");
		check_read_inert(read);
		CHECK(syntax.array.count == count, "a node was made past the array's end");
		return 0;
	}

	/* Custom Edition maps' scripts: 1749's globals are any missing
	global, inert, and nothing is made */
	CASE("custom-edition-unchanged")
	{
		long alternate, global, value;
		long set, read;
		short count;
		char before[sizeof(strings)];

		scripts_begin(MAXIMUM_NODES);
		if_read(alternate_read(&alternate));
		set = set_invert_pitch(TRUE, _hs_type_void, &global, &value);
		read = engine_global("player0_look_invert_pitch", _hs_type_boolean, 0x1A0);
		if_read(read);
		count = syntax.array.count;
		memcpy(before, strings, sizeof(strings));
		CHECK(convert(FALSE), "the scripts were refused");
		check_read_inert(read);
		check_read_inert(alternate);
		CHECK(node(set)->flags == (PRIMITIVE | PERMANENT) && node(set)->constant_type == _hs_type_void,
			"the set was not made inert");
		CHECK(syntax.array.count == count, "nodes were made");
		CHECK(!memcmp(before, strings, sizeof(strings)), "the strings were changed");
		return 0;
	}

	fprintf(stderr, "unknown case: %s\n", case_name);
	return 2;
}
