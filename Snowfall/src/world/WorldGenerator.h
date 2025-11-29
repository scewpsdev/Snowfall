#pragma once

#include "Chunk.h"

#include "graphics/Shader.h"

#include "utils/Random.h"
#include "utils/Simplex.h"

#include <SDL3/SDL.h>


struct WorldGenerator
{
	uint32_t seed;
	Random random;

	Shader* heightmapShader;
	Shader* noiseShader;

	SDL_GPUSampler* sampler;
};


void InitWorldGenerator(WorldGenerator* generator);

void GenerateChunk(WorldGenerator* generator, Chunk* chunk, SDL_GPUTexture* heightmap, SDL_GPUTexture* chunkTexture, SDL_GPUCommandBuffer* cmdBuffer);
