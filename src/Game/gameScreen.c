#include "gameScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"

#include "../UI/button.h"
#include "../UI/text.h"
#include "../System/systems.h"
#include "../Utils/stretchyBuffer.h"

#include "../Math/mathUtil.h"

#include "resources.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#include "titleScreen.h"
#include "gameOverScreen.h"
#include "../Utils/helpers.h"

#include "../sound.h"

#define GAME_CAM_FLAGS 0x1
#define UI_CAM_FLAGS 0x2

#define COMP_TURN_DELAY 0.0f//0.05f
float turnDelay;
int turnIdx;

int gameDifficulty = 50;
int gameMaxEnemy = 15;

typedef void (*PhysicalObjectAction)(int);

enum PlayerInputState {
	PIS_IGNORE,
	PIS_WAITING,
	PIS_ATTACKING,
	PIS_GRABBING,
	PIS_SHOVING,
	PIS_THROWING,
	PIS_LOOKING,
	PIS_WON,
	PIS_LOST
};

enum PlayerInputState playerInputState = PIS_WAITING;

static int buttonSys;

static const int OBJ_FLAG_WEAPON			= 0x001;
static const int OBJ_FLAG_EDIBLE			= 0x002;
static const int OBJ_FLAG_SHOVABLE			= 0x004;
static const int OBJ_FLAG_MONSTER			= 0x008;
static const int OBJ_FLAG_HUMAN				= 0x010;
static const int OBJ_FLAG_IN_USE			= 0x020;
static const int OBJ_FLAG_BG				= 0x040;
static const int OBJ_FLAG_NON_TARGETABLE	= 0x080;
static const int OBJ_FLAG_HORRIFYING		= 0x100;
static const int OBJ_FLAG_AVOID				= 0x200;
static const int OBJ_FLAG_RANGED			= 0x400;

static const int OBJ_STATE_HORRIFIED	= 0x01;
static const int OBJ_STATE_PRONE		= 0x02;
static const int OBJ_STATE_BRAVE		= 0x04;

#define MAX_PHYSICAL_OBJECTS 128

typedef struct {
	char name[32];
	char reference[32];
	int health;
	int maxHealth;
	int damage;
	int will;
	int flags;
	int state;
	int image;
	int tileX, tileY;
	int turnsProne;
	int turnsBrave;
	int heldObject;
	PhysicalObjectAction action;
} PhysicalObject;

PhysicalObject objects[MAX_PHYSICAL_OBJECTS];

static int playerObjIdx;

#define TILE_FLAG_OBSTRUCTS	0x01
typedef struct {
	int image;
	float rotation;
	Vector2 renderPos;

	int flags;
} Tile;

static int highlightX;
static int highlightY;

#define MAX_FLAVOR_STR_LEN 64
#define MAX_FLAVOR_STR_CNT 32
char flavorStrings[MAX_FLAVOR_STR_CNT][MAX_FLAVOR_STR_LEN];
static void drawFlavorText( Vector2 lowerLeft, float lineHeight, int lineCount, char depth )
{
	Vector2 basePos = lowerLeft;
	for( int i = 0; i < lineCount; ++i ) {
		txt_DisplayString( flavorStrings[i], basePos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, depth );
		basePos.y -= lineHeight;
	}
}

static void pushFlavorText( const char* str )
{
	memmove( &flavorStrings[1][0], &flavorStrings[0][0], sizeof( char ) * MAX_FLAVOR_STR_LEN * ( MAX_FLAVOR_STR_CNT - 1 ) );
	memcpy( &flavorStrings[0][0], str, MAX_FLAVOR_STR_LEN );
	flavorStrings[0][MAX_FLAVOR_STR_LEN-1] = '\0';
}

static void clearFlavorText( void )
{
	memset( flavorStrings, 0, sizeof( flavorStrings ) );
}

#define LEVEL_MAX_WIDTH 32
#define LEVEL_MAX_HEIGHT 32
#define LEVEL_TILE_IDX( x, y ) ( (x) + ( (y) * LEVEL_MAX_WIDTH ) )
#define LEVEL_MAX_SIZE ( LEVEL_MAX_WIDTH * LEVEL_MAX_HEIGHT )
static Tile level[LEVEL_MAX_SIZE];

static void centerGameCameraOnTile( int idx )
{
	// actual center of the view area is at <485,235> on the screen
	Vector2 tileCenterPos = level[idx].renderPos;
	Vector2 offset = { 485.0f, 235.0f };
	Vector2 pos;
	vec2_Subtract( &tileCenterPos, &offset, &pos );
	cam_SetNextState( 0, pos );
}

static int objectDistance( int objOne, int objTwo )
{
	return abs( objects[objOne].tileX - objects[objTwo].tileX ) + abs( objects[objOne].tileY - objects[objTwo].tileY );
}

static int objectTileDistance( int obj, int tileX, int tileY )
{
	return abs( objects[obj].tileX - tileX ) + abs( objects[obj].tileY - tileY );
}

static int getObjectAtTile( int tileIdx, int filter )
{
	for( int i = 0; i < MAX_PHYSICAL_OBJECTS; ++i ) {
		if( ( objects[i].flags & OBJ_FLAG_IN_USE ) && !( objects[i].flags & OBJ_FLAG_NON_TARGETABLE ) &&
			( ( filter == 0 ) || ( !( ~objects[i].flags & filter ) ) ) ) {
			int objTileIdx = LEVEL_TILE_IDX( objects[i].tileX, objects[i].tileY );
			if( objTileIdx == tileIdx ) {
				return i;
			}
		}
	}

	return -1;
}

static int getObjectAtPos( int x, int y, int filter )
{
	if( ( x < 0 ) || ( x >= LEVEL_MAX_WIDTH ) || ( y < 0 ) || ( y >= LEVEL_MAX_HEIGHT ) ) {
		return -1;
	}
	return getObjectAtTile( LEVEL_TILE_IDX( x, y ), filter );
}

static int level_TileBlocked( int x, int y )
{
	if( ( x < 0 ) || ( y < 0 ) || ( x >= LEVEL_MAX_WIDTH ) || ( y >= LEVEL_MAX_HEIGHT ) ) {
		return 1;
	}

	return ( level[LEVEL_TILE_IDX(x,y)].flags & TILE_FLAG_OBSTRUCTS ) ? 1 : 0;
}

#define NOT_BLOCKED -1
#define TILE_BLOCKED -2
// returns the index of the object blocking the tile, -1 if it's not blocked, and -2 if the level terrain is blocking the spot
static int level_GetBlocked( int x, int y, int avoidance )
{
	if( level_TileBlocked( x, y ) ) {
		return TILE_BLOCKED;
	}

	for( int i = 0; i < MAX_PHYSICAL_OBJECTS; ++i ) {
		if( !( objects[i].flags & OBJ_FLAG_IN_USE ) ) {
			continue;
		}

		if( objects[i].flags & OBJ_FLAG_BG ) {
			if( !( avoidance && ( objects[i].flags & OBJ_FLAG_AVOID ) ) ) {
				continue;
			}
		}
		 
		if( ( x == objects[i].tileX ) && ( y == objects[i].tileY ) ) {
			return i;
		}
	}

	return NOT_BLOCKED;
}

static int attemptToMoveObject( int object, int xOffset, int yOffset, int avoidance )
{
	int newX = objects[object].tileX + xOffset;
	int newY = objects[object].tileY + yOffset;
	if( level_GetBlocked( newX, newY, avoidance ) == NOT_BLOCKED ) {
		objects[object].tileX = newX;
		objects[object].tileY = newY;
		return 1;
	} else {
		return 0;
	}
}

static void level_CarveOutRect( int left, int right, int top, int bottom )
{
	for( int y = 0; y < LEVEL_MAX_HEIGHT; ++y ) {
		for( int x = 0; x < LEVEL_MAX_WIDTH; ++x ) {
			if( x >= left && x <= right && y >= top && y <= bottom ) {
				level[LEVEL_TILE_IDX(x,y)].flags = 0;
			}
		}
	}
}

static void initObjects( void )
{
	memset( objects, 0, sizeof( objects ) );
}

static int findNextFreeObject( void )
{
	for( int i = 0; i < MAX_PHYSICAL_OBJECTS; ++i ) {
		if( !( objects[i].flags & OBJ_FLAG_IN_USE ) ) {
			return i;
		}
	}

	SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Unable to find empty object slot." );
	return -1;
}

static void destroyObject( int objIdx )
{
	if( ( objIdx < 0 ) || ( objIdx >= MAX_PHYSICAL_OBJECTS ) ) {
		return;
	}
	objects[objIdx].flags &= ~OBJ_FLAG_IN_USE;
}

static int attemptToCreateObject( PhysicalObject* obj )
{
	int idx = findNextFreeObject( );
	if( idx < 0 ) {
		return -1;
	}

	obj->flags |= OBJ_FLAG_IN_USE;
	obj->state = 0;
	memset( &( objects[idx] ), 0, sizeof( PhysicalObject ) );
	memcpy( &( objects[idx] ), obj, sizeof( PhysicalObject ) );
	return idx;
}

static int willTest( int idx )
{
	// if the skald is boosting us then we get a second chance
	int times = ( objects[idx].state & OBJ_STATE_BRAVE ) ? 2 : 1;
	while( times > 0 ) {
		if( ( rand( ) % 100 ) <= objects[idx].will ) {
			return 1;
		}
		--times;
	}

	return 0;
}

static void horrifyCheck( int idx )
{
	char flavor[MAX_FLAVOR_STR_LEN] = { 0 };

	if( !willTest( idx ) ) {	
		sprintf( flavor, "%s is shaking with fear.", objects[idx].reference );
		objects[idx].state |= OBJ_STATE_HORRIFIED;
	} else {
		sprintf( flavor, "%s resists their fear.", objects[idx].reference );
	}

	flavor[0] = (char)toupper( flavor[0] );
	pushFlavorText( flavor );
}

static void horrifyBurst( int baseX, int baseY, int distance )
{
	for( int i = 0; i < MAX_PHYSICAL_OBJECTS; ++i ) {
		if( ( objects[i].flags & OBJ_FLAG_IN_USE ) && ( objects[i].flags & OBJ_FLAG_HUMAN ) ) {
			if( ( abs( objects[i].tileX - baseX ) <= distance ) &&
				( abs( objects[i].tileY - baseY ) <= distance ) ) {
				horrifyCheck( i );
			}
		}
	}
}

static void makeObjectACorpse( int idx )
{
	if( !( ( objects[idx].flags & OBJ_FLAG_IN_USE ) || ( objects[idx].flags & OBJ_FLAG_HUMAN ) ) ) {
		return;
	}

	objects[idx].image = corpseImage;
	objects[idx].flags = OBJ_FLAG_SHOVABLE | OBJ_FLAG_EDIBLE | OBJ_FLAG_IN_USE | OBJ_FLAG_WEAPON | OBJ_FLAG_HORRIFYING;
	objects[idx].state = 0;
	objects[idx].maxHealth = objects[idx].health = 5;
	objects[idx].damage = 10;
	objects[idx].action = NULL;
	strcpy( objects[idx].name, "Corpse" );
	strcpy( objects[idx].reference, "the corpse" );
}

static void monsterAction( int idx )
{
	// see if everyone is dead
	int numAlive = 0;
	for( int i = 0; i < MAX_PHYSICAL_OBJECTS; ++i ) {
		if( ( objects[i].flags & OBJ_FLAG_IN_USE ) && ( objects[i].flags & OBJ_FLAG_HUMAN ) ) {
			++numAlive;
		}
	}

	if( playerInputState == PIS_IGNORE ) {
		if( numAlive == 0 ) {
			playerInputState = PIS_WON;
		} else if( objects[playerObjIdx].health <= 0 ) {
			playerInputState = PIS_LOST;
		} else {
			centerGameCameraOnTile( LEVEL_TILE_IDX( objects[playerObjIdx].tileX, objects[playerObjIdx].tileY ) );
			playerInputState = PIS_WAITING;
		}
	}
}

static void attackAction( int attackerIdx, int defenderIdx, int multiplier )
{
	// damage will be a random number between 0 and the attackers damage
	char flavor[MAX_FLAVOR_STR_LEN] = { 0 };

	// if the attacker is holding something use the damage for that instead
	int horrifying = 0;
	int horrifyingBurst = 0;

	int maxDamage = objects[attackerIdx].damage + 1;
	if( objects[attackerIdx].heldObject >= 0 ) {
		PhysicalObject* weapon = &( objects[objects[attackerIdx].heldObject] );
		horrifying = weapon->flags & OBJ_FLAG_HORRIFYING;
		maxDamage = weapon->damage + 1;

		if( multiplier > 1 ) {
			weapon->health = 0;
		} else {
			weapon->health -= 1;
		}

		if( weapon->health <= 0 ) {
			if( horrifying ) {
				sprintf( flavor, "%s was smashed open, spraying gore everywhere.", weapon->reference, objects[defenderIdx].reference );
				horrifyingBurst = 1;
			} else {
				sprintf( flavor, "%s was smashed into pieces.", weapon->reference, objects[defenderIdx].reference );
			}
			flavor[0] = (char)toupper( flavor[0] );
			pushFlavorText( flavor );
			destroyObject( objects[attackerIdx].heldObject );
			objects[attackerIdx].heldObject = -1;	
		}
	}

	int damage = rand( ) % ( maxDamage );
	if( ( objects[attackerIdx].state & OBJ_STATE_BRAVE ) ) {
		// if we're brave then we do more damage
		int braveDamage = rand( ) % ( maxDamage );
		if( braveDamage > damage ) {
			damage = braveDamage;
		}
	}

	objects[defenderIdx].health -= damage;
	if( objects[defenderIdx].health <= 0 ) {
		if( objects[defenderIdx].flags & OBJ_FLAG_HUMAN ) {
			sprintf( flavor, "%s killed %s.", objects[attackerIdx].reference, objects[defenderIdx].reference );
			makeObjectACorpse( defenderIdx );
		} else if( objects[defenderIdx].flags & OBJ_FLAG_MONSTER ) {
			sprintf( flavor, "%s killed %s.", objects[attackerIdx].reference, objects[defenderIdx].reference );
			makeObjectACorpse( defenderIdx );
			// handle getting input after they die
			objects[defenderIdx].health = 0;
			objects[defenderIdx].action = monsterAction;
		} else {
			sprintf( flavor, "%s destroyed %s.", objects[attackerIdx].reference, objects[defenderIdx].reference );
			destroyObject( defenderIdx );
		}
	} else {
		if( damage > 0 ) {
			if( objects[attackerIdx].flags & OBJ_FLAG_RANGED ) {
				sprintf( flavor, "%s %s %s for %i damage.", objects[attackerIdx].reference, 
					( attackerIdx == playerObjIdx ) ? "shoot" : "shoots",
					objects[defenderIdx].reference, damage );
			} else {
				sprintf( flavor, "%s %s %s for %i damage.", objects[attackerIdx].reference, 
					( attackerIdx == playerObjIdx ) ? "hit" : "hits",
					objects[defenderIdx].reference, damage );
			}
		} else {
			sprintf( flavor, "%s %s %s.", objects[attackerIdx].reference, 
				( attackerIdx == playerObjIdx ) ? "miss" : "misses",
				objects[defenderIdx].reference );
		}
	}

	if( flavor[0] != 0 ) {
		flavor[0] = (char)toupper( flavor[0] );
		pushFlavorText( flavor );
	}

	if( horrifying ) {
		if( horrifyingBurst ) {
			if( multiplier <= 1 ) {
				// horrify every human
				horrifyBurst( objects[defenderIdx].tileX, objects[defenderIdx].tileY, 1000 );
			} else {
				horrifyBurst( objects[defenderIdx].tileX, objects[defenderIdx].tileY, 5 );
			}
		} else {
			// only horrify those nearby
			horrifyBurst( objects[defenderIdx].tileX, objects[defenderIdx].tileY, 3 );
		}
	}
}

static void fireAction( int idx )
{
	// see if there's anything on top of us, if there is then damage them
	int targetIdx = getObjectAtPos( objects[idx].tileX, objects[idx].tileY, 0 );
	if( targetIdx >= 0 ) {
		// special filter for a player's corpse
		if( objects[targetIdx].health > 0 ) {
			attackAction( idx, targetIdx, 1 );
		}
	}
	turnDelay = 0.0f;
}

static int moveAwayFromObject( int ourIdx, int theirIdx )
{
	int moveX, moveY;
	int success = 0;
	moveX = objects[ourIdx].tileX - objects[theirIdx].tileX;
	if( moveX != 0 ) {
		moveX = moveX / abs( moveX );
	}

	moveY = objects[ourIdx].tileY - objects[theirIdx].tileY;
	if( moveY != 0 ) {
		moveY = moveY / abs( moveY );
	}

	if( rand( ) % 2 ) {
		success = attemptToMoveObject( ourIdx, moveX, 0, 1 );
		if( !success ) {
			success = attemptToMoveObject( ourIdx, 0 , moveY, 1 );
		}
	} else {
		success = attemptToMoveObject( ourIdx, 0 , moveY, 1 );
		if( !success ) {
			success = attemptToMoveObject( ourIdx, moveX, 0, 1 );
		}
	}

	return success;
}

static int moveTowardsObject( int ourIdx, int theirIdx )
{
	int success = 0;
	int moveX, moveY;
	moveX = objects[theirIdx].tileX - objects[ourIdx].tileX;
	if( moveX != 0 ) {
		moveX = moveX / abs( moveX );
	}

	moveY = objects[theirIdx].tileY - objects[ourIdx].tileY;
	if( moveY != 0 ) {
		moveY = moveY / abs( moveY );
	}

	if( rand( ) % 2 ) {
		success = attemptToMoveObject( ourIdx, moveX, 0, 1 );
		if( !success ) {
			success = attemptToMoveObject( ourIdx, 0 , moveY, 1 );
		}
	} else {
		success = attemptToMoveObject( ourIdx, 0 , moveY, 1 );
		if( !success ) {
			success = attemptToMoveObject( ourIdx, moveX, 0, 1 );
		}
	}

	if( !success ) {
		if( moveX == 0 ) {
			moveX = ( rand( ) % 2 ) ? -1 : 1;
		} else {
			moveX = 0;
		}

		if( moveY == 0 ) {
			moveY = ( rand( ) % 2 ) ? -1 : 1;
		} else {
			moveY = 0;
		}

		if( rand( ) % 2 ) {
			success = attemptToMoveObject( ourIdx, moveX, 0, 1 );
			if( !success ) {
				success = attemptToMoveObject( ourIdx, 0 , moveY, 1 );
			}
		} else {
			success = attemptToMoveObject( ourIdx, 0 , moveY, 1 );
			if( !success ) {
				success = attemptToMoveObject( ourIdx, moveX, 0, 1 );
			}
		}
	}

	return success;
}

// returns if there were no blocking actions taken, primarily handling prone and horrified
static int standardHumanAction( int idx )
{
	if( objects[idx].state & OBJ_STATE_BRAVE ) {
		++objects[idx].turnsBrave;
		if( objects[idx].turnsBrave >= 3 ) {
			objects[idx].state &= ~OBJ_STATE_BRAVE;
		}
	}

	int ret = 0;
	char flavor[MAX_FLAVOR_STR_LEN] = { 0 };
	if( objects[idx].state & OBJ_STATE_PRONE ) {
		++objects[idx].turnsProne;
		if( objects[idx].turnsProne >= 2 ) {
			sprintf( flavor, "%s stands up.", objects[idx].reference );
			objects[idx].state &= ~OBJ_STATE_PRONE;
		} else {
			sprintf( flavor, "%s is pulling themself up.", objects[idx].reference );
		}
		
		ret = 1;
	} else if( objects[idx].state & OBJ_STATE_HORRIFIED ) {
		if( willTest( idx ) ) {
			sprintf( flavor, "%s steels their will and looks your way.", objects[idx].reference );
			objects[idx].state &= ~OBJ_STATE_HORRIFIED;
		} else {
			switch( rand( ) % 4 ) {
			case 0:
				sprintf( flavor, "%s vomits in terror.", objects[idx].reference );
				break;
			case 1:
				sprintf( flavor, "%s runs away.", objects[idx].reference );
				// have them move away from the monster
				moveAwayFromObject( idx, playerObjIdx );
				break;
			case 2:
				sprintf( flavor, "%s soils themself.", objects[idx].reference );
				break;
			case 3:
				sprintf( flavor, "%s collapses into a trembling heap.", objects[idx].reference );
				objects[idx].turnsProne = 0;
				objects[idx].state |= OBJ_STATE_PRONE;
				break;
			}
		}
		ret = 1;
	}

	if( flavor[0] != 0 ) {
		flavor[0] = (char)toupper( flavor[0] );
		pushFlavorText( flavor );
	}

	return ret;
}

static void meleeAction( int idx )
{
	if( standardHumanAction( idx ) ) {
		return;
	}

	// if we're next to the monster then attack it
	// otherwise move towards it
	if( objectDistance( idx, playerObjIdx ) == 1 ) {
		if( objects[playerObjIdx].health > 0 ) {
			attackAction( idx, playerObjIdx, 1 );
		}
	} else {
		moveTowardsObject( idx, playerObjIdx );
	}
}

static void rangedAction( int idx )
{
	if( standardHumanAction( idx ) ) {
		return;
	}

	// try to move to a desired range (lets say about 8 units away), if we're at our desired range, or can't move and within range then attack
	int dist = objectDistance( idx, playerObjIdx );
	int moveSuccess = 0;
	int range = 4;
	if( dist < range ) {
		moveSuccess = moveAwayFromObject( idx, playerObjIdx );
	} else if( dist > range ) {
		moveSuccess = moveTowardsObject( idx, playerObjIdx );
	}

	if( !moveSuccess && ( dist <= range ) ) {
		if( objects[playerObjIdx].health > 0 ) {
			attackAction( idx, playerObjIdx, 1 );
		}
	}
}

static void skaldAction( int idx )
{
	if( standardHumanAction( idx ) ) {
		return;
	}

	// skalds will avoid the monster until it gets with range, then it will start going after the player
	// they will also recite every turn, making all other humans brave
	if( rand( ) % 2 ) {
		for( int i = 0; i < MAX_PHYSICAL_OBJECTS; ++i ) {
			if( ( objects[i].flags & OBJ_FLAG_IN_USE ) && ( objects[i].flags & OBJ_FLAG_HUMAN ) ) {
				objects[i].state |= OBJ_STATE_BRAVE;
				objects[i].turnsBrave = 0; // this will last a few turns after the skald dies
			}
		}

		switch( rand( ) % 4 ) {
		case 0:
			pushFlavorText( "The skald sings of a great king." );
			break;
		case 1:
			pushFlavorText( "The skald tells a joke." );
			break;
		case 2:
			pushFlavorText( "The skald recites a bawdy poem." );
			break;
		case 3:
			pushFlavorText( "The skald chants a religious verse." );
			break;
		}
	}

	int dist = objectDistance( idx, playerObjIdx );
	if( dist < 4 ) {
		meleeAction( idx );
	} else if( dist > 8 ) {
		moveTowardsObject( idx, playerObjIdx );
	} else {
		moveAwayFromObject( idx, playerObjIdx );
	}
}

static void ulfhendarAction( int idx )
{
	meleeAction( idx );
	meleeAction( idx );
}

static void grabObject( int grabber, int grabbed )
{
	if( !( objects[grabbed].flags & OBJ_FLAG_WEAPON ) ) {
		return;
	}

	char flavor[MAX_FLAVOR_STR_LEN] = { 0 };
	sprintf( flavor, "%s %s %s.", objects[grabber].reference,
		( grabber == playerObjIdx ) ? "grab" : "grabs",
		objects[grabbed].reference );
	flavor[0] = (char)toupper( flavor[0] );
	pushFlavorText( flavor );

	// don't show the grabbed object, will be drawn under the grabber
	objects[grabbed].tileX = -1;
	objects[grabbed].tileY = -1;

	objects[grabber].heldObject = grabbed;
}

static void createMonster( int x, int y )
{
	if( level_GetBlocked( x, y, 0 ) != NOT_BLOCKED ) {
		return;
	}
	PhysicalObject obj;
	obj.image = monsterImage;
	obj.tileX = x;
	obj.tileY = y;
	obj.flags = OBJ_FLAG_MONSTER;
	obj.health = obj.maxHealth = 25;
	obj.damage = 5;
	obj.action = monsterAction;
	obj.heldObject = -1;
	strcpy( obj.name, "You" );
	strcpy( obj.reference, "you" );
	playerObjIdx = attemptToCreateObject( &obj );
}

static void createWarrior( int x, int y )
{
	if( level_GetBlocked( x, y, 0 ) != NOT_BLOCKED ) {
		return;
	}
	int objIdx;
	PhysicalObject obj;
	obj.image = warriorImage;
	obj.tileX = x;
	obj.tileY = y;
	obj.flags = OBJ_FLAG_EDIBLE | OBJ_FLAG_HUMAN | OBJ_FLAG_SHOVABLE;
	obj.health = obj.maxHealth = 5;
	obj.damage = 3;
	obj.will = 25;
	obj.action = meleeAction;
	obj.heldObject = -1;
	strcpy( obj.name, "Warrior" );
	strcpy( obj.reference, "the warrior" );
	objIdx = attemptToCreateObject( &obj );
}

static void createBerserker( int x, int y )
{
	if( level_GetBlocked( x, y, 0 ) != NOT_BLOCKED ) {
		return;
	}
	int objIdx;
	PhysicalObject obj;
	obj.image = berserkerImage;
	obj.tileX = x;
	obj.tileY = y;
	obj.flags = OBJ_FLAG_EDIBLE | OBJ_FLAG_HUMAN | OBJ_FLAG_SHOVABLE;
	obj.health = obj.maxHealth = 15;
	obj.damage = 5;
	obj.will = 50;
	obj.action = meleeAction;
	obj.heldObject = -1;
	strcpy( obj.name, "Berserker" );
	strcpy( obj.reference, "the berserker" );
	objIdx = attemptToCreateObject( &obj );
}

static void createUlfhednar( int x, int y )
{
	if( level_GetBlocked( x, y, 0 ) != NOT_BLOCKED ) {
		return;
	}
	int objIdx;
	PhysicalObject obj;
	obj.image = ulfhednarImage;
	obj.tileX = x;
	obj.tileY = y;
	obj.flags = OBJ_FLAG_EDIBLE | OBJ_FLAG_HUMAN | OBJ_FLAG_SHOVABLE;
	obj.health = obj.maxHealth = 10;
	obj.damage = 3;
	obj.will = 50;
	obj.action = ulfhendarAction;
	obj.heldObject = -1;
	strcpy( obj.name, "Ulfhednar" );
	strcpy( obj.reference, "the ulfhednar" );
	objIdx = attemptToCreateObject( &obj );
}

static void createArcher( int x, int y )
{
	if( level_GetBlocked( x, y, 0 ) != NOT_BLOCKED ) {
		return;
	}
	int objIdx;
	PhysicalObject obj;
	obj.image = archerImage;
	obj.tileX = x;
	obj.tileY = y;
	obj.flags = OBJ_FLAG_EDIBLE | OBJ_FLAG_HUMAN | OBJ_FLAG_SHOVABLE | OBJ_FLAG_RANGED;
	obj.health = obj.maxHealth = 5;
	obj.damage = 2;
	obj.will = 25;
	obj.action = rangedAction;
	obj.heldObject = -1;
	strcpy( obj.name, "Archer" );
	strcpy( obj.reference, "the archer" );
	objIdx = attemptToCreateObject( &obj );
}

static void createSkald( int x, int y )
{
	if( level_GetBlocked( x, y, 0 ) != NOT_BLOCKED ) {
		return;
	}
	int objIdx;
	PhysicalObject obj;
	obj.image = skaldImage;
	obj.tileX = x;
	obj.tileY = y;
	obj.flags = OBJ_FLAG_EDIBLE | OBJ_FLAG_HUMAN | OBJ_FLAG_SHOVABLE;
	obj.health = obj.maxHealth = 5;
	obj.damage = 2;
	obj.will = 25;
	obj.action = skaldAction;
	obj.heldObject = -1;
	strcpy( obj.name, "Skald" );
	strcpy( obj.reference, "the skald" );
	objIdx = attemptToCreateObject( &obj );
}

static void createPillar( int x, int y )
{
	if( level_GetBlocked( x, y, 0 ) != NOT_BLOCKED ) {
		return;
	}
	int objIdx;
	PhysicalObject obj;
	obj.image = pillarImage;
	obj.tileX = x;
	obj.tileY = y;
	obj.flags = 0;
	obj.maxHealth = obj.health = 75;
	obj.action = NULL;
	obj.heldObject = -1;
	strcpy( obj.name, "Pillar" );
	strcpy( obj.reference, "the pillar" );
	objIdx = attemptToCreateObject( &obj );
}

static void createTable( int x, int y )
{
	if( level_GetBlocked( x, y, 0 ) != NOT_BLOCKED ) {
		return;
	}
	int objIdx;
	PhysicalObject obj;
	obj.image = tableImage;
	obj.tileX = x;
	obj.tileY = y;
	obj.maxHealth = obj.health = 10;
	obj.flags = OBJ_FLAG_SHOVABLE;
	obj.action = NULL;
	obj.heldObject = -1;
	strcpy( obj.name, "Table" );
	strcpy( obj.reference, "the table" );
	objIdx = attemptToCreateObject( &obj );
}

static void createChair( int x, int y )
{
	if( level_GetBlocked( x, y, 0 ) != NOT_BLOCKED ) {
		return;
	}
	int objIdx;
	PhysicalObject obj;
	obj.image = chairImage;
	obj.tileX = x;
	obj.tileY = y;
	obj.maxHealth = obj.health = 3;
	obj.damage = 10;
	obj.flags = OBJ_FLAG_SHOVABLE | OBJ_FLAG_WEAPON | OBJ_FLAG_BG;
	obj.action = NULL;
	obj.heldObject = -1;
	strcpy( obj.name, "Chair" );
	strcpy( obj.reference, "the chair" );
	objIdx = attemptToCreateObject( &obj );
}

static void createFire( int x, int y )
{
	if( level_GetBlocked( x, y, 0 ) != NOT_BLOCKED ) {
		return;
	}
	int objIdx;
	PhysicalObject obj;
	obj.image = fireImage;
	obj.tileX = x;
	obj.tileY = y;
	obj.flags = OBJ_FLAG_BG | OBJ_FLAG_NON_TARGETABLE | OBJ_FLAG_AVOID;
	obj.maxHealth = obj.health = -1;
	obj.damage = 5;
	obj.action = fireAction;
	obj.heldObject = -1;
	strcpy( obj.name, "Fire" );
	strcpy( obj.reference, "the fire" );
	objIdx = attemptToCreateObject( &obj );
}

// returns if the shove was successful
static int shoveAction( int shovedIdx, int shoveX, int shoveY )
{
	if( !( objects[shovedIdx].flags & OBJ_FLAG_SHOVABLE ) ) {
		return 0;
	}

	int blocked = level_GetBlocked( objects[shovedIdx].tileX + shoveX, objects[shovedIdx].tileY + shoveY, 0 );
	int success = 0;

	if( blocked == NOT_BLOCKED ) {
		attemptToMoveObject( shovedIdx, shoveX, shoveY, 0 );
		success = 1;
	} else if( blocked == TILE_BLOCKED ) {
		success = 0;
	} else {
		// hit an object, see if it can also be shoved
		success = shoveAction( blocked, shoveX, shoveY );
		if( success ) {
			attemptToMoveObject( shovedIdx, shoveX, shoveY, 0 );
		}
	}

	char flavor[MAX_FLAVOR_STR_LEN] = { 0 };
	if( objects[shovedIdx].flags & OBJ_FLAG_HUMAN ) {
		if( success ) {
			sprintf( flavor, "%s is shoved back and to the floor.", objects[shovedIdx].reference );
		} else {
			sprintf( flavor, "%s is shoved to the floor.", objects[shovedIdx].reference );
		}
		objects[shovedIdx].state |= OBJ_STATE_PRONE;
		objects[shovedIdx].turnsProne = 0;
	} else {
		if( success ) {
			sprintf( flavor, "%s is shoved back.", objects[shovedIdx].reference );
		} else {
			sprintf( flavor, "%s doesn't budge.", objects[shovedIdx].reference );
		}
	}
	if( flavor[0] != 0 ) {
		flavor[0] = (char)toupper( flavor[0] );
		pushFlavorText( flavor );
	}

	return success;
}

static const int DIR_UP			= 1 << 0;
static const int DIR_UP_RIGHT	= 1 << 1;
static const int DIR_RIGHT		= 1 << 2;
static const int DIR_DOWN_RIGHT	= 1 << 3;
static const int DIR_DOWN		= 1 << 4;
static const int DIR_DOWN_LEFT	= 1 << 5;
static const int DIR_LEFT		= 1 << 6;
static const int DIR_UP_LEFT	= 1 << 7;
static void createLevel( void )
{
	playerInputState = PIS_IGNORE;
	highlightX = -1;
	highlightY = -1;

	turnIdx = 0;
	turnDelay = 0.0f;

	clearFlavorText( );
	pushFlavorText( "You have entered the great hall, slay them all!" );

	// fill up the level and then carve out the hall
	for( int y = 0; y < LEVEL_MAX_HEIGHT; ++y ) {
		for( int x = 0; x < LEVEL_MAX_WIDTH; ++x ) {
			level[LEVEL_TILE_IDX(x,y)].flags |= TILE_FLAG_OBSTRUCTS;
		}
	}
	initObjects( );

	int centerX = LEVEL_MAX_WIDTH / 2;
	int centerY = LEVEL_MAX_HEIGHT / 2;

	int leftMost, rightMost, topMost, bottomMost;

	int left = centerX - ( ( rand() % 10 ) + 5 );
	int right = centerX + ( ( rand() % 10 ) + 5 );
	int top = centerY - ( ( rand() % 5 ) + 5 );
	int bottom = centerY + ( ( rand() % 5 ) + 5 );
	level_CarveOutRect( left, right, top, bottom );

	leftMost = left;
	rightMost = right;
	topMost = top;
	bottomMost = bottom;

	left = centerX - ( ( rand() % 5 ) + 5 );
	right = centerX + ( ( rand() % 5 ) + 5 );
	top = centerY - ( ( rand() % 10 ) + 5 );
	bottom = centerY + ( ( rand() % 10 ) + 5 );
	level_CarveOutRect( left, right, top, bottom );

	leftMost = left < leftMost ? left : leftMost;
	rightMost = right > rightMost ? right : rightMost;
	topMost = top < topMost ? top : topMost;
	bottomMost = bottom < bottomMost ? bottom : bottomMost;

	int itemX, itemY;
	// place objects in the level
	//  place fire pit
	itemX = ( leftMost + rightMost ) / 2;
	itemY = ( leftMost + rightMost ) / 2;
	for( int xOff = -1; xOff <= 1; ++xOff ) {
		for( int yOff = -1; yOff <= 1; ++yOff ) {
			createFire( itemX + xOff, itemY + yOff );
		}
	}
	
	//  place pillars, these will be placed around the fire, stretching horizontally and vertically
	//   left horizontal
	int pillarOffset = 0;
	while( pillarOffset < LEVEL_MAX_HEIGHT ) {
		createPillar( itemX - 4, 2 + itemY + pillarOffset );
		createPillar( itemX - 4, 2 + itemY - pillarOffset );
		createPillar( itemX + 4, 2 + itemY + pillarOffset );
		createPillar( itemX + 4, 2 + itemY - pillarOffset );
		pillarOffset += 4;
	}
	pillarOffset = 0;
	while( pillarOffset < LEVEL_MAX_WIDTH ) {
		createPillar( 2 + itemX + pillarOffset, itemY - 4 );
		createPillar( 2 + itemX - pillarOffset, itemY - 4 );
		createPillar( 2 + itemX - pillarOffset, itemY + 4 );
		createPillar( 2 + itemX + pillarOffset, itemY + 4 );
		pillarOffset += 4;
	}

	//  place tables and chairs
	int numTables = 6 + ( rand( ) % 5 );
	for( int i = 0; i < numTables; ++i ) {
		int tableX;
		int tableY;

		do {
			tableX = ( ( rand( ) % 2 == 0 ) ? 1 : -1 ) * ( 3 + ( rand( ) % ( LEVEL_MAX_WIDTH / 2 ) ) ) + itemX;
			tableY = ( ( rand( ) % 2 == 0 ) ? 1 : -1 ) * ( 3 + ( rand( ) % ( LEVEL_MAX_HEIGHT / 2 ) ) ) + itemX;
		} while( level_GetBlocked( tableX, tableY, 0 ) != NOT_BLOCKED );

		destroyObject( getObjectAtPos( tableX, tableY, 0 ) );
		createTable( tableX, tableY );
		if( getObjectAtPos( tableX - 1, tableY, 0 ) < 0 ) createChair( tableX - 1, tableY );
		if( getObjectAtPos( tableX + 1, tableY, 0 ) < 0 ) createChair( tableX + 1, tableY );
		if( getObjectAtPos( tableX , tableY - 1, 0 ) < 0 ) createChair( tableX, tableY - 1 );
		if( getObjectAtPos( tableX , tableY + 1, 0 ) < 0 ) createChair( tableX, tableY + 1 );
	}

	// place the player in the level
	int playerX = -1;
	int playerY = -1;
	while( level_GetBlocked( playerX, playerY, 0 ) != NOT_BLOCKED ) {
		switch( rand( ) % 4 ) {
		case 0:
			playerX = leftMost + rand( ) % 3;
			playerY = topMost + rand( ) % ( bottomMost - topMost );
			break;
		case 1:
			playerX = rightMost - rand( ) % 3;
			playerY = topMost + rand( ) % ( bottomMost - topMost );
			break;
		case 2:
			playerX = leftMost + rand( ) % ( rightMost - leftMost );
			playerY = topMost + rand( ) % 3;
			break;
		case 3:
			playerX = leftMost + rand( ) % ( rightMost - leftMost );
			playerY = bottomMost - rand( ) % 3;
			break;
		}
	}
	createMonster( playerX, playerY );

	// place enemies in the level
	int difficultyLeft = gameDifficulty;
	typedef struct {
		int score;
		void (*spawn)(int, int);
	} EnemySpawning;

	EnemySpawning enemyTypes[] = {
		{ 10, &createWarrior },
		{ 10, &createWarrior },
		{ 10, &createWarrior },
		{ 10, &createWarrior },
		{ 15, &createArcher },
		{ 15, &createArcher },
		{ 30, &createSkald },
		{ 30, &createUlfhednar },
		{ 30, &createUlfhednar },
		{ 30, &createUlfhednar },
		{ 20, &createBerserker },
		{ 20, &createBerserker },
		{ 20, &createBerserker }
	};
	int minScore = 10; // this has to match the minimum score in the spawning list
	while( difficultyLeft > 10 ) {
		int enemyX;
		int enemyY;

		// get enemy position
		do {
			enemyX = ( ( rand( ) % 2 == 0 ) ? 1 : -1 ) * ( 3 + ( rand( ) % ( LEVEL_MAX_WIDTH / 2 ) ) ) + itemX;
			enemyY = ( ( rand( ) % 2 == 0 ) ? 1 : -1 ) * ( 3 + ( rand( ) % ( LEVEL_MAX_HEIGHT / 2 ) ) ) + itemX;
		} while( level_GetBlocked( enemyX, enemyY, 0 ) != NOT_BLOCKED );

		// choose enemy type
		int choice;
		do {
			choice = rand( ) % ARRAY_SIZE( enemyTypes );
		} while( ( enemyTypes[choice].score > difficultyLeft ) || ( enemyTypes[choice].score > gameMaxEnemy ) );
		difficultyLeft -= enemyTypes[choice].score;
		enemyTypes[choice].spawn( enemyX, enemyY );
	}

	// get the actual images to use to draw everything
	for( int y = 0; y < LEVEL_MAX_HEIGHT; ++y ) {
		for( int x = 0; x < LEVEL_MAX_WIDTH; ++x ) {
			Tile* tile = &( level[LEVEL_TILE_IDX( x, y )] );
			if( tile->flags & TILE_FLAG_OBSTRUCTS ) {
				// look at surrounding tiles to determine what we should look like
				
				int surroundingTiles = 0;
				surroundingTiles |= level_TileBlocked( x,		y - 1 ) ?	DIR_UP : 0;
				surroundingTiles |= level_TileBlocked( x + 1,	y - 1 ) ?	DIR_UP_RIGHT : 0;
				surroundingTiles |= level_TileBlocked( x + 1,	y ) ?		DIR_RIGHT : 0;
				surroundingTiles |= level_TileBlocked( x + 1,	y + 1 ) ?	DIR_DOWN_RIGHT : 0;
				surroundingTiles |= level_TileBlocked( x,		y + 1 ) ?	DIR_DOWN : 0;
				surroundingTiles |= level_TileBlocked( x - 1,	y + 1 ) ?	DIR_DOWN_LEFT : 0;
				surroundingTiles |= level_TileBlocked( x - 1,	y ) ?		DIR_LEFT : 0;
				surroundingTiles |= level_TileBlocked( x - 1,	y - 1 ) ?	DIR_UP_LEFT : 0;

				tile->image = floorImage;
				tile->rotation = DEG_TO_RAD( 45.0f );

				if( surroundingTiles == ( DIR_UP | DIR_UP_RIGHT | DIR_RIGHT | DIR_DOWN_RIGHT | DIR_DOWN | DIR_DOWN_LEFT | DIR_LEFT | DIR_UP_LEFT ) ) {
					tile->image = -1;
					tile->rotation = 0.0f;
				} else if( ( ( ~surroundingTiles & ( DIR_UP | DIR_LEFT | DIR_UP_LEFT ) ) == 0 ) &&
					!( surroundingTiles & ( DIR_RIGHT | DIR_DOWN | DIR_DOWN_RIGHT ) ) ) {

					tile->image = outerCornerWallImage;
					tile->rotation = 0.0f;
				} else if( ( ( ~surroundingTiles & ( DIR_UP | DIR_RIGHT | DIR_UP_RIGHT ) ) == 0 ) &&
					!( surroundingTiles & ( DIR_LEFT | DIR_DOWN | DIR_DOWN_LEFT ) ) ) {

					tile->image = outerCornerWallImage;
					tile->rotation = DEG_TO_RAD( 90.0f );
				} else if( ( ( ~surroundingTiles & ( DIR_DOWN | DIR_RIGHT | DIR_DOWN_RIGHT ) ) == 0 ) &&
					!( surroundingTiles & ( DIR_LEFT | DIR_UP | DIR_UP_LEFT ) ) ) {

					tile->image = outerCornerWallImage;
					tile->rotation = DEG_TO_RAD( 180.0f );
				} else if( ( ( ~surroundingTiles & ( DIR_DOWN | DIR_LEFT | DIR_DOWN_LEFT ) ) == 0 ) &&
					!( surroundingTiles & ( DIR_RIGHT | DIR_UP | DIR_UP_RIGHT ) ) ) {

					tile->image = outerCornerWallImage;
					tile->rotation = DEG_TO_RAD( 270.0f );
				} else if( ( ( ~surroundingTiles & ( DIR_RIGHT | DIR_DOWN ) ) == 0 ) && !( surroundingTiles & DIR_DOWN_RIGHT ) ) {
					tile->image = innerCornerWallImage;
					tile->rotation = 0.0f;
				} else if( ( ( ~surroundingTiles & ( DIR_LEFT | DIR_DOWN ) ) == 0 ) && !( surroundingTiles & DIR_DOWN_LEFT ) ) {
					tile->image = innerCornerWallImage;
					tile->rotation = DEG_TO_RAD( 90.0f );
				} else if( ( ( ~surroundingTiles & ( DIR_LEFT | DIR_UP ) ) == 0 ) && !( surroundingTiles & DIR_UP_LEFT ) ) {
					tile->image = innerCornerWallImage;
					tile->rotation = DEG_TO_RAD( 180.0f );
				} else if( ( ( ~surroundingTiles & ( DIR_RIGHT | DIR_UP ) ) == 0 ) && !( surroundingTiles & DIR_UP_RIGHT ) ) {
					tile->image = innerCornerWallImage;
					tile->rotation = DEG_TO_RAD( 270.0f );
				} else if( ( ~surroundingTiles & ( DIR_LEFT | DIR_RIGHT | DIR_UP ) ) == 0) {
					tile->image = wallImage;
					tile->rotation = 0.0f;
				} else if( ( ~surroundingTiles & ( DIR_LEFT | DIR_RIGHT | DIR_DOWN ) ) == 0 ) {
					tile->image = wallImage;
					tile->rotation = DEG_TO_RAD( 180.0f );
				} else if( ( ~surroundingTiles & ( DIR_LEFT | DIR_UP | DIR_DOWN ) ) == 0 ) {
					tile->image = wallImage;
					tile->rotation = DEG_TO_RAD( -90.0f );
				} else if( ( ~surroundingTiles & ( DIR_RIGHT | DIR_UP | DIR_DOWN ) ) == 0 ) {
					tile->image = wallImage;
					tile->rotation = DEG_TO_RAD( 90.0f );
				}
			} else {
				tile->image = floorImage;
				tile->rotation = 0.0f;
			}
			
			tile->renderPos.x = 16.0f + ( x * 32.0f );
			tile->renderPos.y = 16.0f + ( y * 32.0f );
		}
	}

	centerGameCameraOnTile( LEVEL_TILE_IDX( playerX, playerY ) );
}

static void drawLevel( void )
{
	for( int i = 0; i < LEVEL_MAX_SIZE; ++i ) {
		Tile* tile = &( level[i] );
		if( tile->image != -1 ) {
			img_Draw_r( tile->image, GAME_CAM_FLAGS, tile->renderPos, tile->renderPos, tile->rotation, tile->rotation, 0 );
		}
	}
}

static void drawWindow( Vector2 upperLeft, Vector2 lowerRight, Color clr, char depth )
{
	Vector2 imgSize, imgPos, imgScale;
	Vector2 innerUpperLeft;
	Vector2 innerLowerRight;
	// assuming everything will stretch and not tile
	// upper left corner
	img_GetSize( windowEdgeImages[WE_UL], &imgSize );
	vec2_AddScaled( &upperLeft, &imgSize, 0.5f, &imgPos );
	img_Draw_c( windowEdgeImages[WE_UL], UI_CAM_FLAGS, imgPos, imgPos, clr, clr, depth );
	vec2_Add( &upperLeft, &imgSize, &innerUpperLeft );

	// upper right corner
	img_GetSize( windowEdgeImages[WE_UR], &imgSize );
	imgPos.x = lowerRight.x - ( imgSize.x / 2.0f );
	imgPos.y = upperLeft.y + ( imgSize.y / 2.0f );
	img_Draw_c( windowEdgeImages[WE_UR], UI_CAM_FLAGS, imgPos, imgPos, clr, clr, depth );

	// lower left corner
	img_GetSize( windowEdgeImages[WE_LL], &imgSize );
	imgPos.x = upperLeft.x + ( imgSize.x / 2.0f );
	imgPos.y = lowerRight.y - ( imgSize.y / 2.0f );
	img_Draw_c( windowEdgeImages[WE_LL], UI_CAM_FLAGS, imgPos, imgPos, clr, clr, depth );

	// lower right corner
	img_GetSize( windowEdgeImages[WE_LR], &imgSize );
	vec2_AddScaled( &lowerRight, &imgSize, -0.5f, &imgPos );
	img_Draw_c( windowEdgeImages[WE_LR], UI_CAM_FLAGS, imgPos, imgPos, clr, clr, depth );
	vec2_Subtract( &lowerRight, &imgSize, &innerLowerRight );

	// upper edge
	img_GetSize( windowEdgeImages[WE_UM], &imgSize );
	imgPos.x = ( innerUpperLeft.x + innerLowerRight.x ) / 2.0f;
	imgPos.y = upperLeft.y + ( imgSize.y / 2.0f );
	imgScale.x = ( innerLowerRight.x - innerUpperLeft.x ) / imgSize.x;
	imgScale.y = 1.0f;
	img_Draw_sv_c( windowEdgeImages[WE_UM], UI_CAM_FLAGS, imgPos, imgPos, imgScale, imgScale, clr, clr, depth );

	// lower edge
	img_GetSize( windowEdgeImages[WE_LM], &imgSize );
	imgPos.x = ( innerUpperLeft.x + innerLowerRight.x ) / 2.0f;
	imgPos.y = lowerRight.y - ( imgSize.y / 2.0f );
	imgScale.x = ( innerLowerRight.x - innerUpperLeft.x ) / imgSize.x;
	imgScale.y = 1.0f;
	img_Draw_sv_c( windowEdgeImages[WE_LM], UI_CAM_FLAGS, imgPos, imgPos, imgScale, imgScale, clr, clr, depth );

	// left edge
	img_GetSize( windowEdgeImages[WE_ML], &imgSize );
	imgPos.x = upperLeft.x + ( imgSize.x / 2.0f );
	imgPos.y = ( innerUpperLeft.y + innerLowerRight.y ) / 2.0f;
	imgScale.x = 1.0f;
	imgScale.y = ( innerLowerRight.y - innerUpperLeft.y ) / imgSize.y;
	img_Draw_sv_c( windowEdgeImages[WE_ML], UI_CAM_FLAGS, imgPos, imgPos, imgScale, imgScale, clr, clr, depth );

	// right edge
	img_GetSize( windowEdgeImages[WE_MR], &imgSize );
	imgPos.x = lowerRight.x - ( imgSize.x / 2.0f );
	imgPos.y = ( innerUpperLeft.y + innerLowerRight.y ) / 2.0f;
	imgScale.x = 1.0f;
	imgScale.y = ( innerLowerRight.y - innerUpperLeft.y ) / imgSize.y;
	img_Draw_sv_c( windowEdgeImages[WE_MR], UI_CAM_FLAGS, imgPos, imgPos, imgScale, imgScale, clr, clr, depth );

	// center
	img_GetSize( windowEdgeImages[WE_MM], &imgSize );
	vec2_Lerp( &lowerRight, &upperLeft, 0.5f, &imgPos );
	imgScale.x = ( innerLowerRight.x - innerUpperLeft.x ) / imgSize.x;
	imgScale.y = ( innerLowerRight.y - innerUpperLeft.y ) / imgSize.y;
	img_Draw_sv_c( windowEdgeImages[WE_MM], UI_CAM_FLAGS, imgPos, imgPos, imgScale, imgScale, clr, clr, depth );
}

static int gameScreen_Enter( void )
{
	// render ui over game
	cam_TurnOnFlags( 0, GAME_CAM_FLAGS );
	cam_TurnOnFlags( 1, UI_CAM_FLAGS );

	if( !mute ) {
		playSong( "Music/game.it" );
	}
	
	gfx_SetClearColor( CLR_BLACK );

	pushFlavorText( "You have entered the great hall, slay them all!" );

	initObjects( );

	createLevel( );

	return 1;
}

static int gameScreen_Exit( void )
{
	cam_TurnOffFlags( 0, GAME_CAM_FLAGS );
	cam_TurnOffFlags( 1, UI_CAM_FLAGS );
	
	return 1;
}

static void gameScreen_ProcessEvents( SDL_Event* e )
{
	// system input
	if( e->type == SDL_KEYDOWN ) {
		if( e->key.keysym.sym == SDLK_q ) {
			gsmEnterState( &globalFSM, &titleScreenState );
		}
	}

	// character input
	if( playerInputState == PIS_IGNORE ) {
		return;
	}

	if( e->type == SDL_KEYDOWN ) {
		if( playerInputState == PIS_WON ) {
			gameDifficulty += 15;
			gameMaxEnemy += 5;
			createLevel( );
			return;
		}

		if( playerInputState == PIS_LOST ) {
			gsmEnterState( &globalFSM, &gameOverScreenState );
			return;
		}

		if( e->key.keysym.sym == SDLK_a ) {
			playerInputState = PIS_ATTACKING;
			pushFlavorText( "Choose direction to attack." );
		} else if( e->key.keysym.sym == SDLK_g ) {
			if( objects[playerObjIdx].heldObject < 0 ) {
				playerInputState = PIS_GRABBING;
				pushFlavorText( "Choose direction to grab." );
			} else {
				pushFlavorText( "Already holding onto something, get rid of it first." );
			}
		} else if( e->key.keysym.sym == SDLK_s ) {
			playerInputState = PIS_SHOVING;
			pushFlavorText( "Choose direction to shove." );
		} else if( e->key.keysym.sym == SDLK_l ) {
			highlightX = objects[playerObjIdx].tileX;
			highlightY = objects[playerObjIdx].tileY;
			playerInputState = PIS_LOOKING;
			pushFlavorText( "Use the arrow keys to move the reticle." );
		} else if( e->key.keysym.sym == SDLK_e ) {
			// eat, if we're holding onto anything edible then take a bite out of them
			char flavor[MAX_FLAVOR_STR_LEN];
			if( objects[playerObjIdx].heldObject != -1 ) {
				PhysicalObject* obj = &( objects[objects[playerObjIdx].heldObject] );
				if( obj->flags & OBJ_FLAG_EDIBLE ) {
					objects[playerObjIdx].health += obj->health;
					if( objects[playerObjIdx].health > objects[playerObjIdx].maxHealth ) {
						objects[playerObjIdx].health = objects[playerObjIdx].maxHealth;
					}
					
					sprintf( flavor, "You quickly devour the %s. Yum!", obj->reference );
					pushFlavorText( flavor );
					horrifyBurst( objects[playerObjIdx].tileX, objects[playerObjIdx].tileY, 6 );

					destroyObject( objects[playerObjIdx].heldObject );
					objects[playerObjIdx].heldObject = -1;
				} else {
					sprintf( flavor, "You can't eat the %s.", obj->reference );
					pushFlavorText( flavor );
				}
			} else {
				pushFlavorText( "Not holding anything to eat." );
			}
		} else if( e->key.keysym.sym == SDLK_t ) {
			// throw, if we're holding onto anything throwable then throw it
			if( objects[playerObjIdx].heldObject != -1 ) {
				playerInputState = PIS_THROWING;
				highlightX = objects[playerObjIdx].tileX;
				highlightY = objects[playerObjIdx].tileY;
				pushFlavorText( "Use the arrow keys to move the reticle." );
			} else {
				pushFlavorText( "Not holding anything to throw." );
			}
		} else if( e->key.keysym.sym == SDLK_ESCAPE ) {
			if( playerInputState != PIS_WAITING ) {
				highlightX = -1;
				highlightY = -1;
				pushFlavorText( "Action cancelled." );
				centerGameCameraOnTile( LEVEL_TILE_IDX( objects[playerObjIdx].tileX, objects[playerObjIdx].tileY ) );
			}
			playerInputState = PIS_WAITING;
		} else if( e->key.keysym.sym == SDLK_PERIOD ) {
			highlightX = -1;
			highlightY = -1;
			pushFlavorText( "Skipping turn." );
			centerGameCameraOnTile( LEVEL_TILE_IDX( objects[playerObjIdx].tileX, objects[playerObjIdx].tileY ) );
			playerInputState = PIS_IGNORE;
		}

		int moveX = 0;
		int moveY = 0;
		int confirm = 0;
		if( e->key.keysym.sym == SDLK_UP ) {
			moveY = -1;
		} else if( e->key.keysym.sym == SDLK_DOWN ) {
			moveY = 1;
		} else if( e->key.keysym.sym == SDLK_LEFT ) {
			moveX = -1;
		} else if( e->key.keysym.sym == SDLK_RIGHT ) {
			moveX = 1;
		} else if( e->key.keysym.sym == SDLK_RETURN ) {
			confirm = 1;
		}

		if( playerInputState == PIS_WAITING && ( ( moveX != 0 ) || ( moveY != 0 ) ) ) {
			int objIdx = getObjectAtPos( objects[playerObjIdx].tileX + moveX, objects[playerObjIdx].tileY + moveY, OBJ_FLAG_HUMAN );
			if( ( objIdx < 0 ) || ( objects[objIdx].flags & OBJ_FLAG_BG ) ) {
				if( attemptToMoveObject( playerObjIdx, moveX, moveY, 0 ) ) {
					playerInputState = PIS_IGNORE;
				}
			} else {
				attackAction( playerObjIdx, objIdx, 1 );
				playerInputState = PIS_IGNORE;
			}
			centerGameCameraOnTile( LEVEL_TILE_IDX( objects[playerObjIdx].tileX, objects[playerObjIdx].tileY ) );
		} else if( playerInputState == PIS_SHOVING && ( ( moveX != 0 ) || ( moveY != 0 ) ) ) {
			int objIdx = getObjectAtPos( objects[playerObjIdx].tileX + moveX, objects[playerObjIdx].tileY + moveY, OBJ_FLAG_HUMAN | OBJ_FLAG_SHOVABLE );
			if( objIdx < 0 ) {
				// no human object, see if there's anything else
				objIdx = getObjectAtPos( objects[playerObjIdx].tileX + moveX, objects[playerObjIdx].tileY + moveY, OBJ_FLAG_SHOVABLE );
			}

			if( objIdx >= 0 ) {
				shoveAction( objIdx, moveX, moveY );
				playerInputState = PIS_IGNORE;
			} else {
				pushFlavorText( "There is nothing there to shove." );
				playerInputState = PIS_WAITING;
			}
		} else if( playerInputState == PIS_ATTACKING && ( ( moveX != 0 ) || ( moveY != 0 ) ) ) {
			int objIdx = getObjectAtPos( objects[playerObjIdx].tileX + moveX, objects[playerObjIdx].tileY + moveY, OBJ_FLAG_HUMAN );
			if( objIdx < 0 ) {
				// no human object, see if there's anything else
				objIdx = getObjectAtPos( objects[playerObjIdx].tileX + moveX, objects[playerObjIdx].tileY + moveY, 0 );
			}

			if( objIdx >= 0 ) {
				attackAction( playerObjIdx, objIdx, 1 );
				playerInputState = PIS_IGNORE;
			} else {
				pushFlavorText( "There is nothing there to attack." );
				playerInputState = PIS_WAITING;
			}
		} else if( playerInputState == PIS_GRABBING && ( ( moveX != 0 ) || ( moveY != 0 ) ) ) {
			int objIdx = getObjectAtPos( objects[playerObjIdx].tileX + moveX, objects[playerObjIdx].tileY + moveY, OBJ_FLAG_WEAPON | OBJ_FLAG_HORRIFYING );
			if( objIdx < 0 ) {
				objIdx = getObjectAtPos( objects[playerObjIdx].tileX + moveX, objects[playerObjIdx].tileY + moveY, OBJ_FLAG_WEAPON );
			}

			if( objIdx >= 0 ) {
				grabObject( playerObjIdx, objIdx );
				playerInputState = PIS_IGNORE;
			} else {
				pushFlavorText( "There is nothing there to grab." );
				playerInputState = PIS_WAITING;
			}
		} else if( playerInputState == PIS_THROWING && ( ( moveX != 0 ) || ( moveY != 0 ) || ( confirm != 0 ) ) ) {
			if( ( moveX != 0 ) || ( moveY != 0 ) ) {
				// move the highlight
				int newXPos = highlightX + moveX;
				int newYPos = highlightY + moveY;

				int diff = abs( objects[playerObjIdx].tileX - newXPos ) + abs( objects[playerObjIdx].tileY - newYPos );

				if( diff >= 10 ) {
					pushFlavorText( "Spot is too far away." );
				} else if( !level_TileBlocked( newXPos, newYPos ) ) {
					highlightX = newXPos;
					highlightY = newYPos;
					centerGameCameraOnTile( LEVEL_TILE_IDX( highlightX, highlightY ) );
				}
			} else if( confirm ) {
				// throw the object at the spot, it will damage whatever is in that spot for double it's normal damage, and then shatter
				int objIdx = getObjectAtPos( highlightX, highlightY, OBJ_FLAG_HUMAN );
				if( objIdx < 0 ) {
					objIdx = getObjectAtPos( highlightX, highlightY, 0 );
				}

				if( objIdx >= 0 ) {
					attackAction( playerObjIdx, objIdx, 2 );
				} else {
					char flavor[MAX_FLAVOR_STR_LEN];
					PhysicalObject* obj = &( objects[objects[playerObjIdx].heldObject] );
					if( obj->flags & OBJ_FLAG_HORRIFYING ) {
						sprintf( flavor, "%s bursts open on hitting the floor.", obj->reference );
						horrifyBurst( highlightX, highlightY, 8 );
					} else {
						sprintf( flavor, "%s shatters on hitting the floor.", obj->reference );
					}
					flavor[0] = (char)toupper( flavor[0] );
					pushFlavorText( flavor );

					destroyObject( objects[playerObjIdx].heldObject );
					objects[playerObjIdx].heldObject = -1;
				}
				highlightX = -1;
				highlightY = -1;
				
				playerInputState = PIS_IGNORE;
			}
		} else if( playerInputState == PIS_LOOKING && ( ( moveX != 0 ) || ( moveY != 0 ) ) ) {
			// move the highlight
			int newXPos = highlightX + moveX;
			int newYPos = highlightY + moveY;

			int diff = abs( objects[playerObjIdx].tileX - newXPos ) + abs( objects[playerObjIdx].tileY - newYPos );

			if( !level_TileBlocked( newXPos, newYPos ) ) {
				highlightX = newXPos;
				highlightY = newYPos;
				centerGameCameraOnTile( LEVEL_TILE_IDX( highlightX, highlightY ) );
			}
		}
	}
}

static void gameScreen_Process( void )
{
}

static void drawObjectStatusWindow( int objIdx, Vector2 upperLeft, Color clr )
{
	Vector2 lowerRight;// = { 170.0f, 600.0f };
	lowerRight.x = 170.0f;
	lowerRight.y = upperLeft.y + 75.0f;

	drawWindow( upperLeft, lowerRight, clr, -10 );

	Vector2 textPos = upperLeft;
	textPos.x += 8.0f;
	textPos.y += 2.0f;
	txt_DisplayString( objects[objIdx].name, textPos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontLargeText, UI_CAM_FLAGS, -9 );

	textPos.y += 32.0f;
	if( objects[objIdx].maxHealth >= 0 ) {
		char healthStr[128];
		Color healthClr = CLR_GREEN;
		if( objects[objIdx].health <= ( 1.0f / 3.0f ) * objects[objIdx].maxHealth ) {
			healthClr = CLR_RED;
		} else if( objects[objIdx].health <= ( 2.0f / 3.0f ) * objects[objIdx].maxHealth ) {
			healthClr = CLR_YELLOW;
		}
		sprintf( healthStr, "Health: %i/%i", objects[objIdx].health, objects[objIdx].maxHealth );
		txt_DisplayString( healthStr, textPos, healthClr, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, UI_CAM_FLAGS, -9 );
	}

	textPos.y += 16.0f;
	char statusStr[128] = { 0 };
	if( objects[objIdx].state & OBJ_STATE_HORRIFIED ) {
		strcat( statusStr, "Horrified " );
	}
	if( objects[objIdx].state & OBJ_STATE_PRONE ) {
		strcat( statusStr, "Prone " );
	}
	if( objects[objIdx].state & OBJ_STATE_BRAVE ) {
		strcat( statusStr, "Brave " );
	}
	txt_DisplayString( statusStr, textPos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, fontSmallText, UI_CAM_FLAGS, -9 );
}

static void drawValidInputs( void )
{
	Vector2 pos = { 6.0f, 400.0f };

	switch( playerInputState ) {
	case PIS_IGNORE:
		txt_DisplayString( "Not your turn", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		break;
	case PIS_WAITING:
		txt_DisplayString( "Waiting", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		break;
	case PIS_ATTACKING:
		txt_DisplayString( "Attacking", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		break;
	case PIS_GRABBING:
		txt_DisplayString( "Grabbing", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		break;
	case PIS_SHOVING:
		txt_DisplayString( "Shoving", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		break;
	case PIS_THROWING:
		txt_DisplayString( "Throwing", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		break;
	case PIS_LOOKING:
		txt_DisplayString( "Looking", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		break;
	case PIS_WON:
		txt_DisplayString( "WINNER!", pos, CLR_GREEN, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 16.0f;
		break;
	case PIS_LOST:
		txt_DisplayString( "Dead", pos, CLR_RED, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 16.0f;
		break;
	}
	pos.y += 16.0f;

	txt_DisplayString( "Valid Inputs:", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
	pos.y += 16.0f;

	if( playerInputState == PIS_WON ) {
		txt_DisplayString( "Press any key to\ncontinue.", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		return;
	} else if( playerInputState == PIS_LOST ) {
		txt_DisplayString( "Press any key to\ncontinue.", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		return;
	} else if( playerInputState == PIS_WAITING ) {
		txt_DisplayString( "Move with arrow keys", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 16.0f;

		txt_DisplayString( "'a' attack in a direction", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 16.0f;

		txt_DisplayString( "'s' shove in a direction", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 16.0f;

		if( objects[playerObjIdx].heldObject >= 0 ) {
			txt_DisplayString( "'t' to throw held object", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
			pos.y += 16.0f;

			if( objects[objects[playerObjIdx].heldObject].flags & OBJ_FLAG_EDIBLE ) {
				txt_DisplayString( "'e' to eat held object", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
				pos.y += 16.0f;
			}
		} else {
			txt_DisplayString( "'g' grab an object", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
			pos.y += 16.0f;
		}

		txt_DisplayString( "'l' look around", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
			pos.y += 16.0f;
	} else if( playerInputState == PIS_LOOKING ) {
		txt_DisplayString( "Look around with\narrow keys", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 32.0f;

		txt_DisplayString( "ESC to stop looking", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 16.0f;
	} else if( playerInputState == PIS_THROWING ) {
		txt_DisplayString( "Choose target with\narrow keys", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 32.0f;

		txt_DisplayString( "ESC to stop throwing", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 16.0f;

		txt_DisplayString( "ENTER to throw object", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 16.0f;
	} else if( playerInputState == PIS_ATTACKING ) {
		txt_DisplayString( "Choose direction with\narrow keys", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 32.0f;

		txt_DisplayString( "ESC to stop attacking", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 16.0f;
	} else if( playerInputState == PIS_GRABBING ) {
		txt_DisplayString( "Choose direction with\narrow keys", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 32.0f;

		txt_DisplayString( "ESC to stop grabbing", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 16.0f;
	} else if( playerInputState == PIS_SHOVING ) {
		txt_DisplayString( "Choose direction with\narrow keys", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 32.0f;

		txt_DisplayString( "ESC to stop shoving", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 16.0f;
	}

	if( playerInputState != PIS_IGNORE ) {
		txt_DisplayString( "'.' to skip turn", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
		pos.y += 16.0f;
	}

	txt_DisplayString( "'q' to quit", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, fontSmallText, UI_CAM_FLAGS, 0 );
	pos.y += 16.0f;
}

static void gameScreen_Draw( void )
{
	// main status window
	Vector2 upperLeft = { 0.0f, 0.0f };
	Vector2 lowerRight = { 170.0f, 600.0f };
	drawWindow( upperLeft, lowerRight, CLR_WHITE, -10 );

	// individual status windows
	//  players window
	drawObjectStatusWindow( turnIdx, upperLeft, CLR_WHITE );

	//  highlighted object window
	int objIdx = getObjectAtPos( highlightX, highlightY, OBJ_FLAG_HUMAN );
	if( objIdx < 0 ) {
		objIdx = getObjectAtPos( highlightX, highlightY, 0 );
	}
	if( objIdx >= 0 ) {
		upperLeft.y += 75.0f;
		drawObjectStatusWindow( objIdx, upperLeft, CLR_RED );
	}

	// grab the three closest humans and display their status as well
	int closestIndices[3] = { -1, -1, -1 };
	int distances[3] = { 10000, 10000, 10000 };
	for( int i = 0; i < MAX_PHYSICAL_OBJECTS; ++i ) {
		if( ( objects[i].flags & OBJ_FLAG_IN_USE ) && ( objects[i].flags & OBJ_FLAG_HUMAN ) ) {
			for( int j = 0; j < 3; ++j ) {
				int dist = objectDistance( playerObjIdx, i );
				if( ( closestIndices[j] < 0 ) || ( dist <= distances[j] ) ) {
					// this list will be sorted by distance, so we're inserting at j, move everything else down
					for( int k = 2; k > j; --k ) {
						closestIndices[k] = closestIndices[k-1];
						distances[k] = distances[k-1];
					}
					closestIndices[j] = i;
					distances[j] = dist;
					break;
				}
			}
		}
	}

	for( int i = 0; i < 3; ++i ) {
		if( closestIndices[i] >= 0 && ( objects[closestIndices[i]].flags & OBJ_FLAG_IN_USE ) ) {
			upperLeft.y += 75.0f;
			drawObjectStatusWindow( closestIndices[i], upperLeft, CLR_WHITE );
		}
	}

	// output text window
	upperLeft.x = 170.0f;
	upperLeft.y = 430.0f;
	lowerRight.x = 800.0f;
	lowerRight.y = 600.0f;
	drawWindow( upperLeft, lowerRight, CLR_WHITE, -10 );

	Vector2 textLowerLeft;
	textLowerLeft.x = upperLeft.x + 8.0f;
	textLowerLeft.y = lowerRight.y - 8.0f;
	drawFlavorText( textLowerLeft, 16.0f, 10, -9 );

	// info window
	/*upperLeft.x = 174.0f;
	upperLeft.y = 4.0f;
	lowerRight.x = 796.0f;
	lowerRight.y = 40.0f;
	drawWindow( upperLeft, lowerRight, -10 );*/

	drawValidInputs( );

	drawLevel( );

	if( ( highlightX >= 0 ) && ( highlightY >= 0 ) ) {
		img_Draw( highlightImage, GAME_CAM_FLAGS, level[LEVEL_TILE_IDX( highlightX, highlightY )].renderPos, level[LEVEL_TILE_IDX( highlightX, highlightY )].renderPos, 10 );
	}

	for( int i = 0; i < MAX_PHYSICAL_OBJECTS; ++i ) {
		if( objects[i].flags & OBJ_FLAG_IN_USE ) {
			Vector2 objPos = level[LEVEL_TILE_IDX(objects[i].tileX,objects[i].tileY)].renderPos;
			char depth = 5;
			if( objects[i].flags & OBJ_FLAG_BG ) {
				depth = 4;
			}
			img_Draw( objects[i].image, GAME_CAM_FLAGS, objPos, objPos, 5 );
			Vector2 statePos = objPos;
			statePos.y -= 14.0f;
			if( objects[i].state & OBJ_STATE_PRONE ) {
				statePos.x = objPos.x - 14.0f;
				img_Draw( proneStatusImage, GAME_CAM_FLAGS, statePos, statePos, 6 );
			}
			if( objects[i].state & OBJ_STATE_HORRIFIED ) {
				statePos.x = objPos.x;
				img_Draw( horrifiedStatusImage, GAME_CAM_FLAGS, statePos, statePos, 6 );
			}
			if( objects[i].state & OBJ_STATE_BRAVE ) {
				statePos.x = objPos.x + 14.0f;
				img_Draw( braveStatusImage, GAME_CAM_FLAGS, statePos, statePos, 6 );
			}

			if( objects[i].heldObject != -1 ) {
				statePos.x = objPos.x - 4.0f;
				statePos.y = objPos.y + 4.0f;
				img_Draw( objects[objects[i].heldObject].image, GAME_CAM_FLAGS, statePos, statePos, 4 );
			}
		}
	}
}

static void gameScreen_PhysicsTick( float dt )
{
	turnDelay -= dt;
	if( turnDelay <= 0.0f ) {
		if( ( turnIdx != playerObjIdx ) || ( playerInputState == PIS_IGNORE ) ) {
			do {
				++turnIdx;
				if( turnIdx >= MAX_PHYSICAL_OBJECTS ) {
					turnIdx = 0;
				}
			} while( !( objects[turnIdx].flags & OBJ_FLAG_IN_USE ) || ( objects[turnIdx].action == NULL ) );
			turnDelay = COMP_TURN_DELAY;
			objects[turnIdx].action( turnIdx );
		}
	}
}

struct GameState gameScreenState = { gameScreen_Enter, gameScreen_Exit, gameScreen_ProcessEvents,
	gameScreen_Process, gameScreen_Draw, gameScreen_PhysicsTick };