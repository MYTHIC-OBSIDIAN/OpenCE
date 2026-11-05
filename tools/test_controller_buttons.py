"""Production controller identity, preferences, pending menu state and glyph layout."""
import json
from pathlib import Path
import re

import pytest
from harness import function
from test_hud_button_style import run_c

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / 'port/linux/include/controller_buttons.h').read_text()


def without_includes(source):
    return '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))


def test_identity_and_one_style_for_the_computer(tmp_path):
    code = r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef enum {SDL_GAMEPAD_TYPE_UNKNOWN,SDL_GAMEPAD_TYPE_STANDARD,
 SDL_GAMEPAD_TYPE_XBOX360,SDL_GAMEPAD_TYPE_XBOXONE,SDL_GAMEPAD_TYPE_PS3,
 SDL_GAMEPAD_TYPE_PS4,SDL_GAMEPAD_TYPE_PS5,SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO,
 SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT,SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT,
 SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR} SDL_GamepadType;
typedef struct {SDL_GamepadType type;int id;} SDL_Gamepad;
static SDL_GamepadType SDL_GetGamepadType(SDL_Gamepad *p){return p->type;}
static char const *SDL_GetGamepadName(SDL_Gamepad *p){return "Test controller";}
static unsigned SDL_GetGamepadID(SDL_Gamepad *p){return p->id;}
static unsigned short SDL_GetGamepadVendor(SDL_Gamepad *p){return 123;}
static unsigned short SDL_GetGamepadProduct(SDL_Gamepad *p){return 456;}
static char const *manual="xbox_modern";
static char const *config_string(char const *s){assert(!strcmp(s,"display.hud_button_style"));return manual;}
'''
    code += HEADER + without_includes((ROOT / 'port/linux/src/controller_buttons.c').read_text()) + r'''
int main(void){
 SDL_Gamepad a={SDL_GAMEPAD_TYPE_PS5,7},b={SDL_GAMEPAD_TYPE_XBOXONE,9},c={SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO,11};
 struct controller_device info;int type,i;
 char const *names[]={"classic","gems","menu_gems","xbox_modern"};
 controller_device_update(0,&a);controller_device_update(1,&b);controller_device_update(2,&c);
 controller_device_get(0,&info);assert(info.family==CONTROLLER_PLAYSTATION && info.type==SDL_GAMEPAD_TYPE_PS5);
 assert(info.instance==7 && info.vendor==123 && info.product==456 && !strcmp(info.name,"Test controller"));
 /* (one style, whatever controller any player has) */
 assert(controller_button_style()==3);
 for(i=0;i<CONTROLLER_BUTTON_STYLES;i++){manual=names[i];assert(controller_button_style()==i);}
 manual="dark_gems";assert(controller_button_style()==2);
 /* (a PlayStation or Switch style an earlier build saved: classic) */
 manual="ps_modern";assert(controller_button_style()==0);
 manual="switch_gems";assert(controller_button_style()==0);
 manual="bad";assert(controller_button_style()==0);
 controller_device_update(0,NULL);controller_device_get(0,&info);assert(!info.family && !info.name[0] && !info.instance);
 for(type=0;type<=10;type++){
  a.type=(SDL_GamepadType)type;controller_device_update(0,&a);controller_device_get(0,&info);
  assert(info.family==(type<2?0:type<4?1:type<7?2:3));
 }
 controller_device_get(-1,&info);assert(!info.family);controller_device_get(4,&info);assert(!info.family);
 assert(controller_button_style_index(NULL)==0);
 return 0;
}
'''
    run_c(tmp_path, code)


def test_pending_menu_bumper_visibility_and_focus(tmp_path):
    code = r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define NUMBEROF(a) (sizeof(a)/sizeof(*(a)))
#define FALSE 0
typedef int boolean;
struct widget_instance {
 char const *name;short vertical_offset;int visible,definition_tag_index;
 struct widget_instance *child,*next,*parent,*focused_child;
 struct {struct {short selected_index;}list;}parameters;
};
struct pc_menu_setting {int value_count;char const *values[4];};
static struct pc_menu_setting setting={4,{"classic","gems","menu_gems","xbox_modern"}};
static struct widget_instance spinner;
static struct pc_menu_setting *pc_menu_setting_get(int id){(void)id;return &setting;}
static struct widget_instance *named(struct widget_instance *list,char const *name,int n){
 struct widget_instance *p;(void)n;
 if(!strcmp(name,"hud_button_style_spinner"))return &spinner;
 for(p=list->child;p;p=p->next)if(!strcmp(p->name,name))return p;
 return 0;
}
'''
    code += HEADER + function((ROOT / 'port/linux/src/controller_buttons.c').read_text(), 'controller_button_style_index') + \
        (ROOT / 'port/linux/game/controller_button_menu.inl').read_text() + r'''
int main(void){
 struct widget_instance list={0},other={0},style={0},bumper={0},bar={0};int j,repeat;
 list.child=&other;other.next=&style;style.next=&bumper;bumper.next=&bar;
 other.name="op_intro";style.name="op_hud_button_style";bumper.name="op_hud_bumper_style";bar.name="button_bar";
 style.visible=1;bumper.vertical_offset=133;bar.vertical_offset=414;
 for(repeat=0;repeat<40;repeat++)for(j=0;j<4;j++){
  spinner.parameters.list.selected_index=j;
  list.focused_child=&bumper;controller_button_rows_show(&list);
  assert(bumper.visible==(j<3) && style.visible);
  assert(list.focused_child==(j<3?&bumper:&style));
  assert(list.parameters.list.selected_index==(j<3?2:1));
  list.focused_child=&other;controller_button_rows_show(&list);
  assert(list.focused_child==&other && list.parameters.list.selected_index==0);
  assert(bumper.vertical_offset==133 && bar.vertical_offset==414);
 }
 spinner.parameters.list.selected_index=-1;controller_button_rows_show(&list);assert(!bumper.visible);
 spinner.parameters.list.selected_index=4;controller_button_rows_show(&list);assert(!bumper.visible);
 list.child=&bar;controller_button_rows_show(&list);
 return 0;
}
'''
    run_c(tmp_path, code)


def test_artwork_mapping_transparency_labels_and_regeneration():
    import numpy as np
    from PIL import Image
    import controller_button_assets as assets
    entries = assets.sources()
    # (the Xbox's only: no PlayStation or Switch artwork)
    assert len(entries) == 14
    assert [e['source'] for e in entries[:4]] == [f'xbox/{b}_modern.png' for b in 'ABXY']
    assert all(e['source'].startswith('xbox/') for e in entries)
    assert not any(p.name.startswith(('ps_', 'switch_')) for p in assets.ASSETS.glob('*.png'))
    for entry in entries:
        expected = assets.render(entry)
        actual = Image.open(assets.ASSETS / (entry['name'] + '.png'))
        assert np.array_equal(np.asarray(expected), np.asarray(actual)), entry['name']
        bounds = actual.getchannel('A').getbbox()
        assert bounds and bounds[0] > 0 and bounds[1] > 0 and bounds[2] < 512 and bounds[3] < 512


def test_menu_generated_and_default_on():
    import port_settings
    for name, lines in port_settings.settings_files().items():
        assert (ROOT / 'port/assets/menus/ce' / name).read_text() == '\n'.join(lines)
    # Dynamic captions must not be overwritten or resized to the static string
    # by ui_widget's text update before the next frame.
    import xml.etree.ElementTree as ET
    prefix = 'main_menu.settings_select.player_setup.player_profile_edit'
    menu = ET.parse(ROOT / f'port/assets/menus/ce/{prefix}.customisation.xml')
    labels = [w for w in menu.getroot().findall('widget')
              if '/hud_button_style' in w.get('name', '') and w.get('name', '').endswith('_label')]
    # (one style for the computer: a fixed label, no player's)
    assert len(labels) == 1
    assert all('string_list' in w.attrib and 'string_index' in w.attrib for w in labels)
    rows = port_settings.SCREENS['customisation']['rows']
    assert next(r[2] for r in rows if r[1] == 'display.hud_button_style') == port_settings.XBOX_BUTTON_STYLES
    assert len(port_settings.XBOX_BUTTON_STYLES) == 4
    assert next(r[2] for r in rows if r[1] == 'display.hud_bumper_style') == [('ORIGINAL XBOX', 'original'), ('MODERN XBOX', 'modern')]
    # (Settings > Customisation's, its rows last: the profile menu opens it)
    assert any('customisation_settings_screen' in e.get('open', '') for e in menu.iter('on'))
    assert rows[-1][1] == 'display.hud_bumper_style'
    assert not any(r[1] == 'display.hud_bumper_style' or r[1].startswith('display.hud_button_style')
                   for r in port_settings.SCREENS['video_settings/graphics']['rows'])
    assert f'ce/{prefix}.customisation.xml' in json.loads((ROOT / 'port/assets/menus/menus.json').read_text())['files']
    config = (ROOT / 'port/linux/src/port_config.c').read_text()
    assert 'input.auto_button_detection' not in config and 'hud_button_style_ps' not in config
    assert 'ps_classic' not in config and 'switch_classic' not in config
    assert r'"display.hud_bumper_style", _config_string, "\"original\""' in config


def test_real_glyph_geometry_opacity_cache_and_teardown(tmp_path):
    source = (ROOT / 'port/linux/game/controller_glyphs.c').read_text()
    metadata = (ROOT / 'port/linux/game/controller_glyph_assets.h').read_text()
    source = source.replace('#include "controller_glyph_assets.h"', metadata)
    source = without_includes(source)
    code = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define NUMBEROF(a) (sizeof(a)/sizeof(*(a)))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define NONE (-1)
#define TRUE 1
#define FALSE 0
typedef int boolean;typedef float real;typedef unsigned long pixel32;
typedef struct {short x0,y0,x1,y1;}rectangle2d;
struct bitmap_data {unsigned long *hardware_format;};
struct icon_hud_element_definition {short frame_rate;unsigned flags;pixel32 color;};
struct dynamic_screen_vertex {struct {real x,y;}position,texture_coordinates;pixel32 color;};
struct rasterizer_dynamic_screen_geometry_parameters {
 struct bitmap_data *map[3];struct {real i,j;}map_texture_scale[3],map_scale[3];int framebuffer_blend_function;
};
static struct {short local_player_index;}render;
static struct player_datum {short local_player_index;}player;
static long local_player_get_player_index(short p){return p==2?NONE:123;}
static struct player_datum *player_get(long p){assert(p==123);return &player;}
static int computer_style=3;
static int controller_button_style(void){return computer_style;}
static char const *bumper_style="original";
static char const *config_string(char const *s){assert(!strcmp(s,"display.hud_bumper_style"));return bumper_style;}
static int players=1,allocations,live,draws,fail;static long ticks,last_asset;
static struct dynamic_screen_vertex drawn[4];
static long game_time_get(void){return ticks;}
static int local_player_count(void){return players;}
static real draw_unicode_string_capital_middle(rectangle2d *r,void const *s){return 100;}
static struct bitmap_data *bitmap_2d_new(int w,int h,int m,int f){
 struct bitmap_data *b;if(fail==1)return NULL;assert(w==64&&h==64&&!m&&f==11);
 b=calloc(1,sizeof(*b));allocations++;live++;return b;
}
static int rasterizer_bitmap_new(struct bitmap_data *b){b->hardware_format=(unsigned long*)b;return fail!=2;}
static void bitmap_delete(struct bitmap_data *b){free(b);live--;}
static void rasterizer_psuedo_dynamic_screen_quad_draw(struct rasterizer_dynamic_screen_geometry_parameters *p,
 struct dynamic_screen_vertex *v){assert(p->framebuffer_blend_function==0);memcpy(drawn,v,sizeof(drawn));draws++;}
'''
    code += (ROOT / 'port/linux/game/hud_button_layout.h').read_text() + source + r'''
long hud_hires_asset_count(void){return NUMBEROF(controller_glyph_assets);}
char const *hud_hires_asset_tag(long a){return controller_glyph_assets[a].tag;}
void hud_hires_register_placeholder(long a,unsigned long const *p){if(p)last_asset=a;}
static void near(real a,real b){assert(a-b<0.001f && b-a<0.001f);}
int main(void){
 struct icon_hud_element_definition icon={10,0,0};rectangle2d cursor;
 int style,index,frame,i,before;real scale,x,y,w,h,cellw,cellh;short const *bounds;
 /* (the computer's one style, whichever player) */
 for(i=0;i<4;i++){render.local_player_index=(short)i;computer_style=i;assert(controller_glyph_style()==i);}
 bumper_style="modern";
 for(players=1;players<=2;players++)for(style=0;style<4;style++)for(index=0;index<18;index++)for(frame=0;frame<3;frame++){
  cursor.x0=100;cursor.y0=50;cursor.x1=500;cursor.y1=150;ticks=frame*10;
  if((style<3 && index!=4 && index!=5) || (index>=8 && index<14)){before=draws;assert(!controller_glyph_draw(style,index,&icon,&cursor,0x7fffffff));assert(draws==before&&cursor.x0==100);continue;}
  assert(controller_glyph_draw(style,index,&icon,&cursor,0x7fffffff));assert(drawn[0].color==0x7fffffff);
  scale=players==1?1:.75f;
  bounds=controller_glyph_assets[controller_glyph_asset_index(style,index<6?index:hud_button_control_slot(index,frame))].bounds;
  w=drawn[2].position.x-drawn[0].position.x;h=drawn[2].position.y-drawn[0].position.y;
  x=drawn[0].position.x+(bounds[0]+bounds[2])*w/1024;
  y=drawn[0].position.y+(bounds[1]+bounds[3])*h/1024;
  cellw=(index<6?21:index<8?44:32)*scale;cellh=(index<6?26:index<8?33:34)*scale;
  near(x,104+(scale-1)*4+cellw/2);near(y,100);
  assert((bounds[2]-bounds[0])*w/512<=cellw+.001f && (bounds[3]-bounds[1])*h/512<=cellh+.001f);
  assert(cursor.x0==100+(short)((index<4?22:index<6?20:index<8?45:33)*scale));
  before=allocations;icon.flags=2;icon.color=0x40123456;
  assert(controller_glyph_draw(style,index,&icon,&cursor,0xffffffff));assert(drawn[0].color==0x40ffffff&&before==allocations);icon.flags=0;
 }
 assert(live==14 && allocations==14);controller_glyphs_dispose();assert(!live);
 controller_glyphs_dispose();assert(!live);
 /* Only black/RB and white/LB change. All three styles share the modern
  * artwork cache, including after preference changes and map teardown. */
 for(i=0;i<10;i++)for(style=0;style<3;style++)for(index=0;index<18;index++){
  bumper_style="original";assert(!controller_glyph_draw(style,index,&icon,&cursor,0));
  bumper_style="modern";before=draws;
  assert(controller_glyph_draw(style,index,&icon,&cursor,0)==(index==4||index==5));
  if(index==4||index==5){assert(draws==before+1);assert(controller_glyph_asset_index(style,index)==index);}
 }
 assert(live==2);controller_glyphs_dispose();assert(!live);
 bumper_style="invalid";assert(!controller_glyph_draw(0,4,&icon,&cursor,0));
 bumper_style="original";
 for(i=1;i<=2;i++){fail=i;assert(!controller_glyph_draw(3,0,&icon,&cursor,0));assert(!live);controller_glyphs_dispose();}
 fail=0;assert(controller_glyph_draw(3,0,&icon,&cursor,0));assert(drawn[0].color==0xffffff);
 for(style=-1;style<3;style++)assert(!controller_glyph_draw(style,0,&icon,&cursor,0));
 assert(!controller_glyph_draw(4,0,&icon,&cursor,0));controller_glyphs_dispose();assert(!live);
 return 0;
}
'''
    run_c(tmp_path, code)


def test_classic_letters_are_transparent_cutouts():
    import numpy as np
    from PIL import Image
    from scipy.ndimage import binary_fill_holes
    for letter in 'abxy':
        rgba = np.asarray(Image.open(ROOT / f'port/assets/buttons/classic_{letter}__0.png'))
        solid = rgba[..., 3] >= 128
        holes = binary_fill_holes(solid) & ~solid
        assert np.count_nonzero(holes & (rgba[..., 3] == 0)) > 500
        # No painted black letter remains on any of the coloured faces.
        assert np.min(np.max(rgba[..., :3][solid], axis=1)) > 100
