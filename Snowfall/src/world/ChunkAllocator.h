#pragma once

#include "graphics/VertexBuffer.h"


#define MAX_LOADED_CHUNKS 8192


struct ChunkAllocation
{
	int offset;
	int count;

	ChunkAllocation* next;
};

struct ChunkAllocator
{
	int maxVertices;

	ChunkAllocation* first;
};


void InitChunkAllocator(ChunkAllocator* allocator, int maxVertices);

int AllocateChunk(ChunkAllocator* allocator, int vertexCount);
void DeallocateChunk(ChunkAllocator* allocator, int offset, int count);
