#include "ChunkMesher.h"

#include "Application.h"

#include "graphics/VertexBuffer.h"
#include "graphics/IndexBuffer.h"


const static ivec3 vertices[6 * 3] =
{
	// left
	ivec3(0, 2, 0),
	ivec3(0, 0, 0),
	ivec3(0, 0, 2),

	// right
	ivec3(1, 0, 2),
	ivec3(1, 0, 0),
	ivec3(1, 2, 0),

	// down
	ivec3(0, 0, 2),
	ivec3(0, 0, 0),
	ivec3(2, 0, 0),

	// up
	ivec3(2, 1, 0),
	ivec3(0, 1, 0),
	ivec3(0, 1, 2),

	// forward
	ivec3(2, 0, 0),
	ivec3(0, 0, 0),
	ivec3(0, 2, 0),

	// back
	ivec3(0, 2, 1),
	ivec3(0, 0, 1),
	ivec3(2, 0, 1),
};


extern GameMemory* memory;
extern SDL_GPUCommandBuffer* cmdBuffer;


void InitChunkBuilder(ChunkMesher* mesher, int vertexCapacity)
{
	mesher->vertexData = (uint32_t*)BumpAllocatorCalloc(&memory->transientAllocator, vertexCapacity, sizeof(uint32_t));
	mesher->numVertices = 0;
	mesher->vertexCapacity = vertexCapacity;
}

static uint32_t EncodeVertexData(ivec3 position, int sx, int sy, int faceDirection, int colorID)
{
	uint32_t x = position.x;
	uint32_t y = position.y;
	uint32_t z = position.z;

	SDL_assert(x < CHUNK_SIZE && y < CHUNK_SIZE && z < CHUNK_SIZE);
	SDL_assert(sx > 0 && sx <= CHUNK_SIZE && sy > 0 && sy <= CHUNK_SIZE);
	SDL_assert(faceDirection < 6);
	SDL_assert(colorID < 16);

	uint32_t data = 0;
	data |= x;
	data |= (y << 5);
	data |= (z << 10);
	data |= ((sx - 1) << 15);
	data |= ((sy - 1) << 20);
	data |= (faceDirection << 25);
	data |= (colorID << 28);

	return data;
}

/*
void ChunkBuilderAddMesh(ChunkMesher* mesher, int numVertices, const ivec3* vertices, int numIndices, const int* indices, ivec3 position, int faceDirection, int colorID)
{
	SDL_assert(false);

	if (mesher->numVertices + numVertices > mesher->vertexCapacity)
		ResizeVertices(builder, mesher->numVertices + numVertices);
	if (mesher->numIndices + numIndices > mesher->indexCapacity)
		ResizeIndices(builder, mesher->numIndices + numIndices);

	int indexOffset = mesher->numVertices;

	for (int i = 0; i < numVertices; i++)
	{
		//uint32_t data = EncodeVertexData(position/* + vertices[i]*, faceDirection, colorID);
		//mesher->vertexData[mesher->numVertices + i] = data;
	}
	mesher->numVertices += numVertices;

	for (int i = 0; i < numIndices; i++)
		mesher->indexData[mesher->numIndices + i] = indexOffset + indices[i];
	mesher->numIndices += numIndices;
}
*/

static void ChunkBuilderAddFace(ChunkMesher* mesher, ivec3 position, int sx, int sy, int faceDirection, int colorID)
{
	//const int numVertices = 3;
	//const int numIndices = 6;
	//
	//const int indices[] = {
	//	0, 1, 2, 2, 1, 3
	//};

	SDL_assert(mesher->numVertices + 3 <= mesher->vertexCapacity);
	//SDL_assert(mesher->numIndices + numIndices <= mesher->indexCapacity);

	//int indexOffset = mesher->numVertices;

	uint32_t data = EncodeVertexData(position /*+ vertices[faceDirection * 3 + i]*/, sx, sy, faceDirection, colorID);
	mesher->vertexData[mesher->numVertices + 0] = data;
	mesher->vertexData[mesher->numVertices + 1] = data;
	mesher->vertexData[mesher->numVertices + 2] = data;
	mesher->numVertices += 3;

	//for (int i = 0; i < numIndices; i++)
	//	mesher->indexData[mesher->numIndices + i] = indexOffset + indices[i];
	//mesher->numIndices += numIndices;
}

static Chunk* GetChunkAtWorldPos(ivec3 position, GameState* game)
{
	int x = (int)floorf(position.x / (float)CHUNK_SIZE);
	int z = (int)floorf(position.z / (float)CHUNK_SIZE);
	int y = (int)floorf(position.y / (float)CHUNK_SIZE);
	if (x >= -CHUNK_LOD_DISTANCE && x < CHUNK_LOD_DISTANCE && z >= -CHUNK_LOD_DISTANCE && z < CHUNK_LOD_DISTANCE && y == 0)
	{
		Chunk* chunk = game->chunkGrid[(x + CHUNK_LOD_DISTANCE) + (z + CHUNK_LOD_DISTANCE) * (2 * CHUNK_LOD_DISTANCE)];
		SDL_assert(chunk->isLoaded
			&& position.x >= chunk->position.x && position.x < chunk->position.x + CHUNK_SIZE * ipow(2, chunk->lod)
			&& position.y >= chunk->position.y && position.y < chunk->position.y + CHUNK_SIZE * ipow(2, chunk->lod)
			&& position.z >= chunk->position.z && position.z < chunk->position.z + CHUNK_SIZE * ipow(2, chunk->lod));
		return chunk;
	}
	return nullptr;

	for (int i = 0; i <= game->lastLoadedChunk; i++)
	{
		Chunk* chunk = &game->chunks[i];
		if (chunk->isLoaded
			&& position.x >= chunk->position.x && position.x < chunk->position.x + CHUNK_SIZE * ipow(2, chunk->lod)
			&& position.y >= chunk->position.y && position.y < chunk->position.y + CHUNK_SIZE * ipow(2, chunk->lod)
			&& position.z >= chunk->position.z && position.z < chunk->position.z + CHUNK_SIZE * ipow(2, chunk->lod))
		{
			return chunk;
		}
	}
	return nullptr;
}

static BlockData* GetBlockAtWorldPos(ivec3 position, GameState* game)
{
	if (Chunk* chunk = GetChunkAtWorldPos(position, game))
	{
		ivec3 localpos = (position - chunk->position) / ipow(2, chunk->lod);
		return chunk->getBlockData(localpos.x, localpos.y, localpos.z);
	}
	return nullptr;
}

static void GetFaceSize(int x, int y, bool slice[CHUNK_SIZE * CHUNK_SIZE], int* sx, int* sy)
{
	*sx = 1;
	*sy = 1;

	int yy = y + 1;
	while (yy < CHUNK_SIZE)
	{
		bool next = slice[yy + x * CHUNK_SIZE];
		if (next)
		{
			(*sy)++;
			yy++;
		}
		else
		{
			break;
		}
	}

	int xx = x + 1;
	for (int col = xx; col < CHUNK_SIZE; col++)
	{
		bool matches = true;
		for (int i = y; i < yy; i++)
		{
			bool next = slice[i + col * CHUNK_SIZE];
			if (!next)
			{
				matches = false;
				break;
			}
		}

		if (matches)
			(*sx)++;
		else
			break;
	}
}

static uint32_t TrailingZeros(uint32_t value)
{
	unsigned long result = 0;
	if (_BitScanForward(&result, value))
		return result;
	return 32u;
}

static uint32_t TrailingOnes(uint32_t value)
{
	return TrailingZeros(~value);
}

static void GreedyMesh(ChunkMesher* mesher, Chunk* chunk, ChunkAllocator* allocator, GameState* game)
{
#define CHUNK_SIZE_P (CHUNK_SIZE + 2)
	uint64_t binaryGrid[CHUNK_SIZE_P * CHUNK_SIZE_P * 3];

	uint64_t faceMasks[CHUNK_SIZE_P * CHUNK_SIZE_P * 6];

	uint32_t slicesXY[CHUNK_SIZE * CHUNK_SIZE * 2];
	uint32_t slicesZY[CHUNK_SIZE * CHUNK_SIZE * 2];
	uint32_t slicesXZ[CHUNK_SIZE * CHUNK_SIZE * 2];

	SDL_memset(binaryGrid, 0, sizeof(binaryGrid));

	SDL_memset(faceMasks, 0, sizeof(faceMasks));

	SDL_memset(slicesXY, 0, sizeof(slicesXY));
	SDL_memset(slicesZY, 0, sizeof(slicesZY));
	SDL_memset(slicesXZ, 0, sizeof(slicesXZ));

	// build axis bit fields
	for (int z = 0; z < CHUNK_SIZE_P; z++)
	{
		for (int y = 0; y < CHUNK_SIZE_P; y++)
		{
			for (int x = 0; x < CHUNK_SIZE_P; x++)
			{
				BlockData* block = chunk->getBlockData(x - 1, y - 1, z - 1);
				if (!block) block = GetBlockAtWorldPos(chunk->position + ivec3(x - 1, y - 1, z - 1), game);
				if (block && block->id > 0)
				{
					binaryGrid[y * CHUNK_SIZE_P + x + 0 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << z;
					binaryGrid[y * CHUNK_SIZE_P + z + 1 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << x;
					binaryGrid[z * CHUNK_SIZE_P + x + 2 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << y;
				}
			}
		}
	}

	// binary face culling
	for (int i = 0; i < CHUNK_SIZE_P; i++)
	{
		for (int j = 0; j < CHUNK_SIZE_P; j++)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				uint64_t column = binaryGrid[i * CHUNK_SIZE_P + j + axis * CHUNK_SIZE_P * CHUNK_SIZE_P];
				faceMasks[i * CHUNK_SIZE_P + j + (axis * 2 + 0) * CHUNK_SIZE_P * CHUNK_SIZE_P] = column & ~(column << 1); // i- face
				faceMasks[i * CHUNK_SIZE_P + j + (axis * 2 + 1) * CHUNK_SIZE_P * CHUNK_SIZE_P] = column & ~(column >> 1); // i+ face
			}
		}
	}

	// sort face culling data into slices for face generation
	for (int i = 0; i < CHUNK_SIZE; i++)
	{
		for (int j = 0; j < CHUNK_SIZE; j++)
		{
			{ // x- face
				uint64_t column = faceMasks[(i + 1) * CHUNK_SIZE_P + (j + 1) + 2 * CHUNK_SIZE_P * CHUNK_SIZE_P];
				column >>= 1;
				column &= ~(1ull << CHUNK_SIZE);
				SDL_assert(column < 0x100000000);

				while (column > 0)
				{
					int slice = TrailingZeros((uint32_t)column);

					column &= ~(1 << slice);

					slicesZY[slice * CHUNK_SIZE + j] |= 1 << i;
				}
			}
			{ // x+ face
				uint64_t column = faceMasks[(i + 1) * CHUNK_SIZE_P + (j + 1) + 3 * CHUNK_SIZE_P * CHUNK_SIZE_P];
				column >>= 1;
				column &= ~(1ull << CHUNK_SIZE);
				SDL_assert(column < 0x100000000);

				while (column > 0)
				{
					int slice = TrailingZeros((uint32_t)column);

					column &= ~(1 << slice);

					slicesZY[slice * CHUNK_SIZE + j + CHUNK_SIZE * CHUNK_SIZE] |= 1 << i;
				}
			}
			{ // y- face
				uint64_t column = faceMasks[(i + 1) * CHUNK_SIZE_P + (j + 1) + 4 * CHUNK_SIZE_P * CHUNK_SIZE_P];
				column >>= 1;
				column &= ~(1ull << CHUNK_SIZE);
				SDL_assert(column < 0x100000000);

				while (column > 0)
				{
					int slice = TrailingZeros((uint32_t)column);

					column &= ~(1 << slice);

					slicesXZ[slice * CHUNK_SIZE + j] |= 1 << i;
				}
			}
			{ // y+ face
				uint64_t column = faceMasks[(i + 1) * CHUNK_SIZE_P + (j + 1) + 5 * CHUNK_SIZE_P * CHUNK_SIZE_P];
				column >>= 1;
				column &= ~(1ull << CHUNK_SIZE);
				SDL_assert(column < 0x100000000);

				while (column > 0)
				{
					int slice = TrailingZeros((uint32_t)column);

					column &= ~(1 << slice);

					slicesXZ[slice * CHUNK_SIZE + j + CHUNK_SIZE * CHUNK_SIZE] |= 1 << i;
				}
			}
			{ // z- face
				uint64_t column = faceMasks[(i + 1) * CHUNK_SIZE_P + (j + 1) + 0 * CHUNK_SIZE_P * CHUNK_SIZE_P];
				column >>= 1;
				column &= ~(1ull << CHUNK_SIZE);
				SDL_assert(column < 0x100000000);

				while (column > 0)
				{
					int slice = TrailingZeros((uint32_t)column);

					column &= ~(1 << slice);

					slicesXY[slice * CHUNK_SIZE + j] |= 1 << i;
				}
			}
			{ // z+ face
				uint64_t column = faceMasks[(i + 1) * CHUNK_SIZE_P + (j + 1) + 1 * CHUNK_SIZE_P * CHUNK_SIZE_P];
				column >>= 1;
				column &= ~(1ull << CHUNK_SIZE);
				SDL_assert(column < 0x100000000);

				while (column > 0)
				{
					int slice = TrailingZeros((uint32_t)column);

					column &= ~(1 << slice);

					slicesXY[slice * CHUNK_SIZE + j + CHUNK_SIZE * CHUNK_SIZE] |= 1 << i;
				}
			}
		}
	}

	// face generation
	chunk->vertexOffsets[0] = mesher->numVertices;
	for (int i = 0; i < CHUNK_SIZE; i++)
	{
		for (int j = 0; j < CHUNK_SIZE; j++)
		{
			uint32_t column = slicesZY[i * CHUNK_SIZE + j];
			int y = 0;
			while (y < CHUNK_SIZE)
			{
				y += TrailingZeros(column >> y);
				if (y >= CHUNK_SIZE)
					continue;

				uint32_t sy = TrailingOnes(column >> y);

				uint32_t trimmedMask = ((uint32_t)(1ull << (y + sy)) - 1) >> y;
				uint32_t mask = trimmedMask << y;

				int sx = 1;
				while (j + sx < CHUNK_SIZE)
				{
					uint32_t trimmedNextCol = (slicesZY[i * CHUNK_SIZE + j + sx] >> y) & trimmedMask;
					if (trimmedNextCol != trimmedMask)
						break;

					slicesZY[i * CHUNK_SIZE + j + sx] &= ~mask;
					sx++;
				}

				ivec3 position = ivec3(i, y, j);

				ChunkBuilderAddFace(mesher, position, sx, sy, 0, chunk->getBlockData(position)->id);

				y += sy;
			}
		}
	}
	chunk->vertexCounts[0] = mesher->numVertices - chunk->vertexOffsets[0];

	chunk->vertexOffsets[1] = mesher->numVertices;
	for (int i = 0; i < CHUNK_SIZE; i++)
	{
		for (int j = 0; j < CHUNK_SIZE; j++)
		{
			uint32_t column = slicesZY[i * CHUNK_SIZE + j + CHUNK_SIZE * CHUNK_SIZE];
			int y = 0;
			while (y < CHUNK_SIZE)
			{
				y += TrailingZeros(column >> y);
				if (y >= CHUNK_SIZE)
					continue;

				uint32_t sy = TrailingOnes(column >> y);

				uint32_t trimmedMask = ((uint32_t)(1ull << (y + sy)) - 1) >> y;
				uint32_t mask = trimmedMask << y;

				int sx = 1;
				while (j + sx < CHUNK_SIZE)
				{
					uint32_t trimmedNextCol = (slicesZY[i * CHUNK_SIZE + j + sx + CHUNK_SIZE * CHUNK_SIZE] >> y) & trimmedMask;
					if (trimmedNextCol != trimmedMask)
						break;

					slicesZY[i * CHUNK_SIZE + j + sx + CHUNK_SIZE * CHUNK_SIZE] &= ~mask;
					sx++;
				}

				ivec3 position = ivec3(i, y, j);

				ChunkBuilderAddFace(mesher, position, sx, sy, 1, chunk->getBlockData(position)->id);

				y += sy;
			}
		}
	}
	chunk->vertexCounts[1] = mesher->numVertices - chunk->vertexOffsets[1];

	chunk->vertexOffsets[2] = mesher->numVertices;
	for (int i = 0; i < CHUNK_SIZE; i++)
	{
		for (int j = 0; j < CHUNK_SIZE; j++)
		{
			uint32_t column = slicesXZ[i * CHUNK_SIZE + j];
			int y = 0;
			while (y < CHUNK_SIZE)
			{
				y += TrailingZeros(column >> y);
				if (y >= CHUNK_SIZE)
					continue;

				uint32_t sy = TrailingOnes(column >> y);

				uint32_t trimmedMask = ((uint32_t)(1ull << (y + sy)) - 1) >> y;
				uint32_t mask = trimmedMask << y;

				int sx = 1;
				while (j + sx < CHUNK_SIZE)
				{
					uint32_t trimmedNextCol = (slicesXZ[i * CHUNK_SIZE + j + sx] >> y) & trimmedMask;
					if (trimmedNextCol != trimmedMask)
						break;

					slicesXZ[i * CHUNK_SIZE + j + sx] &= ~mask;
					sx++;
				}

				ivec3 position = ivec3(j, i, y);

				ChunkBuilderAddFace(mesher, position, sx, sy, 2, chunk->getBlockData(position)->id);

				y += sy;
			}
		}
	}
	chunk->vertexCounts[2] = mesher->numVertices - chunk->vertexOffsets[2];

	chunk->vertexOffsets[3] = mesher->numVertices;
	for (int i = 0; i < CHUNK_SIZE; i++)
	{
		for (int j = 0; j < CHUNK_SIZE; j++)
		{
			uint32_t column = slicesXZ[i * CHUNK_SIZE + j + CHUNK_SIZE * CHUNK_SIZE];
			int y = 0;
			while (y < CHUNK_SIZE)
			{
				y += TrailingZeros(column >> y);
				if (y >= CHUNK_SIZE)
					continue;

				uint32_t sy = TrailingOnes(column >> y);

				uint32_t trimmedMask = ((uint32_t)(1ull << (y + sy)) - 1) >> y;
				uint32_t mask = trimmedMask << y;

				int sx = 1;
				while (j + sx < CHUNK_SIZE)
				{
					uint32_t trimmedNextCol = (slicesXZ[i * CHUNK_SIZE + j + sx + CHUNK_SIZE * CHUNK_SIZE] >> y) & trimmedMask;
					if (trimmedNextCol != trimmedMask)
						break;

					slicesXZ[i * CHUNK_SIZE + j + sx + CHUNK_SIZE * CHUNK_SIZE] &= ~mask;
					sx++;
				}

				ivec3 position = ivec3(j, i, y);

				ChunkBuilderAddFace(mesher, position, sx, sy, 3, chunk->getBlockData(position)->id);

				y += sy;
			}
		}
	}
	chunk->vertexCounts[3] = mesher->numVertices - chunk->vertexOffsets[3];

	chunk->vertexOffsets[4] = mesher->numVertices;
	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			uint32_t column = slicesXY[z * CHUNK_SIZE + x];
			int y = 0;
			while (y < CHUNK_SIZE)
			{
				y += TrailingZeros(column >> y);
				if (y >= CHUNK_SIZE)
					continue;

				uint32_t sy = TrailingOnes(column >> y);

				uint32_t trimmedMask = ((uint32_t)(1ull << (y + sy)) - 1) >> y;
				uint32_t mask = trimmedMask << y;

				int sx = 1;
				while (x + sx < CHUNK_SIZE)
				{
					uint32_t trimmedNextCol = (slicesXY[z * CHUNK_SIZE + x + sx] >> y) & trimmedMask;
					if (trimmedNextCol != trimmedMask)
						break;

					slicesXY[z * CHUNK_SIZE + x + sx] &= ~mask;
					sx++;
				}

				ivec3 position = ivec3(x, y, z);

				ChunkBuilderAddFace(mesher, position, sx, sy, 4, chunk->getBlockData(position)->id);

				y += sy;
			}
		}
	}
	chunk->vertexCounts[4] = mesher->numVertices - chunk->vertexOffsets[4];

	chunk->vertexOffsets[5] = mesher->numVertices;
	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			uint32_t column = slicesXY[z * CHUNK_SIZE + x + CHUNK_SIZE * CHUNK_SIZE];
			int y = 0;
			while (y < CHUNK_SIZE)
			{
				y += TrailingZeros(column >> y);
				if (y >= CHUNK_SIZE)
					continue;

				uint32_t sy = TrailingOnes(column >> y);

				uint32_t trimmedMask = ((uint32_t)(1ull << (y + sy)) - 1) >> y;
				uint32_t mask = trimmedMask << y;

				int sx = 1;
				while (x + sx < CHUNK_SIZE)
				{
					uint32_t trimmedNextCol = (slicesXY[z * CHUNK_SIZE + x + sx + CHUNK_SIZE * CHUNK_SIZE] >> y) & trimmedMask;
					if (trimmedNextCol != trimmedMask)
						break;

					slicesXY[z * CHUNK_SIZE + x + sx + CHUNK_SIZE * CHUNK_SIZE] &= ~mask;
					sx++;
				}

				ivec3 position = ivec3(x, y, z);

				ChunkBuilderAddFace(mesher, position, sx, sy, 5, chunk->getBlockData(position)->id);

				y += sy;
			}
		}
	}
	chunk->vertexCounts[5] = mesher->numVertices - chunk->vertexOffsets[5];
}

static void BuildGreedyFaces(ChunkMesher* mesher, int faceDirection, Chunk* chunk, ChunkAllocator* allocator, GameState* game)
{
	const int axis = faceDirection / 2;
	const int sgn = faceDirection % 2;

	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		bool sliceBits[CHUNK_SIZE * CHUNK_SIZE];
		SDL_memset(sliceBits, 0, sizeof(sliceBits));

		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				ivec3 position = ivec3(x, y, z);
				ivec3 direction = ivec3(0, 0, 1);

				if (axis == 0)
				{
					position = ivec3(position.z, position.y, position.x);
					direction = ivec3(direction.z, direction.y, direction.x);
				}
				else if (axis == 1)
				{
					position = ivec3(position.x, position.z, position.y);
					direction = ivec3(direction.x, direction.z, direction.y);
				}

				if (sgn == 0)
					direction = -direction;

				BlockData* block = chunk->getBlockData(position);

				if (block->id > 0)
				{
					BlockData* next = chunk->getBlockData(position + direction);
					if (!next) next = GetBlockAtWorldPos(chunk->position + position + direction, game);

					if (!next || !next->id)
					{
						//sliceBits[j] |= 1 << y;
						sliceBits[y + x * CHUNK_SIZE] = true;
					}
				}
			}
		}

		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				if (sliceBits[y + x * CHUNK_SIZE])
				{
					int sx = 0, sy = 0;
					GetFaceSize(x, y, sliceBits, &sx, &sy);

					for (int yy = 0; yy < sy; yy++)
					{
						for (int xx = 0; xx < sx; xx++)
						{
							sliceBits[(y + yy) + (x + xx) * CHUNK_SIZE] = false;
						}
					}

					ivec3 position = ivec3(x, y, z);

					if (axis == 0)
						position = ivec3(position.z, position.y, position.x);
					else if (axis == 1)
						position = ivec3(position.x, position.z, position.y);

					ChunkBuilderAddFace(mesher, position, sx, sy, faceDirection, chunk->getBlockData(position)->id);

					y += sy - 1;
				}
			}
		}
	}
}

void ChunkBuilderRun(ChunkMesher* mesher, Chunk* chunk, ChunkAllocator* allocator, GameState* game)
{
	SDL_assert(chunk->isLoaded);

	/*
	if (chunk->vertexBuffer)
	{
		SDL_assert(chunk->indexBuffer);
		DestroyVertexBuffer(chunk->vertexBuffer);
		chunk->vertexBuffer = nullptr;
		DestroyIndexBuffer(chunk->indexBuffer);
		chunk->indexBuffer = nullptr;
	}
	*/

	InitChunkBuilder(mesher, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * 6 * 4 / 2); // we divide by 2 since in the worst case scenario only every 2nd block is solid

	uint64_t beforeMeshing = SDL_GetTicksNS();
	for (int i = 0; i < 0; i++)
	{
		chunk->vertexOffsets[i] = mesher->numVertices;
		BuildGreedyFaces(mesher, i, chunk, allocator, game);
		chunk->vertexCounts[i] = mesher->numVertices - chunk->vertexOffsets[i];
	}

	GreedyMesh(mesher, chunk, allocator, game);

	uint64_t afterMeshing = SDL_GetTicksNS();
	SDL_Log("meshing %.2f ms", (afterMeshing - beforeMeshing) / 1e6f);

	/*
	for (int i = 0; i < CHUNK_SIZE; i++)
	{
		for (int j = 0; j < CHUNK_SIZE; j++)
		{
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				BlockData* block = chunk->getBlockData(j, y, i);

				if (block->id > 0)
				{
					BlockData* left = chunk->getBlockData(j - 1, y, i);
					BlockData* right = chunk->getBlockData(j + 1, y, i);
					BlockData* down = chunk->getBlockData(j, y - 1, i);
					BlockData* up = chunk->getBlockData(j, y + 1, i);
					BlockData* forward = chunk->getBlockData(j, y, i - 1);
					BlockData* back = chunk->getBlockData(j, y, i + 1);

					if (!left) left = GetBlockAtWorldPos(chunk->position + ivec3(j - 1, y, i), game);
					if (!right) right = GetBlockAtWorldPos(chunk->position + ivec3(j + 1, y, i), game);
					if (!down) down = GetBlockAtWorldPos(chunk->position + ivec3(j, y - 1, i), game);
					if (!up) up = GetBlockAtWorldPos(chunk->position + ivec3(j, y + 1, i), game);
					if (!forward) forward = GetBlockAtWorldPos(chunk->position + ivec3(j, y, i - 1), game);
					if (!back) back = GetBlockAtWorldPos(chunk->position + ivec3(j, y, i + 1), game);

					ivec3 blockPosition(j, y, i);

					//if (!left || !left->id) ChunkBuilderAddMesh(builder, 4, leftFace, 6, faceIndices, blockPosition, 0, block->colorID);
					//if (!right || !right->id) ChunkBuilderAddMesh(builder, 4, rightFace, 6, faceIndices, blockPosition, 1, block->colorID);
					//if (!down || !down->id) ChunkBuilderAddMesh(builder, 4, bottomFace, 6, faceIndices, blockPosition, 2, block->colorID);
					//if (!up || !up->id) ChunkBuilderAddMesh(builder, 4, topFace, 6, faceIndices, blockPosition, 3, block->colorID);
					//if (!forward || !forward->id) ChunkBuilderAddMesh(builder, 4, backFace, 6, faceIndices, blockPosition, 4, block->colorID);
					//if (!back || !back->id) ChunkBuilderAddMesh(builder, 4, frontFace, 6, faceIndices, blockPosition, 5, block->colorID);

					if (!left || !left->id) ChunkBuilderAddFace(builder, blockPosition, 1, 1, 0, block->id);
					if (!right || !right->id) ChunkBuilderAddFace(builder, blockPosition, 1, 1, 1, block->id);
					if (!down || !down->id) ChunkBuilderAddFace(builder, blockPosition, 1, 1, 2, block->id);
					//if (!up || !up->id) ChunkBuilderAddFace(builder, blockPosition, 1, 1, 3, block->id);
					//if (!forward || !forward->id) ChunkBuilderAddFace(builder, blockPosition, 1, 1, 4, block->id);
					//if (!back || !back->id) ChunkBuilderAddFace(builder, blockPosition, 5, block->id);
				}
			}
		}
	}
	*/

	SDL_assert(mesher->numVertices <= CHUNK_VERTEX_BUFFER_SIZE);
	int offset = AllocateChunk(allocator, mesher->numVertices);

	for (int i = 0; i < 6; i++)
		chunk->vertexOffsets[i] += offset;

	//chunk->vertexBufferOffset = offset;
	//chunk->numVertices = mesher->numVertices;

	UpdateVertexBuffer(allocator->vertexBuffer, chunk->vertexOffsets[0] * sizeof(uint32_t), (uint8_t*)mesher->vertexData, mesher->numVertices * sizeof(uint32_t), cmdBuffer);

	//ChunkBuilderCreateBuffers(builder, &chunk->instanceBuffer);
	//ChunkBuilderCreateBuffers(builder, &chunk->vertexBuffer, &chunk->indexBuffer);

	chunk->needsUpdate = false;
	chunk->hasMesh = true;
}

/*
void ChunkBuilderAddFace(ChunkMesher* mesher, ivec3 position, int faceDirection, int colorID)
{
	if (mesher->numVertices + 1 > mesher->vertexCapacity)
		ResizeVertices(builder, mesher->numVertices + 1);

	uint32_t data = EncodeVertexData(position, faceDirection, colorID);
	mesher->vertexData[mesher->numVertices++] = data;
}

void ChunkBuilderCreateBuffers(ChunkMesher* mesher, InstanceBuffer** instanceBuffer)
{
	*instanceBuffer = CreateInstanceBuffer(mesher->numVertices, sizeof(uint32_t), (uint8_t*)mesher->vertexData, mesher->numVertices * sizeof(uint32_t), cmdBuffer);
}
*/

/*
int ChunkBuilderUpdateBuffer(ChunkMesher* mesher, VertexBuffer* vertexBuffer)
{
	UpdateVertexBuffer(vertexBuffer, )

	UpdateVertexBuffer(vertexBuffer, (uint8_t*)mesher->vertexData, mesher->numVertices * sizeof(uint32_t), cmdBuffer);

	//*vertexBuffer = CreateVertexBuffer(mesher->numVertices, chunkBufferLayouts, (uint8_t*)mesher->vertexData, mesher->numVertices * sizeof(uint32_t), cmdBuffer);
	//*indexBuffer = CreateIndexBuffer(mesher->numIndices, SDL_GPU_INDEXELEMENTSIZE_32BIT, (uint8_t*)mesher->indexData, mesher->numIndices * sizeof(int), cmdBuffer);

	return mesher->numVertices;
}
*/
