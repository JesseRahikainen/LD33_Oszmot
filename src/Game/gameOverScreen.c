#include "gameOverScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"

#include "../UI/text.h"
#include "resources.h"
#include "titleScreen.h"

#include "../sound.h"

#include <stdlib.h>

static float timeAlive;

static int gameOverScreen_Enter( void )
{
	timeAlive = 0.0f;
	cam_TurnOnFlags( 0, 1 );
	cam_SetNextState( 0, VEC2_ZERO );
	
	gfx_SetClearColor( CLR_BLACK );

	if( !mute ) {
		playSong( "Music/game_over.it" );
	}

	return 1;
}

static int gameOverScreen_Exit( void )
{
	cam_TurnOffFlags( 0, 1 );

	return 1;
}

static void gameOverScreen_ProcessEvents( SDL_Event* e )
{
	if( ( e->type == SDL_KEYDOWN ) && ( timeAlive >= 0.75f ) ) {
		gsmEnterState( &globalFSM, &titleScreenState );
	}
}

static void gameOverScreen_Process( void )
{
}

static void gameOverScreen_Draw( void )
{
	char endString[256] = { 0 };

	Vector2 pos = { 400.0f, 200.0f };
	txt_DisplayString( "You have died at\nthe hands of a human.\nTheir legend lasts\nfor centuries.",
		pos, CLR_RED, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, fontSmallTitle, 1, 0 );

	pos.y = 550.0f;
	txt_DisplayString( "Press any key to return to the title screen",
		pos, CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_BASE_LINE, fontLargeText, 1, 0 );
}

static void gameOverScreen_PhysicsTick( float dt )
{
	timeAlive += dt;
}

struct GameState gameOverScreenState = { gameOverScreen_Enter, gameOverScreen_Exit, gameOverScreen_ProcessEvents,
	gameOverScreen_Process, gameOverScreen_Draw, gameOverScreen_PhysicsTick };