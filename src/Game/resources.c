#include "resources.h"

#include <SDL_log.h>

#include "../UI/text.h"
#include "../Graphics/imageSheets.h"
#include "../Graphics/images.h"
#include "../Graphics/shaderManager.h"

#define LOAD_AND_TEST( file, func, id ) \
	id = func( file ); if( id < 0 ) { \
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Error loading resource file %s.", file ); }

#define LOAD_AND_TEST_IMG( file, shaderType, id ) \
	id = img_Load( file, shaderType ); if( id < 0 ) { \
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Error loading resource file %s.", file ); }

#define LOAD_AND_TEST_FNT( file, size, id ) \
	id = txt_LoadFont( file, size ); if( id < 0 ) { \
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Error loading resource file %s.", file ); }

#define LOAD_AND_TEST_SS( file, ids ) \
	{ ids = NULL; int ret = img_LoadSpriteSheet( file, ST_DEFAULT, &ids ); if( ret < 0 ) { \
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Error loading resource file %s.", file ); } }


void loadAllResources( void )
{
	txt_Init( );

	mute = 0;

	LOAD_AND_TEST_FNT( "Fonts/kenpixel_blocks.ttf", 128.0f, fontLargeTitle );
	LOAD_AND_TEST_FNT( "Fonts/kenpixel_blocks.ttf", 64.0f, fontSmallTitle );
	LOAD_AND_TEST_FNT( "Fonts/kenpixel_mini_square.ttf", 32.0f, fontLargeText );
	LOAD_AND_TEST_FNT( "Fonts/kenpixel_mini_square.ttf", 16.0f, fontSmallText );
	LOAD_AND_TEST_SS( "Images/window_border.ss", windowEdgeImages );
	
	LOAD_AND_TEST_IMG( "Images/floor.png", ST_DEFAULT, floorImage );
	LOAD_AND_TEST_IMG( "Images/wall.png", ST_DEFAULT, wallImage );
	LOAD_AND_TEST_IMG( "Images/wall_inner_corner.png", ST_DEFAULT, innerCornerWallImage );
	LOAD_AND_TEST_IMG( "Images/wall_outer_corner.png", ST_DEFAULT, outerCornerWallImage );
	LOAD_AND_TEST_IMG( "Images/darkness.png", ST_DEFAULT, darknessImage );

	LOAD_AND_TEST_IMG( "Images/monster.png", ST_DEFAULT, monsterImage );
	LOAD_AND_TEST_IMG( "Images/warrior.png", ST_DEFAULT, warriorImage );
	LOAD_AND_TEST_IMG( "Images/berserker.png", ST_DEFAULT, berserkerImage );
	LOAD_AND_TEST_IMG( "Images/ulfhednar.png", ST_DEFAULT, ulfhednarImage );
	LOAD_AND_TEST_IMG( "Images/archer.png", ST_DEFAULT, archerImage );
	LOAD_AND_TEST_IMG( "Images/skald.png", ST_DEFAULT, skaldImage );
	LOAD_AND_TEST_IMG( "Images/table.png", ST_DEFAULT, tableImage );
	LOAD_AND_TEST_IMG( "Images/chair.png", ST_DEFAULT, chairImage );
	LOAD_AND_TEST_IMG( "Images/fire_pit.png", ST_DEFAULT, fireImage );
	LOAD_AND_TEST_IMG( "Images/pillar.png", ST_DEFAULT, pillarImage );
	LOAD_AND_TEST_IMG( "Images/corpse.png", ST_DEFAULT, corpseImage );

	LOAD_AND_TEST_IMG( "Images/horrified.png", ST_DEFAULT, horrifiedStatusImage );
	LOAD_AND_TEST_IMG( "Images/prone.png", ST_DEFAULT, proneStatusImage );
	LOAD_AND_TEST_IMG( "Images/brave.png", ST_DEFAULT, braveStatusImage );

	LOAD_AND_TEST_IMG( "Images/highlight.png", ST_DEFAULT, highlightImage );
}