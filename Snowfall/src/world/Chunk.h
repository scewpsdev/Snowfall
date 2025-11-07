#pragma once

#include "graphics/VertexBuffer.h"
#include "graphics/IndexBuffer.h"

#include "math/Vector.h"


#define CHUNK_SIZE 32
#define CHUNK_VERTEX_BUFFER_SIZE (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE /** 6 * 3 / 2 / 4*/)


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

	int id;
	ivec3 position;
	int lod;
	int chunkScale;

	bool isLoaded;
	bool hasMesh;
	bool isEmpty;
	//VertexBuffer* vertexBuffer;
	//IndexBuffer* indexBuffer;
	bool needsUpdate;

	int vertexOffsets[6];
	int vertexCounts[6];

	inline int getTotalVertexCount() const
	{
		return vertexCounts[0] + vertexCounts[1] + vertexCounts[2] + vertexCounts[3] + vertexCounts[4] + vertexCounts[5];
	}

	//int vertexBufferOffset;
	//int numVertices;

	inline BlockData* getBlockData(int x, int y, int z) { return (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) ? &blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] : nullptr; }
	inline BlockData* getBlockData(ivec3 position) { return getBlockData(position.x, position.y, position.z); }
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
