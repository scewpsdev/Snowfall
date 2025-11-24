#pragma once

#include "math/Vector.h"


//int GetChunkGridIdxFromPosition(const ivec3& position, int lod);
int GetChunkGridIdxFromGridPosition(vec3 gridPosition);
Chunk* GetChunkAtGridPosition(ivec3 position, int lod);
uint8_t GetChunkFlagsAtGridPosition(ivec3 position, int lod);
//uint8_t GetChunkFlagsAtWorldPos(ivec3 position, int lod, GameState* game);
