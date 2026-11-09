/*
CUSTOM_EDITION_SCRIPTS.C

Makes the compiled scripts of Halo Custom Edition maps run on this build's
script interpreter (custom_edition_cache.h), and those of build 1749's maps
(halo1749.exe: port/linux/src/build_1749.c), whose functions are numbered
otherwise too: b30.map's 1,209 calls name 89 functions by 1749's numbers,
which this build has, by name, all of them.

A compiled script refers to every function it calls, and every engine
global it uses, by its index in the engine's table. Halo PC's tables aren't
this build's: Halo PC has entries this build doesn't, mixed in with the
shared ones, so past some point the same index means a different entry
(timberland.map's player_effect_start would call the function 29 places
before it).
Compiled scripts also keep every name in their string data, so each call
and engine global is looked up again here by name, with the game's own
hs_find_function_by_name and hs_find_global_by_name.

Anything this build doesn't have does nothing (missing_value). A call of a
missing function becomes a constant of the call's type, and so does a read
of a missing engine global; a set of one becomes a constant too, so nothing
is written. The constant is the type's default: nothing, false, 0, an
enumeration's first value, the empty string, or NONE for objects, tags and
the scenario's lists. Only a script index has no such default, so a map that
would need one is refused.
Both builds number their value types alike: every call in the maps examined
has the same types here (docs/custom_edition_caches.md, and
tools/custom_edition_script_names.py checks a map's calls against this
build's tables).

Build 1749 had some engine globals this build has as functions
(build_1749_globals): a10.map's cryo bay sets player0_look_invert_pitch to
try the player's look inverted, and reads it to say which was kept, where
this build calls player0_look_invert_pitch and
player0_look_pitch_is_inverted. A set of one is made a call of this
build's function with the value, in place; a read, a call of its reader
with a node of its own (the array has room past the nodes in use). The
scripts' loading finds each call's function again by its name
(hs_compile_postprocess), so a name a call needs must be in the string
data: where it is already, or written over the name of an engine global
the conversion made inert, which no node reads any more. Without either,
the global is made inert, as any other missing one is.
*/

/* ---------- headers */

#include "cseries.h"
#include "errors.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "tag_files/tag_groups.h"
#include "hs/hs.h"
#include "hs/hs_scenario_definitions.h"
#include "scenario/scenario_definitions.h"
#include "cache_file_formats.h"
#include "custom_edition_cache.h"

#include <string.h>

/* ---------- constants */

/* a syntax node's flags, as hs_runtime.c names them */
enum
{
	_hs_syntax_node_primitive_bit = 0,
	_hs_syntax_node_script_bit,
	_hs_syntax_node_global_bit,
	_hs_syntax_node_permanent_bit,
};

/* engine globals are told from the scenario's by this bit of their
designator (hs_find_global_by_name) */
#define HS_EXTERNAL_GLOBAL_DESIGNATOR_BIT 15

/* missing names are logged up to this many times */
#define MAXIMUM_MISSING_NAME_MESSAGES 8

/* the names of engine globals made inert that are kept, whose bytes a name
a call needs may be written over (build_1749_name_offset) */
#define MAXIMUM_INERT_NAMES 16

/* the bytes at the end of the string data that the console's expressions are
written to (hs_compile.c's HS_MAXIMUM_DYNAMIC_SOURCE_DATA_BYTES): no name
the scripts' loading reads starts in them (hs_verify_source_offset) */
#define STRING_CONSOLE_BYTES 0x400

/* ---------- structures */

struct scripts_conversion
{
	/* whose maps' scripts, for the log: "custom edition", "build 1749" */
	char const *who;
	/* build 1749's (build_1749_globals) */
	boolean build_1749;
	/* the array the nodes are in, whose counts a new node raises */
	struct data_array *syntax;
	struct hs_syntax_node *nodes;
	/* the nodes the map has (those made here come after them) */
	long node_count;
	char const *strings;
	unsigned long string_bytes;
	/* where the empty string at the end of the string data is (an Xbox
	address, as a string constant's value is), or 0 */
	long empty_string;
	long functions_renumbered;
	long globals_renumbered;
	/* reads and sets of build 1749's engine globals made calls */
	long globals_made_calls;
	/* where the names of engine globals made inert are in the string data */
	long inert_names[MAXIMUM_INERT_NAMES];
	short inert_name_count;
	/* what is missing and has no harmless value: the map can't run */
	long missing_count;
	long made_inert;
};

/* build 1749's engine globals that this build has as functions: a read of
one is a call of reader, a set of one a call of writer with the value
(NULL: made inert) */
struct build_1749_global
{
	char const *name;
	char const *reader;
	char const *writer;
};

/* ---------- globals */

static struct build_1749_global const build_1749_globals[] =
{
	/* (a10.map's cryo bay) */
	{ "player0_look_invert_pitch", "player0_look_pitch_is_inverted", "player0_look_invert_pitch" },
};

/* ---------- private code */

/* the string at `offset` in the script string data, or NULL unless it lies
within it */
static char const *script_string_get(
	struct scripts_conversion const *conversion,
	long offset)
{
	return offset >= 0 &&
		(unsigned long)offset < conversion->string_bytes &&
		memchr(conversion->strings + offset, 0, conversion->string_bytes - offset) ?
		conversion->strings + offset :
		NULL;
}

/* the harmless value of a value type, which a call of a function this build
lacks or a read of an engine global it lacks gives instead; FALSE for a
script's index, which has none */
static boolean missing_value(
	struct scripts_conversion const *conversion,
	short type,
	long *value)
{
	if (type >= _hs_type_void && type <= _hs_type_long_integer)
		*value = 0;
	else if (type == _hs_type_string)
		*value = conversion->empty_string;
	else if (type >= _hs_type_enum_game_difficulty && type <= _hs_type_enum_hud_corner)
		*value = 0;
	else if (type != _hs_type_script && hs_type_valid(type))
		*value = NONE;
	else
		return FALSE;
	return type != _hs_type_string || conversion->empty_string;
}

/* The node (a call, or a reference to an engine global) made a constant of
its own type, for the missing function or global `name`; logged, and
counted as missing when its type has no harmless value. */
static void node_make_inert(
	struct scripts_conversion *conversion,
	struct hs_syntax_node *node,
	char const *kind,
	char const *name)
{
	long value;
	/* (a reference to a global names it; a call's offset is its source's) */
	boolean global_reference = TEST_FLAG(node->flags, _hs_syntax_node_primitive_bit) &&
		TEST_FLAG(node->flags, _hs_syntax_node_global_bit);

	if (!missing_value(conversion, node->type, &value))
	{
		if (conversion->missing_count < MAXIMUM_MISSING_NAME_MESSAGES)
		{
			error(_error_silent, "%s: the scripts use the %s '%s', which this build does not have", conversion->who,
				kind, name);
		}
		conversion->missing_count++;
		return;
	}
	if (conversion->made_inert < MAXIMUM_MISSING_NAME_MESSAGES)
	{
		error(_error_silent, "%s: the scripts use the %s '%s', which this build does not have: it does nothing",
			conversion->who, kind, name);
	}
	/* (still permanent: hs_node_gc frees a scenario node that isn't, and the
	script then halts on it as "unused or changed") */
	node->flags = FLAG(_hs_syntax_node_primitive_bit) | (node->flags & FLAG(_hs_syntax_node_permanent_bit));
	node->constant_type = node->type;
	node->data = value;
	conversion->made_inert++;
	/* (a global's name, which nodes of other globals may name too: kept,
	once) */
	if (global_reference)
	{
		short name_index;

		for (name_index = 0; name_index < conversion->inert_name_count; name_index++)
		{
			if (conversion->inert_names[name_index] == node->string_offset)
				break;
		}
		if (name_index == conversion->inert_name_count && name_index < MAXIMUM_INERT_NAMES)
			conversion->inert_names[conversion->inert_name_count++] = node->string_offset;
	}
}

/* the build 1749 engine global called name (build_1749_globals), or NULL:
always, for other maps */
static struct build_1749_global const *build_1749_global_get(
	struct scripts_conversion const *conversion,
	char const *name)
{
	short global_index;

	if (!conversion->build_1749 || !name)
		return NULL;
	for (global_index = 0; global_index < (short)NUMBEROF(build_1749_globals); global_index++)
	{
		if (!strcmp(build_1749_globals[global_index].name, name))
			return &build_1749_globals[global_index];
	}

	return NULL;
}

/* the bytes of the string data the scripts' loading may read names from */
static unsigned long string_name_bytes(
	struct scripts_conversion const *conversion)
{
	return conversion->string_bytes > STRING_CONSOLE_BYTES ? conversion->string_bytes - STRING_CONSOLE_BYTES : 0;
}

/* whether a node's text is read where the scripts are loaded: a function's
name, a global's, or a constant of a type parsed from its text (strings,
objects' and the scenario's names: hs_compile_postprocess) */
static boolean node_text_read(
	struct hs_syntax_node const *node)
{
	return node->datum_header &&
		TEST_FLAG(node->flags, _hs_syntax_node_primitive_bit) &&
		(node->type == _hs_function_name ||
			TEST_FLAG(node->flags, _hs_syntax_node_global_bit) ||
			node->type >= _hs_type_string);
}

/* whether a node's text is read from the string data's bytes first to last */
static boolean string_read(
	struct scripts_conversion const *conversion,
	long first,
	long last)
{
	long node_index;

	for (node_index = 0; node_index < conversion->syntax->count; node_index++)
	{
		struct hs_syntax_node const *node = &conversion->nodes[node_index];

		if (node_text_read(node) && node->string_offset >= first && node->string_offset <= last)
			return TRUE;
	}

	return FALSE;
}

/* where the string data has the name a call needs: where it is, or written
over the name of an engine global made inert that no node reads any more,
and long enough; NONE if neither */
static long build_1749_name_offset(
	struct scripts_conversion *conversion,
	char const *name)
{
	unsigned long name_bytes = string_name_bytes(conversion);
	long name_length = (long)strlen(name);
	long offset;
	short name_index;

	for (offset = 0; (unsigned long)offset < name_bytes; offset++)
	{
		char const *string = script_string_get(conversion, offset);

		if (string && !strcmp(string, name))
			return offset;
	}
	for (name_index = 0; name_index < conversion->inert_name_count; name_index++)
	{
		long first = conversion->inert_names[name_index];
		char const *string = first >= 0 && (unsigned long)first < name_bytes ? script_string_get(conversion, first) : NULL;

		/* (the whole of a string, not the end of another's) */
		if (!string || (first > 0 && conversion->strings[first - 1]) || (long)strlen(string) < name_length ||
			string_read(conversion, first, first + (long)strlen(string)))
		{
			continue;
		}
		memcpy((char *)string, name, name_length + 1);
		conversion->inert_names[name_index] = conversion->inert_names[--conversion->inert_name_count];
		return first;
	}

	return NONE;
}

/* a new node after those in use (the array's count raised, as datum_new
would), zeroed but for its identifier; NONE if the array has no room */
static long node_new(
	struct scripts_conversion *conversion)
{
	struct data_array *syntax = conversion->syntax;
	short identifier = syntax->next_identifier;
	long absolute_index = syntax->count;

	if (absolute_index >= syntax->maximum_count || !identifier)
		return NONE;
	syntax->next_identifier++;
	if (!syntax->next_identifier)
		syntax->next_identifier = (short)0x8000;
	syntax->count++;
	syntax->actual_count++;
	memset(&conversion->nodes[absolute_index], 0, sizeof(struct hs_syntax_node));
	conversion->nodes[absolute_index].datum_header = identifier;

	return (long)(unsigned short)identifier << SHORT_BITS | absolute_index;
}

/* (build 1749's) the set call of an engine global made a call of the
global's writer with its value, in place: (set g v) is (writer v), and
the global's node is freed; FALSE if it can't be */
static boolean build_1749_global_set_convert(
	struct scripts_conversion *conversion,
	struct hs_syntax_node *call,
	struct hs_syntax_node *name_node,
	struct hs_syntax_node *global)
{
	struct build_1749_global const *entry =
		build_1749_global_get(conversion, script_string_get(conversion, global->string_offset));
	struct hs_function_definition const *function;
	struct hs_syntax_node const *value;
	short function_index;
	long name_offset;
	long global_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(name_node->next_node_index);

	if (!entry || !entry->writer || (function_index = hs_find_function_by_name(entry->writer)) == NONE ||
		global->next_node_index == NONE ||
		DATUM_INDEX_TO_ABSOLUTE_INDEX(global->next_node_index) >= conversion->node_count)
	{
		return FALSE;
	}
	function = hs_function_get(function_index);
	value = &conversion->nodes[DATUM_INDEX_TO_ABSOLUTE_INDEX(global->next_node_index)];
	/* (the value is its one argument, of its type; the set's value is
	unused, as the function returns none) */
	if (function->parameter_count != 1 || function->parameter_types[0] != value->type ||
		value->next_node_index != NONE || function->return_type != call->type ||
		(name_offset = build_1749_name_offset(conversion, entry->writer)) == NONE)
	{
		return FALSE;
	}
	call->function_index = function_index;
	name_node->function_index = function_index;
	name_node->string_offset = name_offset;
	name_node->next_node_index = global->next_node_index;
	/* (no longer linked: freed, as datum_delete would) */
	global->datum_header = 0;
	conversion->syntax->actual_count--;
	if (global_index < conversion->syntax->first_free_absolute_index)
		conversion->syntax->first_free_absolute_index = (short)global_index;
	conversion->globals_made_calls++;

	return TRUE;
}

/* (build 1749's) a read of an engine global made a call of its reader, in
place, with a new node for the reader's name; FALSE if it can't be */
static boolean build_1749_global_read_convert(
	struct scripts_conversion *conversion,
	struct hs_syntax_node *reference)
{
	struct build_1749_global const *entry =
		build_1749_global_get(conversion, script_string_get(conversion, reference->string_offset));
	struct hs_function_definition const *function;
	struct hs_syntax_node *name_node;
	short function_index;
	long name_offset;
	long name_index;

	if (!entry || !entry->reader || (function_index = hs_find_function_by_name(entry->reader)) == NONE)
		return FALSE;
	function = hs_function_get(function_index);
	if (function->parameter_count != 0 || function->return_type != reference->type ||
		(name_offset = build_1749_name_offset(conversion, entry->reader)) == NONE ||
		(name_index = node_new(conversion)) == NONE)
	{
		return FALSE;
	}
	name_node = &conversion->nodes[DATUM_INDEX_TO_ABSOLUTE_INDEX(name_index)];
	name_node->function_index = function_index;
	name_node->type = _hs_function_name;
	name_node->flags = FLAG(_hs_syntax_node_primitive_bit) | FLAG(_hs_syntax_node_permanent_bit);
	name_node->next_node_index = NONE;
	name_node->string_offset = name_offset;
	reference->function_index = function_index;
	reference->flags &= FLAG(_hs_syntax_node_permanent_bit);
	reference->data = name_index;
	conversion->globals_made_calls++;

	return TRUE;
}

/* whether the node refers to an engine global this build does not have */
static boolean engine_global_missing(
	struct scripts_conversion const *conversion,
	struct hs_syntax_node const *node)
{
	char const *name = script_string_get(conversion, node->string_offset);
	short designator;

	if (!name || !TEST_FLAG(node->flags, _hs_syntax_node_primitive_bit) ||
		!TEST_FLAG(node->flags, _hs_syntax_node_global_bit) ||
		!TEST_FLAG(node->short_value, HS_EXTERNAL_GLOBAL_DESIGNATOR_BIT))
	{
		return FALSE;
	}
	designator = hs_find_global_by_name(name);
	return designator == NONE || !TEST_FLAG(designator, HS_EXTERNAL_GLOBAL_DESIGNATOR_BIT);
}

/* A call names its function with its first child, which has the function's
index too; FALSE when the call is not laid out so. */
static boolean function_call_convert(
	struct scripts_conversion *conversion,
	struct hs_syntax_node *call)
{
	long name_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(call->data);
	struct hs_syntax_node *name_node;
	char const *name;
	short function_index;

	if (name_index >= conversion->node_count)
	{
		return FALSE;
	}
	name_node = &conversion->nodes[name_index];
	name = script_string_get(conversion, name_node->string_offset);
	if (!name || name_node->type != _hs_function_name)
	{
		return FALSE;
	}
	function_index = hs_find_function_by_name(name);
	if (function_index == NONE)
	{
		node_make_inert(conversion, call, "function", name);
	}
	/* (a set of an engine global this build lacks writes nothing: its first
	argument is the global, which the interpreter would write by its index) */
	else if (!strcmp(name, "set") && name_node->next_node_index != NONE &&
		DATUM_INDEX_TO_ABSOLUTE_INDEX(name_node->next_node_index) < conversion->node_count &&
		engine_global_missing(conversion,
			&conversion->nodes[DATUM_INDEX_TO_ABSOLUTE_INDEX(name_node->next_node_index)]))
	{
		/* (build 1749's: a set of one this build has as a function, a call) */
		if (build_1749_global_set_convert(conversion, call, name_node,
				&conversion->nodes[DATUM_INDEX_TO_ABSOLUTE_INDEX(name_node->next_node_index)]))
		{
			return TRUE;
		}
		node_make_inert(conversion, call, "engine global",
			script_string_get(conversion,
				conversion->nodes[DATUM_INDEX_TO_ABSOLUTE_INDEX(name_node->next_node_index)].string_offset));
	}
	else if (call->function_index != function_index)
	{
		call->function_index = function_index;
		name_node->function_index = function_index;
		conversion->functions_renumbered++;
	}

	return TRUE;
}

/* FALSE when the global's name is not in the string data */
static boolean engine_global_convert(
	struct scripts_conversion *conversion,
	struct hs_syntax_node *reference)
{
	char const *name = script_string_get(conversion, reference->string_offset);
	short designator;

	if (!name)
	{
		return FALSE;
	}
	designator = hs_find_global_by_name(name);
	if (designator == NONE || !TEST_FLAG(designator, HS_EXTERNAL_GLOBAL_DESIGNATOR_BIT))
	{
		struct build_1749_global const *entry = build_1749_global_get(conversion, name);

		/* (build 1749's read of one this build has as a function: a call,
		once every other missing global's name is free, scripts_convert) */
		if (!entry || !entry->reader)
			node_make_inert(conversion, reference, "engine global", name);
	}
	else if (reference->short_value != designator)
	{
		reference->short_value = designator;
		conversion->globals_renumbered++;
	}

	return TRUE;
}

/* FALSE after logging why when the scripts cannot run: the syntax data
(syntax_bytes at syntax), and the string data (string_bytes at strings, at
the Xbox address strings_address) of the scenario tag_name */
static boolean scripts_convert(
	char const *who,
	boolean build_1749,
	char const *tag_name,
	struct data_array *syntax,
	unsigned long syntax_bytes,
	char const *strings,
	unsigned long string_bytes,
	long strings_address)
{
	struct scripts_conversion conversion = { 0 };
	long node_index;

	if (!syntax)
	{
		return TRUE;
	}
	conversion.who = who;
	conversion.build_1749 = build_1749;
	conversion.syntax = syntax;
	conversion.strings = strings;
	conversion.string_bytes = strings ? string_bytes : 0;
	/* (every string in the data ends with a 0, so the last byte is one, read
	as the empty string) */
	if (conversion.string_bytes && !conversion.strings[conversion.string_bytes - 1])
		conversion.empty_string = strings_address + (long)conversion.string_bytes - 1;
	if (syntax_bytes < sizeof(*syntax) ||
		syntax->size != sizeof(struct hs_syntax_node) ||
		syntax->count < 0 ||
		syntax->count > syntax->maximum_count ||
		syntax_bytes < sizeof(*syntax) + (unsigned long)syntax->maximum_count * sizeof(struct hs_syntax_node))
	{
		error(_error_silent, "%s: the script syntax of '%s' is not laid out as this build's", who, tag_name);
		return FALSE;
	}
	/* the nodes follow the array (its data pointer is set there when the
	scripts are loaded, hs_scenario_postprocess), and those in use come
	first */
	conversion.nodes = (struct hs_syntax_node *)(syntax + 1);
	conversion.node_count = syntax->count;

	/* the calls first, then the engine globals: a set of a global this build
	lacks is found while its global is still a reference */
	for (node_index = 0; node_index < 2 * conversion.node_count; node_index++)
	{
		boolean globals = node_index >= conversion.node_count;
		struct hs_syntax_node *node = &conversion.nodes[node_index % conversion.node_count];
		boolean converted = TRUE;

		if (!node->datum_header)
		{
			continue;
		}
		/* (a call of one of the scenario's scripts names it by the scenario's
		own index) */
		if (!globals && !TEST_FLAG(node->flags, _hs_syntax_node_primitive_bit) &&
			!TEST_FLAG(node->flags, _hs_syntax_node_script_bit))
		{
			converted = function_call_convert(&conversion, node);
		}
		else if (globals && TEST_FLAG(node->flags, _hs_syntax_node_primitive_bit) &&
			TEST_FLAG(node->flags, _hs_syntax_node_global_bit) &&
			TEST_FLAG(node->short_value, HS_EXTERNAL_GLOBAL_DESIGNATOR_BIT))
		{
			converted = engine_global_convert(&conversion, node);
		}
		if (!converted)
		{
			error(
				_error_silent,
				"%s: script syntax node %ld of '%s' does not name what it uses",
				who,
				node_index % conversion.node_count,
				tag_name);
			return FALSE;
		}
	}
	/* then build 1749's reads of engine globals this build has as
	functions: calls of them, with the names of the globals made inert free
	to hold theirs (build_1749_name_offset) */
	for (node_index = 0; conversion.build_1749 && node_index < conversion.node_count; node_index++)
	{
		struct hs_syntax_node *node = &conversion.nodes[node_index];

		if (node->datum_header && engine_global_missing(&conversion, node) &&
			!build_1749_global_read_convert(&conversion, node))
		{
			node_make_inert(&conversion, node, "engine global", script_string_get(&conversion, node->string_offset));
		}
	}
	error(
		_error_silent,
		"%s: %ld script calls and %ld engine global references were given this build's index",
		who,
		conversion.functions_renumbered,
		conversion.globals_renumbered);
	if (conversion.globals_made_calls)
	{
		error(
			_error_silent,
			"%s: %ld reads and sets of engine globals were made calls of this build's functions",
			who,
			conversion.globals_made_calls);
	}
	if (conversion.missing_count)
	{
		error(
			_error_silent,
			"%s: the scripts use %ld functions or engine globals this build does not have, giving script indices, and cannot run",
			who,
			conversion.missing_count);
		return FALSE;
	}

	return TRUE;
}

/* FALSE after logging why when the scenario's scripts cannot run */
static boolean scenario_scripts_convert(
	byte *tag_cache,
	unsigned long loaded_bytes,
	int32_t tag_index,
	struct scenario *scenario)
{
	uint32_t syntax_bytes = 0;
	uint32_t string_bytes = 0;
	struct data_array *syntax = custom_edition_cache_data_get(tag_cache, loaded_bytes, &scenario->hs_syntax_data, &syntax_bytes);
	char const *strings = custom_edition_cache_data_get(tag_cache, loaded_bytes, &scenario->hs_string_constants, &string_bytes);

	return scripts_convert(
		"custom edition",
		FALSE,
		custom_edition_cache_tag_name(tag_cache, loaded_bytes, tag_index),
		syntax,
		syntax_bytes,
		strings,
		string_bytes,
		(long)scenario->hs_string_constants.address);
}

/* ---------- public code */

boolean custom_edition_scripts_convert(
	byte *tag_cache,
	unsigned long loaded_bytes)
{
	struct scenario *scenario;
	int32_t tag_index = NONE;

	while ((scenario = custom_edition_cache_tag_next(tag_cache, loaded_bytes, SCENARIO_TAG, sizeof(*scenario), &tag_index)) != NULL)
	{
		if (!scenario_scripts_convert(tag_cache, loaded_bytes, tag_index, scenario))
		{
			return FALSE;
		}
	}

	return TRUE;
}

boolean build_1749_scripts_convert(
	void *scenario_definition,
	char const *map_name)
{
	struct scenario *scenario = scenario_definition;

	/* (the tag validator has found both data in the tags) */
	return scripts_convert(
		"build 1749",
		TRUE,
		map_name,
		scenario->hs_syntax_data.size > 0 ? scenario->hs_syntax_data.address : NULL,
		(unsigned long)scenario->hs_syntax_data.size,
		scenario->hs_string_constants.size > 0 ? scenario->hs_string_constants.address : NULL,
		(unsigned long)(scenario->hs_string_constants.size > 0 ? scenario->hs_string_constants.size : 0),
		(long)scenario->hs_string_constants.address);
}
