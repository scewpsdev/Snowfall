#pragma once

//#include "graphics/InstanceBuffer.h"
#include "graphics/VertexBuffer.h"
#include "graphics/IndexBuffer.h"

#include "math/Vector.h"


struct ChunkMesher
{
	uint32_t* vertexData;
	int numVertices;
	int vertexCapacity;
};


void InitChunkBuilder(ChunkMesher* mesher, int vertexCapacity);

//void ChunkBuilderAddMesh(ChunkMesher* mesher, int numVertices, const ivec3* vertices, int numIndices, const int* indices, ivec3 position, int faceDirection, int colorID);
//void ChunkBuilderAddFace(ChunkMesher* mesher, ivec3 position, int faceDirection, int colorID);
//void ChunkBuilderCreateBuffers(ChunkMesher* mesher, InstanceBuffer** instanceBuffer);
//void ChunkBuilderCreateBuffers(ChunkMesher* mesher, VertexBuffer** vertexBuffer, IndexBuffer** indexBuffer);

void ChunkBuilderRun(ChunkMesher* mesher, struct Chunk* chunk, struct ChunkAllocator* allocator, struct GameState* game);
