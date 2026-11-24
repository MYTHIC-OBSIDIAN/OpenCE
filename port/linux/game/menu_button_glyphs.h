#ifndef MENU_BUTTON_GLYPHS_H
#define MENU_BUTTON_GLYPHS_H
#include "math/integer_math.h"

/* Recognized stock face-button legends only. Returning false preserves native art. */
boolean menu_button_glyph_draw(short button, short port, rectangle2d const *bounds,
    rectangle2d const *clip, pixel32 color);
boolean menu_button_glyph_inline(short icon, short port, rectangle2d *cursor);

/* Keep independent join instructions; suppress only duplicated legend actions. */
wchar_t const *menu_button_glyph_join_hint(boolean can_join, short port);

boolean menu_button_glyph_join_draw(wchar_t const *text, short port, rectangle2d const *bounds);

boolean menu_button_glyph_keyboard_active(short port);

/* the carnage report's prompt "B =quit  A =pick game" being drawn
 * (game_engine.c): XBOX 360's round buttons smaller there, below the
 * panel's line */
void menu_button_glyph_set_postgame(boolean postgame);
/* that prompt with keys (the keyboard used last): how much wider it is than
 * with the controller's buttons, its keys being wider and set further apart */
short menu_button_glyph_postgame_extra(wchar_t const *text, short port);
/* the PC version's prompt, its ESCAPE and ENTER as the B and A buttons'
 * tokens (in buffer), so that it shows the keys or the buttons, whichever
 * was used last */
wchar_t const *menu_button_glyph_postgame_prompt(wchar_t const *text, wchar_t *buffer, long capacity);

short menu_button_glyph_key_width(short button);
short menu_button_glyph_compact_key_width(short button);

#endif
