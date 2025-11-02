#pragma once

#include "graphics/VertexBuffer.h"


#define MAX_LOADED_CHUNKS 4096


struct ChunkAllocation
{
	int offset;
	int count;

	ChunkAllocation* next;
};

struct ChunkAllocator
{
	int maxVertices;
	VertexBuffer* vertexBuffer;

	int numAllocations;
	ChunkAllocation allocations[MAX_LOADED_CHUNKS];
	ChunkAllocation* first;
};


void InitChunkAllocator(ChunkAllocator* allocator, VertexBuffer* vertexBuffer, int maxVertices);

int AllocateChunk(ChunkAllocator* allocator, int vertexCount);
