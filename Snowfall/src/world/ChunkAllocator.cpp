#include "ChunkAllocator.h"


void InitChunkAllocator(ChunkAllocator* allocator, VertexBuffer* vertexBuffer, int maxVertices)
{
	allocator->maxVertices = maxVertices;
	allocator->vertexBuffer = vertexBuffer;
}

int AllocateChunk(ChunkAllocator* allocator, int vertexCount)
{
	SDL_assert(allocator->numAllocations < MAX_LOADED_CHUNKS);

	if (!allocator->first)
	{
		ChunkAllocation* allocation = &allocator->allocations[allocator->numAllocations++];
		allocation->offset = 0;
		allocation->count = vertexCount;
		allocator->first = allocation;

		return allocation->offset;
	}
	else
	{
		ChunkAllocation* allocation = allocator->first;
		while (allocation->next)
		{
			int gap = allocation->next->offset - (allocation->offset + allocation->count);
			if (gap >= vertexCount)
			{
				ChunkAllocation* newAllocation = &allocator->allocations[allocator->numAllocations++];
				newAllocation->offset = allocation->offset + allocation->count;
				newAllocation->count = vertexCount;
				newAllocation->next = allocation->next;
				allocation->next = newAllocation;
				return newAllocation->offset;
			}

			allocation = allocation->next;
		}

		ChunkAllocation* newAllocation = &allocator->allocations[allocator->numAllocations++];
		newAllocation->offset = allocation->offset + allocation->count;
		newAllocation->count = vertexCount;
		allocation->next = newAllocation;

		SDL_assert(newAllocation->offset + newAllocation->count <= allocator->maxVertices);

		return newAllocation->offset;
	}
}
