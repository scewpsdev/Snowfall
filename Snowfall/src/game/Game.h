#pragma once

#include "math/Vector.h"


int GetChunkGridIdxFromPosition(ivec3 position, int lod);
Chunk* GetChunkAtWorldPosWithLOD(ivec3 position, int lod, GameState* game);
uint8_t GetChunkFlagsAtWorldPos(ivec3 position, int lod, GameState* game);

bool GetSolidAtWorldPos(ivec3 position, int lod, struct GameState* game);
