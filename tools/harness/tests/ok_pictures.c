#include "harness.h"
/* The OK pictures of the settings menu and Edit Multiplayer Gametype: one
picture for both, the Xbox's with display.intro "xbox", else Halo PC's
(menu_functions.c's profile_ok_picture, gametype_ok_picture) */
#define _button_preset_swap_b_and_right_thumb 4
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
	} else CASE("gametype") {
		picture.name = "playlist_edit_ext_desc_pic";
		intro = "xbox"; picture.animation.current_frame_index = 7; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 7, "the Xbox's OK picture replaced");
		intro = "pc"; gametype_ok_picture(&list);
		CHECK(picture.animation.current_frame_index == 9, "Halo PC's OK picture not shown");
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
	} else return 2;
	return 0;
}
