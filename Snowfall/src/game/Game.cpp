#include "graphics/VertexBuffer.h"

#include "math/Vector.h"


#define CHUNK_LOD_DISTANCE 16


struct ChunkUniforms
{
	Matrix pv;
};


static void InitChunk(Chunk* chunk, int id, const ivec3& position, int lod)
{
	chunk->id = id;
	chunk->position = position;
	chunk->lod = lod;
	chunk->isLoaded = true;
	if (chunk->id > game->lastLoadedChunk)
		game->lastLoadedChunk = chunk->id;
}

static Chunk* GetAvailableChunk(int* chunkID)
{
	for (int i = 0; i < MAX_LOADED_CHUNKS; i++)
	{
		Chunk* chunk = &game->chunks[i];
		if (!chunk->isLoaded)
		{
			*chunkID = i;
			return chunk;
		}
	}
	return nullptr;
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
	for (int z = -CHUNK_LOD_DISTANCE; z < CHUNK_LOD_DISTANCE; z++)
	{
		for (int x = -CHUNK_LOD_DISTANCE; x < CHUNK_LOD_DISTANCE; x++)
		{
			int y = 0;
			int chunkID;
			Chunk* chunk = GetAvailableChunk(&chunkID);
			ivec3 position = ivec3(x, y, z) * CHUNK_SIZE;
			InitChunk(chunk, chunkID, position, 0);
			GenerateChunk(&game->worldGenerator, chunk);
		}
	}
	/*
	for (int z = -CHUNK_LOD_DISTANCE; z < CHUNK_LOD_DISTANCE; z++)
	{
		for (int x = -CHUNK_LOD_DISTANCE; x < CHUNK_LOD_DISTANCE; x++)
		{
			int y = 0;

			int lod = 1;
			int chunkSize = ipow(2, lod);

			int xx = x * chunkSize;
			int yy = y * chunkSize;
			int zz = z * chunkSize;

			if (!(xx >= -CHUNK_LOD_DISTANCE && xx < CHUNK_LOD_DISTANCE && zz >= -CHUNK_LOD_DISTANCE && zz < CHUNK_LOD_DISTANCE))
			{
				int chunkID;
				Chunk* chunk = GetAvailableChunk(&chunkID);
				ivec3 position = ivec3(xx, yy, zz) * CHUNK_SIZE;
				InitChunk(chunk, chunkID, position, lod);
				GenerateChunk(&game->worldGenerator, chunk);
			}
		}
	}
	for (int z = -CHUNK_LOD_DISTANCE; z < CHUNK_LOD_DISTANCE; z++)
	{
		for (int x = -CHUNK_LOD_DISTANCE; x < CHUNK_LOD_DISTANCE; x++)
		{
			int y = 0;

			int lod = 2;
			int chunkSize = ipow(2, lod);

			int xx = x * chunkSize;
			int yy = y * chunkSize;
			int zz = z * chunkSize;

			if (!(xx >= -CHUNK_LOD_DISTANCE * 2 && xx < CHUNK_LOD_DISTANCE * 2 && zz >= -CHUNK_LOD_DISTANCE * 2 && zz < CHUNK_LOD_DISTANCE * 2))
			{
				int chunkID;
				Chunk* chunk = GetAvailableChunk(&chunkID);
				ivec3 position = ivec3(xx, yy, zz) * CHUNK_SIZE;
				InitChunk(chunk, chunkID, position, lod);
				GenerateChunk(&game->worldGenerator, chunk);
			}
		}
	}
	*/

	/*
	for (int i = 0; i < MAX_LOADED_CHUNKS; i++)
	{
		Chunk* chunk = &game->chunks[i];
		int x = i % 32 - 16;
		int z = i / 32 - 16;
		int y = 0;
		ivec3 position = ivec3(x, y, z) * CHUNK_SIZE;
		InitChunk(chunk, position);
		GenerateChunk(&game->worldGenerator, chunk);
	}
	*/

	game->cameraPosition = vec3(40, 150, 40);
	game->cameraPitch = -0.25f * PI;
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


	for (int i = 0; i < game->lastLoadedChunk; i++)
	{
		Chunk* chunk = &game->chunks[i];
		if (chunk->isLoaded && chunk->needsUpdate)
		{
			ChunkBuilderRun(&game->chunkBuilder, chunk, &game->chunkAllocator, game);
			break; // update only 1 chunk mesh per frame to keep memory usage low
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
	game->numLoadedChunks = 0;
	game->numRenderedChunks = 0;
	game->numRenderedVertices = 0;

	Chunk** chunkDrawList = (Chunk**)BumpAllocatorCalloc(&memory->transientAllocator, game->lastLoadedChunk + 1, sizeof(Chunk*));
	int numDrawChunks = 0;

	for (int i = 0; i < game->lastLoadedChunk; i++)
	{
		Chunk* chunk = &game->chunks[i];
		if (chunk->isLoaded && chunk->hasMesh)
		{
			AABB aabb = { chunk->position, chunk->position + CHUNK_SIZE * ipow(2, chunk->lod) };
			if (FrustumCulling(aabb, frustumPlanes))
			{
				chunkDrawList[numDrawChunks++] = chunk;
			}

			game->numLoadedChunks++;
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

		int vertexOffset = chunk->vertexOffsets[0];
		int vertexCount = 0;
		for (int j = 0; j < 6; j++)
			vertexCount += chunk->vertexCounts[j];

		SDL_GPUIndirectDrawCommand* drawCommand = &drawCommands[numDrawCommands];
		drawCommand->num_vertices = vertexCount;
		drawCommand->num_instances = 1;
		drawCommand->first_vertex = vertexOffset;
		drawCommand->first_instance = 0;

		ChunkData* storageData = &chunkStorageData[numDrawCommands];
		storageData->position = chunk->position;
		storageData->scale = ipow(2, chunk->lod);

		numDrawCommands++;

		game->numRenderedVertices += vertexCount;

		/*
		ivec3 cameraChunk = (ivec3)floor(game->cameraPosition / (CHUNK_SIZE * ipow(2, chunk->lod)));
		ivec3 dir = chunk->position / (CHUNK_SIZE * ipow(2, chunk->lod)) - cameraChunk;
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

		/*
		if (chunk->isLoaded && chunk->instanceBuffer)
		{
			SDL_GPUBufferBinding vertexBindings[1];
			vertexBindings[0].buffer = chunk->instanceBuffer->buffer;
			vertexBindings[0].offset = 0;

			SDL_BindGPUVertexBuffers(renderPass, 0, vertexBindings, 1);

			ChunkUniforms uniforms = {};
			uniforms.projection = Matrix::Perspective(60 * Deg2Rad, width / (float)height, 0.1f, 1000);
			uniforms.view = Matrix::Rotate(game->cameraRotation.conjugated()) * Matrix::Translate(-game->cameraPosition);
			uniforms.chunkPosition = chunk->position;
			SDL_PushGPUVertexUniformData(cmdBuffer, 0, &uniforms, sizeof(uniforms));

			SDL_DrawGPUPrimitives(renderPass, 4, chunk->instanceBuffer->numInstances, 0, 0);
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
	Matrix projection = Matrix::Perspective(60 * Deg2Rad, width / (float)height, 1, 2000);
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
