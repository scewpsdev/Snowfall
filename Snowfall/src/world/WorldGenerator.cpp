#include "WorldGenerator.h"


void InitWorldGenerator(WorldGenerator* generator)
{
	generator->seed = 12345;
	generator->random = Random(generator->seed);
	generator->simplex = Simplex(generator->seed);
}

static float SampleNoise(WorldGenerator* generator, float x, float z, int octaves, float frequencyMultiplier, float amplitudeMultiplier)
{
	float noise = 0;
	for (int i = 0; i < octaves; i++)
	{
		float frequency = powf(frequencyMultiplier, (float)i);
		float amplitude = powf(amplitudeMultiplier, (float)i);
		noise += amplitude * generator->simplex.sample2f(x * frequency, z * frequency);
	}
	return noise;
}

void GenerateChunk(WorldGenerator* generator, Chunk* chunk)
{
	float frequency = 1.0f / 512;
	float amplitude = 64;

	chunk->isEmpty = true;
	SDL_memset(chunk->blocks, 0, sizeof(chunk->blocks));

	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			int worldX = chunk->position.x + x * chunk->chunkScale;
			int worldZ = chunk->position.z + z * chunk->chunkScale;
			float noise = SampleNoise(generator, worldX * frequency, worldZ * frequency, 5, 2, 0.5f);
			int height = (int)SDL_roundf(powf(2, noise) * amplitude);
			//if (height > chunk->position.y + CHUNK_SIZE * chunkSize)
			//	continue;

			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				int worldY = chunk->position.y + y * chunk->chunkScale;
				if (worldY < height)
				{
					BlockData* block = chunk->getBlockData(x, y, z);
					block->id = 1 + (uint8_t)generator->random.next() % 15;
					chunk->isEmpty = false;
				}
			}
		}
	}

	if (chunk->isEmpty)
		chunk->hasMesh = true;

	chunk->needsUpdate = true;
}
