#include "Chunk.h"

#include "Application.h"
#include "ChunkAllocator.h"

#include "math/Vector.h"


/*
const ivec3 bottomFace[] = {
	{0, 0, 0},
	{1, 0, 0},
	{1, 0, 1},
	{0, 0, 1},
};

const ivec3 topFace[] = {
	{0, 1, 0},
	{0, 1, 1},
	{1, 1, 1},
	{1, 1, 0},
};

const ivec3 leftFace[] = {
	{0, 0, 0},
	{0, 0, 1},
	{0, 1, 1},
	{0, 1, 0},
};

const ivec3 rightFace[] = {
	{1, 0, 0},
	{1, 1, 0},
	{1, 1, 1},
	{1, 0, 1},
};

const ivec3 backFace[] = {
	{0, 0, 0},
	{0, 1, 0},
	{1, 1, 0},
	{1, 0, 0},
};

const ivec3 frontFace[] = {
	{0, 0, 1},
	{1, 0, 1},
	{1, 1, 1},
	{0, 1, 1},
};

const int faceIndices[] = {
	0, 1, 2, 2, 1, 3
};
*/


extern SDL_GPUCommandBuffer* cmdBuffer;


/*
void DisableChunk(Chunk* chunk)
{
	SDL_assert(chunk->isLoaded);
	if (chunk->vertexBuffer)
	{
		//SDL_assert(chunk->indexBuffer);
		DestroyVertexBuffer(chunk->vertexBuffer);
		chunk->vertexBuffer = nullptr;
		//DestroyIndexBuffer(chunk->indexBuffer);
		//chunk->indexBuffer = nullptr;
	}
	chunk->isLoaded = false;
}
*/
