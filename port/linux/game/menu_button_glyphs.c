/* Optional artwork in existing PC-menu face-button slots; no input changes. */
#include "cseries.h"
#include "menu_button_glyphs.h"
#include "controller_glyphs.h"
#include "controller_buttons.h"
#include "keyboard_prompts.h"
#include "keyboard_glyphs.h"
#include "hud_button_layout.h"
#include "bitmaps/bitmaps.h"
#include "tag_files/tag_groups.h"
#include "text/draw_string.h"
#include "rasterizer/rasterizer.h"
#include <string.h>
#include <wchar.h>

struct bitmap_data *hud_hires_button_bitmap(short style, short slot);

/* The menus' A, B, X and Y (their legends and the buttons in their text, the
 * pause menus' included) follow BUTTON STYLE, as the HUD's do, in either
 * menus: XBOX GEMS, XBOX DARK GEMS and XBOX 360 draw theirs; XBOX CLASSIC
 * keeps the menus' own, which are its buttons. Key names for a keyboard when
 * the keyboard or mouse was used last (keyboard_prompts.c). */
/* the carnage report's prompt being drawn (menu_button_glyph_set_postgame) */
#define MENU_BUTTON_GLYPH_POSTGAME_SCALE 0.7f
static boolean menu_button_glyph_postgame = FALSE;
/* (its keys, wider than the buttons: a gap before each prompt after the
 * first and after each key, the prompts drawn so far counted) */
#define MENU_BUTTON_GLYPH_POSTGAME_KEY_GAP 14
#define MENU_BUTTON_GLYPH_POSTGAME_KEY_TRAIL 4
static short menu_button_glyph_postgame_keys = 0;

void menu_button_glyph_set_postgame(boolean postgame)
{
    menu_button_glyph_postgame = postgame;
    menu_button_glyph_postgame_keys = 0;
}

static boolean menu_button_glyph_enabled(short button, short port)
{
    if (!(button >= 0 && button < 4 && port >= 0 && port < 4)) return FALSE;
    /* (key names whichever menus, PC or Xbox, when the keyboard or mouse
     * was used last) */
    if (keyboard_prompt_enabled(port)) return TRUE;
    /* (the carnage report's prompt draws XBOX CLASSIC's too: the game's own
     * there are the HUD's message icons, drawn at the HUD's scale, far too
     * large on the full screen, and some maps' sheets lack them) */
    return controller_button_style() != 0 || menu_button_glyph_postgame;
}

/* These are fixed keyboard_gamepad menu controls, independent of gameplay binds.
 * Other buttons have no unambiguous keyboard equivalent and stay native. */
static wchar_t const *menu_button_key(short button)
{
    static wchar_t const *keys[] = {L"Enter", L"Esc", L"Del", L"Tab"};
    return button >= 0 && button < 4 ? keys[button] : NULL;
}

/* OpenCE supplies a menu font and positions the imported GulchCE renderer.
 * Compact slots use smaller lettering with comfortable padding inside the outline. */
static real menu_button_key_layout(short button, rectangle2d const *box, pixel32 color, boolean compact)
{
    long font = tag_loaded('font', "ui\\small_ui"), old_font;
    short style, justification, initial, paragraph;
    unsigned long flags;
    real_argb_color old_color;
    rectangle2d measure = {0}, cursor = {0};
    real_rectangle2d ink, capitals;
    real width, height, middle;
    char label[16];
    unsigned int i;
    wchar_t const *key = menu_button_key(button);
    if (!key || font == NONE) return 0.0f;
    for (i = 0; key[i] && i < NUMBEROF(label) - 1; i++) label[i] = (char)key[i];
    label[i] = 0;
    old_font = draw_string_get_font();
    draw_string_get_color(&old_color);
    draw_string_get_format(&style, &justification, &flags);
    draw_string_get_indents(&initial, &paragraph);
    draw_string_set_draw_mode(font, NONE, 0, 0, &old_color);
    draw_string_set_indents(0, 0);
    measure.x1 = measure.y1 = 2048;
    rasterizer_unicode_string_ink_bounds(&measure, L"H", &capitals);
    rasterizer_unicode_string_ink_bounds(&measure, key, &ink);
    height = MAX(20.0f, capitals.y1 - capitals.y0 + 10.0f);
    width = MAX(height, (ink.x1 - ink.x0) * (compact ? KEYBOARD_GLYPH_COMPACT_TEXT_SCALE : 1.0f) +
        (compact ? KEYBOARD_GLYPH_COMPACT_PADDING : 12.0f));
    middle = (capitals.y0 + capitals.y1) * .5f;
    if (box)
    {
        cursor.x0 = (short)(box->x1 - width - 4.0f + .5f);
        cursor.x1 = 2048;
        cursor.y0 = (short)((box->y0 + box->y1) * .5f - middle + .5f);
        cursor.y1 = 2048;
        if (compact) keyboard_glyph_draw_compact(label, &cursor, color);
        else keyboard_glyph_draw(label, &cursor, color, TRUE);
    }
    draw_string_set_draw_mode(old_font, style, justification, flags, &old_color);
    draw_string_set_indents(initial, paragraph);
    return width;
}

short menu_button_glyph_key_width(short button)
{
    return (short)(menu_button_key_layout(button, NULL, 0, FALSE) + .999f);
}

short menu_button_glyph_compact_key_width(short button)
{
    return (short)(menu_button_key_layout(button, NULL, 0, TRUE) + .999f);
}

boolean menu_button_glyph_draw(short button, short port, rectangle2d const *bounds,
    rectangle2d const *clip, pixel32 color)
{
    struct bitmap_data *bitmap;
    struct rasterizer_dynamic_screen_geometry_parameters parameters;
    struct dynamic_screen_vertex vertices[4];
    real_rectangle2d uv;
    rectangle2d key_box;
    short style, i;
    boolean compact;
    real width, height, fit, x, y, art_width, art_height;
    if (!menu_button_glyph_enabled(button, port)) return FALSE;
    width = bounds->x1 - bounds->x0; height = bounds->y1 - bounds->y0;
    if (width <= 0.0f || height <= 0.0f) return FALSE;
    /* A partially clipped slot keeps the native renderer's clipping contract. */
    if (clip && (bounds->x0 < clip->x0 || bounds->y0 < clip->y0 ||
        bounds->x1 > clip->x1 || bounds->y1 > clip->y1)) return FALSE;
    if (keyboard_prompt_enabled(port))
    {
        /* Keep the label-facing edge fixed. The original controller picture is
         * tall and narrow; key names need a wider cap with a horizontal baseline. */
        compact = width <= 16.0f && height <= 20.0f;
        key_box = *bounds;
        key_box.x0 = key_box.x1 - (compact ? menu_button_glyph_compact_key_width(button) :
            menu_button_glyph_key_width(button));
        if (key_box.x0 < 0) return FALSE;
        return menu_button_key_layout(button, &key_box, color, compact) > 0.0f;
    }
    /* (one style for the whole computer in this build: controller_buttons.c) */
    style = (short)controller_button_style();
    if (style < 3)
    {
        bitmap = hud_hires_button_bitmap(style, button);
        x = y = 0.0f; art_width = 21.0f; art_height = 26.0f;
        hud_button_face_bounds(style, button, &x, &y, &art_width, &art_height);
        /* Legacy helper describes the 168x208 sampled portion of its 256px sheet. */
        uv.x0 = -x / art_width * (168.0f / 256.0f);
        uv.y0 = -y / art_height * (208.0f / 256.0f);
        uv.x1 = (21.0f - x) / art_width * (168.0f / 256.0f);
        uv.y1 = (26.0f - y) / art_height * (208.0f / 256.0f);
    }
    else bitmap = controller_glyph_bitmap(style, button, &uv);
    if (!bitmap) return FALSE;
    art_width = uv.x1 - uv.x0; art_height = uv.y1 - uv.y0;
    fit = MIN(width / art_width, height / art_height);
    /* (the carnage report's prompt: the buttons fill the box, over the
     * panel's blue line above them; smaller, about the box's middle, so they
     * sit below it at about the height of the "=quit" beside them) */
    if (menu_button_glyph_postgame) fit *= MENU_BUTTON_GLYPH_POSTGAME_SCALE;
    x = bounds->x0 + (width - art_width * fit) * .5f;
    y = bounds->y0 + (height - art_height * fit) * .5f;
    memset(&parameters, 0, sizeof(parameters));
    parameters.map[0] = bitmap;
    parameters.map_texture_scale[0].i = parameters.map_texture_scale[0].j = 1.0f;
    parameters.map_scale[0].i = parameters.map_scale[0].j = 1.0f;
    for (i = 0; i < 4; i++)
    {
        boolean right = ((i + 1) & 2) != 0, bottom = i > 1;
        vertices[i].position.x = x + (right ? art_width * fit : 0.0f);
        vertices[i].position.y = y + (bottom ? art_height * fit : 0.0f);
        vertices[i].texture_coordinates.x = right ? uv.x1 : uv.x0;
        vertices[i].texture_coordinates.y = bottom ? uv.y1 : uv.y0;
        vertices[i].color = color;
    }
    rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
    return TRUE;
}

boolean menu_button_glyph_inline(short icon, short port, rectangle2d *cursor)
{
    rectangle2d box = *cursor;
    real_argb_color color;
    short middle;
    boolean keys;
    /* (the carnage report's prompt is the whole screen's, not a player's:
     * the first port's, which has the keyboard, whichever player the text
     * was last drawn for) */
    if (menu_button_glyph_postgame) port = 0;
    /* Literal face-button tokens already have an unambiguous menu meaning. */
    if (!menu_button_glyph_enabled(icon, port)) return FALSE;
    middle = draw_unicode_string_capital_middle(cursor, L"");
    box.x0 += 4;
    keys = menu_button_glyph_postgame && keyboard_prompt_enabled(port);
    if (keys && menu_button_glyph_postgame_keys > 0) box.x0 += MENU_BUTTON_GLYPH_POSTGAME_KEY_GAP;
    box.x1 = box.x0 + (keyboard_prompt_enabled(port) ? menu_button_glyph_key_width(icon) : 26);
    box.y0 = middle - 13; box.y1 = middle + 13;
    draw_string_get_color(&color);
    if (!menu_button_glyph_draw(icon, port, &box, NULL,
        ((pixel32)(color.alpha * 255.0f) << 24) | 0xffffffUL)) return FALSE;
    cursor->x0 = box.x1;
    if (keys)
    {
        cursor->x0 += MENU_BUTTON_GLYPH_POSTGAME_KEY_TRAIL;
        menu_button_glyph_postgame_keys++;
    }
    return TRUE;
}

/* the PC version's carnage report prompt ("ESCAPE = Quit    ENTER =
 * Continue", strings.xml's) names its keys in text, whatever was used last:
 * its ESCAPE and ENTER become the B and A buttons' tokens, which show the
 * keys (Esc, Enter) after the keyboard or mouse and the controller's buttons
 * after a controller, as the Xbox's prompt does. The text itself when it
 * names neither or will not fit. */
wchar_t const *menu_button_glyph_postgame_prompt(wchar_t const *text, wchar_t *buffer, long capacity)
{
    static struct { wchar_t const *name, *token; } const names[] =
        { { L"ESCAPE", L"%b-button" }, { L"ENTER", L"%a-button" } };
    long length = 0, add;
    short i;
    boolean replaced = FALSE;
    wchar_t const *at;
    if (!text || !buffer || capacity <= 0) return text;
    for (at = text; *at; )
    {
        wchar_t const *piece = NULL;
        for (i = 0; i < (short)NUMBEROF(names) && !piece; i++)
        {
            if (!wcsncmp(at, names[i].name, wcslen(names[i].name)))
            {
                piece = names[i].token;
                at += wcslen(names[i].name);
                replaced = TRUE;
            }
        }
        add = piece ? (long)wcslen(piece) : 1;
        if (length + add >= capacity) return text;
        if (piece) memcpy(buffer + length, piece, add * sizeof(wchar_t));
        else buffer[length] = *at++;
        length += add;
    }
    buffer[length] = 0;
    return replaced ? buffer : text;
}

/* the carnage report's prompt with keys: how much wider than with buttons
 * (each key's width over a button's 26, and the gaps), so that it may be
 * moved left by as much and end where the buttons' does */
short menu_button_glyph_postgame_extra(wchar_t const *text, short port)
{
    static wchar_t const *const tokens[4] = {L"%a-button", L"%b-button", L"%x-button", L"%y-button"};
    short extra = 0, count = 0, button;
    wchar_t const *at;
    if (!text || !keyboard_prompt_enabled(port)) return 0;
    for (at = wcschr(text, L'%'); at; at = wcschr(at + 1, L'%'))
    {
        for (button = 0; button < 4; button++)
        {
            if (!wcsncmp(at, tokens[button], wcslen(tokens[button])) && menu_button_glyph_enabled(button, port))
            {
                extra += menu_button_glyph_key_width(button) - 26 + MENU_BUTTON_GLYPH_POSTGAME_KEY_TRAIL;
                if (count++ > 0) extra += MENU_BUTTON_GLYPH_POSTGAME_KEY_GAP;
                break;
            }
        }
    }
    return extra;
}

wchar_t const *menu_button_glyph_join_hint(boolean can_join, short port)
{
    if (menu_button_glyph_enabled(0, port) && keyboard_prompt_enabled(port))
        return can_join ? L"SPACE: join" : L"";
    return can_join ? L"SPACE: join    ENTER: select    ESC: back" :
        L"ENTER: select    ESC: back";
}

/* The stock mixed text/icon routine measures each run separately. For this
 * identified PC join label, center the complete localized line as one unit. */
boolean menu_button_glyph_join_draw(wchar_t const *text, short port, rectangle2d const *bounds)
{
    wchar_t prefix[64], suffix[64];
    wchar_t const *token;
    size_t length;
    rectangle2d measure = *bounds, box = *bounds;
    real_rectangle2d before, after;
    real width, icon_width, x, after_x;
    real_argb_color color;
    short style, justification, initial, paragraph, middle;
    unsigned long flags;
    boolean drawn = FALSE;
    /* (another player's quarter after the keyboard: only a controller joins
     * there, the keyboard being player 1's, so its A in words, no picture
     * beside the keys the rest of the screen shows; the longest that fits) */
    static wchar_t const *const words[] = { L"A on a controller", L"A on a pad", L"A" };
    wchar_t line[192];
    real_rectangle2d ink;
    short word;
    boolean in_words = port > 0 && port < 4 && keyboard_prompt_enabled(0);
    if (!in_words && !menu_button_glyph_enabled(0, port)) return FALSE;
    token = wcsstr(text, L"%a-button");
    if (!token || wcschr(token + 9, L'%')) return FALSE;
    length = token - text;
    if (length >= NUMBEROF(prefix) || wcslen(token + 9) >= NUMBEROF(suffix)) return FALSE;
    memcpy(prefix, text, length * sizeof(wchar_t)); prefix[length] = 0;
    wcscpy(suffix, token + 9);
    draw_string_get_format(&style, &justification, &flags);
    draw_string_get_indents(&initial, &paragraph);
    draw_string_set_format(NONE, 0, 0);
    draw_string_set_indents(0, 0);
    measure.x0 = 0; measure.x1 = 2048;
    if (in_words)
    {
        for (word = 0; word < (short)NUMBEROF(words) && !drawn; word++)
        {
            if (wcslen(prefix) + wcslen(words[word]) + wcslen(suffix) >= NUMBEROF(line)) continue;
            wcscpy(line, prefix); wcscat(line, words[word]); wcscat(line, suffix);
            rasterizer_unicode_string_ink_bounds(&measure, line, &ink);
            width = ink.x1 - ink.x0;
            if (width <= bounds->x1 - bounds->x0 || word == (short)NUMBEROF(words) - 1)
            {
                x = (bounds->x0 + bounds->x1 - width) * .5f;
                rasterizer_draw_unicode_string_offset(&measure, line, x - ink.x0, 0.0f);
                drawn = TRUE;
            }
        }
        draw_string_set_format(style, justification, flags);
        draw_string_set_indents(initial, paragraph);
        return drawn;
    }
    rasterizer_unicode_string_ink_bounds(&measure, prefix, &before);
    rasterizer_unicode_string_ink_bounds(&measure, suffix, &after);
    icon_width = keyboard_prompt_enabled(port) ? (real)menu_button_glyph_key_width(0) : 26.0f;
    width = before.x1 - before.x0 + after.x1 - after.x0 + icon_width + 12.0f;
    if (width <= bounds->x1 - bounds->x0)
    {
        x = (bounds->x0 + bounds->x1 - width) * .5f;
        box.x0 = (short)(x + before.x1 - before.x0 + 6.0f + .5f);
        box.x1 = box.x0 + (short)icon_width;
        middle = draw_unicode_string_capital_middle(&measure, L"");
        box.y0 = middle - 13; box.y1 = middle + 13;
        draw_string_get_color(&color);
        drawn = menu_button_glyph_draw(0, port, &box, NULL,
            ((pixel32)(color.alpha * 255.0f) << 24) | 0xffffffUL);
        if (drawn)
        {
            after_x = x + before.x1 - before.x0 + icon_width + 12.0f;
            rasterizer_draw_unicode_string_offset(&measure, prefix, x - before.x0, 0.0f);
            rasterizer_draw_unicode_string_offset(&measure, suffix, after_x - after.x0, 0.0f);
        }
    }
    draw_string_set_format(style, justification, flags);
    draw_string_set_indents(initial, paragraph);
    return drawn;
}

boolean menu_button_glyph_keyboard_active(short port)
{
    return menu_button_glyph_enabled(0, port) && keyboard_prompt_enabled(port);
}
