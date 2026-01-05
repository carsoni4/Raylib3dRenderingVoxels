#ifndef CHUNK_H
#define CHUNK_H

#include "raylib.h"
#include "config.h"
#include <stdbool.h>

typedef struct Chunk {
    int blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_WIDTH];
    Mesh mesh;
    Model model;
} Chunk;

typedef enum BlockVal {
    AIR,
    SOLID
} BlockVal;

extern Chunk ***chunks;
extern int currentRenderDistance;

void allocateChunks(int worldSize);
void freeChunks(int worldSize);
void initChunks();
Vector3 getPlayerChunkPos(Camera camera);
void updateVisibleChunks(Vector3 playerChunkPos, Shader fogShader);

int getBlockAtWorld(int wx, int wy, int wz);
int setBlockAtWorld(int wx, int wy, int wz, int blockID, BlockFace placeface, Shader fogShader);

#endif