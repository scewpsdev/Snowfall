#include "WorldGenerator.h"

#include "Application.h"


extern SDL_GPUDevice* device;


void InitWorldGenerator(WorldGenerator* generator)
{
	generator->seed = 12345;
	generator->random = Random(generator->seed);

	generator->heightmapShader = LoadComputeShader("res/shaders/heightmap.cs.glsl.bin");
	generator->noiseShader = LoadComputeShader("res/shaders/worldgen.cs.glsl.bin");
}

static float SampleNoise(float x, float z, int octaves, float frequencyMultiplier, float amplitudeMultiplier)
{
	float frequency = 1.0f;
	float amplitude = 1.0f;

	float sum = 0;
	int offset = 0;

	float noise = 0;
	for (int i = 0; i < octaves; i++)
	{
		noise += amplitude * Simplex2f(x * frequency + offset, z * frequency);
		sum += amplitude;

		frequency *= frequencyMultiplier;
		amplitude *= amplitudeMultiplier;
		offset = hash(offset) % 1000;
	}

	return noise / sum;
}

static float Continentalness(WorldGenerator* generator, int worldX, int worldZ)
{
	const int octaves = 5;
	const float frequencyMultiplier = 2;
	const float amplitudeMultiplier = 0.5f;

	const float baseFrequency = 1.0f / 8192;

	float x = baseFrequency * worldX;
	float z = baseFrequency * worldZ;

	float noise = SampleNoise(x, z, octaves, frequencyMultiplier, amplitudeMultiplier);

	return noise;
}

static float Erosion(WorldGenerator* generator, int worldX, int worldZ)
{
	const int octaves = 8;
	const float frequencyMultiplier = 4;
	const float amplitudeMultiplier = 0.25f;

	const float baseFrequency = 1.0f / 4096;

	float x = baseFrequency * worldX;
	float z = baseFrequency * worldZ;

	float noise = SampleNoise(-x - 1000, z, octaves, frequencyMultiplier, amplitudeMultiplier);
	//noise = 1 - SDL_fabsf(noise);
	//noise = SDL_powf(noise, 4);
	noise = noise * 0.5f + 0.5f;

	return noise;
}

static float PeaksAndValleys(WorldGenerator* generator, int worldX, int worldZ)
{
	const int octaves = 5;
	const float frequencyMultiplier = 2;
	const float amplitudeMultiplier = 0.5f;

	const float baseFrequency = 1.0f / 256;

	float x = baseFrequency * worldX;
	float z = baseFrequency * worldZ;

	float noise = SampleNoise(-x, -z - 1000, octaves, frequencyMultiplier, amplitudeMultiplier);

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

static void GenerateHeightmap(WorldGenerator* generator, Chunk* chunk, SDL_GPUBuffer* outputBuffer, SDL_GPUCommandBuffer* cmdBuffer)
{
	SDL_GPUStorageBufferReadWriteBinding bufferBinding = {};
	bufferBinding.buffer = outputBuffer;
	bufferBinding.cycle = false;
	SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(cmdBuffer, nullptr, 0, &bufferBinding, 1);
	SDL_BindGPUComputePipeline(computePass, generator->heightmapShader->compute);

	UniformData params = {};
	params.chunkPosition = chunk->position;
	params.chunkScale = chunk->chunkScale;
	params.baseFrequency = 1.0f / 512;
	params.frequencyMultiplier = 2.0f;
	params.amplitudeMultiplier = 0.5f;
	params.numOctaves = 5;
	SDL_PushGPUComputeUniformData(cmdBuffer, 0, &params, sizeof(params));

	SDL_DispatchGPUCompute(computePass, 1, 1, 1);

	SDL_EndGPUComputePass(computePass);
}

static void GenerateDensity(WorldGenerator* generator, Chunk* chunk, SDL_GPUBuffer* heightmapBuffer, SDL_GPUBuffer* outputBuffer, SDL_GPUTransferBuffer* readbackBuffer, SDL_GPUCommandBuffer* cmdBuffer)
{
	SDL_GPUStorageBufferReadWriteBinding bufferBinding = {};
	bufferBinding.buffer = outputBuffer;
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

	SDL_GPUBuffer* inputBuffers[] = { heightmapBuffer };
	SDL_BindGPUComputeStorageBuffers(computePass, 0, inputBuffers, 1);

	SDL_DispatchGPUCompute(computePass, CHUNK_SIZE / 8, CHUNK_SIZE / 8, CHUNK_SIZE / 8);

	SDL_EndGPUComputePass(computePass);

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuffer);

	SDL_GPUBufferRegion src = {};
	src.buffer = outputBuffer;
	src.offset = 0;
	src.size = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * sizeof(float);

	SDL_GPUTransferBufferLocation dst = {};
	dst.transfer_buffer = readbackBuffer;
	dst.offset = 0;

	SDL_DownloadFromGPUBuffer(copyPass, &src, &dst);

	SDL_EndGPUCopyPass(copyPass);
}

void GenerateChunk(WorldGenerator* generator, ChunkGeneratorThreadData* threadData, Chunk* chunk)
{
	uint64_t before = SDL_GetTicksNS();

	chunk->isEmpty = true;

	SDL_GPUCommandBuffer* cmdBuffer = SDL_AcquireGPUCommandBuffer(device);

	GenerateHeightmap(generator, chunk, threadData->heightmapOutputBuffer, cmdBuffer);
	GenerateDensity(generator, chunk, threadData->heightmapOutputBuffer, threadData->noiseOutputBuffer, threadData->noiseReadbackBuffer, cmdBuffer);

	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdBuffer);

	if (!SDL_WaitForGPUFences(device, true, &fence, 1))
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "%s", SDL_GetError());

	SDL_ReleaseGPUFence(device, fence);

	// TODO
	// worldgen write to texture memory
	// greedy meshing on gpu
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

	/*
	float heightmap[CHUNK_SIZE * CHUNK_SIZE];
	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			int worldX = chunk->position.x + x * chunk->chunkScale;
			int worldZ = chunk->position.z + z * chunk->chunkScale;

			float continentalness = Continentalness(generator, worldX, worldZ);

			float height =
				continentalness < -0.1f ? remap(continentalness, -1, -0.1f, -100, -20) :
				continentalness < -0.05f ? remap(smoothstep(0, 1, remap(continentalness, -0.1f, -0.05f, 0, 1)), 0, 1, -20, 10) :
				continentalness < 0.2f ? remap(continentalness, -0.05f, 0.2f, 10, 15) :
				continentalness < 0.35f ? remap(smoothstep(0, 1, remap(continentalness, 0.2f, 0.35f, 0, 1)), 0, 1, 15, 40) :
				remap(continentalness, 0.35f, 1, 40, 70);

			float erosion = Erosion(generator, worldX, worldZ);
			erosion = remap(erosion, 0, 1, -2, 3);
			float heightMultiplier =
				erosion < 0.1f ? remap(erosion, 0, 0.1f, 1, 0.7f) :
				erosion < 0.25f ? remap(erosion, 0.1f, 0.25f, 0.7f, 0.5f) :
				erosion < 0.27f ? remap(erosion, 0.25f, 0.27f, 0.5f, 0.6f) :
				erosion < 0.55f ? remap(erosion, 0.27f, 0.55f, 0.6f, 0.17f) :
				erosion < 0.75f ? remap(erosion, 0.55f, 0.75f, 0.17f, 0.14f) :
				erosion < 0.85f ? remap(1 - SDL_powf(remap(erosion, 0.75f, 0.85f, -1, 1), 2), 0, 1, 0.14f, 0.5f) :
				remap(erosion, 0.85f, 1, 0.14f, 0.05f);
			height *= heightMultiplier;

			float pnv = PeaksAndValleys(generator, worldX, worldZ);
			height += pnv * 10;

			heightmap[x + z * CHUNK_SIZE] = height;
		}
	}
	*/

	uint32_t* blockData = (uint32_t*)SDL_MapGPUTransferBuffer(device, threadData->noiseReadbackBuffer, false);
	SDL_assert(blockData);

	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				/*
				float terrainHeight = heightmap[x + z * CHUNK_SIZE];

				int worldX = chunk->position.x + x * chunk->chunkScale;
				int worldY = chunk->position.y + y * chunk->chunkScale;
				int worldZ = chunk->position.z + z * chunk->chunkScale;

				float density = noise[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE]; // Density(generator, worldX, worldY, worldZ, terrainHeight);

				const float squashingFactor = 0.02f;
				density += (terrainHeight - worldY) * squashingFactor;

				block->id = density > 0 ? BLOCK_TYPE_STONE : worldY < 0 ? BLOCK_TYPE_WATER : BLOCK_TYPE_NONE;
				*/

				BlockData* block = chunk->getBlockData(x, y, z);

				block->id = blockData[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE];

				if (block->id) chunk->isEmpty = false;
			}
		}
	}

	SDL_UnmapGPUTransferBuffer(device, threadData->noiseReadbackBuffer);

	uint64_t after = SDL_GetTicksNS();
	SDL_Log("worldgen %.2f ms", (after - before) / 1e6f);

	chunk->needsMeshUpdate = true;
}
