#include "harness.h"
#include <ctype.h>
#include <strings.h>
/* MCC maps of the custom_maps folder: their virtual package's path
(xbox_files.c's translate_custom_mcc), and their place after the Custom
Edition maps of CUSTOM EDITION and CUSTOM CAMPAIGNS (menu_functions.c's
custom_mcc_index) */
enum { MAP_KIND_SINGLEPLAYER, MAP_KIND_MULTIPLAYER, MAP_KIND_DIGSITE, MAP_KIND_CUSTOM_MULTIPLAYER,
	MAP_KIND_CUSTOM_CAMPAIGN, MAP_KIND_MCC_SINGLEPLAYER, MAP_KIND_MCC_MULTIPLAYER };
static char custom_maps_setting[256];
static const char *config_string(const char *name){(void)name;return custom_maps_setting;}
static const char *platform_data_root(void){return "/data";}
/* the data root has custom_maps as Custom_Maps, and in it the folder Mercury */
static int posix_find_entry_case_insensitive(const char *folder,const char *name,char *found,unsigned long size)
{
 if (!strcmp(folder,"/data") && !strcasecmp(name,"custom_maps")) {snprintf(found,size,"Custom_Maps");return 1;}
 if (!strcasecmp(name,"mercury")) {snprintf(found,size,"Mercury");return 1;}
 return 0;
}
static int names_match(const char *a,const char *b,unsigned long length)
{
 unsigned long i;
 for (i=0;i<length;i++) {if (tolower((unsigned char)a[i])!=tolower((unsigned char)b[i])) return 0; if (!a[i]) return 1;}
 return 1;
}
static void trim_separators(char *path){size_t n=strlen(path);while (n>1&&(path[n-1]=='/'||path[n-1]=='\\')) path[--n]=0;}
/* 3 Custom Edition multiplayer maps and 2 campaign ones; MCC's of custom_maps: 2 and 1 */
static short custom_edition_maps_count(boolean campaign){return campaign?2:3;}
static short mcc_maps_custom_index(short row,boolean campaign)
{
 static const short multiplayer[]={7,9},solo[]={4};
 if (campaign) return row==0?solo[0]:NONE;
 return row>=0&&row<2?multiplayer[row]:NONE;
}
#include "under_test.inc"
static void translated(const char *path,const char *folder,const char *rest)
{
 char resolved[256]; const char *after=NULL;
 CHECK(translate_custom_mcc(path,resolved,sizeof(resolved),&after),"%s not translated",path);
 CHECK(!strcmp(resolved,folder),"%s: %s, not %s",path,resolved,folder);
 CHECK(!strcmp(after,rest),"%s: the rest %s, not %s",path,after,rest);
}
static void not_translated(const char *path)
{
 char resolved[256]; const char *after=NULL;
 CHECK(!translate_custom_mcc(path,resolved,sizeof(resolved),&after),"%s translated",path);
}
int main(int argc,char **argv)
{
 const char *case_name=argc>1?argv[1]:"";
 CASE("paths") {
  translated("custom_maps\\level.map","/data/Custom_Maps","\\level.map");
  translated("CUSTOM_MAPS/level.map","/data/Custom_Maps","/level.map");
  translated("custom_maps+mercury\\a10.map","/data/Custom_Maps/Mercury","\\a10.map");
  /* (a package folder's own resources) */
  translated("custom_maps+mercury\\bitmaps.map","/data/Custom_Maps/Mercury","\\bitmaps.map");
  /* (custom_maps' own are Custom Edition's: never a loose MCC map's) */
  not_translated("custom_maps\\bitmaps.map");
  not_translated("custom_maps/Sounds.map");
  not_translated("custom_maps\\loc.map");
  not_translated("custom_mapsx\\level.map");
  not_translated("123456\\level.map");
  not_translated("custom_maps+\\level.map");
  not_translated("custom_maps+mercury");
  snprintf(custom_maps_setting,sizeof(custom_maps_setting),"/elsewhere/maps/");
  translated("custom_maps\\level.map","/elsewhere/maps","\\level.map");
  translated("custom_maps+mercury\\a10.map","/elsewhere/maps/Mercury","\\a10.map");
 } else CASE("lists") {
  CHECK(custom_mcc_index(MAP_KIND_CUSTOM_MULTIPLAYER,2)==NONE,"a Custom Edition map taken for MCC's");
  CHECK(custom_mcc_index(MAP_KIND_CUSTOM_MULTIPLAYER,3)==7,"the first MCC multiplayer map not after them");
  CHECK(custom_mcc_index(MAP_KIND_CUSTOM_MULTIPLAYER,4)==9,"the second MCC multiplayer map");
  CHECK(custom_mcc_index(MAP_KIND_CUSTOM_MULTIPLAYER,5)==NONE,"past the end");
  CHECK(custom_mcc_index(MAP_KIND_CUSTOM_CAMPAIGN,1)==NONE,"a Custom Edition campaign map taken for MCC's");
  CHECK(custom_mcc_index(MAP_KIND_CUSTOM_CAMPAIGN,2)==4,"the MCC campaign map not after them");
  CHECK(custom_mcc_index(MAP_KIND_DIGSITE,5)==NONE,"another kind's entry taken for MCC's");
  CHECK(custom_mcc_index(MAP_KIND_MCC_MULTIPLAYER,5)==NONE,"MCC MULTIPLAYER's entry taken for custom_maps'");
 } else return 2;
 return 0;
}
