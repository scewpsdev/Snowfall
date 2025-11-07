#pragma once

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include <stdlib.h>

#include "Resource.h"

#include "utils/BumpAllocator.h"

#include "graphics/VertexBuffer.h"
#include "graphics/IndexBuffer.h"
#include "graphics/InstanceBuffer.h"
#include "graphics/IndirectBuffer.h"
#include "graphics/StorageBuffer.h"
#include "graphics/Shader.h"
#include "graphics/Texture.h"
#include "graphics/RenderTarget.h"
#include "graphics/GraphicsPipeline.h"

#include "renderer/Renderer2D.h"

#include "math/Math.h"
#include "math/Vector.h"
#include "math/Quaternion.h"
#include "math/Matrix.h"

#include "world/WorldGenerator.h"
#include "world/Chunk.h"
#include "world/ChunkMesher.h"
#include "world/ChunkAllocator.h"


#define PROJECT_PATH "D:\\Dev\\Snowfall\\Snowfall"


struct GameMemory
{
	uint64_t constantMemorySize;
	uint8_t* constantMemory;

	uint64_t transientMemorySize;
	uint8_t* transientMemory;

	BumpAllocator constantAllocator;
	BumpAllocator transientAllocator;
};

struct PlatformCallbacks
{
	void (*compileResources)();
};

struct GraphicsState
{
#define MAX_VERTEX_BUFFERS 4096
	VertexBuffer vertexBuffers[MAX_VERTEX_BUFFERS];
	int numVertexBuffers;

#define MAX_INDEX_BUFFERS 1024
	IndexBuffer indexBuffers[MAX_INDEX_BUFFERS];
	int numIndexBuffers;

#define MAX_INSTANCE_BUFFERS 256
	InstanceBuffer instanceBuffers[MAX_INSTANCE_BUFFERS];
	int numInstanceBuffers;

#define MAX_INDIRECT_BUFFERS 16
	IndirectBuffer indirectBuffers[MAX_INDIRECT_BUFFERS];
	int numIndirectBuffers;

#define MAX_STORAGE_BUFFERS 64
	StorageBuffer storageBuffers[MAX_STORAGE_BUFFERS];
	int numStorageBuffers;

#define MAX_SHADERS 256
	Shader shaders[MAX_SHADERS];
	int numShaders;

#define MAX_TEXTURES 1024
	Texture textures[MAX_TEXTURES];
	int numTextures;

#define MAX_RENDER_TARGETS 256
	RenderTarget renderTargets[MAX_RENDER_TARGETS];
	int numRenderTargets;

#define MAX_GRAPHICS_PIPELINES 64
	GraphicsPipeline graphicsPipelines[MAX_GRAPHICS_PIPELINES];
	int numGraphicsPipelines;
};

struct ChunkLODLevel
{
#define CHUNK_LOD_DISTANCE 16
	Chunk* chunkGrid[CHUNK_LOD_DISTANCE * CHUNK_LOD_DISTANCE * CHUNK_LOD_DISTANCE];
#define CHUNK_FLAG_EMPTY (1 << 0)
#define CHUNK_FLAG_SOLID (1 << 1)
	uint8_t chunkFlags[CHUNK_LOD_DISTANCE * CHUNK_LOD_DISTANCE * CHUNK_LOD_DISTANCE];
};

struct GameState
{
	Shader* chunkShader;
	GraphicsPipeline* chunkPipeline;

	bool mouseLocked;
	vec3 cameraPosition;
	float cameraPitch, cameraYaw;
	Quaternion cameraRotation;

	WorldGenerator worldGenerator;
	ChunkMesher chunkBuilder;
	ChunkAllocator chunkAllocator;

	Chunk chunks[MAX_LOADED_CHUNKS];
	int numLoadedChunks;
	int lastLoadedChunk;

#define NUM_CHUNK_LOD_LEVELS 6
	ChunkLODLevel lods[NUM_CHUNK_LOD_LEVELS];

	VertexBuffer* chunkVertexBuffer;
	StorageBuffer* chunkStorageBuffer;
	IndirectBuffer* chunkDrawBuffer;

	int numRenderedChunks;
	int numRenderedVertices;
};

struct AppState
{
	PlatformCallbacks platformCallbacks;

	SDL_Window* window;
	SDL_GPUDevice* device;

	SDL_GPUTexture* depthTexture;

	uint64_t now;
	uint64_t lastFrame;
	uint64_t lastSecond;
	int frameCounter;

	int numKeys;
	const bool* keys;
	bool* lastKeys;

	vec2 mousePosition;
	vec2 lastMousePosition;
	vec2 mouseDelta;
	SDL_MouseButtonFlags mouseButtons;
	SDL_MouseButtonFlags lastMouseButtons;

	GraphicsState graphics;
	ResourceState resourceState;
	GameState game;
};
