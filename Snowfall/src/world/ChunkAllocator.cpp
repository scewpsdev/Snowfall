#include "ChunkAllocator.h"


void InitChunkAllocator(ChunkAllocator* allocator, int maxVertices)
{
	allocator->maxVertices = maxVertices;
}

int AllocateChunk(ChunkAllocator* allocator, int vertexCount)
{
	//SDL_assert(allocator->numAllocations < MAX_LOADED_CHUNKS * 2);

	if (!allocator->first)
	{
		ChunkAllocation* allocation = new ChunkAllocation();
		allocation->offset = 0;
		allocation->count = vertexCount;
		allocation->next = nullptr;
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
				ChunkAllocation* newAllocation = new ChunkAllocation();
				newAllocation->offset = allocation->offset + allocation->count;
				newAllocation->count = vertexCount;
				newAllocation->next = allocation->next;
				allocation->next = newAllocation;
				return newAllocation->offset;
			}

			allocation = allocation->next;
		}

		ChunkAllocation* newAllocation = new ChunkAllocation();
		newAllocation->offset = allocation->offset + allocation->count;
		newAllocation->count = vertexCount;
		allocation->next = newAllocation;

		SDL_assert(newAllocation->offset + newAllocation->count <= allocator->maxVertices);

		return newAllocation->offset;
	}
}

void DeallocateChunk(ChunkAllocator* allocator, int offset, int count)
{
	ChunkAllocation* allocation = allocator->first;
	SDL_assert(allocation);
	while (allocation->next)
	{
		if (allocation->next->offset == offset)
		{
			SDL_assert(allocation->next->count == count);
			ChunkAllocation* alloc = allocation->next;
			allocation->next = alloc->next;
			delete alloc;
			return;

			// TODO deallocate chunk on deload
			// also how do we remove the chunk allocation data from the array without looping through it?
			// because if we do that whats the point of the linked list. idk how to solve this efficiently.
			// i dont want to heap alloc the allocation data
		}
		allocation = allocation->next;
	}
	SDL_assert(false);
}
