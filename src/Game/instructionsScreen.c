#include "instructionsScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"

#include "../UI/text.h"
#include "titleScreen.h"
#include "resources.h"

static float timeAlive;

static int instructionsScreen_Enter( void )
{
	timeAlive = 0.0f;
	cam_TurnOnFlags( 0, 1 );
	cam_SetNextState( 0, VEC2_ZERO );
	
	gfx_SetClearColor( CLR_BLACK );

	return 1;
}

static int instructionsScreen_Exit( void )
{
	cam_TurnOffFlags( 0, 1 );

	return 1;
}

static void instructionsScreen_ProcessEvents( SDL_Event* e )
{
	if( ( e->type == SDL_KEYDOWN ) && ( timeAlive >= 0.75f ) ) {
		gsmEnterState( &globalFSM, &titleScreenState );
	}
}

static void instructionsScreen_Process( void )
{
}

static void instructionsScreen_Draw( void )
{
	Vector2 pos = { 10.0f, 10.0f };
	txt_DisplayString(
		"This is a simple turn based strategy game where you play as a monster inspired by Grendel.\n"
		"You break into a mead hall each night and need to slay all the warriors there.",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );

	pos.y += 32.0f;
	txt_DisplayString(
		"Controls:",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontLargeText, 1, 0 );
	pos.y += 32.0f;
	txt_DisplayString(
		"Arrows keys - If looking around or throwing\n"
		"              moves the reticle.\n"
		"              If not moves to an empty spot\n"
		"              or attacks any enemy.",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	pos.y += 64.0f;
	txt_DisplayString(
		"s - Shoves an object, if it's a human they'll fall over.\n",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	pos.y += 16.0f;
	txt_DisplayString(
		"g - Grabs a chair or corpse.\n",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	pos.y += 16.0f;
	txt_DisplayString(
		"t - Throws the object you're currently holding.\n",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	pos.y += 16.0f;
	txt_DisplayString(
		"e - Eats a corpse your holding.\n",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	pos.y += 16.0f;
	txt_DisplayString(
		"l - Starts looking around.\n",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	pos.y += 16.0f;
	txt_DisplayString(
		". - Skips turn.\n",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	pos.y += 16.0f;
	txt_DisplayString(
		"Esc - Cancels look, attack, grab, and throw.\n",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	pos.y += 16.0f;
	txt_DisplayString(
		"Enter - Confirm throw target.\n",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	pos.y += 16.0f;

	Vector2 imgPos = { 416.0f, 0.0f };
	pos.x = 400.0f;
	pos.y = 42.0f;
	txt_DisplayString(
		"Enemies:",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontLargeText, 1, 0 );
	pos.x = 446.0f;
	pos.y += 32.0f;
	txt_DisplayString(
		"Warrior - Standard fighter.\n",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	imgPos.y = pos.y + 16.0f;
	img_Draw( warriorImage, 1, imgPos, imgPos, 0 );
	pos.y += 32.0f;

	txt_DisplayString(
		"Archer - Ranged fighter.\n"
		"         Tries to keep their distance.\n",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	imgPos.y = pos.y + 16.0f;
	img_Draw( archerImage, 1, imgPos, imgPos, 0 );
	pos.y += 48.0f;

	txt_DisplayString(
		"Berserker - Powerful fighter wearing a bear skin.\n"
		"            Stronger than a warrior and more\n"
		"            resistant to the horrors inflict.\n",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	imgPos.y = pos.y + 16.0f;
	img_Draw( berserkerImage, 1, imgPos, imgPos, 0 );
	pos.y += 64.0f;

	txt_DisplayString(
		"Skald - An entertainer. Not especially\n"
		"        strong, but increases the strength\n"
		"        and will of all other enemies.\n",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	imgPos.y = pos.y + 16.0f;
	img_Draw( skaldImage, 1, imgPos, imgPos, 0 );
	pos.y += 64.0f;

	txt_DisplayString(
		"Ulfhednar - Cousin to the berserker, wears\n"
		"            a wolf skin and is able to take\n"
		"            two actions each turn.\n",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	imgPos.y = pos.y + 16.0f;
	img_Draw( ulfhednarImage, 1, imgPos, imgPos, 0 );
	pos.y += 64.0f;


	pos.x = 10.0f;
	pos.y = 350.0f;
	txt_DisplayString(
		"Enemy Statuses:",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontLargeText, 1, 0 );
	imgPos.x = pos.x + 8.0f;
	pos.y += 32.0f;
	pos.x += 16.0f;

	txt_DisplayString(
		"Horrified - Enemies will run away, collapse, or be incapacitated until they gather up their courage.",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	imgPos.y = pos.y + 8.0f;
	img_Draw( horrifiedStatusImage, 1, imgPos, imgPos, 0 );
	pos.y += 16.0f;

	txt_DisplayString(
		"Prone - Enemies on the ground will have to get up before they do anything else.",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	imgPos.y = pos.y + 8.0f;
	img_Draw( proneStatusImage, 1, imgPos, imgPos, 0 );
	pos.y += 16.0f;

	txt_DisplayString(
		"Brave - Enemies encouraged by Skalds do more damage and are able to resist being horrified easier.",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	imgPos.y = pos.y + 8.0f;
	img_Draw( braveStatusImage, 1, imgPos, imgPos, 0 );
	pos.y += 16.0f;


	pos.x = 10.0f;
	pos.y = 435.0f;
	txt_DisplayString(
		"Tips:",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontLargeText, 1, 0 );
	pos.y += 32.0f;
	txt_DisplayString(
		"Grab and throw chairs and corpses.\n"
		"Horrified enemies are less of a danger.\n"
		"Use corpses to horrify enemies.\n"
		"Shoving something will try to shove everything behind it.\n"
		"Enemies avoid fires, but can be shoved in.",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );
	
	pos.x = 410.0f;
	pos.y = 435.0f + 32.0f;
	txt_DisplayString(
		"Eat corpses to restore health.\n"
		"Attacking while holding onto something does more damage.\n"
		"Thrown objects are good for archers and skalds.\n"
		"Look around at the start.\n"
		"The more a corpe has been hit with the less it will heal.",
		pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, 1, 0 );

	Vector2 anyKeyPos = { 400.0f, 590.0f };
	txt_DisplayString( "Press any key to return to the title screen",
		anyKeyPos, CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_BASE_LINE, fontLargeText, 1, 0 );
}

static void instructionsScreen_PhysicsTick( float dt )
{
	timeAlive += dt;
}

struct GameState instructionsScreenState = { instructionsScreen_Enter, instructionsScreen_Exit, instructionsScreen_ProcessEvents,
	instructionsScreen_Process, instructionsScreen_Draw, instructionsScreen_PhysicsTick };