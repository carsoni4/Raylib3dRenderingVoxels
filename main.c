#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "config.h"
#include "chunk.h"
#include "mesh.h"

//TODO: Move this somewhere it makes sense
static Vector3 lastPlayerChunkPos = {0,0,0};
static Shader fogShader = {0};
static int fogDensityLoc = 0;
static float fogDensity = FOG_VALUE;

//TODO: Move this somewhere that makes sense. probably a player action file with setblock and getblock
bool raycastVoxel(Camera camera, float maxDistance, Vector3 *outBlock, BlockFace *outFace)
{
    //TODO: Cleanup the logic here
    Vector2 screenCenter = (Vector2){ (float)SCREEN_WIDTH*0.5f, (float)SCREEN_HEIGHT*0.5f };
    Ray ray = GetMouseRay(screenCenter, camera);
    Vector3 dir = ray.direction;

    int x = (int)floor(ray.position.x);
    int y = (int)floor(ray.position.y);
    int z = (int)floor(ray.position.z);

    if(getBlockAtWorld(x, y, z)){
        *outBlock = (Vector3){(float)x, (float)y, (float)z};
        return true;
    }

    int stepX = (dir.x > 0.f) ? 1 : -1;
    int stepY = (dir.y > 0.f) ? 1 : -1;
    int stepZ = (dir.z > 0.f) ? 1 : -1;

    float tDeltaX = (dir.x == 0.f) ? INFINITY : fabs(1.f / dir.x);
    float tDeltaY = (dir.y == 0.f) ? INFINITY : fabs(1.f / dir.y);
    float tDeltaZ = (dir.z == 0.f) ? INFINITY : fabs(1.f / dir.z);

    float rx = ray.position.x, ry = ray.position.y, rz = ray.position.z;
    float tMaxX = (dir.x > 0.f) ? ((x + 1.0f) - rx) * tDeltaX : (rx - (float)x) * tDeltaX;
    float tMaxY = (dir.y > 0.f) ? ((y + 1.0f) - ry) * tDeltaY : (ry - (float)y) * tDeltaY;
    float tMaxZ = (dir.z > 0.f) ? ((z + 1.0f) - rz) * tDeltaZ : (rz - (float)z) * tDeltaZ;

    float t = 0.0f;
    while (t <= maxDistance) {
        // Step to next voxel
        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                x += stepX;
                t = tMaxX;
                tMaxX += tDeltaX;
                if (getBlockAtWorld(x, y, z)) {
                    *outBlock = (Vector3){x, y, z};
                    *outFace = (stepX > 0) ? FACE_NEG_X : FACE_POS_X;
                    return true;
                } 
            }
            else 
            {
                z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                if (getBlockAtWorld(x, y, z)) {
                    *outBlock = (Vector3){x, y, z};
                    *outFace = (stepZ > 0) ? FACE_NEG_Z : FACE_POS_Z;
                    return true;
                }
            }
        }
        else 
        {
            if (tMaxY < tMaxZ) {
                y += stepY;
                t = tMaxY;
                tMaxY += tDeltaY;
                if (getBlockAtWorld(x, y, z)) {
                    *outBlock = (Vector3){x, y, z};
                    *outFace = (stepY > 0) ? FACE_NEG_Y : FACE_POS_Y;
                    return true;
                }
            } 
            else {
                z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                if (getBlockAtWorld(x, y, z)) {
                    *outBlock = (Vector3){x, y, z};
                    *outFace = (stepZ > 0) ? FACE_NEG_Z : FACE_POS_Z;
                    return true;
                }
            }
        }
    }
    return false;
}

int main(void) 
{
    //Main Window Handling
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Carson's Game");
    RenderTexture2D target = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    SetTargetFPS(TARGET_FPS);    
    DisableCursor();
    
    //Map Memory and then init chunks
    allocateChunks(WORLD_SIZE_CHUNKS);
    fogShader = LoadShader("shader/fog.vs", "shader/fog.fs");
    fogShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(fogShader, "viewPos");
    fogShader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(fogShader, "matModel");
    initChunks();

    fogDensityLoc = GetShaderLocation(fogShader, "fogDensity");
    SetShaderValue(fogShader, fogDensityLoc, &fogDensity, SHADER_UNIFORM_FLOAT);

    //Camera Setup
    Camera3D camera = {0};
    camera.position = (Vector3){ (WORLD_SIZE_CHUNKS * CHUNK_WIDTH)/2, (float)CHUNK_HEIGHT, (WORLD_SIZE_CHUNKS * CHUNK_WIDTH)/2 };//CENTER OF WORLD
    camera.target = (Vector3){ 1.0f, 1.0f, 1.0f }; //Camera is looking here
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f }; //up-vector (rotation towards target) idrk what this means
    camera.fovy = 90.0f; //field of view? I assume there is also an x one. Something to look into
    camera.projection = CAMERA_PERSPECTIVE;

    Shader pxShader = LoadShader(0, "shader/pixelizer.fs");
    int resolutionLoc = GetShaderLocation(pxShader, "resolution");
    int pixelSizeLoc = GetShaderLocation(pxShader, "pixelSize");

    float resolution[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
    SetShaderValue(pxShader, resolutionLoc, resolution, SHADER_UNIFORM_VEC2);

    float pixelSize = 5.0f; //bigger = more pixelated
    SetShaderValue(pxShader, pixelSizeLoc, &pixelSize, SHADER_UNIFORM_FLOAT);

    //TODO make sure these are ok to be here, do I need to alloc, what ever
    Vector3 highlighted = {0.0f,0.0f,0.0f}; //What the players looking at
    bool casting = false;
    BlockFace placeFace;

    Vector3 playerChunkPos = getPlayerChunkPos(camera);
    updateVisibleChunks(playerChunkPos, fogShader);
    lastPlayerChunkPos = playerChunkPos;

    while(!WindowShouldClose())
    {

        playerChunkPos = getPlayerChunkPos(camera);
        if (playerChunkPos.x != lastPlayerChunkPos.x || playerChunkPos.z != lastPlayerChunkPos.z)
        {
            updateVisibleChunks(playerChunkPos, fogShader);
            lastPlayerChunkPos = playerChunkPos;
        }

        if (raycastVoxel(camera, MAX_REACH, &highlighted, &placeFace))
            casting = true;
        else
            casting = false;
        
        if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && casting)
            setBlockAtWorld((int)highlighted.x, (int)highlighted.y, (int)highlighted.z, 0, FACE_NONE, fogShader);
        if(IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && casting)
            setBlockAtWorld((int)highlighted.x, (int)highlighted.y, (int)highlighted.z, 1, placeFace, fogShader);

        UpdateCamera(&camera, CAMERA_FREE); //Update the camera every frame
        SetShaderValue(fogShader, fogShader.locs[SHADER_LOC_VECTOR_VIEW], &camera.position.x, SHADER_UNIFORM_VEC3);
        BeginTextureMode(target);
            ClearBackground(SKY_COLOR);
            //3D Rendering
            BeginMode3D(camera);
                // Only render chunks within render distance
                int pcx = (int)playerChunkPos.x;
                int pcz = (int)playerChunkPos.z;
                
                for(int cx = pcx - currentRenderDistance; cx <= pcx + currentRenderDistance; cx++)
                {
                    for(int cz = pcz - currentRenderDistance; cz <= pcz + currentRenderDistance; cz++)
                    {
                        if (cx < 0 || cx >= WORLD_SIZE_CHUNKS) continue;
                        if (cz < 0 || cz >= WORLD_SIZE_CHUNKS) continue;
                        
                        // Only draw if mesh exists
                        if (chunks[cx][0][cz].mesh.vertexCount > 0)
                        {
                            DrawModel(chunks[cx][0][cz].model, 
                                     (Vector3){cx * CHUNK_WIDTH, 0, cz * CHUNK_WIDTH}, 
                                     1.0f, WHITE);
                        }
                    }
                }                
                if(casting) {
                    for (int s = 0; s < 4; s++) 
                    {
                        float scale = 1.0f + 0.01f * (float)s; //adjust multiplier for desired thickness
                        DrawCubeWires((Vector3){highlighted.x + 0.5f, highlighted.y + 0.5f, highlighted.z + 0.5f}, scale * 1.0f, scale * 1.0f, scale * 1.0f, Fade(YELLOW, 1.0f - 0.15f * s));
                    }
                }
            EndMode3D();
        EndTextureMode();
        
        BeginDrawing();
            BeginShaderMode(pxShader);
                DrawTextureRec(target.texture, (Rectangle){ 0, 0, (float)target.texture.width, -(float)target.texture.height }, (Vector2){ 0, 0 }, WHITE);
            EndShaderMode();
            //UI Last So Its On Top
            DrawText(TextFormat("Highlighted Block X: %d Y: %d Z: %d", (int)highlighted.x, (int)highlighted.y, (int)highlighted.z), 10, 50, 20, SKYBLUE);
            DrawText(TextFormat("%d", GetFPS()), 10, 10, 30, CROSSHAIR_COLOR);
            DrawText("+", SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 40, CROSSHAIR_COLOR);
        EndDrawing();
    }
    UnloadShader(pxShader);
    UnloadRenderTexture(target);
    freeChunks(WORLD_SIZE_CHUNKS);
    UnloadShader(fogShader);
    CloseWindow();
    return 0;
}