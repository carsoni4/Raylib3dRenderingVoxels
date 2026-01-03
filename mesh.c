#include "mesh.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

static const Vector3 faceNormals[6] = {
    {0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}
};

Mesh generateChunkMesh(struct Chunk ***chunks, int cx, int cy, int cz)
{
    Mesh mesh = {0};

    int totalBlocks = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_WIDTH;
    mesh.vertexCount = totalBlocks * 6 * 4;    //TODO: Both worst case, can probably be reduced somehow so look into that but idk rn.
    mesh.triangleCount = totalBlocks * 6 * 2; 
    mesh.vertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.texcoords = MemAlloc(mesh.vertexCount * 2 * sizeof(float));
    mesh.indices = MemAlloc(mesh.triangleCount * 3 * sizeof(unsigned short));


    int v = 0;
    int idx = 0;

    for(int face = 0; face < 6; face++)
        greedyMesh(&mesh, chunks, &v, &idx, face, cx, cy, cz);
    

    // Update counts based on what we actually wrote
    mesh.vertexCount = v;
    mesh.triangleCount = idx / 3;
    //Shader Allocation
    mesh.animVertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    for (int vi = 0; vi < mesh.vertexCount * 3; vi++)
        mesh.animVertices[vi] = mesh.vertices[vi];

//TODO: simple lighting, kinda of like it but can explore more later
//choose light direction and ambient
Vector3 lightDir = (Vector3){ -1.0f, -1.0f, -0.6f };
float llen = sqrtf(lightDir.x*lightDir.x + lightDir.y*lightDir.y + lightDir.z*lightDir.z);
lightDir.x /= llen; lightDir.y /= llen; lightDir.z /= llen;
float ambient = 0.25f;

//allocate colors (4 bytes per vertex)
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

void greedyMesh(Mesh *mesh, struct Chunk ***chunks, int *v, int *i, int face, int cx, int cy, int cz)
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

                    if (ncx < 0 || ncx >= WORLD_SIZE_CHUNKS || ncy < 0 || ncy >= 1 || ncz < 0 || ncz >= WORLD_SIZE_CHUNKS)
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
                
                addFace(mesh, v, i, face, offset, width, height);
                
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

void addFace(Mesh *mesh, int *v, int *i, int face, Vector3 offset, int width, int height)
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