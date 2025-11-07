#pragma once

#include "math/Vector.h"


int GetChunkGridIdxFromPosition(ivec3 position, int lod);

bool GetSolidAtWorldPos(ivec3 position, int lod, struct GameState* game);
