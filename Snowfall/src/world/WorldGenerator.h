#pragma once

#include <stdint.h>

#include "Chunk.h"

#include "utils/Random.h"
#include "utils/Simplex.h"


struct WorldGenerator
{
	uint32_t seed;
	Random random;
	Simplex simplex;
};


void InitWorldGenerator(WorldGenerator* generator);

void GenerateChunk(WorldGenerator* generator, Chunk* chunk);
