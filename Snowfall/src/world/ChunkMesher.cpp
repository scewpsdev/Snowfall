#include "ChunkMesher.h"

#include "Application.h"

#include "game/Game.h"

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


void InitChunkMesher(ChunkMesher* mesher)
{
	//mesher->vertexData = (uint32_t*)BumpAllocatorCalloc(&memory->transientAllocator, vertexCapacity, sizeof(uint32_t));
	//mesher->numVertices = 0;
	//mesher->vertexCapacity = vertexCapacity;
}

static uint32_t EncodeVertexData(ivec3 position, int sx, int sy, int faceDirection, int colorID)
{
	uint32_t x = position.x;
	uint32_t y = position.y;
	uint32_t z = position.z;

	SDL_assert(x < CHUNK_SIZE && y < CHUNK_SIZE && z < CHUNK_SIZE);
	SDL_assert(sx > 0 && sx <= CHUNK_SIZE && sy > 0 && sy <= CHUNK_SIZE);
	SDL_assert(faceDirection < 6);
	SDL_assert(colorID >= 0 && colorID < 16);

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

static void ChunkBuilderAddFace(ChunkMesher* mesher, ivec3 position, int sx, int sy, int faceDirection, uint8_t blockType)
{
	SDL_assert(mesher->numVertices + 3 <= CHUNK_MESHER_VERTEX_CAPACITY);

	int colorID = blockType - 1;
	uint32_t data = EncodeVertexData(position /*+ vertices[faceDirection * 3 + i]*/, sx, sy, faceDirection, colorID);

	mesher->vertexData[mesher->numVertices + 0] = data;
	mesher->vertexData[mesher->numVertices + 1] = data;
	mesher->vertexData[mesher->numVertices + 2] = data;
	mesher->numVertices += 3;
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
	//unsigned long result = 0;
	//if (_BitScanForward(&result, value))
	//	return result;
	//return 32u;

	// https://stackoverflow.com/questions/7812044/finding-trailing-0s-in-a-binary-number
	unsigned int v = value;      // 32-bit word input to count zero bits on right
	unsigned int c = 32; // c will be the number of zero bits on the right
	v &= -(signed int)v;
	if (v) c--;
	if (v & 0x0000FFFF) c -= 16;
	if (v & 0x00FF00FF) c -= 8;
	if (v & 0x0F0F0F0F) c -= 4;
	if (v & 0x33333333) c -= 2;
	if (v & 0x55555555) c -= 1;
	return c;
}

static uint32_t TrailingOnes(uint32_t value)
{
	return TrailingZeros(~value);
}

static void GreedyMesh(ChunkMesher* mesher, const Chunk* chunk, const Chunk* neighbors[6], uint32_t neighborFlags[6], GameState* game)
{
	uint64_t* binaryGrid = mesher->binaryGrid;

	uint64_t* faceMasks = mesher->faceMasks;

	//uint32_t* slicesXY = mesher->slicesXY;
	//uint32_t* slicesZY = mesher->slicesZY;
	//uint32_t* slicesXZ = mesher->slicesXZ;

	SDL_memset(binaryGrid, 0, sizeof(mesher->binaryGrid));

	SDL_memset(faceMasks, 0, sizeof(mesher->faceMasks));

	//SDL_memset(slicesXY, 0, sizeof(mesher->slicesXY));
	//SDL_memset(slicesZY, 0, sizeof(mesher->slicesZY));
	//SDL_memset(slicesXZ, 0, sizeof(mesher->slicesXZ));

	// build axis bit fields
	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				bool solid = chunk->blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE].id;
				if (solid)
				{
					binaryGrid[(y + 1) * CHUNK_SIZE_P + (x + 1) + 0 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (z + 1);
					binaryGrid[(y + 1) * CHUNK_SIZE_P + (z + 1) + 1 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (x + 1);
					binaryGrid[(z + 1) * CHUNK_SIZE_P + (x + 1) + 2 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (y + 1);
				}
			}
		}
	}
	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			{
				bool solid = neighborFlags[0] ? neighborFlags[0] & CHUNK_FLAG_SOLID : neighbors[0] && neighbors[0]->blocks[(CHUNK_SIZE - 1) + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE].id;
				if (solid)
				{
					binaryGrid[(y + 1) * CHUNK_SIZE_P + 0 + 0 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (z + 1);
					binaryGrid[(y + 1) * CHUNK_SIZE_P + (z + 1) + 1 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << 0;
					binaryGrid[(z + 1) * CHUNK_SIZE_P + 0 + 2 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (y + 1);
				}
			}
			{
				bool solid = neighborFlags[1] ? neighborFlags[1] & CHUNK_FLAG_SOLID : neighbors[1] && neighbors[1]->blocks[(0) + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE].id;
				if (solid)
				{
					binaryGrid[(y + 1) * CHUNK_SIZE_P + (CHUNK_SIZE + 1) + 0 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (z + 1);
					binaryGrid[(y + 1) * CHUNK_SIZE_P + (z + 1) + 1 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (CHUNK_SIZE + 1);
					binaryGrid[(z + 1) * CHUNK_SIZE_P + (CHUNK_SIZE + 1) + 2 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (y + 1);
				}
			}
		}
	}
	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			{
				bool solid = neighborFlags[2] ? neighborFlags[2] & CHUNK_FLAG_SOLID : neighbors[2] && neighbors[2]->blocks[x + (CHUNK_SIZE - 1) * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE].id;
				if (solid)
				{
					binaryGrid[(0) * CHUNK_SIZE_P + (x + 1) + 0 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (z + 1);
					binaryGrid[(0) * CHUNK_SIZE_P + (z + 1) + 1 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (x + 1);
					binaryGrid[(z + 1) * CHUNK_SIZE_P + (x + 1) + 2 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (0);
				}
			}
			{
				bool solid = neighborFlags[3] ? neighborFlags[3] & CHUNK_FLAG_SOLID : neighbors[3] && neighbors[3]->blocks[x + (0) * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE].id;
				if (solid)
				{
					binaryGrid[(CHUNK_SIZE + 1) * CHUNK_SIZE_P + (x + 1) + 0 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (z + 1);
					binaryGrid[(CHUNK_SIZE + 1) * CHUNK_SIZE_P + (z + 1) + 1 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (x + 1);
					binaryGrid[(z + 1) * CHUNK_SIZE_P + (x + 1) + 2 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (CHUNK_SIZE + 1);
				}
			}
		}
	}
	for (int y = 0; y < CHUNK_SIZE; y++)
	{
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			{
				bool solid = neighborFlags[4] ? neighborFlags[4] & CHUNK_FLAG_SOLID : neighbors[4] && neighbors[4]->blocks[x + y * CHUNK_SIZE + (CHUNK_SIZE - 1) * CHUNK_SIZE * CHUNK_SIZE].id;
				if (solid)
				{
					binaryGrid[(y + 1) * CHUNK_SIZE_P + (x + 1) + 0 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (0);
					binaryGrid[(y + 1) * CHUNK_SIZE_P + (0) + 1 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (x + 1);
					binaryGrid[(0) * CHUNK_SIZE_P + (x + 1) + 2 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (y + 1);
				}
			}
			{
				bool solid = neighborFlags[5] ? neighborFlags[5] & CHUNK_FLAG_SOLID : neighbors[5] && neighbors[5]->blocks[x + y * CHUNK_SIZE + 0 * CHUNK_SIZE * CHUNK_SIZE].id;
				if (solid)
				{
					binaryGrid[(y + 1) * CHUNK_SIZE_P + (x + 1) + 0 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (CHUNK_SIZE + 1);
					binaryGrid[(y + 1) * CHUNK_SIZE_P + (CHUNK_SIZE + 1) + 1 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (x + 1);
					binaryGrid[(CHUNK_SIZE + 1) * CHUNK_SIZE_P + (x + 1) + 2 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << (y + 1);
				}
			}
		}
	}

	/*
	for (int z = 0; z < CHUNK_SIZE_P; z++)
	{
		int zneighbor = z == 0 ? -1 : z == CHUNK_SIZE_P - 1 ? 1 : 0;
		for (int y = 0; y < CHUNK_SIZE_P; y++)
		{
			int yneighbor = y == 0 ? -1 : y == CHUNK_SIZE_P - 1 ? 1 : 0;
			for (int x = 0; x < CHUNK_SIZE_P; x++)
			{
				int xneighbor = x == 0 ? -1 : x == CHUNK_SIZE_P - 1 ? 1 : 0;

				if ((xneighbor != 0) + (yneighbor != 0) + (zneighbor != 0) > 1)
					continue;

				bool solid;
				if (x == 0) solid = neighborFlags[0] ? neighborFlags[0] & CHUNK_FLAG_SOLID : neighbors[0] && neighbors[0]->getBlockData(CHUNK_SIZE - 1, y - 1, z - 1)->id;
				else if (x == CHUNK_SIZE_P - 1) solid = neighborFlags[1] ? neighborFlags[1] & CHUNK_FLAG_SOLID : neighbors[1] && neighbors[1]->getBlockData(0, y - 1, z - 1)->id;
				else if (y == 0) solid = neighborFlags[2] ? neighborFlags[2] & CHUNK_FLAG_SOLID : neighbors[2] && neighbors[2]->getBlockData(x - 1, CHUNK_SIZE - 1, z - 1)->id;
				else if (y == CHUNK_SIZE_P - 1) solid = neighborFlags[3] ? neighborFlags[3] & CHUNK_FLAG_SOLID : neighbors[3] && neighbors[3]->getBlockData(x - 1, 0, z - 1)->id;
				else if (z == 0) solid = neighborFlags[4] ? neighborFlags[4] & CHUNK_FLAG_SOLID : neighbors[4] && neighbors[4]->getBlockData(x - 1, y - 1, CHUNK_SIZE - 1)->id;
				else if (z == CHUNK_SIZE_P - 1) solid = neighborFlags[5] ? neighborFlags[5] & CHUNK_FLAG_SOLID : neighbors[5] && neighbors[5]->getBlockData(x - 1, y - 1, 0)->id;
				else solid = chunk->getBlockData(x - 1, y - 1, z - 1)->id;
				//if (!block) solid = GetSolidAtWorldPos(chunk->position + ivec3(x - 1, y - 1, z - 1) * chunk->chunkScale, chunk->lod, game);
				if (solid)
				{
					binaryGrid[y * CHUNK_SIZE_P + x + 0 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << z;
					binaryGrid[y * CHUNK_SIZE_P + z + 1 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << x;
					binaryGrid[z * CHUNK_SIZE_P + x + 2 * CHUNK_SIZE_P * CHUNK_SIZE_P] |= 1ull << y;
				}
			}
		}
	}
	*/

	// binary face culling
	for (int axis = 0; axis < 3; axis++)
	{
		for (int i = 0; i < CHUNK_SIZE_P; i++)
		{
			for (int j = 0; j < CHUNK_SIZE_P; j++)
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

					const BlockData* block = chunk->getBlockData(slice, i, j);

					mesher->greedyPlanes[block->id].slicesZY[slice * CHUNK_SIZE + j] |= 1 << i;
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

					const BlockData* block = chunk->getBlockData(slice, i, j);

					mesher->greedyPlanes[block->id].slicesZY[CHUNK_SIZE * CHUNK_SIZE + slice * CHUNK_SIZE + j] |= 1 << i;
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

					const BlockData* block = chunk->getBlockData(j, slice, i);

					mesher->greedyPlanes[block->id].slicesXZ[slice * CHUNK_SIZE + j] |= 1 << i;
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

					const BlockData* block = chunk->getBlockData(j, slice, i);

					mesher->greedyPlanes[block->id].slicesXZ[slice * CHUNK_SIZE + j + CHUNK_SIZE * CHUNK_SIZE] |= 1 << i;
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

					const BlockData* block = chunk->getBlockData(j, i, slice);

					mesher->greedyPlanes[block->id].slicesXY[slice * CHUNK_SIZE + j] |= 1 << i;
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

					const BlockData* block = chunk->getBlockData(j, i, slice);

					mesher->greedyPlanes[block->id].slicesXY[slice * CHUNK_SIZE + j + CHUNK_SIZE * CHUNK_SIZE] |= 1 << i;
				}
			}
		}
	}

	// face generation
	mesher->vertexOffsets[0] = mesher->numVertices;
	for (int i = 0; i < mesher->greedyPlanes.capacity; i++)
	{
		auto slot = &mesher->greedyPlanes.slots[i];
		if (slot->state == SLOT_USED)
		{
			uint8_t blockType = slot->key;
			GreedyPlane* greedyPlane = &slot->value;

			for (int i = 0; i < CHUNK_SIZE; i++)
			{
				for (int j = 0; j < CHUNK_SIZE; j++)
				{
					uint32_t column = greedyPlane->slicesZY[i * CHUNK_SIZE + j];

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
							uint32_t trimmedNextCol = (greedyPlane->slicesZY[i * CHUNK_SIZE + j + sx] >> y) & trimmedMask;
							if (trimmedNextCol != trimmedMask)
								break;

							greedyPlane->slicesZY[i * CHUNK_SIZE + j + sx] &= ~mask;
							sx++;
						}

						ivec3 position = ivec3(i, y, j);

						ChunkBuilderAddFace(mesher, position, sx, sy, 0, (uint8_t)blockType);

						y += sy;
					}
				}
			}
		}
	}
	mesher->vertexCounts[0] = mesher->numVertices - mesher->vertexOffsets[0];

	mesher->vertexOffsets[1] = mesher->numVertices;
	for (int i = 0; i < mesher->greedyPlanes.capacity; i++)
	{
		auto slot = &mesher->greedyPlanes.slots[i];
		if (slot->state == SLOT_USED)
		{
			uint8_t blockType = slot->key;
			GreedyPlane* greedyPlane = &slot->value;

			for (int i = 0; i < CHUNK_SIZE; i++)
			{
				for (int j = 0; j < CHUNK_SIZE; j++)
				{
					uint32_t column = greedyPlane->slicesZY[i * CHUNK_SIZE + j + CHUNK_SIZE * CHUNK_SIZE];

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
							uint32_t trimmedNextCol = (greedyPlane->slicesZY[i * CHUNK_SIZE + j + sx + CHUNK_SIZE * CHUNK_SIZE] >> y) & trimmedMask;
							if (trimmedNextCol != trimmedMask)
								break;

							greedyPlane->slicesZY[i * CHUNK_SIZE + j + sx + CHUNK_SIZE * CHUNK_SIZE] &= ~mask;
							sx++;
						}

						ivec3 position = ivec3(i, y, j);

						ChunkBuilderAddFace(mesher, position, sx, sy, 1, (uint8_t)blockType);

						y += sy;
					}
				}
			}
		}
	}
	mesher->vertexCounts[1] = mesher->numVertices - mesher->vertexOffsets[1];

	mesher->vertexOffsets[2] = mesher->numVertices;
	for (int i = 0; i < mesher->greedyPlanes.capacity; i++)
	{
		auto slot = &mesher->greedyPlanes.slots[i];
		if (slot->state == SLOT_USED)
		{
			uint8_t blockType = slot->key;
			GreedyPlane* greedyPlane = &slot->value;

			for (int i = 0; i < CHUNK_SIZE; i++)
			{
				for (int j = 0; j < CHUNK_SIZE; j++)
				{
					uint32_t column = greedyPlane->slicesXZ[i * CHUNK_SIZE + j];

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
							uint32_t trimmedNextCol = (greedyPlane->slicesXZ[i * CHUNK_SIZE + j + sx] >> y) & trimmedMask;
							if (trimmedNextCol != trimmedMask)
								break;

							greedyPlane->slicesXZ[i * CHUNK_SIZE + j + sx] &= ~mask;
							sx++;
						}

						ivec3 position = ivec3(j, i, y);

						ChunkBuilderAddFace(mesher, position, sx, sy, 2, (uint8_t)blockType);

						y += sy;
					}
				}
			}
		}
	}
	mesher->vertexCounts[2] = mesher->numVertices - mesher->vertexOffsets[2];

	mesher->vertexOffsets[3] = mesher->numVertices;
	for (int i = 0; i < mesher->greedyPlanes.capacity; i++)
	{
		auto slot = &mesher->greedyPlanes.slots[i];
		if (slot->state == SLOT_USED)
		{
			uint8_t blockType = slot->key;
			GreedyPlane* greedyPlane = &slot->value;

			for (int i = 0; i < CHUNK_SIZE; i++)
			{
				for (int j = 0; j < CHUNK_SIZE; j++)
				{
					uint32_t column = greedyPlane->slicesXZ[i * CHUNK_SIZE + j + CHUNK_SIZE * CHUNK_SIZE];

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
							uint32_t trimmedNextCol = (greedyPlane->slicesXZ[i * CHUNK_SIZE + j + sx + CHUNK_SIZE * CHUNK_SIZE] >> y) & trimmedMask;
							if (trimmedNextCol != trimmedMask)
								break;

							greedyPlane->slicesXZ[i * CHUNK_SIZE + j + sx + CHUNK_SIZE * CHUNK_SIZE] &= ~mask;
							sx++;
						}

						ivec3 position = ivec3(j, i, y);

						ChunkBuilderAddFace(mesher, position, sx, sy, 3, (uint8_t)blockType);

						y += sy;
					}
				}
			}
		}
	}
	mesher->vertexCounts[3] = mesher->numVertices - mesher->vertexOffsets[3];

	mesher->vertexOffsets[4] = mesher->numVertices;
	for (int i = 0; i < mesher->greedyPlanes.capacity; i++)
	{
		auto slot = &mesher->greedyPlanes.slots[i];
		if (slot->state == SLOT_USED)
		{
			uint8_t blockType = slot->key;
			GreedyPlane* greedyPlane = &slot->value;

			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				for (int x = 0; x < CHUNK_SIZE; x++)
				{
					uint32_t column = greedyPlane->slicesXY[z * CHUNK_SIZE + x];

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
							uint32_t trimmedNextCol = (greedyPlane->slicesXY[z * CHUNK_SIZE + x + sx] >> y) & trimmedMask;
							if (trimmedNextCol != trimmedMask)
								break;

							greedyPlane->slicesXY[z * CHUNK_SIZE + x + sx] &= ~mask;
							sx++;
						}

						ivec3 position = ivec3(x, y, z);

						ChunkBuilderAddFace(mesher, position, sx, sy, 4, (uint8_t)blockType);

						y += sy;
					}
				}
			}
		}
	}
	mesher->vertexCounts[4] = mesher->numVertices - mesher->vertexOffsets[4];

	mesher->vertexOffsets[5] = mesher->numVertices;
	for (int i = 0; i < mesher->greedyPlanes.capacity; i++)
	{
		auto slot = &mesher->greedyPlanes.slots[i];
		if (slot->state == SLOT_USED)
		{
			uint8_t blockType = slot->key;
			GreedyPlane* greedyPlane = &slot->value;

			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				for (int x = 0; x < CHUNK_SIZE; x++)
				{
					uint32_t column = greedyPlane->slicesXY[z * CHUNK_SIZE + x + CHUNK_SIZE * CHUNK_SIZE];

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
							uint32_t trimmedNextCol = (greedyPlane->slicesXY[z * CHUNK_SIZE + x + sx + CHUNK_SIZE * CHUNK_SIZE] >> y) & trimmedMask;
							if (trimmedNextCol != trimmedMask)
								break;

							greedyPlane->slicesXY[z * CHUNK_SIZE + x + sx + CHUNK_SIZE * CHUNK_SIZE] &= ~mask;
							sx++;
						}

						ivec3 position = ivec3(x, y, z);

						ChunkBuilderAddFace(mesher, position, sx, sy, 5, (uint8_t)blockType);

						y += sy;
					}
				}
			}
		}
	}
	mesher->vertexCounts[5] = mesher->numVertices - mesher->vertexOffsets[5];
}

void ChunkMesherRun(ChunkMesher* mesher, const Chunk* chunk, GameState* game)
{
	SDL_assert(chunk->isActive);

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

	uint64_t before = SDL_GetTicksNS();

	mesher->numVertices = 0;
	InitHashMap(&mesher->greedyPlanes);
	//mesher->greedyPlanes.clear();

	//chunk->vertexOffset = 0;
	//for (int i = 0; i < 6; i++)
	//	BuildGreedyFaces(mesher, i, chunk, allocator, game);
	//chunk->vertexCount = mesher->numVertices;

	const Chunk* neighbors[6];
	uint32_t neighborFlags[6];

	neighbors[0] = GetChunkAtWorldPosWithLOD(chunk->position + ivec3::Left * CHUNK_SIZE * chunk->chunkScale, chunk->lod, game);
	neighbors[1] = GetChunkAtWorldPosWithLOD(chunk->position + ivec3::Right * CHUNK_SIZE * chunk->chunkScale, chunk->lod, game);
	neighbors[2] = GetChunkAtWorldPosWithLOD(chunk->position + ivec3::Down * CHUNK_SIZE * chunk->chunkScale, chunk->lod, game);
	neighbors[3] = GetChunkAtWorldPosWithLOD(chunk->position + ivec3::Up * CHUNK_SIZE * chunk->chunkScale, chunk->lod, game);
	neighbors[4] = GetChunkAtWorldPosWithLOD(chunk->position + ivec3::Forward * CHUNK_SIZE * chunk->chunkScale, chunk->lod, game);
	neighbors[5] = GetChunkAtWorldPosWithLOD(chunk->position + ivec3::Back * CHUNK_SIZE * chunk->chunkScale, chunk->lod, game);

	neighborFlags[0] = GetChunkFlagsAtWorldPos(chunk->position + ivec3::Left * CHUNK_SIZE * chunk->chunkScale, chunk->lod, game);
	neighborFlags[1] = GetChunkFlagsAtWorldPos(chunk->position + ivec3::Right * CHUNK_SIZE * chunk->chunkScale, chunk->lod, game);
	neighborFlags[2] = GetChunkFlagsAtWorldPos(chunk->position + ivec3::Down * CHUNK_SIZE * chunk->chunkScale, chunk->lod, game);
	neighborFlags[3] = GetChunkFlagsAtWorldPos(chunk->position + ivec3::Up * CHUNK_SIZE * chunk->chunkScale, chunk->lod, game);
	neighborFlags[4] = GetChunkFlagsAtWorldPos(chunk->position + ivec3::Forward * CHUNK_SIZE * chunk->chunkScale, chunk->lod, game);
	neighborFlags[5] = GetChunkFlagsAtWorldPos(chunk->position + ivec3::Back * CHUNK_SIZE * chunk->chunkScale, chunk->lod, game);

	GreedyMesh(mesher, chunk, neighbors, neighborFlags, game);

	uint64_t after = SDL_GetTicksNS();
	//SDL_Log("meshing %.2f ms", (after - before) / 1e6f);

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

	//ChunkBuilderCreateBuffers(builder, &chunk->instanceBuffer);
	//ChunkBuilderCreateBuffers(builder, &chunk->vertexBuffer, &chunk->indexBuffer);

	//chunk->needsMeshUpdate = false;
	//chunk->hasMesh = true;
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
