#include "harness.h"
/* The OK pictures of the settings menu and Edit Multiplayer Gametype: one
picture for both, the Xbox's with display.intro "xbox", else Halo PC's
(menu_functions.c's profile_ok_picture, gametype_ok_picture) */
#define _button_preset_standard 0
#define _button_preset_swap_b_and_right_thumb 4
/* (the edited profile's button settings, for profile_gamepad_layout) */
struct player_profile { struct { unsigned char button_preset; } controller_settings; };
static struct player_profile edited;
static struct player_profile *player_ui_get_edit_player_profile(void) { return &edited; }
#define NUMBEROF(array) (sizeof(array) / sizeof((array)[0]))
struct widget_instance
{
	char const *name;
	struct widget_instance *focused_child;
	struct { struct { struct widget_instance *extended_description; } list; } parameters;
	struct { short current_frame_index; } animation;
};
static char const *intro = "xbox";
static char const *config_string(char const *name) { (void)name; return intro; }
static struct widget_instance description, picture, bar, ok, cancel, row, list;
static struct widget_instance *named(struct widget_instance *parent, char const *name, short index)
{
	(void)index;
	return parent == &description && picture.name && !strcmp(name, picture.name) ? &picture : NULL;
}
#include "under_test.inc"
static void focus(struct widget_instance *child)
{
	list.focused_child = child == &ok || child == &cancel ? &bar : child;
	bar.focused_child = child == &ok || child == &cancel ? child : &ok;
}
int main(int argc, char **argv)
{
	const char *case_name = argc > 1 ? argv[1] : "";
	list.parameters.list.extended_description = &description;
	bar.name = "profile_edit_button_bar"; ok.name = "profile_edit_button_ok"; cancel.name = "x"; row.name = "row";
	CASE("profile") {
		short intro_index;
		static char const *const intros[] = { "xbox", "pc" };
		static const short frames[] = { 15, 9 };
		picture.name = "profile_edit_extended_desc_pic";
		for (intro_index = 0; intro_index < 2; intro_index++)
		{
			intro = intros[intro_index];
			picture.animation.current_frame_index = 3;
			focus(&row); profile_ok_picture(&list);
			CHECK(picture.animation.current_frame_index == 3, "a row's picture changed");
			focus(&ok); profile_ok_picture(&list);
			CHECK(picture.animation.current_frame_index == frames[intro_index], "%s: OK shows frame %d",
				intro, picture.animation.current_frame_index);
			profile_ok_picture(&list);
			CHECK(picture.animation.current_frame_index == frames[intro_index], "%s: OK's picture not kept", intro);
			focus(&cancel); profile_ok_picture(&list);
			CHECK(picture.animation.current_frame_index == 3, "%s: the row's not back: %d", intro,
				picture.animation.current_frame_index);
		}
		/* (the intro changed while OK has the focus: the other OK picture,
		and the row's still back after) */
		intro = "xbox"; focus(&ok); profile_ok_picture(&list);
		intro = "pc"; profile_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 9, "the intro's change not followed");
		focus(&row); profile_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 3, "the row's lost: %d", picture.animation.current_frame_index);
		/* (CHANGE NAME's: the Xbox's with PLATFORM "xbox", Halo PC's else,
		and back after OK) */
		intro = "pc"; picture.animation.current_frame_index = 0; profile_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 0, "Halo PC's CHANGE NAME picture replaced");
		intro = "xbox"; profile_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 16, "the Xbox's CHANGE NAME picture not shown: %d",
			picture.animation.current_frame_index);
		focus(&ok); profile_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 15, "OK's picture not shown from CHANGE NAME");
		focus(&row); profile_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 16, "CHANGE NAME's picture not back: %d",
			picture.animation.current_frame_index);
		intro = "pc"; profile_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 0, "Halo PC's CHANGE NAME picture not back");
		/* (CONTROLS SETUP's the same) */
		intro = "pc"; picture.animation.current_frame_index = 1; profile_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 1, "Halo PC's CONTROLS SETUP picture replaced");
		intro = "xbox"; profile_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 17, "the Xbox's CONTROLS SETUP picture not shown: %d",
			picture.animation.current_frame_index);
		intro = "pc"; profile_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 1, "Halo PC's CONTROLS SETUP picture not back");
		/* (another row's left be) */
		intro = "xbox"; picture.animation.current_frame_index = 3; profile_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 3, "row 3's picture changed");
	} else CASE("gametype") {
		short intro_row;
		picture.name = "playlist_edit_ext_desc_pic";
		intro = "xbox"; picture.animation.current_frame_index = 7; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 7, "the Xbox's OK picture replaced");
		intro = "pc"; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 11, "Halo PC's OK picture not shown");
		intro = "xbox"; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 7, "the Xbox's OK picture not back");
		picture.animation.current_frame_index = 6; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 6, "a row's picture changed");
		/* (ITEM OPTIONS: the Xbox's picture with PLATFORM "xbox", Halo PC's
		else) */
		picture.animation.current_frame_index = 3; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 8, "the Xbox's ITEM OPTIONS picture not shown");
		intro = "pc"; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 3, "Halo PC's ITEM OPTIONS picture not back");
		gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 3, "Halo PC's ITEM OPTIONS picture replaced");
		/* (GAME OPTIONS the same) */
		picture.animation.current_frame_index = 1; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 1, "Halo PC's GAME OPTIONS picture replaced");
		intro = "xbox"; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 9, "the Xbox's GAME OPTIONS picture not shown");
		gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 9, "the Xbox's GAME OPTIONS picture replaced");
		intro = "pc"; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 1, "Halo PC's GAME OPTIONS picture not back");
		/* (CHANGE NAME the same) */
		picture.animation.current_frame_index = 0; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 0, "Halo PC's CHANGE NAME picture replaced");
		intro = "xbox"; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 10, "the Xbox's CHANGE NAME picture not shown");
		intro = "pc"; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 0, "Halo PC's CHANGE NAME picture not back");
		/* (the rows with one picture) */
		for (intro_row = 0; intro_row < 7; intro_row++)
		{
			if (intro_row <= 1 || intro_row == 3)
				continue;
			picture.animation.current_frame_index = intro_row;
			intro = "xbox"; gametype_ok_picture(&list);
			intro = "pc"; gametype_ok_picture(&list);
			CHECK(picture.animation.current_frame_index == intro_row, "row %d's picture changed", intro_row);
		}
	} else CASE("gamepads") {
		/* (GAMEPADS: the button settings' Xbox pictures with PLATFORM
		"xbox", Halo PC's whatever they are with "pc") */
		static char const *const intros[] = { "xbox", "pc", "xbox" };
		short pass;
		picture.name = "profile_edit_extended_desc_pic";
		for (pass = 0; pass < 3; pass++)
		{
			intro = intros[pass];
			picture.animation.current_frame_index = 2;
			for (edited.controller_settings.button_preset = 0; edited.controller_settings.button_preset < 6;
				edited.controller_settings.button_preset++)
			{
				short expected = !strcmp(intro, "pc") ? 18 :
					edited.controller_settings.button_preset < 5 ? 10 + edited.controller_settings.button_preset : 2;
				profile_gamepad_layout(&picture);
				CHECK(picture.animation.current_frame_index == expected, "%s, preset %d: frame %d, not %d", intro,
					edited.controller_settings.button_preset, picture.animation.current_frame_index, expected);
			}
		}
		/* (another row's left be) */
		intro = "pc"; picture.animation.current_frame_index = 3; profile_gamepad_layout(&picture);
		CHECK(picture.animation.current_frame_index == 3, "row 3's picture changed");
	} else CASE("multiplayer") {
		/* (the Multiplayer menu's: JOIN GAME's and CREATE GAME's INTERNET
		and LAN the Xbox's with PLATFORM "xbox") */
		static const short xbox_frames[] = { 6, 7, 6, 7, 4, 5 };
		static const short pc_frames[] = { 0, 1, 2, 3, 8, 5 };
		short item;
		picture.name = "multiplayer_options_pic";
		for (item = 0; item < 6; item++)
		{
			intro = "xbox"; picture.animation.current_frame_index = item; multiplayer_picture(&list);
			CHECK(picture.animation.current_frame_index == xbox_frames[item], "xbox: item %d shows frame %d", item,
				picture.animation.current_frame_index);
			multiplayer_picture(&list);
			CHECK(picture.animation.current_frame_index == xbox_frames[item], "xbox: item %d's picture not kept", item);
			intro = "pc"; picture.animation.current_frame_index = item; multiplayer_picture(&list);
			CHECK(picture.animation.current_frame_index == pc_frames[item], "pc: item %d shows frame %d", item,
				picture.animation.current_frame_index);
			multiplayer_picture(&list);
			CHECK(picture.animation.current_frame_index == pc_frames[item], "pc: item %d's picture not kept", item);
		}
		/* (PLATFORM changed on the button bar: Halo PC's back) */
		intro = "pc"; picture.animation.current_frame_index = 7; multiplayer_picture(&list);
		CHECK(picture.animation.current_frame_index == 1, "Halo PC's LAN picture not back");
		intro = "xbox"; picture.animation.current_frame_index = 8; multiplayer_picture(&list);
		CHECK(picture.animation.current_frame_index == 4, "the Xbox's CO-OP picture not back");
	} else return 2;
	return 0;
}
