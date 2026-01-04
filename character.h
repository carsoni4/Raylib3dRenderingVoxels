#ifndef CHARACTER_H
#define CHARACTER_H

#include "raylib.h"
#include <stdbool.h>

#define GRAVITY -10.0f
#define MOVEMENT_SPEED 5.0f
#define JUMP_FORCE 10.0f
#define CAMERA_HEIGHT 0.5f

//TODO: Implement character and first person camera. Might wanna add the camera to the struct tbh. I think we can do collisions based on a rough block with 
//Width and height instead of whatever character model I come up with. Probably would feel better anyway.
typedef struct Player{
    Vector3 position;
    Vector3 velocity;
    bool onGround;
    Vector2 size; //width, height
} Player;

extern Player player;

void spawnPlayer(Vector3 position);
void updatePlayer(float dt, Camera3D *camera);
void applyPlayerCamera(Camera3D *playerCamera);

#endif