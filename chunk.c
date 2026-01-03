#include "chunk.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "mesh.h"

Chunk ***chunks = NULL;
int currentRenderDistance = DEFAULT_RENDER_DISTANCE;

void allocateChunks(int worldSize)
{
    chunks = (struct Chunk ***)malloc(worldSize * sizeof(struct Chunk **));
    for (int cx = 0; cx < worldSize; cx++)
    {
        chunks[cx] = (struct Chunk **)malloc(1 * sizeof(struct Chunk *));
        chunks[cx][0] = (struct Chunk *)malloc(worldSize * sizeof(struct Chunk));
    }
}

void freeChunks(int worldSize)
{
    if (chunks == NULL) return;
    
    for (int cx = 0; cx < worldSize; cx++)
    {
        for (int cy = 0; cy < 1; cy++)
        {
            for (int cz = 0; cz < worldSize; cz++)
            {
                if(chunks[cx][cy][cz].model.meshCount > 0)
                    UnloadModel(chunks[cx][cy][cz].model);
            }
        }
        free(chunks[cx][0]);
        free(chunks[cx]);
    }
    free(chunks);
    chunks = NULL;
}

static int floor_div(int a, int b)
{
    int d = a / b;
    if ((a ^ b) < 0 && (a % b)) d -= 1;
    return d;
}

void initChunks()
{
    printf("START");
    // Initialize all chunks
    for(int cx = 0; cx < WORLD_SIZE_CHUNKS; cx++)
    {
        for(int cy = 0; cy < 1; cy++)
        {
            for(int cz = 0; cz < WORLD_SIZE_CHUNKS; cz++)
            {
                // Initialize blocks within this chunk
                for(int x = 0; x < CHUNK_WIDTH; x++)
                {
                    for(int y = 0; y < CHUNK_HEIGHT; y++)
                    {
                        for(int z = 0; z < CHUNK_WIDTH; z++)
                        {
                            //TODO: implement a better noise
                            // simple sin heightmap
                            float freq = 0.12f;
                            float amp  = (CHUNK_HEIGHT * 0.5f);
                            int baseH  = CHUNK_HEIGHT / 3;

                            int worldX = cx*CHUNK_WIDTH + x;
                            int worldZ = cz*CHUNK_WIDTH + z;
                            int height = (int)(((sinf(worldX * freq) + sinf(worldZ * freq * 0.5f)) * 0.5f) * amp) + baseH;
                            if (height < 0) height = 0;
                            if (height >= CHUNK_HEIGHT) height = CHUNK_HEIGHT - 1;

                            chunks[cx][cy][cz].blocks[x][y][z] = (y <= height) ? 1 : 0;
                        }
                    }
                }
                //Dont generate yet, only want to when needed.
                chunks[cx][cy][cz].mesh = (Mesh){0};
                chunks[cx][cy][cz].model = (Model){0};
            }
        }
    }
}

void updateVisibleChunks(Vector3 playerChunkPos, Shader fogShader)
{
    int pcx = (int)playerChunkPos.x;
    int pcy = (int)playerChunkPos.y;
    int pcz = (int)playerChunkPos.z;

    for(int cx = pcx - currentRenderDistance; cx <= pcx + currentRenderDistance; cx++)
    {
        for(int cz = pcz - currentRenderDistance; cz <= pcz + currentRenderDistance; cz++)
        {
            if (cx < 0 || cx >= WORLD_SIZE_CHUNKS) continue;
            if (cz < 0 || cz >= WORLD_SIZE_CHUNKS) continue;
            
            //If chunk does not exist, generate it
            if(chunks[cx][pcy][cz].mesh.vertexCount == 0)
            {
                chunks[cx][pcy][cz].mesh = generateChunkMesh(chunks, cx, pcy, cz);
                chunks[cx][pcy][cz].model = LoadModelFromMesh(chunks[cx][pcy][cz].mesh);
                chunks[cx][pcy][cz].model.materials[0].shader = fogShader;
            }
        }
    }
    //Unload if outside render distance
    for(int cx = 0; cx < WORLD_SIZE_CHUNKS; cx++)
    {
        for(int cz = 0; cz < WORLD_SIZE_CHUNKS; cz++)
        {
            int dx = abs(cx - pcx);
            int dz = abs(cz - pcz);
            if ((dx > currentRenderDistance + 2 || dz > currentRenderDistance + 2) && chunks[cx][pcy][cz].mesh.vertexCount > 0)
            {
                UnloadModel(chunks[cx][pcy][cz].model);
                chunks[cx][pcy][cz].mesh = (Mesh){0};
                chunks[cx][pcy][cz].model = (Model){0};
            }
        }
    }
}

Vector3 getPlayerChunkPos(Camera camera)
{
    int cx = floor_div((int)camera.position.x, CHUNK_WIDTH);
    int cy = 0;
    int cz = floor_div((int)camera.position.z, CHUNK_WIDTH);    

    return (Vector3){(float)cx, (float)cy, (float)cz}; 
}

int getBlockAtWorld(int wx, int wy, int wz)
{
    int cx = floor_div(wx, CHUNK_WIDTH);
    int cy = floor_div(wy, CHUNK_HEIGHT);
    int cz = floor_div(wz, CHUNK_WIDTH);

    if (cy < 0 || cy >= 1) return 0;
    if (cx < 0 || cx >= WORLD_SIZE_CHUNKS) return 0;
    if (cz < 0 || cz >= WORLD_SIZE_CHUNKS) return 0;

    int lx = wx - cx * CHUNK_WIDTH;
    int ly = wy - cy * CHUNK_HEIGHT;
    int lz = wz - cz * CHUNK_WIDTH;

    if (lx < 0 || lx >= CHUNK_WIDTH) return 0;
    if (ly < 0 || ly >= CHUNK_HEIGHT) return 0;
    if (lz < 0 || lz >= CHUNK_WIDTH) return 0;

    return chunks[cx][cy][cz].blocks[lx][ly][lz];
}

int setBlockAtWorld(int wx, int wy, int wz, int blockID, BlockFace placeface, Shader fogShader)
{
    int cx = floor_div(wx, CHUNK_WIDTH);
    int cy = floor_div(wy, CHUNK_HEIGHT);
    int cz = floor_div(wz, CHUNK_WIDTH);

    switch(placeface)
    {
        case FACE_NEG_X: wx -=1; break;
        case FACE_POS_X: wx +=1; break;
        case FACE_NEG_Y: wy -=1; break;
        case FACE_POS_Y: wy +=1; break;
        case FACE_NEG_Z: wz -=1; break;
        case FACE_POS_Z: wz +=1; break;
    }

    if (cy < 0 || cy >= 1) return 0;
    if (cx < 0 || cx >= WORLD_SIZE_CHUNKS) return 0;
    if (cz < 0 || cz >= WORLD_SIZE_CHUNKS) return 0;
    int lx = wx - cx * CHUNK_WIDTH;
    int ly = wy - cy * CHUNK_HEIGHT;
    int lz = wz - cz * CHUNK_WIDTH;

    if (lx < 0 || lx >= CHUNK_WIDTH) return 0;
    if (ly < 0 || ly >= CHUNK_HEIGHT) return 0;
    if (lz < 0 || lz >= CHUNK_WIDTH) return 0;

    chunks[cx][cy][cz].blocks[lx][ly][lz] = blockID;

    //Regenerate mesh for affected chunk/s (Regenerate neighboring chunks if on edge)
    //TODO: Make sure this cant go out of bounds. I think this is fine when it is moved to infinite world
    if(lx == 15)
    {
        UnloadModel(chunks[cx+1][cy][cz].model);
        chunks[cx+1][cy][cz].mesh = generateChunkMesh(chunks, cx+1, cy, cz);
        chunks[cx+1][cy][cz].model = LoadModelFromMesh(chunks[cx+1][cy][cz].mesh);
        chunks[cx+1][cy][cz].model.materials[0].shader = fogShader;
    } 
    else if(lx == 0)
    {
        UnloadModel(chunks[cx-1][cy][cz].model);
        chunks[cx-1][cy][cz].mesh = generateChunkMesh(chunks, cx-1, cy, cz);
        chunks[cx-1][cy][cz].model = LoadModelFromMesh(chunks[cx-1][cy][cz].mesh);
        chunks[cx-1][cy][cz].model.materials[0].shader = fogShader;
    }
    if(lz == 15)
    {
        UnloadModel(chunks[cx][cy][cz+1].model);
        chunks[cx][cy][cz+1].mesh = generateChunkMesh(chunks, cx, cy, cz+1);
        chunks[cx][cy][cz+1].model = LoadModelFromMesh(chunks[cx][cy][cz+1].mesh);
        chunks[cx][cy][cz+1].model.materials[0].shader = fogShader;
    }
    else if(lz == 0)
    {
        UnloadModel(chunks[cx][cy][cz-1].model);
        chunks[cx][cy][cz-1].mesh = generateChunkMesh(chunks, cx, cy, cz-1);
        chunks[cx][cy][cz-1].model = LoadModelFromMesh(chunks[cx][cy][cz-1].mesh);
        chunks[cx][cy][cz-1].model.materials[0].shader = fogShader;
    }

    UnloadModel(chunks[cx][cy][cz].model);
    chunks[cx][cy][cz].mesh = generateChunkMesh(chunks, cx, cy, cz);
    chunks[cx][cy][cz].model = LoadModelFromMesh(chunks[cx][cy][cz].mesh);
    chunks[cx][cy][cz].model.materials[0].shader = fogShader;
    return 1;
}