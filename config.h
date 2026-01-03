#ifndef CONFIG_H
#define CONFIG_H
#include "raylib.h"

//CONSTANTS
#define TARGET_FPS 120
#define SCREEN_WIDTH 1600
#define SCREEN_HEIGHT 900
#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 64
#define SKY_COLOR (Color){100, 100, 100, 255} //TODO: Decide on fog color
#define MAX_REACH 8.0f
#define CROSSHAIR_COLOR SKYBLUE
#define DEFAULT_RENDER_DISTANCE 10
#define WORLD_SIZE_CHUNKS 100 //100 x 100 Chunk World Size
#define GLSL_VERSION 330
#define FOG_VALUE .0125f

//ENUMS
typedef enum BlockFace {
    FACE_POS_X,
    FACE_NEG_X,
    FACE_POS_Y,
    FACE_NEG_Y,
    FACE_POS_Z,
    FACE_NEG_Z,
    FACE_NONE
} BlockFace;

#endif
