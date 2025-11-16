#include "WorldGenerator.h"

#include "Application.h"


extern SDL_GPUDevice* device;


void InitWorldGenerator(WorldGenerator* generator)
{
	generator->seed = 12345;
	generator->random = Random(generator->seed);

	generator->noiseShader = LoadComputeShader("res/shaders/simplex.cs.glsl.bin");
}

static float SampleNoise(float x, float z, int octaves, float frequencyMultiplier, float amplitudeMultiplier)
{
	float frequency = 1.0f;
	float amplitude = 1.0f;

	float sum = 0;

	float noise = 0;
	for (int i = 0; i < octaves; i++)
	{
		noise += amplitude * Simplex2f(x * frequency + hash(i) % 1000, z * frequency);
		sum += amplitude;

		frequency *= frequencyMultiplier;
		amplitude *= amplitudeMultiplier;
	}

	return noise / sum;
}

static float Continentalness(WorldGenerator* generator, int worldX, int worldZ)
{
	const int octaves = 3;
	const float frequencyMultiplier = 2;
	const float amplitudeMultiplier = 0.5f;

	const float baseFrequency = 1.0f / 8192 / 2;

	float x = baseFrequency * worldX;
	float z = baseFrequency * worldZ;

	float noise = SampleNoise(x, z, octaves, frequencyMultiplier, amplitudeMultiplier);

	return noise;
}

static float Erosion(WorldGenerator* generator, int worldX, int worldZ)
{
	const int octaves = 5;
	const float frequencyMultiplier = 4;
	const float amplitudeMultiplier = 0.25f;

	const float baseFrequency = 1.0f / 2048;

	float x = baseFrequency * worldX;
	float z = baseFrequency * worldZ;

	float noise = SampleNoise(-x - 1000, z, octaves, frequencyMultiplier, amplitudeMultiplier);
	//noise = SDL_powf(2, noise * 0.5f + 0.5f) * 2 - 3;
	noise = 1 - SDL_fabsf(noise);
	noise = SDL_powf(noise, 4);

	return noise;
}

static float PeaksAndValleys(WorldGenerator* generator, int worldX, int worldZ)
{
	const int octaves = 3;
	const float frequencyMultiplier = 2;
	const float amplitudeMultiplier = 0.5f;

	const float baseFrequency = 1.0f / 256;

	float x = baseFrequency * worldX;
	float z = baseFrequency * worldZ;

	float noise = SampleNoise(-x, -z - 1000, octaves, frequencyMultiplier, amplitudeMultiplier);
	noise = SDL_fabsf(noise) * 2 - 1;

	return noise;
}

struct UniformData
{
	ivec3 chunkPosition;
	int chunkScale;

	float baseFrequency;
	float frequencyMultiplier;
	float amplitudeMultiplier;
	float numOctaves;
};

void GenerateChunk(WorldGenerator* generator, ChunkGeneratorThreadData* threadData, Chunk* chunk)
{
	uint64_t before = SDL_GetTicksNS();

	chunk->isEmpty = true;

	SDL_GPUCommandBuffer* cmdBuffer = SDL_AcquireGPUCommandBuffer(device);

	SDL_GPUStorageBufferReadWriteBinding bufferBinding = {};
	bufferBinding.buffer = threadData->noiseOutputBuffer;
	bufferBinding.cycle = false;
	SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(cmdBuffer, nullptr, 0, &bufferBinding, 1);
	SDL_BindGPUComputePipeline(computePass, generator->noiseShader->compute);

	UniformData params = {};
	params.chunkPosition = chunk->position;
	params.chunkScale = chunk->chunkScale;
	params.baseFrequency = 1.0f / 256;
	params.frequencyMultiplier = 2.0f;
	params.amplitudeMultiplier = 0.5f;
	params.numOctaves = 5;
	SDL_PushGPUComputeUniformData(cmdBuffer, 0, &params, sizeof(params));

	SDL_DispatchGPUCompute(computePass, CHUNK_SIZE / 8, CHUNK_SIZE / 8, CHUNK_SIZE / 8);

	SDL_EndGPUComputePass(computePass);

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuffer);

	SDL_GPUBufferRegion src = {};
	src.buffer = threadData->noiseOutputBuffer;
	src.offset = 0;
	src.size = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * sizeof(float);

	SDL_GPUTransferBufferLocation dst = {};
	dst.transfer_buffer = threadData->noiseReadbackBuffer;
	dst.offset = 0;

	SDL_DownloadFromGPUBuffer(copyPass, &src, &dst);

	SDL_EndGPUCopyPass(copyPass);

	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdBuffer);

	// TODO
	// noise to spline remapping
	// rivers
	// different block types
	// move heightmap generation to gpu
	// biomes
	// trees
	// grass
	// atmosphere
	// day & night
	// caves
	// structures
	// dungeons

	float heightmap[CHUNK_SIZE * CHUNK_SIZE];
	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			int worldX = chunk->position.x + x * chunk->chunkScale;
			int worldZ = chunk->position.z + z * chunk->chunkScale;

			float continentalness = Continentalness(generator, worldX, worldZ);

			float height = continentalness * 200;

			float erosion = Erosion(generator, worldX, worldZ);
			height *= erosion;

			//float pnv = PeaksAndValleys(generator, worldX, worldZ);
			//height += pnv * 5;

			heightmap[x + z * CHUNK_SIZE] = height;
		}
	}

	if (!SDL_WaitForGPUFences(device, true, &fence, 1))
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "%s", SDL_GetError());

	SDL_ReleaseGPUFence(device, fence);

	float* noise = (float*)SDL_MapGPUTransferBuffer(device, threadData->noiseReadbackBuffer, false);
	SDL_assert(noise);

	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				float terrainHeight = heightmap[x + z * CHUNK_SIZE];

				int worldX = chunk->position.x + x * chunk->chunkScale;
				int worldY = chunk->position.y + y * chunk->chunkScale;
				int worldZ = chunk->position.z + z * chunk->chunkScale;

				BlockData* block = chunk->getBlockData(x, y, z);

				float density = noise[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE]; // Density(generator, worldX, worldY, worldZ, terrainHeight);

				const float squashingFactor = 0.03f;
				density += (terrainHeight - worldY) * squashingFactor;

				block->id = density > 0 ? BLOCK_TYPE_STONE : worldY < 0 ? BLOCK_TYPE_WATER : BLOCK_TYPE_NONE;

				if (block->id) chunk->isEmpty = false;
			}
		}
	}

	SDL_UnmapGPUTransferBuffer(device, threadData->noiseReadbackBuffer);

	uint64_t after = SDL_GetTicksNS();
	SDL_Log("worldgen %.2f ms", (after - before) / 1e6f);

	chunk->needsMeshUpdate = true;
}
