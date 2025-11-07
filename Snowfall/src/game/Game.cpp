#include "Game.h"

#include "graphics/VertexBuffer.h"

#include "math/Vector.h"


struct ChunkUniforms
{
	Matrix pv;
};


static Chunk* GetAvailableChunk()
{
	for (int i = 0; i < MAX_LOADED_CHUNKS; i++)
	{
		Chunk* chunk = &game->chunks[i];
		if (!chunk->isLoaded)
		{
			chunk->id = i;
			chunk->isLoaded = true;
			game->numLoadedChunks++;

			if (chunk->id > game->lastLoadedChunk)
				game->lastLoadedChunk = chunk->id;

			return chunk;
		}
	}
	return nullptr;
}

int GetChunkGridIdxFromPosition(ivec3 position, int lod)
{
	const int chunkScales[10] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 };
	int chunkScale = chunkScales[lod];
	int x = idivfloor(position.x, CHUNK_SIZE * chunkScale);
	int z = idivfloor(position.z, CHUNK_SIZE * chunkScale);
	int y = idivfloor(position.y, CHUNK_SIZE * chunkScale);
	if (x >= -CHUNK_LOD_DISTANCE / 2 && x < CHUNK_LOD_DISTANCE / 2 && y >= -CHUNK_LOD_DISTANCE / 2 && y < CHUNK_LOD_DISTANCE / 2 && z >= -CHUNK_LOD_DISTANCE / 2 && z < CHUNK_LOD_DISTANCE / 2)
		return (x + CHUNK_LOD_DISTANCE / 2) + (y + CHUNK_LOD_DISTANCE / 2) * CHUNK_LOD_DISTANCE + (z + CHUNK_LOD_DISTANCE / 2) * CHUNK_LOD_DISTANCE * CHUNK_LOD_DISTANCE;
	return -1;
}

Chunk* GetChunkAtWorldPosWithLOD(ivec3 position, int lod, GameState* game)
{
	int gridIdx = GetChunkGridIdxFromPosition(position, lod);
	if (gridIdx != -1)
	{
		Chunk* chunk = game->lods[lod].chunkGrid[gridIdx];
		if (chunk && chunk->isLoaded
			&& position.x >= chunk->position.x && position.x < chunk->position.x + CHUNK_SIZE * chunk->chunkScale
			&& position.y >= chunk->position.y && position.y < chunk->position.y + CHUNK_SIZE * chunk->chunkScale
			&& position.z >= chunk->position.z && position.z < chunk->position.z + CHUNK_SIZE * chunk->chunkScale)
			return chunk;
	}
	return nullptr;
}

uint32_t GetChunkFlagsAtWorldPos(ivec3 position, int lod, GameState* game)
{
	int gridIdx = GetChunkGridIdxFromPosition(position, lod);
	if (gridIdx != -1)
	{
		uint32_t flags = game->lods[lod].chunkFlags[gridIdx];
		return flags;
	}
	return 0;
}

bool GetSolidAtWorldPos(ivec3 position, int lod, GameState* game)
{
	uint32_t flags = GetChunkFlagsAtWorldPos(position, lod, game);
	if (flags & CHUNK_FLAG_EMPTY)
		return false;
	else if (flags & CHUNK_FLAG_SOLID)
		return true;
	if (Chunk* chunk = GetChunkAtWorldPosWithLOD(position, lod, game))
	{
		ivec3 localpos = (position - chunk->position) / chunk->chunkScale;
		BlockData* block = chunk->getBlockData(localpos.x, localpos.y, localpos.z);
		return block->id;
	}
	return false;
}

static Chunk* InitChunk(const ivec3& position, int lod)
{
	Chunk* chunk = GetAvailableChunk();

	chunk->position = position;
	chunk->lod = lod;
	chunk->chunkScale = ipow(2, lod);

	int gridIdx = GetChunkGridIdxFromPosition(position, lod);
	SDL_assert(gridIdx != -1);
	SDL_assert(game->lods[lod].chunkGrid[gridIdx] == nullptr);
	game->lods[lod].chunkGrid[gridIdx] = chunk;
	game->lods[lod].chunkFlags[gridIdx] = 0;

	return chunk;
}

static void UnloadChunk(Chunk* chunk)
{
	if (game->lastLoadedChunk == chunk->id)
	{
		for (int i = game->lastLoadedChunk - 1; i >= 0; i--)
		{
			if (game->chunks[i].isLoaded)
			{
				game->lastLoadedChunk = i;
				break;
			}
		}
	}

	int vertexOffset = chunk->vertexOffsets[0];
	int vertexCount = chunk->getTotalVertexCount();
	if (vertexCount > 0)
		DeallocateChunk(&game->chunkAllocator, vertexOffset, vertexCount);

	int gridIdx = GetChunkGridIdxFromPosition(chunk->position, chunk->lod);
	SDL_assert(gridIdx != -1);
	SDL_assert(game->lods[chunk->lod].chunkGrid[gridIdx] == chunk);
	game->lods[chunk->lod].chunkGrid[gridIdx] = nullptr;
	game->lods[chunk->lod].chunkFlags[gridIdx] = 0;

	game->numLoadedChunks--;

	chunk->id = -1;
	chunk->position = ivec3(0);
	chunk->lod = -1;
	chunk->isLoaded = false;
	chunk->hasMesh = false;
	chunk->needsUpdate = false;
}

void GameInit()
{
	game->chunkShader = LoadGraphicsShader("res/shaders/chunk.vs.glsl.bin", "res/shaders/chunk.fs.glsl.bin");

	AddFileWatcher(PROJECT_PATH "/res/shaders/chunk.vs.glsl");
	AddFileWatcher(PROJECT_PATH "/res/shaders/chunk.fs.glsl");

	GraphicsPipelineInfo cubePipelineInfo = CreateGraphicsPipelineInfo(game->chunkShader, 1, chunkBufferLayouts);
	game->chunkPipeline = CreateGraphicsPipeline(&cubePipelineInfo);

	game->chunkVertexBuffer = CreateVertexBuffer(MAX_LOADED_CHUNKS * CHUNK_VERTEX_BUFFER_SIZE, &chunkBufferLayouts[0], nullptr, MAX_LOADED_CHUNKS * CHUNK_VERTEX_BUFFER_SIZE * sizeof(uint32_t), cmdBuffer);
	game->chunkStorageBuffer = CreateStorageBuffer(nullptr, MAX_LOADED_CHUNKS * 6 * sizeof(ChunkData), cmdBuffer);
	game->chunkDrawBuffer = CreateIndirectBuffer(MAX_LOADED_CHUNKS * 6, false);

	InitChunkAllocator(&game->chunkAllocator, game->chunkVertexBuffer, MAX_LOADED_CHUNKS * CHUNK_VERTEX_BUFFER_SIZE);

	InitWorldGenerator(&game->worldGenerator);
	for (int d = 1; d <= CHUNK_LOD_DISTANCE / 2; d++)
	{
		for (int z = -d; z < d; z++)
		{
			for (int y = -d; y < d; y++)
			{
				for (int x = -d; x < d; x++)
				{
					int ax = (int)roundf(fabsf(x + 0.5f) + 0.5f);
					int ay = (int)roundf(fabsf(y + 0.5f) + 0.5f);
					int az = (int)roundf(fabsf(z + 0.5f) + 0.5f);
					if (max(max(ax, ay), az) == d)
					{
						ivec3 position = ivec3(x, y, z) * CHUNK_SIZE;
						Chunk* chunk = InitChunk(position, 0);
						GenerateChunk(&game->worldGenerator, chunk);
					}
				}
			}
		}
	}

	game->cameraPosition = vec3(10, 200, 10);
	game->cameraPitch = -0.4f * PI;
	game->cameraYaw = 0.25f * PI;

	game->mouseLocked = true;
}

void GameDestroy()
{
	for (int i = 0; i < MAX_LOADED_CHUNKS; i++)
	{
		Chunk* chunk = &game->chunks[i];
		if (chunk->isLoaded)
		{
			//DisableChunk(chunk);
			chunk->isLoaded = false;
		}
	}

	DestroyGraphicsPipeline(game->chunkPipeline);
	DestroyShader(game->chunkShader);
}

void GameUpdate()
{
	if (FileHasChanged(PROJECT_PATH "/res/shaders/chunk.vs.glsl") || FileHasChanged(PROJECT_PATH "/res/shaders/chunk.fs.glsl"))
	{
		app->platformCallbacks.compileResources();
		ReloadGraphicsShader(game->chunkShader, "res/shaders/chunk.vs.glsl.bin", "res/shaders/chunk.fs.glsl.bin");
		ReloadGraphicsPipeline(game->chunkPipeline);
	}

	for (int i = 0; i <= game->lastLoadedChunk; i++)
	{
		Chunk* chunk = &game->chunks[i];

	}
	if (game->numLoadedChunks < MAX_LOADED_CHUNKS)
	{
		bool found = false;
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
							int ax = abs(x) + (x >= 0 ? 1 : 0);
							int ay = abs(y) + (y >= 0 ? 1 : 0);
							int az = abs(z) + (z >= 0 ? 1 : 0);
							if (max(max(ax, ay), az) == d)
							{
								ivec3 position = ivec3(x, y, z) * chunkSize;
								int gridIdx = GetChunkGridIdxFromPosition(position, lod);
								SDL_assert(gridIdx != -1);
								Chunk* chunk = game->lods[lod].chunkGrid[gridIdx];
								uint32_t flags = game->lods[lod].chunkFlags[gridIdx];
								if (chunk && chunk->isLoaded)
								{
									if (chunk->isEmpty)
									{
										int gridIdx = GetChunkGridIdxFromPosition(chunk->position, chunk->lod);
										int lod = chunk->lod;
										SDL_assert(gridIdx != -1);
										UnloadChunk(chunk);
										game->lods[lod].chunkFlags[gridIdx] |= CHUNK_FLAG_EMPTY;
									}
									else if (chunk->hasMesh && !chunk->needsUpdate && chunk->getTotalVertexCount() == 0)
									{
										int gridIdx = GetChunkGridIdxFromPosition(chunk->position, chunk->lod);
										int lod = chunk->lod;
										SDL_assert(gridIdx != -1);
										UnloadChunk(chunk);
										game->lods[lod].chunkFlags[gridIdx] |= CHUNK_FLAG_SOLID;
									}
									else if (chunk->needsUpdate)
									{
										ChunkBuilderRun(&game->chunkBuilder, chunk, &game->chunkAllocator, game);
										found = true;
										break; // update only 1 chunk mesh per frame to keep memory usage low
									}
								}
								else if (!flags && !chunk)
								{
									chunk = InitChunk(position, lod);
									GenerateChunk(&game->worldGenerator, chunk);

									Chunk* left = GetChunkAtWorldPosWithLOD(position - ivec3(chunkSize, 0, 0), lod, game);
									Chunk* right = GetChunkAtWorldPosWithLOD(position + ivec3(chunkSize, 0, 0), lod, game);
									Chunk* down = GetChunkAtWorldPosWithLOD(position - ivec3(0, chunkSize, 0), lod, game);
									Chunk* up = GetChunkAtWorldPosWithLOD(position + ivec3(0, chunkSize, 0), lod, game);
									Chunk* forward = GetChunkAtWorldPosWithLOD(position - ivec3(0, 0, chunkSize), lod, game);
									Chunk* back = GetChunkAtWorldPosWithLOD(position + ivec3(0, 0, chunkSize), lod, game);

									if (left && left->hasMesh) left->needsUpdate = true;
									if (right && right->hasMesh) right->needsUpdate = true;
									if (down && down->hasMesh) down->needsUpdate = true;
									if (up && up->hasMesh) up->needsUpdate = true;
									if (forward && forward->hasMesh) forward->needsUpdate = true;
									if (back && back->hasMesh) back->needsUpdate = true;

									found = true;
									break;
								}
							}
						}
						if (found) break;
					}
					if (found) break;
				}
				if (found) break;
			}
			if (found) break;
		}
	}

	vec3 delta = vec3::Zero;
	if (app->keys[SDL_SCANCODE_A]) delta += game->cameraRotation.left();
	if (app->keys[SDL_SCANCODE_D]) delta += game->cameraRotation.right();
	if (app->keys[SDL_SCANCODE_S]) delta += game->cameraRotation.back();
	if (app->keys[SDL_SCANCODE_W]) delta += game->cameraRotation.forward();
	if (app->keys[SDL_SCANCODE_SPACE]) delta += vec3::Up;
	if (app->keys[SDL_SCANCODE_LCTRL]) delta += vec3::Down;

	if (delta.lengthSquared() > 0)
	{
		float speed = app->keys[SDL_SCANCODE_LSHIFT] ? 40.0f : app->keys[SDL_SCANCODE_LALT] ? 5.0f : 10.0f;
		vec3 velocity = delta.normalized() * speed;
		vec3 displacement = velocity * deltaTime;
		game->cameraPosition += displacement;
	}

	if (app->keys[SDL_SCANCODE_ESCAPE] && !app->lastKeys[SDL_SCANCODE_ESCAPE])
		game->mouseLocked = !game->mouseLocked;

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
	vec3 aCenter = a->position + 0.5f * CHUNK_SIZE;
	vec3 bCenter = b->position + 0.5f * CHUNK_SIZE;
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
		if (chunk->isLoaded && chunk->hasMesh)
		{
			AABB aabb = { chunk->position, chunk->position + CHUNK_SIZE * chunk->chunkScale };
			if (FrustumCulling(aabb, frustumPlanes))
			{
				chunkDrawList[numDrawChunks++] = chunk;
			}
		}
	}
	game->numRenderedChunks = numDrawChunks;

	SDL_qsort(chunkDrawList, numDrawChunks, sizeof(Chunk*), (SDL_CompareCallback)ChunkComparator);

	int maxDrawCommands = numDrawChunks * 6;
	SDL_GPUIndirectDrawCommand* drawCommands = (SDL_GPUIndirectDrawCommand*)BumpAllocatorCalloc(&memory->transientAllocator, maxDrawCommands, sizeof(SDL_GPUIndirectDrawCommand));
	ChunkData* chunkStorageData = (ChunkData*)BumpAllocatorCalloc(&memory->transientAllocator, maxDrawCommands, sizeof(ChunkData));

	int numDrawCommands = 0;
	for (int i = 0; i < numDrawChunks; i++)
	{
		Chunk* chunk = chunkDrawList[i];

		int vertexCount = 0;
		for (int i = 0; i < 6; i++)
			vertexCount += chunk->vertexCounts[i];

		if (chunk->isEmpty || vertexCount == 0)
			continue;

		///*
		SDL_GPUIndirectDrawCommand* drawCommand = &drawCommands[numDrawCommands];
		drawCommand->num_vertices = vertexCount;
		drawCommand->num_instances = 1;
		drawCommand->first_vertex = chunk->vertexOffsets[0];
		drawCommand->first_instance = 0;

		ChunkData* storageData = &chunkStorageData[numDrawCommands];
		storageData->position = chunk->position;
		storageData->scale = chunk->chunkScale;

		numDrawCommands++;

		game->numRenderedVertices += vertexCount;
		//*/

		/*
		int chunkScale = CHUNK_SIZE * ipow(2, chunk->lod);
		ivec3 cameraChunk = (ivec3)floor(game->cameraPosition / chunkScale);
		ivec3 dir = chunk->position / chunkScale - cameraChunk;
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
		UpdateIndirectBuffer(game->chunkDrawBuffer, drawCommands, numDrawCommands, cmdBuffer);
		UpdateStorageBuffer(game->chunkStorageBuffer, (const uint8_t*)chunkStorageData, numDrawCommands * sizeof(ChunkData), cmdBuffer);
	}

	return numDrawCommands;
}

void GameRender()
{
	Matrix projection = Matrix::Perspective(60 * Deg2Rad, width / (float)height, 1, 8000);
	Matrix view = Matrix::Rotate(game->cameraRotation.conjugated()) * Matrix::Translate(-game->cameraPosition);
	Matrix pv = projection * view;

	vec4 frustumPlanes[6];
	GetFrustumPlanes(pv, frustumPlanes);

	int numDrawCommands = UpdateDrawBuffers(frustumPlanes);

	SDL_GPUColorTargetInfo colorTarget = {};
	colorTarget.clear_color = { 0.4f, 0.4f, 1.0f, 1.0f };
	colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
	colorTarget.store_op = SDL_GPU_STOREOP_STORE;
	colorTarget.texture = swapchain;

	SDL_GPUDepthStencilTargetInfo depthTarget = {};
	depthTarget.clear_depth = 1;
	depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
	depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;
	depthTarget.texture = app->depthTexture;

	SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdBuffer, &colorTarget, 1, &depthTarget);

	SDL_BindGPUGraphicsPipeline(renderPass, game->chunkPipeline->pipeline);

	SDL_GPUBufferBinding vertexBinding;
	vertexBinding.buffer = game->chunkVertexBuffer->buffer;
	vertexBinding.offset = 0;

	SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

	SDL_GPUBuffer* storageBuffer = game->chunkStorageBuffer->buffer;
	SDL_BindGPUVertexStorageBuffers(renderPass, 0, &storageBuffer, 1);

	ChunkUniforms uniforms = {};
	uniforms.pv = pv;
	//uniforms.chunkPosition = chunk->position;
	//uniforms.chunkSize = ipow(2, chunk->lod);
	SDL_PushGPUVertexUniformData(cmdBuffer, 0, &uniforms, sizeof(uniforms));

	SDL_DrawGPUPrimitivesIndirect(renderPass, game->chunkDrawBuffer->buffer, 0, numDrawCommands);

	SDL_EndGPURenderPass(renderPass);
}
