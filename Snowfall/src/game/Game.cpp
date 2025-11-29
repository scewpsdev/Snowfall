#include "Game.h"

#include "graphics/VertexBuffer.h"

#include "math/Vector.h"


static Chunk* GetAvailableChunk()
{
	for (int i = 0; i < MAX_LOADED_CHUNKS; i++)
	{
		Chunk* chunk = &game->chunks[i];
		if (!chunk->isActive)
		{
			chunk->id = i;
			chunk->isActive = true;
			chunk->vertexCount = -1;
			game->numLoadedChunks++;

			if (chunk->id > game->lastLoadedChunk)
				game->lastLoadedChunk = chunk->id;

			return chunk;
		}
	}
	return nullptr;
}

int GetChunkGridIdxFromGridPosition(ivec3 gridPosition)
{
	int x = gridPosition.x;
	int y = gridPosition.y;
	int z = gridPosition.z;
	if (x >= -CHUNK_LOD_DISTANCE / 2 && x < CHUNK_LOD_DISTANCE / 2 && y >= -CHUNK_LOD_DISTANCE / 2 && y < CHUNK_LOD_DISTANCE / 2 && z >= -CHUNK_LOD_DISTANCE / 2 && z < CHUNK_LOD_DISTANCE / 2)
		return (x + CHUNK_LOD_DISTANCE / 2) + (y + CHUNK_LOD_DISTANCE / 2) * CHUNK_LOD_DISTANCE + (z + CHUNK_LOD_DISTANCE / 2) * CHUNK_LOD_DISTANCE * CHUNK_LOD_DISTANCE;
	return -1;
}

int GetChunkGridIdxFromPosition(const ivec3& position, int lod)
{
	int chunkScale = ipow(2, lod);
	int x = idivfloor(position.x, CHUNK_SIZE * chunkScale);
	int z = idivfloor(position.z, CHUNK_SIZE * chunkScale);
	int y = idivfloor(position.y, CHUNK_SIZE * chunkScale);
	if (x >= -CHUNK_LOD_DISTANCE / 2 && x < CHUNK_LOD_DISTANCE / 2 && y >= -CHUNK_LOD_DISTANCE / 2 && y < CHUNK_LOD_DISTANCE / 2 && z >= -CHUNK_LOD_DISTANCE / 2 && z < CHUNK_LOD_DISTANCE / 2)
	{
		int idx = (x + CHUNK_LOD_DISTANCE / 2) + (y + CHUNK_LOD_DISTANCE / 2) * CHUNK_LOD_DISTANCE + (z + CHUNK_LOD_DISTANCE / 2) * CHUNK_LOD_DISTANCE * CHUNK_LOD_DISTANCE;
		SDL_assert(idx >= 0 && idx < CHUNK_LOD_DISTANCE * CHUNK_LOD_DISTANCE * CHUNK_LOD_DISTANCE);
		return idx;
	}
	return -1;
}

Chunk* GetChunkAtGridPosition(ivec3 position, int lod)
{
	int gridIdx = GetChunkGridIdxFromGridPosition(position);
	if (gridIdx != -1)
	{
		Chunk* chunk = game->lods[lod].chunkGrid[gridIdx];
		SDL_assert(!chunk || position == chunk->gridPosition && lod == chunk->lod);
		if (chunk && chunk->isActive)
			return chunk;
	}
	return nullptr;
}

uint8_t GetChunkFlagsAtGridPosition(ivec3 position, int lod)
{
	int gridIdx = GetChunkGridIdxFromGridPosition(position);
	if (gridIdx != -1)
	{
		uint8_t flags = game->lods[lod].chunkFlags[gridIdx];
		return flags;
	}
	return CHUNK_FLAG_SOLID;
}

static void GetChunkNeighbors(Chunk* chunk, Chunk* neighbors[6])
{
	neighbors[0] = GetChunkAtGridPosition(chunk->gridPosition + ivec3::Left, chunk->lod);
	neighbors[1] = GetChunkAtGridPosition(chunk->gridPosition + ivec3::Right, chunk->lod);
	neighbors[2] = GetChunkAtGridPosition(chunk->gridPosition + ivec3::Down, chunk->lod);
	neighbors[3] = GetChunkAtGridPosition(chunk->gridPosition + ivec3::Up, chunk->lod);
	neighbors[4] = GetChunkAtGridPosition(chunk->gridPosition + ivec3::Forward, chunk->lod);
	neighbors[5] = GetChunkAtGridPosition(chunk->gridPosition + ivec3::Back, chunk->lod);
}

static void GetChunkNeighbors(Chunk* chunk, Chunk* neighbors[6], uint32_t neighborFlags[6])
{
	neighbors[0] = GetChunkAtGridPosition(chunk->gridPosition + ivec3::Left, chunk->lod);
	neighbors[1] = GetChunkAtGridPosition(chunk->gridPosition + ivec3::Right, chunk->lod);
	neighbors[2] = GetChunkAtGridPosition(chunk->gridPosition + ivec3::Down, chunk->lod);
	neighbors[3] = GetChunkAtGridPosition(chunk->gridPosition + ivec3::Up, chunk->lod);
	neighbors[4] = GetChunkAtGridPosition(chunk->gridPosition + ivec3::Forward, chunk->lod);
	neighbors[5] = GetChunkAtGridPosition(chunk->gridPosition + ivec3::Back, chunk->lod);

	neighborFlags[0] = GetChunkFlagsAtGridPosition(chunk->gridPosition + ivec3::Left, chunk->lod);
	neighborFlags[1] = GetChunkFlagsAtGridPosition(chunk->gridPosition + ivec3::Right, chunk->lod);
	neighborFlags[2] = GetChunkFlagsAtGridPosition(chunk->gridPosition + ivec3::Down, chunk->lod);
	neighborFlags[3] = GetChunkFlagsAtGridPosition(chunk->gridPosition + ivec3::Up, chunk->lod);
	neighborFlags[4] = GetChunkFlagsAtGridPosition(chunk->gridPosition + ivec3::Forward, chunk->lod);
	neighborFlags[5] = GetChunkFlagsAtGridPosition(chunk->gridPosition + ivec3::Back, chunk->lod);
}

static void CopyChunkNeighbors(Chunk** neighbors, Chunk neighborCopies[6], Chunk* neighborCopyPtrs[6])
{
	for (int i = 0; i < 6; i++)
	{
		if (neighbors[i])
		{
			neighborCopies[i] = *neighbors[i];
			neighborCopyPtrs[i] = &neighborCopies[i];
		}
		else
		{
			neighborCopyPtrs[i] = nullptr;
		}
	}
}

static Chunk* InitChunk(const ivec3& position, int lod)
{
	if (Chunk* chunk = GetAvailableChunk())
	{
		chunk->gridPosition = position;
		chunk->lod = lod;
		chunk->chunkScale = ipow(2, lod);

		int gridIdx = GetChunkGridIdxFromGridPosition(position);
		SDL_assert(gridIdx != -1);
		SDL_assert(game->lods[lod].chunkGrid[gridIdx] == nullptr);
		game->lods[lod].chunkGrid[gridIdx] = chunk;
		game->lods[lod].chunkFlags[gridIdx] = 0;
		SDL_assert(chunk->gridPosition == position);

		return chunk;
	}
	return nullptr;
}

static void UnloadChunk(Chunk* chunk)
{
	if (game->lastLoadedChunk == chunk->id)
	{
		for (int i = game->lastLoadedChunk - 1; i >= 0; i--)
		{
			if (game->chunks[i].isActive)
			{
				game->lastLoadedChunk = i;
				break;
			}
		}
	}

	/*
	int vertexOffset = chunk->vertexOffsets[0];
	int vertexCount = chunk->getTotalVertexCount();
	if (vertexCount > 0)
		DeallocateChunk(&game->chunkAllocator, vertexOffset, vertexCount);
	*/

	int gridIdx = GetChunkGridIdxFromGridPosition(chunk->gridPosition);
	SDL_assert(gridIdx != -1);
	SDL_assert(game->lods[chunk->lod].chunkGrid[gridIdx] == chunk);
	game->lods[chunk->lod].chunkGrid[gridIdx] = nullptr;
	//game->lods[chunk->lod].chunkFlags[gridIdx] = 0;

	game->numLoadedChunks--;

	if (chunk->readbackData)
	{
		chunk->readbackData->refCount--;
		if (chunk->readbackData->refCount == 0)
		{
			SDL_ReleaseGPUFence(device, chunk->readbackData->fence);
			chunk->readbackData->fence = nullptr;
			chunk->readbackData->refCount = 0;
		}

		SDL_LockMutex(game->chunkReadbackMutex);
		PoolRelease(&game->chunkReadbackPool, chunk->readbackData);
		SDL_UnlockMutex(game->chunkReadbackMutex);
		chunk->readbackData = nullptr;
	}

	chunk->isActive = false;
	chunk->isLoaded = false;
	chunk->hasMesh = false;
	chunk->needsMeshUpdate = false;

	SDL_memset(chunk, 0, sizeof(Chunk));

	chunk->id = -1;
	chunk->lod = -1;
	chunk->vertexCount = -1;
	chunk->blockCount = -1;
}

// TODO
// [X] completely async chunk generation queue
// [X] fix chunk generation framerate drops
// [X] separate generation and meshing
// [X] cull chunk sides
// [X] unload empty chunks
// [ ] gpu culling pass (compact indirect buffer, frustum culling, sort front to back, remove invisible faces)
// [ ] read block type from texture in chunk fragment shader
// [ ] add back block types

static void RunGeneratorJobs(ChunkJob* jobs, int numJobs, ChunkReadbackData* readbackData, ChunkGeneratorThreadData* data)
{
	for (int i = 0; i < numJobs; i++)
	{
		SDL_assert(!jobs[i].chunk->readbackData);
	}

	// TODO
	// [X] separate meshing queue for newly generated chunks
	// [ ] do all worldgen readbacks at once
	// [ ] do all meshing uploads at once

	SDL_GPUCommandBuffer* cmdBuffer = SDL_AcquireGPUCommandBuffer(device);

	uint64_t beforeGen = SDL_GetTicksNS();

	for (int i = 0; i < numJobs; i++)
	{
		GenerateChunk(&data->game->worldGenerator, jobs[i].chunk, data->heightmaps[i], game->chunkTexture, cmdBuffer);
	}

	/*
	if (game->gpuMeshing)
	{
		ChunkData chunkData = {};
		chunkData.position = job.chunk->getWorldPosition();
		chunkData.scale = job.chunk->chunkScale;
		UpdateStorageBuffer(data->game->chunkStorageBuffer, job.chunk->getStorageBufferOffset(), (uint8_t*)&chunkData, sizeof(chunkData), game->chunkStorageTransferBuffer->buffer, game->chunkStorageTransferBuffer->cycle, cmdBuffer);
	}
	else
	*/

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuffer);

	for (int i = 0; i < numJobs; i++)
	{
		SDL_GPUTextureRegion src = {};
		src.texture = game->chunkTexture;
		src.mip_level = 0;
		src.layer = 0;
		src.x = jobs[i].chunk->getChunkTextureOffset().x;
		src.y = jobs[i].chunk->getChunkTextureOffset().y;
		src.z = jobs[i].chunk->getChunkTextureOffset().z;
		src.w = CHUNK_SIZE;
		src.h = CHUNK_SIZE;
		src.d = CHUNK_SIZE;

		SDL_GPUTextureTransferInfo dst = {};
		dst.transfer_buffer = readbackData->transferBuffer;
		dst.offset = i * CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * sizeof(uint8_t);
		dst.pixels_per_row = CHUNK_SIZE;
		dst.rows_per_layer = CHUNK_SIZE;

		SDL_DownloadFromGPUTexture(copyPass, &src, &dst);
	}

	SDL_EndGPUCopyPass(copyPass);

	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdBuffer);
	//SDL_WaitForGPUFences(device, true, &fence, 1);
	//SDL_ReleaseGPUFence(device, fence);

	//void* mappedBuffer = SDL_MapGPUTransferBuffer(device, readbackData->transferBuffer, false);
	//SDL_memcpy(job.chunk->blocks, mappedBuffer, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * sizeof(uint8_t));
	//SDL_UnmapGPUTransferBuffer(device, readbackData->transferBuffer);

	readbackData->fence = fence;
	readbackData->refCount = numJobs;

	for (int i = 0; i < numJobs; i++)
	{
		jobs[i].chunk->readbackData = readbackData;
		jobs[i].chunk->readbackDataIdx = i;
		jobs[i].chunk->isLoaded = true;
		jobs[i].chunk->needsMeshUpdate = true;
		jobs[i].chunk->remeshQueued = false;
	}

	//SDL_LockMutex(game->chunkReadbackMutex);
		//PoolRelease(&game->chunkReadbackPool, readbackData);
		//SDL_UnlockMutex(game->chunkReadbackMutex);

	uint64_t afterGen = SDL_GetTicksNS();
	game->chunkGenAcc += afterGen - beforeGen;
	game->chunkGenCounter += numJobs;
}

static void RunMeshingJobs(ChunkJob* jobs, int numJobs, ChunkGeneratorThreadData* data)
{
	for (int i = 0; i < numJobs; i++)
	{
		SDL_assert(!jobs[i].chunk->readbackData);
	}

	uint64_t beforeMesh = SDL_GetTicksNS();

	//ChunkReadbackData* readbackData = PoolAlloc(&game->chunkReadbackPool);

	/*
	if (game->gpuMeshing)
	{
		Chunk* neighbors[6];
		uint32_t neighborFlags[6];
		GetChunkNeighbors(job.chunk, neighbors, neighborFlags);

		//ChunkMesherRunGPU(&data->mesher, data, job.chunk, neighbors, neighborFlags, readbackData->transferBuffer, cmdBuffer);

		//readbackData->fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdBuffer);
		//job.chunk->readbackData = readbackData;
	}
	else
	*/

	//SDL_LockMutex(game->chunkReadbackMutex);
	//PoolRelease(&game->chunkReadbackPool, readbackData);
	//SDL_UnlockMutex(game->chunkReadbackMutex);

	for (int i = 0; i < numJobs; i++)
	{
		jobs[i].chunk->needsMeshUpdate = false;
	}

	SDL_GPUCommandBuffer* cmdBuffer = SDL_AcquireGPUCommandBuffer(device);

	for (int i = 0; i < numJobs; i++)
	{
		// we copy the neighbor chunk data here so the neighbors dont get unloaded while the chunk mesher is running.
		// the current chunk is protected from this with the remeshQueued bool, but not the neighbors.
		// there is still a possibility an unload may occur during the copy, but its less likely.
		// the only reliable fix for this would be to use mutexes for every chunk which introduces a lot of complexity.
		// i decided not to do that because so far the only side effect of this are additional unneeded chunk faces,
		// which can be removed with a simple remesh.
		Chunk* neighbors[6];
		uint32_t neighborFlags[6];
		Chunk neighborCopies[6];
		Chunk* neighborCopyPtrs[6];
		GetChunkNeighbors(jobs[i].chunk, neighbors, neighborFlags);
		CopyChunkNeighbors(neighbors, neighborCopies, neighborCopyPtrs);

		ChunkMesherRun(&data->mesher, data, jobs[i].chunk, neighborCopyPtrs, neighborFlags);

		if (data->mesher.vertexCount > 0)
		{
			SDL_LockMutex(game->chunkVertexBufferMutex);
			UpdateVertexBuffer(game->chunkVertexBuffer, jobs[i].chunk->getVertexBufferOffset() * sizeof(uint32_t), (uint8_t*)data->mesher.vertexData, data->mesher.vertexCount * sizeof(uint32_t), game->chunkVertexTransferBuffer->buffer, game->chunkVertexTransferBuffer->cycle, cmdBuffer);
			SDL_UnlockMutex(game->chunkVertexBufferMutex);
		}
	}

	SDL_SubmitGPUCommandBuffer(cmdBuffer);

	for (int i = 0; i < numJobs; i++)
	{
		jobs[i].chunk->hasMesh = true;
		jobs[i].chunk->remeshQueued = false;
	}

	uint64_t afterMesh = SDL_GetTicksNS();
	game->chunkMeshAcc += afterMesh - beforeMesh;
	game->chunkMeshCounter += numJobs;
}

static int ChunkGeneratorMain(void* ptr)
{
	ChunkGeneratorThreadData* data = (ChunkGeneratorThreadData*)ptr;

	//InitWorldGenerator(&data->generator);
	InitChunkMesher(&data->mesher); // we divide by 2 since in the worst case scenario only every 2nd block is solid

	for (int i = 0; i < CHUNK_BATCH_SIZE; i++)
	{
		SDL_GPUTextureCreateInfo heightmapInfo = {};
		heightmapInfo.type = SDL_GPU_TEXTURETYPE_2D;
		heightmapInfo.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
		heightmapInfo.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
		heightmapInfo.width = CHUNK_SIZE;
		heightmapInfo.height = CHUNK_SIZE;
		heightmapInfo.layer_count_or_depth = 1;
		heightmapInfo.num_levels = 1;
		heightmapInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
		data->heightmaps[i] = SDL_CreateGPUTexture(device, &heightmapInfo);
	}

	SDL_GPUBufferCreateInfo faceMaskBufferInfo = {};
	faceMaskBufferInfo.size = sizeof(uint32_t) + CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * sizeof(uint32_t);
	faceMaskBufferInfo.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
	data->faceMaskBuffer = SDL_CreateGPUBuffer(device, &faceMaskBufferInfo);

	SDL_GPUBufferCreateInfo counterBufferInfo = {};
	counterBufferInfo.size = sizeof(uint32_t) + CHUNK_SIZE * CHUNK_SIZE * 6 * sizeof(uint32_t);
	counterBufferInfo.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
	data->claimedFaceBuffer = SDL_CreateGPUBuffer(device, &counterBufferInfo);

	data->running = true;
	while (data->running)
	{
		ChunkJob generatorJobs[CHUNK_QUEUE_SIZE];
		int numGeneratorJobs = 0;
		ChunkReadbackData* readbackData = nullptr;

		SDL_LockMutex(game->chunkJobMutex);
		SDL_LockMutex(game->chunkReadbackMutex);
		if (game->chunkJobQueue.size > 0 && game->chunkReadbackPool.freeHead != -1)
		{
			for (int i = 0; i < game->chunkJobQueue.size && numGeneratorJobs < CHUNK_BATCH_SIZE; i++)
			{
				QueuePop(&game->chunkJobQueue, &generatorJobs[numGeneratorJobs++]);
			}

			readbackData = PoolAlloc(&game->chunkReadbackPool);
		}
		SDL_UnlockMutex(game->chunkJobMutex);
		SDL_UnlockMutex(game->chunkReadbackMutex);

		if (numGeneratorJobs > 0)
		{
			RunGeneratorJobs(generatorJobs, numGeneratorJobs, readbackData, data);
		}


		ChunkJob meshingJobs[CHUNK_BATCH_SIZE];
		int numMeshingJobs = 0;

		SDL_LockMutex(game->chunkMeshingMutex);
		if (game->chunkMeshingQueue.size > 0)
		{
			for (int i = 0; i < game->chunkMeshingQueue.size && numMeshingJobs < CHUNK_BATCH_SIZE; i++)
			{
				QueuePop(&game->chunkMeshingQueue, &meshingJobs[numMeshingJobs++]);
			}
		}
		SDL_UnlockMutex(game->chunkMeshingMutex);

		SDL_LockMutex(game->chunkRemeshingMutex);
		if (numMeshingJobs < CHUNK_BATCH_SIZE && game->chunkRemeshingQueue.size > 0)
		{
			for (int i = 0; i < game->chunkRemeshingQueue.size && numMeshingJobs < CHUNK_BATCH_SIZE; i++)
			{
				QueuePop(&game->chunkRemeshingQueue, &meshingJobs[numMeshingJobs++]);
			}
		}
		SDL_UnlockMutex(game->chunkRemeshingMutex);

		if (numMeshingJobs > 0)
		{
			RunMeshingJobs(meshingJobs, numMeshingJobs, data);
		}

		if (numGeneratorJobs == 0 && numMeshingJobs == 0)
		{
			SDL_Delay(1);
		}

		/*
		SDL_LockMutex(data->mutex);
		bool runTask = data->hasData && !data->hasFinished;
		SDL_UnlockMutex(data->mutex);

		if (runTask)
		{
			//if (data->generate)
			GenerateChunk(&data->game->worldGenerator, data, &data->chunk);
			//if (data->remesh)
			//	ChunkMesherRun(&data->mesher, &data->chunk, data->game);

			data->chunk.hasMesh = true;
			data->chunk.needsMeshUpdate = false;

			SDL_LockMutex(data->mutex);
			data->hasFinished = true;
			SDL_UnlockMutex(data->mutex);
		}
		*/
	}

	return 0;
}

static SDL_GPUTexture* CreateDepthTarget(int width, int height)
{
	SDL_GPUTextureCreateInfo depthTextureInfo = {};
	depthTextureInfo.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
	depthTextureInfo.width = width;
	depthTextureInfo.height = height;
	depthTextureInfo.layer_count_or_depth = 1;
	depthTextureInfo.num_levels = 1;
	depthTextureInfo.type = SDL_GPU_TEXTURETYPE_2D;
	depthTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
	return SDL_CreateGPUTexture(device, &depthTextureInfo);
}

static RenderTarget* CreateGBuffer(int width, int height)
{
#define GBUFFER_COLOR_ATTACHMENTS 3
	ColorAttachmentInfo colorAttachments[GBUFFER_COLOR_ATTACHMENTS];
	// position
	colorAttachments[0] = {};
	colorAttachments[0].format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
	colorAttachments[0].loadOp = SDL_GPU_LOADOP_CLEAR;
	colorAttachments[0].storeOp = SDL_GPU_STOREOP_STORE;
	colorAttachments[0].clearColor = vec4(0.0f);
	// normal
	colorAttachments[1] = {};
	colorAttachments[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
	colorAttachments[1].loadOp = SDL_GPU_LOADOP_DONT_CARE;
	colorAttachments[1].storeOp = SDL_GPU_STOREOP_STORE;
	colorAttachments[1].clearColor = vec4(0.0f);
	// color
	colorAttachments[2] = {};
	colorAttachments[2].format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT;
	colorAttachments[2].loadOp = SDL_GPU_LOADOP_DONT_CARE;
	colorAttachments[2].storeOp = SDL_GPU_STOREOP_STORE;

	DepthAttachmentInfo depthAttachment = {};
	depthAttachment.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
	depthAttachment.loadOp = SDL_GPU_LOADOP_CLEAR;
	depthAttachment.storeOp = SDL_GPU_STOREOP_STORE;
	depthAttachment.clearDepth = 1;

	return CreateRenderTarget(width, height, GBUFFER_COLOR_ATTACHMENTS, colorAttachments, &depthAttachment);
}

void GameInit()
{
	game->depthTexture = CreateDepthTarget(width, height);
	game->gbuffer = CreateGBuffer(width, height);

	game->chunkShader = LoadGraphicsShader("res/shaders/chunk.vert.bin", "res/shaders/chunk.frag.bin");

	AddFileWatcher(PROJECT_PATH "/res/shaders/chunk.vert");
	AddFileWatcher(PROJECT_PATH "/res/shaders/chunk.frag");

	GraphicsPipelineInfo cubePipelineInfo = CreateGraphicsPipelineInfo(game->chunkShader, 1, chunkBufferLayouts);
	cubePipelineInfo.numColorTargets = GBUFFER_COLOR_ATTACHMENTS;
	cubePipelineInfo.colorTargets[0].format = game->gbuffer->colorAttachmentInfos[0].format;
	cubePipelineInfo.colorTargets[1].format = game->gbuffer->colorAttachmentInfos[1].format;
	cubePipelineInfo.colorTargets[2].format = game->gbuffer->colorAttachmentInfos[2].format;
	cubePipelineInfo.hasDepthTarget = true;
	cubePipelineInfo.depthFormat = game->gbuffer->depthAttachmentInfo.format;
	cubePipelineInfo.bufferDescriptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
	game->chunkPipeline = CreateGraphicsPipeline(&cubePipelineInfo);

	game->chunkVertexBuffer = CreateVertexBuffer(MAX_LOADED_CHUNKS * CHUNK_VERTEX_BUFFER_SIZE, &chunkBufferLayouts[0], SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE, nullptr, MAX_LOADED_CHUNKS * CHUNK_VERTEX_BUFFER_SIZE * sizeof(uint32_t), cmdBuffer);
	game->chunkVertexBufferMutex = SDL_CreateMutex();

	game->chunkStorageBuffer = CreateStorageBuffer(nullptr, MAX_LOADED_CHUNKS * 6 * sizeof(ChunkData), cmdBuffer);
	game->chunkIndirectBuffer = CreateIndirectBuffer(MAX_LOADED_CHUNKS * 6, false, SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE);

	SDL_GPUTextureCreateInfo chunkTextureInfo = {};
	chunkTextureInfo.type = SDL_GPU_TEXTURETYPE_3D;
	chunkTextureInfo.format = SDL_GPU_TEXTUREFORMAT_R8_UINT;
	chunkTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
	chunkTextureInfo.width = CHUNK_TEXTURE_WIDTH * CHUNK_SIZE;
	chunkTextureInfo.height = CHUNK_TEXTURE_WIDTH * CHUNK_SIZE;
	chunkTextureInfo.layer_count_or_depth = CHUNK_TEXTURE_WIDTH * CHUNK_SIZE;
	chunkTextureInfo.num_levels = 1;
	chunkTextureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
	game->chunkTexture = SDL_CreateGPUTexture(device, &chunkTextureInfo);

	game->chunkPalette = LoadTexture("res/textures/palette.png.bin", cmdBuffer);

	game->chunkVertexTransferBuffer = CreateTransferBuffer(CHUNK_VERTEX_BUFFER_SIZE, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, true);
	game->chunkStorageTransferBuffer = CreateTransferBuffer(MAX_LOADED_CHUNKS * sizeof(ChunkData), SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, true);
	game->chunkIndirectTransferBuffer = CreateTransferBuffer(MAX_LOADED_CHUNKS * sizeof(SDL_GPUIndirectDrawCommand), SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, true);

	game->lightingShader = LoadGraphicsShader("res/shaders/lighting.vert.bin", "res/shaders/lighting.frag.bin");

	{
		GraphicsPipelineInfo pipelineInfo = {};

		pipelineInfo.shader = game->lightingShader;
		pipelineInfo.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pipelineInfo.cullMode = SDL_GPU_CULLMODE_BACK;

		pipelineInfo.numColorTargets = 1;
		pipelineInfo.colorTargets[0].format = SDL_GetGPUSwapchainTextureFormat(device, window);
		pipelineInfo.colorTargets[0].blend_state.enable_blend = false;

		pipelineInfo.numAttributes = 1;
		pipelineInfo.attributes[0] = {};
		pipelineInfo.attributes[0].buffer_slot = 0;
		pipelineInfo.attributes[0].location = 0;
		pipelineInfo.attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
		pipelineInfo.attributes[0].offset = 0;

		pipelineInfo.numVertexBuffers = 1;
		pipelineInfo.bufferDescriptions[0].slot = 0;
		pipelineInfo.bufferDescriptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
		pipelineInfo.bufferDescriptions[0].instance_step_rate = 0;
		pipelineInfo.bufferDescriptions[0].pitch = GetVertexFormatSize(pipelineInfo.attributes[0].format);

		game->lightingPipeline = CreateGraphicsPipeline(&pipelineInfo);
	}

	InitScreenQuad(&game->screenQuad, cmdBuffer);

	SDL_GPUSamplerCreateInfo samplerInfo = {};
	game->defaultSampler = SDL_CreateGPUSampler(device, &samplerInfo);

	//InitChunkAllocator(&game->chunkAllocator);

	InitWorldGenerator(&game->worldGenerator);
	//InitChunkMesher(&game->chunkMesher);

	InitQueue(&game->chunkJobQueue);
	InitQueue(&game->chunkMeshingQueue);
	InitQueue(&game->chunkRemeshingQueue);
	InitPool(&game->chunkReadbackPool);

	game->chunkJobMutex = SDL_CreateMutex();
	game->chunkMeshingMutex = SDL_CreateMutex();
	game->chunkRemeshingMutex = SDL_CreateMutex();
	game->chunkReadbackMutex = SDL_CreateMutex();

	for (int i = 0; i < NUM_CHUNK_THREADS; i++)
	{
		game->chunkGeneratorsData[i].game = game;

		char name[32];
		SDL_snprintf(name, 32, "Chunk Generator %d", i);
		game->chunkGenerators[i] = SDL_CreateThread(ChunkGeneratorMain, name, &game->chunkGeneratorsData[i]);
	}

	for (int i = 0; i < game->chunkReadbackPool.capacity; i++)
	{
		ChunkReadbackData* readbackData = &game->chunkReadbackPool.data[i];
		readbackData->fence = nullptr;
		readbackData->refCount = 0;

		SDL_GPUTransferBufferCreateInfo transferInfo = {};
		transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
		transferInfo.size = CHUNK_BATCH_SIZE * CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * sizeof(uint8_t); // sizeof(SDL_GPUIndirectDrawCommand) + sizeof(uint32_t);
		readbackData->transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
	}

	game->cameraPosition = vec3(0, 16, 32);
	//game->cameraPitch = -0.4f * PI;
	//game->cameraYaw = 0.25f * PI;

	game->mouseLocked = true;
}

void GameDestroy()
{
	for (int i = 0; i < NUM_CHUNK_THREADS; i++)
	{
		SDL_Thread* thread = game->chunkGenerators[i];
		ChunkGeneratorThreadData* data = &game->chunkGeneratorsData[i];
		data->running = false;

		int status;
		SDL_WaitThread(thread, &status);

		SDL_assert(status != -1);
	}

	for (int i = 0; i < MAX_LOADED_CHUNKS; i++)
	{
		Chunk* chunk = &game->chunks[i];
		if (chunk->isActive)
		{
			//DisableChunk(chunk);
			chunk->isActive = false;
		}
	}

	DestroyGraphicsPipeline(game->chunkPipeline);
	DestroyShader(game->chunkShader);
}

void GameResize(int newWidth, int newHeight)
{
	if (game->depthTexture)
		SDL_ReleaseGPUTexture(device, game->depthTexture);
	game->depthTexture = CreateDepthTarget(newWidth, newHeight);

	if (game->gbuffer)
		DestroyRenderTarget(game->gbuffer);
	game->gbuffer = CreateGBuffer(newWidth, newHeight);
}

static bool HasChunkGeneratorForGridPosition(ivec3 gridPosition)
{
	for (int i = 0; i < game->chunkJobQueue.size; i++)
	{
		int idx = (game->chunkJobQueue.head + i) % game->chunkJobQueue.capacity;
		if (game->chunkJobQueue.data[idx].chunk->gridPosition == gridPosition)
			return true;
	}
	return false;
}

static bool ChunkGeneratorAvailable()
{
	return game->chunkJobQueue.size < game->chunkJobQueue.capacity;
}

static void QueueChunkGenerator(Chunk* chunk)
{
	chunk->remeshQueued = true;

	ChunkJob job = {};
	job.chunk = chunk;
	job.gridPosition = chunk->gridPosition;
	job.lod = chunk->lod;

	SDL_LockMutex(game->chunkJobMutex);
	QueuePush(&game->chunkJobQueue, job);
	SDL_UnlockMutex(game->chunkJobMutex);
}

static bool HasChunkMesherForChunk(Chunk* chunk)
{
	for (int i = 0; i < game->chunkMeshingQueue.size; i++)
	{
		int idx = (game->chunkMeshingQueue.head + i) % game->chunkMeshingQueue.capacity;
		if (game->chunkMeshingQueue.data[idx].chunk == chunk)
			return true;
	}
	for (int i = 0; i < game->chunkRemeshingQueue.size; i++)
	{
		int idx = (game->chunkRemeshingQueue.head + i) % game->chunkRemeshingQueue.capacity;
		if (game->chunkRemeshingQueue.data[idx].chunk == chunk)
			return true;
	}
	return false;
}

static bool ChunkMesherAvailable(Chunk* chunk)
{
	if (chunk->hasMesh)
		return game->chunkRemeshingQueue.size < game->chunkRemeshingQueue.capacity;
	else
		return game->chunkMeshingQueue.size < game->chunkMeshingQueue.capacity;
}

static void QueueChunkMesher(Chunk* chunk)
{
	chunk->remeshQueued = true;

	ChunkJob job = {};
	job.chunk = chunk;
	job.gridPosition = chunk->gridPosition;
	job.lod = chunk->lod;

	if (chunk->hasMesh)
	{
		SDL_LockMutex(game->chunkRemeshingMutex);
		QueuePush(&game->chunkRemeshingQueue, job);
		SDL_UnlockMutex(game->chunkRemeshingMutex);
	}
	else
	{
		SDL_LockMutex(game->chunkMeshingMutex);
		QueuePush(&game->chunkMeshingQueue, job);
		SDL_UnlockMutex(game->chunkMeshingMutex);
	}
}

/*
static void AcquireChunkGeneratorResults()
{
	int numThreadsFinished = 0;
	int numThreadsRunning = 0;
	for (int i = 0; i < NUM_CHUNK_THREADS; i++)
	{
		ChunkGeneratorThreadData* data = &game->chunkGeneratorsData[i];

		if (data->hasData)
		{
			numThreadsRunning++;
		}

		if (data->hasData && data->hasFinished)
		{
			SDL_LockMutex(data->mutex);
			int gridIdx = GetChunkGridIdxFromPosition(data->chunk.position, data->chunk.lod);
			SDL_assert(gridIdx != -1);

			if (Chunk* chunk = game->lods[data->chunk.lod].chunkGrid[gridIdx])
			{
				SDL_assert(chunk->id == data->chunk.id && chunk->position == data->chunk.position && chunk->lod == data->chunk.lod);
				*chunk = data->chunk;

				chunk->isLoaded = true;
			}

			data->hasData = false;
			data->hasFinished = false;

			SDL_UnlockMutex(data->mutex);

			numThreadsFinished++;
		}
	}
	//SDL_Log("%d threads finished, from %d", numThreadsFinished, numThreadsRunning);
}
*/

static void UpdateChunkVisibility()
{
	for (int i = 0; i <= game->lastLoadedChunk; i++)
	{
		Chunk* chunk = &game->chunks[i];
		if (chunk->isActive && chunk->isLoaded)
		{
			if (chunk->readbackData)
			{
				//SDL_assert(game->gpuMeshing);
				if (SDL_QueryGPUFence(device, chunk->readbackData->fence))
				{
					//void* mappedBuffer = SDL_MapGPUTransferBuffer(device, chunk->readbackData->transferBuffer, false);
					//SDL_GPUIndirectDrawCommand* drawCmd = (SDL_GPUIndirectDrawCommand*)mappedBuffer;
					//uint32_t* blockCounter = (uint32_t*)(drawCmd + 1);
					//chunk->vertexCount = drawCmd->num_instances;
					//chunk->blockCount = (int)*blockCounter;
					//SDL_UnmapGPUTransferBuffer(device, chunk->readbackData->transferBuffer);

					void* mappedBuffer = SDL_MapGPUTransferBuffer(device, chunk->readbackData->transferBuffer, false);
					SDL_memcpy(chunk->blocks, (uint8_t*)mappedBuffer + chunk->readbackDataIdx * CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * sizeof(uint8_t), CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * sizeof(uint8_t));
					SDL_UnmapGPUTransferBuffer(device, chunk->readbackData->transferBuffer);

					chunk->readbackData->refCount--;
					if (chunk->readbackData->refCount == 0)
					{
						SDL_ReleaseGPUFence(device, chunk->readbackData->fence);
						chunk->readbackData->fence = nullptr;
						chunk->readbackData->refCount = 0;

						SDL_LockMutex(game->chunkReadbackMutex);
						PoolRelease(&game->chunkReadbackPool, chunk->readbackData);
						SDL_UnlockMutex(game->chunkReadbackMutex);
					}

					chunk->readbackData = nullptr;

					Chunk* neighbors[6];
					GetChunkNeighbors(chunk, neighbors);
					for (int i = 0; i < 6; i++)
					{
						if (neighbors[i])
							neighbors[i]->needsMeshUpdate = true;
					}
				}
			}

			if (chunk->vertexCount == 0 && !chunk->remeshQueued && !chunk->readbackData)
			{
				int lod = chunk->lod;
				int gridIdx = GetChunkGridIdxFromGridPosition(chunk->gridPosition);
				SDL_assert(gridIdx != -1);

				if (chunk->blockCount == 0)
					game->lods[lod].chunkFlags[gridIdx] |= CHUNK_FLAG_EMPTY;
				else
				{
					SDL_assert(chunk->blockCount == CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
					game->lods[lod].chunkFlags[gridIdx] |= CHUNK_FLAG_SOLID;
				}

				UnloadChunk(chunk);
			}
			/*
			else if (chunk->hasMesh && !chunk->needsMeshUpdate && chunk->getTotalVertexCount() == 0 || chunk->isEmpty)
			{
				int lod = chunk->lod;
				SDL_assert(gridIdx != -1);
				UnloadChunk(chunk);
				game->lods[lod].chunkFlags[gridIdx] |= CHUNK_FLAG_SOLID;
			}
			*/
			else if (chunk->needsMeshUpdate)
			{
				// only remesh
				if (!chunk->remeshQueued && !chunk->readbackData && ChunkMesherAvailable(chunk))
				{
					SDL_assert(!HasChunkMesherForChunk(chunk) && !HasChunkGeneratorForGridPosition(chunk->gridPosition));
					QueueChunkMesher(chunk);
				}

				/*
				int generatorID;
				if (ChunkGeneratorAvailable(&generatorID))
				{
					//QueueChunkGenerator(generatorID, chunk, false, true, game);
				}
				*/
			}
		}
	}
}

// TODO
// break out of loop if no generator is available to avoid random chunks and keep prioritization
// fix chunk batch meshing
static void LoadNewChunks()
{
	for (int lod = 0; lod < NUM_CHUNK_LOD_LEVELS; lod++)
	{
		int chunkSize = CHUNK_SIZE * ipow(2, lod);
		for (int d = lod > 0 ? CHUNK_LOD_DISTANCE / 4 + 1 : 1; d <= CHUNK_LOD_DISTANCE / 2; d++)
		{
			for (int z = -d; z < d; z++)
			{
				for (int y = -d; y < d; y++)
				{
					for (int x = -d; x < d; x++)
					{
						if (!(x == -d || x == d - 1 || y == -d || y == d - 1 || z == -d || z == d - 1))
							continue;

						ivec3 gridPosition = ivec3(x, y, z);
						int gridIdx = (x + CHUNK_LOD_DISTANCE / 2) + (y + CHUNK_LOD_DISTANCE / 2) * CHUNK_LOD_DISTANCE + (z + CHUNK_LOD_DISTANCE / 2) * CHUNK_LOD_DISTANCE * CHUNK_LOD_DISTANCE;
						Chunk* chunk = game->lods[lod].chunkGrid[gridIdx];
						uint32_t flags = game->lods[lod].chunkFlags[gridIdx];
						SDL_assert(!chunk || chunk->gridPosition == gridPosition && chunk->lod == lod);

						if (game->numLoadedChunks < MAX_LOADED_CHUNKS && ChunkGeneratorAvailable())
						{
							if (!flags && !chunk && !HasChunkGeneratorForGridPosition(gridPosition))
							{
								if (chunk = InitChunk(gridPosition, lod))
								{
									QueueChunkGenerator(chunk);
								}
							}
						}
						else
						{
							return;
						}
					}
				}
			}
		}
	}
}

void GameUpdate()
{
	/*
	if (FileHasChanged(PROJECT_PATH "/res/shaders/chunk.vert") || FileHasChanged(PROJECT_PATH "/res/shaders/chunk.frag"))
	{
		app->platformCallbacks.compileResources();
		ReloadGraphicsShader(game->chunkShader, "res/shaders/chunk.vert.bin", "res/shaders/chunk.frag.bin");
		ReloadGraphicsPipeline(game->chunkPipeline);
	}
	*/

	//AcquireChunkGeneratorResults();
	UpdateChunkVisibility();
	LoadNewChunks();

	vec3 delta = vec3::Zero;
	if (app->keys[SDL_SCANCODE_A]) delta += game->cameraRotation.left();
	if (app->keys[SDL_SCANCODE_D]) delta += game->cameraRotation.right();
	if (app->keys[SDL_SCANCODE_S]) delta += game->cameraRotation.back();
	if (app->keys[SDL_SCANCODE_W]) delta += game->cameraRotation.forward();
	if (app->keys[SDL_SCANCODE_SPACE]) delta += vec3::Up;
	if (app->keys[SDL_SCANCODE_LCTRL]) delta += vec3::Down;

	if (delta.lengthSquared() > 0)
	{
		float speed = app->keys[SDL_SCANCODE_LSHIFT] ? 100.0f : app->keys[SDL_SCANCODE_LALT] ? 10.0f : 40.0f;
		vec3 velocity = delta.normalized() * speed;
		vec3 displacement = velocity * deltaTime;
		game->cameraPosition += displacement;
	}

	if (app->keys[SDL_SCANCODE_ESCAPE] && !app->lastKeys[SDL_SCANCODE_ESCAPE])
		game->mouseLocked = !game->mouseLocked;
	if (app->keys[SDL_SCANCODE_F6] && !app->lastKeys[SDL_SCANCODE_F6])
	{
		// regenerate chunks
		for (int i = 0; i < MAX_LOADED_CHUNKS; i++)
		{
			if (game->chunks[i].isActive)
				UnloadChunk(&game->chunks[i]);
		}
	}
	if (app->keys[SDL_SCANCODE_F7] && !app->lastKeys[SDL_SCANCODE_F7])
	{
		// regenerate chunks
		for (int i = 0; i < MAX_LOADED_CHUNKS; i++)
		{
			if (game->chunks[i].isActive)
				game->chunks[i].needsMeshUpdate = true;
		}
	}

	SDL_SetWindowRelativeMouseMode(window, game->mouseLocked);

	if (game->mouseLocked)
	{
		game->cameraYaw -= app->mouseDelta.x * 0.001f;
		game->cameraPitch -= app->mouseDelta.y * 0.001f;
	}

	game->cameraRotation = Quaternion::FromAxisAngle(vec3::Up, game->cameraYaw) * Quaternion::FromAxisAngle(vec3::Right, game->cameraPitch);
}

static bool FrustumCulling(const AABB& aabb, vec4 planes[6])
{
	for (int i = 0; i < 6; i++)
	{
		const vec4& plane = planes[i];
		if (
			dot(plane, vec4(aabb.min, 1)) < 0 &&
			dot(plane, vec4(aabb.max.x, aabb.min.y, aabb.min.z, 1)) < 0 &&
			dot(plane, vec4(aabb.min.x, aabb.max.y, aabb.min.z, 1)) < 0 &&
			dot(plane, vec4(aabb.max.x, aabb.max.y, aabb.min.z, 1)) < 0 &&
			dot(plane, vec4(aabb.min.x, aabb.min.y, aabb.max.z, 1)) < 0 &&
			dot(plane, vec4(aabb.max.x, aabb.min.y, aabb.max.z, 1)) < 0 &&
			dot(plane, vec4(aabb.min.x, aabb.max.y, aabb.max.z, 1)) < 0 &&
			dot(plane, vec4(aabb.max, 1)) < 0
			)
			return false;
	}
	return true;
}

static int SDLCALL ChunkComparator(const void* ap, const void* bp)
{
	const Chunk* a = *(const Chunk**)ap;
	const Chunk* b = *(const Chunk**)bp;
	vec3 aCenter = a->getWorldPosition() + 0.5f * CHUNK_SIZE;
	vec3 bCenter = b->getWorldPosition() + 0.5f * CHUNK_SIZE;
	vec3 toA = aCenter - game->cameraPosition;
	vec3 toB = bCenter - game->cameraPosition;
	float da = toA.lengthSquared();
	float db = toB.lengthSquared();
	return da < db ? -1 : da > db ? 1 : 0;
}

static int UpdateDrawBuffers(vec4 frustumPlanes[6])
{
	game->numRenderedChunks = 0;
	game->numRenderedVertices = 0;

	Chunk** chunkDrawList = (Chunk**)BumpAllocatorCalloc(&memory->transientAllocator, game->lastLoadedChunk + 1, sizeof(Chunk*));
	int numDrawChunks = 0;

	for (int i = 0; i <= game->lastLoadedChunk; i++)
	{
		Chunk* chunk = &game->chunks[i];
		if (chunk->isActive && chunk->hasMesh)
		{
			AABB aabb = { chunk->getWorldPosition(), chunk->getWorldPosition() + CHUNK_SIZE * chunk->chunkScale };
			if (FrustumCulling(aabb, frustumPlanes))
			{
				chunkDrawList[numDrawChunks++] = chunk;
			}
		}
	}
	game->numRenderedChunks = numDrawChunks;

	SDL_qsort(chunkDrawList, numDrawChunks, sizeof(Chunk*), (SDL_CompareCallback)ChunkComparator);

	int maxDrawCommands = numDrawChunks;
	SDL_GPUIndirectDrawCommand* drawCommands = (SDL_GPUIndirectDrawCommand*)BumpAllocatorCalloc(&memory->transientAllocator, maxDrawCommands, sizeof(SDL_GPUIndirectDrawCommand));
	ChunkData* chunkStorageData = (ChunkData*)BumpAllocatorCalloc(&memory->transientAllocator, maxDrawCommands, sizeof(ChunkData));

	int numDrawCommands = 0;
	for (int i = 0; i < numDrawChunks; i++)
	{
		Chunk* chunk = chunkDrawList[i];

		if (chunk->vertexCount == 0)
			continue;

		SDL_GPUIndirectDrawCommand* drawCommand = &drawCommands[numDrawCommands];
		drawCommand->num_vertices = 3;
		drawCommand->num_instances = chunk->vertexCount;
		drawCommand->first_vertex = 0;
		drawCommand->first_instance = chunk->getVertexBufferOffset();

		ChunkData* storageData = &chunkStorageData[numDrawCommands];
		storageData->position = chunk->getWorldPosition();
		storageData->scale = chunk->chunkScale;

		numDrawCommands++;

		game->numRenderedVertices += chunk->vertexCount;

		/*
		ivec3 cameraChunk = (ivec3)floor(game->cameraPosition / CHUNK_SIZE / chunk->chunkScale);
		ivec3 dir = chunk->gridPosition - cameraChunk;
		ivec3 sgn = sign(dir);

		for (int j = 0; j < 6; j++)
		{
			int axis = j / 2;
			int s = j % 2 * 2 - 1;

			bool cullFace = sgn[axis] == s;

			if (!cullFace)
			{
				SDL_GPUIndirectDrawCommand* drawCommand = &drawCommands[numDrawCommands];
				drawCommand->num_vertices = chunk->vertexCounts[j];
				drawCommand->num_instances = 1;
				drawCommand->first_vertex = chunk->vertexOffsets[j];
				drawCommand->first_instance = 0;

				ChunkData* storageData = &chunkStorageData[numDrawCommands];
				storageData->position = chunk->position;
				storageData->scale = ipow(2, chunk->lod);

				numDrawCommands++;

				game->numRenderedVertices += chunk->vertexCounts[j];
			}
		}
		*/
	}

	if (numDrawCommands > 0)
	{
		UpdateIndirectBuffer(game->chunkIndirectBuffer, 0, drawCommands, numDrawCommands, game->chunkIndirectTransferBuffer->buffer, game->chunkIndirectTransferBuffer->cycle, cmdBuffer);
		UpdateStorageBuffer(game->chunkStorageBuffer, 0, (const uint8_t*)chunkStorageData, numDrawCommands * sizeof(ChunkData), game->chunkStorageTransferBuffer->buffer, game->chunkStorageTransferBuffer->cycle, cmdBuffer);
	}

	return numDrawCommands;
}

void GameRender()
{
	Matrix projection = Matrix::Perspective(90 * Deg2Rad, width / (float)height, 1, 8000);
	Matrix view = Matrix::Rotate(game->cameraRotation.conjugated()) * Matrix::Translate(-game->cameraPosition);
	Matrix pv = projection * view;

	vec4 frustumPlanes[6];
	GetFrustumPlanes(pv, frustumPlanes);

	// geometry pass
	{
		if (game->gpuMeshing)
		{
			SDL_GPURenderPass* renderPass = BindRenderTarget(game->gbuffer, cmdBuffer);

			SDL_BindGPUGraphicsPipeline(renderPass, game->chunkPipeline->pipeline);

			SDL_GPUBufferBinding vertexBinding = {};
			vertexBinding.buffer = game->chunkVertexBuffer->buffer;
			vertexBinding.offset = 0;

			SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

			struct UniformData
			{
				Matrix pv;
			};
			UniformData uniforms = {};
			uniforms.pv = pv;
			SDL_PushGPUVertexUniformData(cmdBuffer, 0, &uniforms, sizeof(uniforms));

			SDL_GPUBuffer* storageBuffer = game->chunkStorageBuffer->buffer;
			SDL_BindGPUVertexStorageBuffers(renderPass, 0, &storageBuffer, 1);

			SDL_GPUTextureSamplerBinding bindings[1];
			bindings[0] = {};
			bindings[0].texture = game->chunkPalette->handle;
			bindings[0].sampler = game->defaultSampler;
			SDL_BindGPUFragmentSamplers(renderPass, 0, bindings, 1);

			SDL_DrawGPUPrimitivesIndirect(renderPass, game->chunkIndirectBuffer->buffer, 0, game->lastLoadedChunk + 1);

			SDL_EndGPURenderPass(renderPass);
		}
		else
		{
			int numDrawCommands = UpdateDrawBuffers(frustumPlanes);

			SDL_GPURenderPass* renderPass = BindRenderTarget(game->gbuffer, cmdBuffer);

			SDL_BindGPUGraphicsPipeline(renderPass, game->chunkPipeline->pipeline);

			SDL_GPUBufferBinding vertexBinding = {};
			vertexBinding.buffer = game->chunkVertexBuffer->buffer;
			vertexBinding.offset = 0;

			SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

			struct UniformData
			{
				Matrix pv;
			};
			UniformData uniforms = {};
			uniforms.pv = pv;
			SDL_PushGPUVertexUniformData(cmdBuffer, 0, &uniforms, sizeof(uniforms));

			SDL_GPUBuffer* storageBuffer = game->chunkStorageBuffer->buffer;
			SDL_BindGPUVertexStorageBuffers(renderPass, 0, &storageBuffer, 1);

			SDL_GPUTextureSamplerBinding bindings[1];
			bindings[0] = {};
			bindings[0].texture = game->chunkPalette->handle;
			bindings[0].sampler = game->defaultSampler;
			SDL_BindGPUFragmentSamplers(renderPass, 0, bindings, 1);

			SDL_DrawGPUPrimitivesIndirect(renderPass, game->chunkIndirectBuffer->buffer, 0, numDrawCommands);

			SDL_EndGPURenderPass(renderPass);
		}

		/*
		SDL_GPUBufferBinding vertexBinding;
		vertexBinding.buffer = game->chunkVertexBuffer->buffer;
		vertexBinding.offset = 0;

		SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

		UniformData uniforms = {};
		uniforms.pv = pv;
		SDL_PushGPUVertexUniformData(cmdBuffer, 0, &uniforms, sizeof(uniforms));

		SDL_GPUBuffer* storageBuffer = game->chunkStorageBuffer->buffer;
		SDL_BindGPUVertexStorageBuffers(renderPass, 0, &storageBuffer, 1);

		SDL_GPUTextureSamplerBinding bindings[1];
		bindings[0] = {};
		bindings[0].texture = game->chunkPalette->handle;
		bindings[0].sampler = game->defaultSampler;
		SDL_BindGPUFragmentSamplers(renderPass, 0, bindings, 1);

		//SDL_DrawGPUPrimitivesIndirect(renderPass, game->chunkDrawBuffer->buffer, 0, numDrawCommands);
		*/
	}

	// lighting pass
	{
		SDL_GPUColorTargetInfo colorTarget = {};
		colorTarget.clear_color = { 0.4f, 0.4f, 1.0f, 1.0f };
		colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
		colorTarget.store_op = SDL_GPU_STOREOP_STORE;
		colorTarget.texture = swapchain;

		SDL_GPUDepthStencilTargetInfo depthTarget = {};
		depthTarget.clear_depth = 1;
		depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
		depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;
		depthTarget.texture = game->depthTexture;

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdBuffer, &colorTarget, 1, &depthTarget);

		SDL_BindGPUGraphicsPipeline(renderPass, game->lightingPipeline->pipeline);

		SDL_GPUTexture* gbufferTextures[MAX_COLOR_ATTACHMENTS + 1];
		for (int i = 0; i < game->gbuffer->numColorAttachments; i++)
			gbufferTextures[i] = game->gbuffer->colorAttachments[i];
		gbufferTextures[game->gbuffer->numColorAttachments] = game->gbuffer->depthAttachment;

		RenderScreenQuad(&game->screenQuad, renderPass, game->gbuffer->numColorAttachments + 1, gbufferTextures, game->defaultSampler, cmdBuffer);

		SDL_EndGPURenderPass(renderPass);
	}
}
