#include <stdio.h>
#include "raylib.h"
#include <stdlib.h>
#include <math.h>

#define TARGET_FPS 120
#define SCREEN_WIDTH 1600
#define SCREEN_HEIGHT 900
#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 64
#define RENDER_DISTANCE 10
#define SKY_COLOR BLACK
#define MAX_REACH 8.0f
#define CROSSHAIR_COLOR SKYBLUE

struct Chunk 
{
    int blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_WIDTH];
    Mesh mesh;
    Model model;
};

//Global chunk grid so helpers can access voxels by world coords (Most likely need to change this to main later and add memory allocation)
static struct Chunk chunks[RENDER_DISTANCE][1][RENDER_DISTANCE];

//integer floor division that works for negative coordinates
static int floor_div(int a, int b)
{
    int d = a / b;
    if ((a ^ b) < 0 && (a % b)) d -= 1;
    return d;
}

//False For Air, True Otherwise
int getBlockAtWorld(int wx, int wy, int wz)
{
    int cx = floor_div(wx, CHUNK_WIDTH);
    int cy = floor_div(wy, CHUNK_HEIGHT);
    int cz = floor_div(wz, CHUNK_WIDTH);

    if (cy < 0 || cy >= 1) return 0;
    if (cx < 0 || cx >= RENDER_DISTANCE) return 0;
    if (cz < 0 || cz >= RENDER_DISTANCE) return 0;

    int lx = wx - cx * CHUNK_WIDTH;
    int ly = wy - cy * CHUNK_HEIGHT;
    int lz = wz - cz * CHUNK_WIDTH;

    if (lx < 0 || lx >= CHUNK_WIDTH) return 0;
    if (ly < 0 || ly >= CHUNK_HEIGHT) return 0;
    if (lz < 0 || lz >= CHUNK_WIDTH) return 0;

    return chunks[cx][cy][cz].blocks[lx][ly][lz];
}

//Set block at world coordinates. Returns 1 if set, 0 on out-of-range.
int setBlockAtWorld(int wx, int wy, int wz, int blockID)
{
    int cx = floor_div(wx, CHUNK_WIDTH);
    int cy = floor_div(wy, CHUNK_HEIGHT);
    int cz = floor_div(wz, CHUNK_WIDTH);

    if (cy < 0 || cy >= 1) return 0;
    if (cx < 0 || cx >= RENDER_DISTANCE) return 0;
    if (cz < 0 || cz >= RENDER_DISTANCE) return 0;

    int lx = wx - cx * CHUNK_WIDTH;
    int ly = wy - cy * CHUNK_HEIGHT;
    int lz = wz - cz * CHUNK_WIDTH;

    if (lx < 0 || lx >= CHUNK_WIDTH) return 0;
    if (ly < 0 || ly >= CHUNK_HEIGHT) return 0;
    if (lz < 0 || lz >= CHUNK_WIDTH) return 0;

    chunks[cx][cy][cz].blocks[lx][ly][lz] = blockID;
    return 1;
}

bool raycastVoxel(Camera camera, float maxDistance, Vector3 *outBlock)
{
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
            } else {
                z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
            }
        } else {
            if (tMaxY < tMaxZ) {
                y += stepY;
                t = tMaxY;
                tMaxY += tDeltaY;
            } else {
                z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
            }
        }

        if (getBlockAtWorld(x, y, z)) {
            *outBlock = (Vector3){(float)x, (float)y, (float)z};
            return true;
        }
    }
    return false;
}

static const Vector3 facePositions[6][4] = {
    { {0,0,1}, {1,0,1}, {0,1,1}, {1,1,1} },
    { {1,0,0}, {0,0,0}, {1,1,0}, {0,1,0} },
    { {1,0,1}, {1,0,0}, {1,1,1}, {1,1,0} },
    { {0,0,0}, {0,0,1}, {0,1,0}, {0,1,1} },
    { {0,1,1}, {1,1,1}, {0,1,0}, {1,1,0} },
    { {0,0,0}, {1,0,0}, {0,0,1}, {1,0,1} }
};

static const Vector3 faceNormals[6] = {
    {0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}
};

static const Vector2 faceUvs[4] = {
    {0,0}, {1,0}, {0,1}, {1,1}
};

void AddFace(Mesh *mesh, int *v, int *i, int face, Vector3 offset, int width, int height)
{
    Vector3 p0, p1, p2, p3;
    Vector2 uv0, uv1, uv2, uv3;  // Custom UVs per face

    switch (face)
    {
        case 0: // +Z front
            p0 = (Vector3){0, 0, 1};
            p1 = (Vector3){(float)width, 0, 1};
            p2 = (Vector3){0, (float)height, 1};
            p3 = (Vector3){(float)width, (float)height, 1};
            uv0 = (Vector2){0, 0}; uv1 = (Vector2){1, 0}; 
            uv2 = (Vector2){0, 1}; uv3 = (Vector2){1, 1};
            break;
        case 1: // -Z back
            p0 = (Vector3){0, 0, 0};
            p1 = (Vector3){0, (float)height, 0};
            p2 = (Vector3){(float)width, 0, 0};
            p3 = (Vector3){(float)width, (float)height, 0};
            uv0 = (Vector2){0, 0}; uv1 = (Vector2){0, 1}; 
            uv2 = (Vector2){1, 0}; uv3 = (Vector2){1, 1};
            break;
        case 2: // +X right
            p0 = (Vector3){1, 0, (float)width};
            p1 = (Vector3){1, 0, 0};
            p2 = (Vector3){1, (float)height, (float)width};
            p3 = (Vector3){1, (float)height, 0};
            uv0 = (Vector2){1, 0}; uv1 = (Vector2){0, 0}; 
            uv2 = (Vector2){1, 1}; uv3 = (Vector2){0, 1};
            break;
        case 3: // -X left
            p0 = (Vector3){0, 0, (float)width};
            p1 = (Vector3){0, (float)height, (float)width};
            p2 = (Vector3){0, 0, 0};
            p3 = (Vector3){0, (float)height, 0};
            uv0 = (Vector2){1, 0}; uv1 = (Vector2){1, 1}; 
            uv2 = (Vector2){0, 0}; uv3 = (Vector2){0, 1};
            break;
        case 4: // +Y top
            p0 = (Vector3){0, 1, (float)height};
            p1 = (Vector3){(float)width, 1, (float)height};
            p2 = (Vector3){0, 1, 0};
            p3 = (Vector3){(float)width, 1, 0};
            // Reordered to match: top-left, top-right, bottom-left, bottom-right
            uv0 = (Vector2){0, 1}; uv1 = (Vector2){1, 1}; 
            uv2 = (Vector2){0, 0}; uv3 = (Vector2){1, 0};
            break;
        case 5: // -Y bottom
            p0 = (Vector3){0, 0, 0};
            p1 = (Vector3){(float)width, 0, 0};
            p2 = (Vector3){0, 0, (float)height};
            p3 = (Vector3){(float)width, 0, (float)height};
            // Flipped winding
            uv0 = (Vector2){0, 1}; uv1 = (Vector2){1, 1}; 
            uv2 = (Vector2){0, 0}; uv3 = (Vector2){1, 0};
            break;
        default:
            printf("GOT HERE");
            p0 = p1 = p2 = p3 = (Vector3){0,0,0};
            uv0 = uv1 = uv2 = uv3 = (Vector2){0,0};
            break;
    }

    Vector3 verts[4] = {p0, p1, p2, p3};
    Vector2 uvs[4] = {uv0, uv1, uv2, uv3};

    for (int vi = 0; vi < 4; vi++)
    {
        int vv = *v;
        mesh->vertices[vv*3 + 0] = verts[vi].x + offset.x;
        mesh->vertices[vv*3 + 1] = verts[vi].y + offset.y;
        mesh->vertices[vv*3 + 2] = verts[vi].z + offset.z;

        mesh->normals[vv*3 + 0] = faceNormals[face].x;
        mesh->normals[vv*3 + 1] = faceNormals[face].y;
        mesh->normals[vv*3 + 2] = faceNormals[face].z;

        float tu = uvs[vi].x * width;
        float tv = uvs[vi].y * height;
        
        mesh->texcoords[vv*2 + 0] = tu;
        mesh->texcoords[vv*2 + 1] = tv;

        (*v)++;
    }
    
    mesh->indices[(*i)++] = (*v) - 4;
    mesh->indices[(*i)++] = (*v) - 3;
    mesh->indices[(*i)++] = (*v) - 2;
    mesh->indices[(*i)++] = (*v) - 2;
    mesh->indices[(*i)++] = (*v) - 3;
    mesh->indices[(*i)++] = (*v) - 1;
}

void GreedyMesh(Mesh *mesh, struct Chunk chunks[RENDER_DISTANCE][1][RENDER_DISTANCE], int *v, int *i, int face, int cx, int cy, int cz)
{
    //Determine axis and direction based on face
    //face: 0=+Z, 1=-Z, 2=+X, 3=-X, 4=+Y, 5=-Y
    int axis = face / 2;  // 0=Z, 1=X, 2=Y
    int dir = (face % 2 == 0) ? 1 : -1;  // positive or negative direction
    
    //Set up iteration based on axis
    int depth, width_dim, height_dim;
    int depth_size, width_size, height_size;
    
    if (axis == 0) { // Z axis
        depth_size = CHUNK_WIDTH;
        width_size = CHUNK_WIDTH;
        height_size = CHUNK_HEIGHT;
    } else if (axis == 1) { // X axis
        depth_size = CHUNK_WIDTH;
        width_size = CHUNK_WIDTH;  //actually Z
        height_size = CHUNK_HEIGHT;
    } else { // axis == 2, Y axis
        depth_size = CHUNK_HEIGHT;
        width_size = CHUNK_WIDTH;
        height_size = CHUNK_WIDTH;  //actually Z
    }
    
    for (int d = 0; d < depth_size; d++)
    {
        int mask[CHUNK_WIDTH][CHUNK_HEIGHT] = {0};
        
        //Build Mask
        for (int h = 0; h < height_size; h++)
        {
            for (int w = 0; w < width_size; w++)
            {
                int x, y, z;
                int nx, ny, nz;  // neighbor coords
                
                //Map w, h, d to x, y, z based on axis
                if (axis == 0) { // Z axis
                    x = w; y = h; z = d;
                    nx = x; ny = y; nz = z + dir;
                } else if (axis == 1) { // X axis
                    x = d; y = h; z = w;
                    nx = x + dir; ny = y; nz = z;
                } else { // Y axis
                    x = w; y = d; z = h;
                    nx = x; ny = y + dir; nz = z;
                }
                
                //Check if this block is solid and if neighbor is empty (including across chunk boundaries)
                bool is_solid = chunks[cx][cy][cz].blocks[x][y][z] != 0;

                bool neighborEmpty = false;
                if (nx >= 0 && nx < CHUNK_WIDTH && ny >= 0 && ny < CHUNK_HEIGHT && nz >= 0 && nz < CHUNK_WIDTH)
                {
                    //neighbor inside same chunk
                    neighborEmpty = (chunks[cx][cy][cz].blocks[nx][ny][nz] == 0);
                }
                else
                {
                    //neighbor lies in adjacent chunk — map to neighbor chunk coords and in-chunk indices
                    int ncx = cx + (nx < 0 ? -1 : (nx >= CHUNK_WIDTH ? 1 : 0));
                    int ncy = cy + (ny < 0 ? -1 : (ny >= CHUNK_HEIGHT ? 1 : 0));
                    int ncz = cz + (nz < 0 ? -1 : (nz >= CHUNK_WIDTH ? 1 : 0));

                    if (ncx < 0 || ncx >= RENDER_DISTANCE || ncy < 0 || ncy >= 1 || ncz < 0 || ncz >= RENDER_DISTANCE)
                    {
                        //no neighbor chunk -> treat as air
                        neighborEmpty = true;
                    }
                    else
                    {
                        int bx = (nx < 0) ? (CHUNK_WIDTH - 1) : (nx >= CHUNK_WIDTH ? 0 : nx);
                        int by = (ny < 0) ? (CHUNK_HEIGHT - 1) : (ny >= CHUNK_HEIGHT ? 0 : ny);
                        int bz = (nz < 0) ? (CHUNK_WIDTH - 1) : (nz >= CHUNK_WIDTH ? 0 : nz);
                        neighborEmpty = (chunks[ncx][ncy][ncz].blocks[bx][by][bz] == 0);
                    }
                }

                mask[w][h] = (is_solid && neighborEmpty) ? 1 : 0;
            }
        }
        
        // Merge quads
        for (int h = 0; h < height_size; h++)
        {
            for (int w = 0; w < width_size; w++)
            {
                if (!mask[w][h])
                    continue;
                
                // Determine width
                int width = 1;
                while (w + width < width_size && mask[w + width][h])
                    width++;
                
                // Determine height
                int height = 1;
                bool done = false;
                while (h + height < height_size && !done)
                {
                    for (int k = 0; k < width; k++)
                    {
                        if (!mask[w + k][h + height])
                        {
                            done = true;
                            break;
                        }
                    }
                    if (!done)
                        height++;
                }
                
                // Calculate position based on axis
                Vector3 offset;
                if (axis == 0) { // Z axis
                    offset = (Vector3){(float)w, (float)h, (float)d};
                } else if (axis == 1) { // X axis
                    offset = (Vector3){(float)d, (float)h, (float)w};
                } else { // Y axis
                    offset = (Vector3){(float)w, (float)d, (float)h};
                }
                
                AddFace(mesh, v, i, face, offset, width, height);
                
                // Clear mask
                for (int hh = 0; hh < height; hh++)
                {
                    for (int ww = 0; ww < width; ww++)
                    {
                        mask[w + ww][h + hh] = 0;
                    }
                }
            }
        }
    }
}

Mesh generateChunkMesh(struct Chunk chunks[RENDER_DISTANCE][1][RENDER_DISTANCE], int cx, int cy, int cz)
{
    Mesh mesh = {0};

    int totalBlocks = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_WIDTH;
    mesh.vertexCount = totalBlocks * 6 * 4;    // worst-case: all faces visible
    mesh.triangleCount = totalBlocks * 6 * 2;  // worst-case

    mesh.vertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.texcoords = MemAlloc(mesh.vertexCount * 2 * sizeof(float));
    mesh.indices = MemAlloc(mesh.triangleCount * 3 * sizeof(unsigned short));

    int v = 0;
    int idx = 0;

    for(int face = 0; face < 6; face++)
        GreedyMesh(&mesh, chunks, &v, &idx, face, cx, cy, cz);
    

    // Update counts based on what we actually wrote
    mesh.vertexCount = v;
    mesh.triangleCount = idx / 3;


    // choose light direction and ambient
Vector3 lightDir = (Vector3){ -1.0f, -1.0f, -0.6f };
float llen = sqrtf(lightDir.x*lightDir.x + lightDir.y*lightDir.y + lightDir.z*lightDir.z);
lightDir.x /= llen; lightDir.y /= llen; lightDir.z /= llen;
float ambient = 0.25f;

// allocate colors (4 bytes per vertex)
mesh.colors = MemAlloc(mesh.vertexCount * 4 * sizeof(unsigned char));

for (int vi = 0; vi < mesh.vertexCount; vi++)
{
    float nx = mesh.normals[vi*3 + 0];
    float ny = mesh.normals[vi*3 + 1];
    float nz = mesh.normals[vi*3 + 2];
    float dp = nx*lightDir.x + ny*lightDir.y + nz*lightDir.z;
    float intensity = ambient + (1.0f - ambient) * fmaxf(0.0f, dp);

    unsigned char c = (unsigned char)(fminf(1.0f, intensity) * 255.0f);
    mesh.colors[vi*4 + 0] = c; // R
    mesh.colors[vi*4 + 1] = c; // G
    mesh.colors[vi*4 + 2] = c; // B
    mesh.colors[vi*4 + 3] = 255; // A
}

    UploadMesh(&mesh, false);
    return mesh;
}

int main(void) 
{
    
    // Initialize all chunks
    for(int cx = 0; cx < RENDER_DISTANCE; cx++)
    {
        for(int cy = 0; cy < 1; cy++)
        {
            for(int cz = 0; cz < RENDER_DISTANCE; cz++)
            {
                // Initialize blocks within this chunk
                for(int x = 0; x < CHUNK_WIDTH; x++)
                {
                    for(int y = 0; y < CHUNK_HEIGHT; y++)
                    {
                        for(int z = 0; z < CHUNK_WIDTH; z++)
                        {
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
            }
        }
    }


    //Main Window Handling
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Carson's Game");
    RenderTexture2D target = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    SetTargetFPS(TARGET_FPS);    
    DisableCursor();
    
    //Camera Setup
    Camera3D camera = {0};
    camera.position = (Vector3){ 10.0f, (float)CHUNK_HEIGHT, 10.0f }; //Camera Position
    camera.target = (Vector3){ 1.0f, 1.0f, 1.0f }; //Camera is looking here
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f }; //up-vector (rotation towards target) idrk what this means
    camera.fovy = 90.0f; //field of view? I assume there is also an x one. Something to look into
    camera.projection = CAMERA_PERSPECTIVE;

    // Build chunk mesh and model from the voxel data
    // Generate mesh and model for each chunk
    for(int cx = 0; cx < RENDER_DISTANCE; cx++)
        for(int cy = 0; cy < 1; cy++)
            for(int cz = 0; cz < RENDER_DISTANCE; cz++)
            {
                chunks[cx][cy][cz].mesh = generateChunkMesh(chunks, cx, cy, cz);
                chunks[cx][cy][cz].model = LoadModelFromMesh(chunks[cx][cy][cz].mesh);
            }
   

    Shader pxShader = LoadShader(0, "shader/pixelizer.fs");
    int resolutionLoc = GetShaderLocation(pxShader, "resolution");
    int pixelSizeLoc = GetShaderLocation(pxShader, "pixelSize");

    float resolution[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
    SetShaderValue(pxShader, resolutionLoc, resolution, SHADER_UNIFORM_VEC2);

    float pixelSize = 5.0f; //bigger = more pixelated
    SetShaderValue(pxShader, pixelSizeLoc, &pixelSize, SHADER_UNIFORM_FLOAT);

    //TODO make sure these are ok to be here, do I need to alloc, what ever
    Vector3 highlighted; //What the players looking at
    bool casting = false;
    while(!WindowShouldClose())
    {
        if (raycastVoxel(camera, MAX_REACH, &highlighted))
            casting = true;
        else
            casting = false;
        
        if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && casting) {
            setBlockAtWorld((int)highlighted.x, (int)highlighted.y, (int)highlighted.z, 0);
            //Regenerate mesh for affected chunk TODO: Update neighboring chunks if on edge
            int cx = floor_div((int)highlighted.x, CHUNK_WIDTH);
            int cy = floor_div((int)highlighted.y, CHUNK_HEIGHT);
            int cz = floor_div((int)highlighted.z, CHUNK_WIDTH);
            UnloadModel(chunks[cx][cy][cz].model);
            chunks[cx][cy][cz].mesh = generateChunkMesh(chunks, cx, cy, cz);
            chunks[cx][cy][cz].model = LoadModelFromMesh(chunks[cx][cy][cz].mesh);
        }

        UpdateCamera(&camera, CAMERA_FREE); //Update the camera every frame
        BeginTextureMode(target);
            ClearBackground(SKY_COLOR);
            //3D Rendering
            BeginMode3D(camera);
                // Generate mesh and model for each chunk
                for(int cx = 0; cx < RENDER_DISTANCE; cx++)
                    for(int cy = 0; cy < 1; cy++)
                        for(int cz = 0; cz < RENDER_DISTANCE; cz++)
                        {
                            DrawModel(chunks[cx][cy][cz].model, (Vector3){cx * CHUNK_WIDTH, cy * CHUNK_HEIGHT, cz * CHUNK_WIDTH}, 1.0f, WHITE);
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
                DrawTextureRec(
                    target.texture,
                    (Rectangle){ 0, 0, (float)target.texture.width, -(float)target.texture.height },
                    (Vector2){ 0, 0 },
                    WHITE
                );
            EndShaderMode();
            //UI (After so its on top of 3d models)
            DrawText(TextFormat("Highlighted Block X: %d Y: %d Z: %d", (int)highlighted.x, (int)highlighted.y, (int)highlighted.z), 10, 50, 20, SKYBLUE);
            DrawText(TextFormat("%d", GetFPS()), 10, 10, 30, CROSSHAIR_COLOR);
            DrawText("+", SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 20, CROSSHAIR_COLOR);
        EndDrawing();
    }

    //Cleanup
    for(int cx = 0; cx < RENDER_DISTANCE; cx++)
        for(int cy = 0; cy < 1; cy++)
            for(int cz = 0; cz < RENDER_DISTANCE; cz++)
                UnloadModel(chunks[cx][cy][cz].model);
    UnloadShader(pxShader);
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
