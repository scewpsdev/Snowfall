#pragma once

#include "graphics/VertexBuffer.h"
#include "graphics/IndexBuffer.h"

#include "math/Vector.h"


#define CHUNK_SIZE 32
#define CHUNK_VERTEX_BUFFER_SIZE (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * 2 /** 6 * 3 / 2 / 4*/)
#define CHUNK_TEXTURE_WIDTH 16
#define MAX_LOADED_CHUNKS (CHUNK_TEXTURE_WIDTH * CHUNK_TEXTURE_WIDTH * CHUNK_TEXTURE_WIDTH)


enum BlockType : uint8_t
{
	BLOCK_TYPE_NONE = 0,

	BLOCK_TYPE_STONE,
	BLOCK_TYPE_GRASS,
	BLOCK_TYPE_WATER,
};

struct BlockData
{
	uint8_t id;
};

struct ChunkData
{
	ivec3 position;
	int scale;
};

struct Chunk
{
	//BlockData* blocks; // unused until chunk data readback is implemented
	BlockData blocks[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

	int id;
	ivec3 gridPosition;
	int lod;
	int chunkScale;

	bool isActive; // whether this chunk slot is valid
	bool isLoaded; // whether the block data has been loaded (can't be unloaded due to being empty before its been generated)
	bool hasMesh; // whether the mesh has been generated. is still true even if no vertices were created
	bool needsMeshUpdate;
	bool remeshQueued;

	struct ChunkReadbackData* readbackData;
	int vertexCount;
	int blockCount;


	inline ivec3 getWorldPosition() const { return gridPosition * CHUNK_SIZE * chunkScale; }

	inline const BlockData* getBlockData(int x, int y, int z) const { return (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) ? &blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] : nullptr; }
	inline const BlockData* getBlockData(ivec3 position) const { return getBlockData(position.x, position.y, position.z); }

	inline BlockData* getBlockData(int x, int y, int z) { return (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) ? &blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] : nullptr; }
	inline BlockData* getBlockData(ivec3 position) { return getBlockData(position.x, position.y, position.z); }

	inline int getVertexBufferOffset() const { return id * CHUNK_VERTEX_BUFFER_SIZE; }
	inline int getStorageBufferOffset() const { return id * sizeof(ChunkData); }
	inline int getIndirectBufferOffset() const { return id * sizeof(SDL_GPUIndirectDrawCommand); }
	inline ivec3 getChunkTextureOffset() const { return ivec3(id % CHUNK_TEXTURE_WIDTH, id / CHUNK_TEXTURE_WIDTH % CHUNK_TEXTURE_WIDTH, id / CHUNK_TEXTURE_WIDTH / CHUNK_TEXTURE_WIDTH) * CHUNK_SIZE; }
};


const VertexBufferLayout chunkBufferLayouts[] = {
	{
		1,
		{
			{0, SDL_GPU_VERTEXELEMENTFORMAT_UINT}
		},
		false
	}
};
