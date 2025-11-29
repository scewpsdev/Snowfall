#pragma once

#include "Chunk.h"

//#include "graphics/InstanceBuffer.h"
#include "graphics/VertexBuffer.h"
#include "graphics/IndexBuffer.h"
#include "graphics/Shader.h"

#include "math/Vector.h"

#include "utils/HashMap.h"


struct GreedyPlane
{
	uint32_t slicesXY[CHUNK_SIZE * CHUNK_SIZE * 2] = {};
	uint32_t slicesZY[CHUNK_SIZE * CHUNK_SIZE * 2] = {};
	uint32_t slicesXZ[CHUNK_SIZE * CHUNK_SIZE * 2] = {};
};

struct ChunkMesher
{
#define CHUNK_SIZE_P (CHUNK_SIZE + 2)

	uint64_t binaryGrid[CHUNK_SIZE_P * CHUNK_SIZE_P * 3];

	uint64_t faceMasks[CHUNK_SIZE_P * CHUNK_SIZE_P * 6];

#define MAX_BLOCK_TYPES 32
	HashMap<uint8_t, GreedyPlane, MAX_BLOCK_TYPES> greedyPlanes;

	//uint32_t slicesXY[CHUNK_SIZE * CHUNK_SIZE * 2 * 256];
	//uint32_t slicesZY[CHUNK_SIZE * CHUNK_SIZE * 2 * 256];
	//uint32_t slicesXZ[CHUNK_SIZE * CHUNK_SIZE * 2 * 256];

#define CHUNK_MESHER_VERTEX_CAPACITY (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * 6 * 4 / 2)
	uint32_t vertexData[CHUNK_MESHER_VERTEX_CAPACITY];
	//int vertexCapacity;

	int vertexCount;
	int blockCount;

	Shader* faceDetectShader;
	Shader* vertexGenShader;
	Shader* clearBufferShader;

	SDL_GPUSampler* sampler;
};


void InitChunkMesher(ChunkMesher* mesher);

//void ChunkBuilderAddMesh(ChunkMesher* mesher, int numVertices, const ivec3* vertices, int numIndices, const int* indices, ivec3 position, int faceDirection, int colorID);
//void ChunkBuilderAddFace(ChunkMesher* mesher, ivec3 position, int faceDirection, int colorID);
//void ChunkBuilderCreateBuffers(ChunkMesher* mesher, InstanceBuffer** instanceBuffer);
//void ChunkBuilderCreateBuffers(ChunkMesher* mesher, VertexBuffer** vertexBuffer, IndexBuffer** indexBuffer);

void ChunkMesherRunGPU(ChunkMesher* mesher, struct ChunkGeneratorThreadData* threadData, Chunk* chunk, Chunk** neighbors, uint32_t* neighborFlags, SDL_GPUTransferBuffer* readbackBuffer, SDL_GPUCommandBuffer* cmdBuffer);
void ChunkMesherRun(ChunkMesher* mesher, ChunkGeneratorThreadData* threadData, Chunk* chunk, Chunk** neighbors, uint32_t* neighborFlags);
