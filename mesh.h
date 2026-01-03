#ifndef MESH_H
#define MESH_H

#include "chunk.h"

Mesh generateChunkMesh(Chunk ***chunks, int cx, int cy, int cz);
void greedyMesh(Mesh *mesh, Chunk ***chunks, int *v, int *i, int face, int cx, int cy, int cz);
void addFace(Mesh *mesh, int *v, int *i, int face, Vector3 offset, int width, int height);

#endif
