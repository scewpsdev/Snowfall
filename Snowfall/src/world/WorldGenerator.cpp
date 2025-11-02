#include "WorldGenerator.h"


void InitWorldGenerator(WorldGenerator* generator)
{
	generator->seed = 12345;
	generator->random = Random(generator->seed);
	generator->simplex = Simplex(generator->seed);
}

void GenerateChunk(WorldGenerator* generator, Chunk* chunk)
{
	/*
	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				BlockData* block = chunk->getBlockData(x, y, z);
				block->id = generator->random.next() % 2;
				block->colorID = generator->random.next() % 128;
			}
		}
	}
	*/

	///*
	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			float frequency = 0.01f;
			float amplitude = 16;
			int chunkSize = ipow(2, chunk->lod);
			float noise = generator->simplex.sample2f((chunk->position.x + x * chunkSize) * frequency, (chunk->position.z + z * chunkSize) * frequency);
			int height = (int)(SDL_roundf(16 + noise * amplitude) / chunkSize);
			for (int y = 0; y < height; y++)
			{
				BlockData* block = chunk->getBlockData(x, y, z);
				block->id = 1 + (uint8_t)generator->random.next() % 15;
			}
		}
	}
	//*/

	chunk->needsUpdate = true;
}
