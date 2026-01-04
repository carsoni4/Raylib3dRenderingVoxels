#include "raylib.h"
#include <stdbool.h>
#include "character.h"
#include "chunk.h"
#include <math.h>

//Needed for linker
Player player;

void spawnPlayer(Vector3 position)
{
    player.position = position;
    player.velocity = (Vector3){0,0,0};
    player.onGround = false; //TODO: This probably needs to getBlock under position at some point
    player.size = (Vector2){1.0f, 1.0f};
}

static bool isSolidBlock(int x, int y, int z)
{
    return getBlockAtWorld(x,y,z) != AIR;
}

static bool checkGrounded(Player *player)
{
    float footYf = player->position.y - (player->size.y * 0.5f) - 0.05f;

    int footX = (int)floorf(player->position.x);
    int footY = (int)floorf(footYf);
    int footZ = (int)floorf(player->position.z);

    return isSolidBlock(footX, footY, footZ);
}

void updatePlayer(float dt, Camera3D *camera)
{
    // Forward vector (XZ plane)
    Vector3 forward = {
        camera->target.x - camera->position.x,
        0.0f,
        camera->target.z - camera->position.z
    };

    float fLen = sqrtf(forward.x * forward.x + forward.z * forward.z);
    if (fLen > 0.0f)
    {
        forward.x /= fLen;
        forward.z /= fLen;
    }

    Vector3 right = {
        forward.z,
        0.0f,
        -forward.x
    };

    Vector3 move = {0};

    if (IsKeyDown(KEY_W)) { move.x += forward.x; move.z += forward.z; }
    if (IsKeyDown(KEY_S)) { move.x -= forward.x; move.z -= forward.z; }
    if (IsKeyDown(KEY_A)) { move.x += right.x;   move.z += right.z; }
    if (IsKeyDown(KEY_D)) { move.x -= right.x;   move.z -= right.z; }

    float mLen = sqrtf(move.x * move.x + move.z * move.z);
    if (mLen > 0.0f)
    {
        move.x = (move.x / mLen) * MOVEMENT_SPEED;
        move.z = (move.z / mLen) * MOVEMENT_SPEED;
    }

    player.velocity.x = move.x;
    player.velocity.z = move.z;

    if (!player.onGround)
        player.velocity.y += GRAVITY * dt;
    else if (player.velocity.y < 0.0f)
        player.velocity.y = 0.0f;

    if (player.onGround && IsKeyPressed(KEY_SPACE))
    {
        player.velocity.y = JUMP_FORCE;
        player.onGround = false;
    }

    player.position.x += player.velocity.x * dt;
    player.position.z += player.velocity.z * dt;


    //TODO: Improve Collision It sucks
    float nextY = player.position.y + player.velocity.y * dt;
    float footYf = nextY - (player.size.y * 0.5f);

    int minX = (int)floorf(player.position.x - player.size.x * 0.5f);
    int maxX = (int)floorf(player.position.x + player.size.x * 0.5f);
    int minZ = (int)floorf(player.position.z - player.size.x * 0.5f);
    int maxZ = (int)floorf(player.position.z + player.size.x * 0.5f);
    int footY = (int)floorf(footYf);

    bool hitGround = false;

    for (int x = minX; x <= maxX; x++)
    {
        for (int z = minZ; z <= maxZ; z++)
        {
            if (isSolidBlock(x, footY, z))
            {
                hitGround = true;
                break;
            }
        }
        if (hitGround) break;
    }

    if (hitGround)
    {
        player.onGround = true;
        player.velocity.y = 0.0f;
        player.position.y = (float)footY + 1.0f + (player.size.y * 0.5f);
    }
    else
    {
        player.onGround = false;
        player.position.y = nextY;
    }
}

void applyPlayerCamera(Camera3D *camera)
{
    Vector3 desiredPos = {
        player.position.x,
        player.position.y + CAMERA_HEIGHT,
        player.position.z
    };

    Vector3 delta = (Vector3){desiredPos.x - camera->position.x, desiredPos.y - camera->position.y, desiredPos.z - camera->position.z};

    camera->position = desiredPos;
    camera->target = (Vector3){delta.x + camera->target.x, delta.y + camera->target.y, delta.z + camera->target.z};
}