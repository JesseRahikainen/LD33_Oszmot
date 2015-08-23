#include "titleScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"

#include "../UI/text.h"
#include "../UI/button.h"
#include "../System/systems.h"
#include "resources.h"

#include "gameScreen.h"
#include "instructionsScreen.h"
#include "../sound.h"

static float timeAlive;
static int restartSong = 1;

static int titleScreen_Enter( void )
{
	timeAlive = 0.0f;
	cam_TurnOnFlags( 0, 1 );

	if( restartSong && !mute ) {
		playSong( "Music/start.it" );
	}
	
	gfx_SetClearColor( CLR_BLACK );

	return 1;
}

static int titleScreen_Exit( void )
{
	cam_TurnOffFlags( 0, 1 );

	return 1;
}

static void titleScreen_ProcessEvents( SDL_Event* e )
{
	if( timeAlive <= 0.25f ) {
		return;
	}

	if( e->type == SDL_KEYDOWN ) {
		switch( e->key.keysym.sym  ) {
		case SDLK_s:
			gameDifficulty = 50;
			gameMaxEnemy = 15;
			gsmEnterState( &globalFSM, &gameScreenState );
			restartSong = 1;
			break;
		case SDLK_i:
			gsmEnterState( &globalFSM, &instructionsScreenState );
			restartSong = 0;
			break;
		case SDLK_m:
			mute = !mute;
			if( mute ) {
				stopSong( );
			} else {
				playSong( "Music/start.it" );
			}
			break;
		case SDLK_q:
			exit( 0 );
			break;
		}
	}
}

static void titleScreen_Process( void )
{
	cam_SetNextState( 0, VEC2_ZERO );
}

static void titleScreen_Draw( void )
{
	Vector2 titlePos = { 400.0f, 150.0f };
	txt_DisplayString( "the halls of", titlePos, CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_BOTTOM, fontSmallTitle, 1, 0 );
	txt_DisplayString( "OSZMOT", titlePos, CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_TOP, fontLargeTitle, 1, 0 );

	Vector2 inputPos = { 400.0f, 450.0f };
	txt_DisplayString( "s - Start Game", inputPos, CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_BASE_LINE, fontLargeText, 1, 0 );
	inputPos.y += 32.0f;

	txt_DisplayString( "m - Mute Music", inputPos, CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_BASE_LINE, fontLargeText, 1, 0 );
	inputPos.y += 32.0f;

	txt_DisplayString( "i - Instructions", inputPos, CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_BASE_LINE, fontLargeText, 1, 0 );
	inputPos.y += 32.0f;

	txt_DisplayString( "q - Quit", inputPos, CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_BASE_LINE, fontLargeText, 1, 0 );
}

static void titleScreen_PhysicsTick( float dt )
{
	timeAlive += dt;
}

struct GameState titleScreenState = { titleScreen_Enter, titleScreen_Exit, titleScreen_ProcessEvents,
	titleScreen_Process, titleScreen_Draw, titleScreen_PhysicsTick };