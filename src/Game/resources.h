#ifndef RESOURCES_H
#define RESOURCES_H

int mute;

int fontLargeTitle;
int fontSmallTitle;
int fontLargeText;
int fontSmallText;

enum WindowEdge {
	WE_UL,
	WE_UM,
	WE_UR,
	WE_ML,
	WE_MM,
	WE_MR,
	WE_LL,
	WE_LM,
	WE_LR,
	WINDOW_EDGE_COUNT
};

int* windowEdgeImages;

int floorImage;
int wallImage;
int innerCornerWallImage;
int outerCornerWallImage;
int darknessImage;

int monsterImage;
int warriorImage;
int berserkerImage;
int ulfhednarImage;
int archerImage;
int skaldImage;
int tableImage;
int chairImage;
int fireImage;
int pillarImage;
int corpseImage;

int horrifiedStatusImage;
int proneStatusImage;
int braveStatusImage;

int highlightImage;

void loadAllResources( void );

#endif /* inclusion guard */