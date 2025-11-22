#pragma once

#include "graphics/VertexBuffer.h"
#include "graphics/IndexBuffer.h"

#include "math/Vector.h"


#define CHUNK_SIZE 32
#define CHUNK_VERTEX_BUFFER_SIZE (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * 2 /** 6 * 3 / 2 / 4*/)
#define CHUNK_TEXTURE_WIDTH 16
#define MAX_LOADED_CHUNKS 2 //(CHUNK_TEXTURE_WIDTH * CHUNK_TEXTURE_WIDTH * CHUNK_TEXTURE_WIDTH)


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
	BlockData blocks[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];
	bool isEmpty;

	int id;
	ivec3 position;
	int lod;
	int chunkScale;

	bool isActive; // whether this chunk slot is valid
	bool isLoaded; // whether the block data has been loaded (can't be unloaded due to being empty before its been generated)
	bool hasMesh; // whether the mesh has been generated. is still true even if no vertices were created
	bool needsMeshUpdate;

	int vertexOffsets[6];
	int vertexCounts[6];

	inline int getTotalVertexCount() const
	{
		return vertexCounts[0] + vertexCounts[1] + vertexCounts[2] + vertexCounts[3] + vertexCounts[4] + vertexCounts[5];
	}

	//int vertexBufferOffset;
	//int numVertices;

	inline const BlockData* getBlockData(int x, int y, int z) const { return (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) ? &blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] : nullptr; }
	inline const BlockData* getBlockData(ivec3 position) const { return getBlockData(position.x, position.y, position.z); }

	inline BlockData* getBlockData(int x, int y, int z) { return (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) ? &blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] : nullptr; }
	inline BlockData* getBlockData(ivec3 position) { return getBlockData(position.x, position.y, position.z); }

	inline int getVertexBufferOffset() const { return id * CHUNK_VERTEX_BUFFER_SIZE; }
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
