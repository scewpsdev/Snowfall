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
extern SDL_GPUDevice* device;


void InitChunkMesher(ChunkMesher* mesher)
{
	//mesher->vertexData = (uint32_t*)BumpAllocatorCalloc(&memory->transientAllocator, vertexCapacity, sizeof(uint32_t));
	//mesher->numVertices = 0;
	//mesher->vertexCapacity = vertexCapacity;

	mesher->faceDetectShader = LoadComputeShader("res/shaders/mesh_face_detect.comp.bin");
	mesher->vertexGenShader = LoadComputeShader("res/shaders/mesh_vertex_gen.comp.bin");
	mesher->clearBufferShader = LoadComputeShader("res/shaders/clear_buffer.comp.bin");

	SDL_GPUSamplerCreateInfo samplerInfo = {};
	mesher->sampler = SDL_CreateGPUSampler(device, &samplerInfo);
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

static void GreedyMesh(ChunkMesher* mesher, const Chunk* chunk, const Chunk* neighbors[6], uint32_t neighborFlags[6])
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

static void GenerateMeshDetectFaces(ChunkMesher* mesher, Chunk* chunk, Chunk** neighbors, uint32_t* neighborFlags, SDL_GPUTexture* voxelData, SDL_GPUBuffer* outputBuffer, SDL_GPUCommandBuffer* cmdBuffer)
{
	// clear face mask buffer and block counter
	{
		SDL_GPUStorageBufferReadWriteBinding bufferBinding = {};
		bufferBinding.buffer = outputBuffer;
		bufferBinding.cycle = false;
		SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(cmdBuffer, nullptr, 0, &bufferBinding, 1);
		SDL_BindGPUComputePipeline(computePass, mesher->clearBufferShader->compute);

		SDL_DispatchGPUCompute(computePass, (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE + 1 + 255) / 256, 1, 1);

		SDL_EndGPUComputePass(computePass);
	}

	SDL_GPUStorageBufferReadWriteBinding bufferBinding = {};
	bufferBinding.buffer = outputBuffer;
	bufferBinding.cycle = false;
	SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(cmdBuffer, nullptr, 0, &bufferBinding, 1);
	SDL_BindGPUComputePipeline(computePass, mesher->faceDetectShader->compute);

	SDL_GPUTextureSamplerBinding samplerBinding = {};
	samplerBinding.texture = voxelData;
	samplerBinding.sampler = mesher->sampler;
	SDL_BindGPUComputeSamplers(computePass, 0, &samplerBinding, 1);

	struct UniformData
	{
		ivec3 chunkTextureOffset;
		int padding0;
		ivec3 neighbor0TextureOffset;
		int padding1;
		ivec3 neighbor1TextureOffset;
		int padding2;
		ivec3 neighbor2TextureOffset;
		int padding3;
		ivec3 neighbor3TextureOffset;
		int padding4;
		ivec3 neighbor4TextureOffset;
		int padding5;
		ivec3 neighbor5TextureOffset;
		int padding6;
	};
	UniformData params = {};
	params.chunkTextureOffset = chunk->getChunkTextureOffset();
	params.neighbor0TextureOffset = neighbors[0] ? neighbors[0]->getChunkTextureOffset() : neighborFlags[0] && neighborFlags[0] & CHUNK_FLAG_SOLID ? ivec3(-2) : ivec3(-1);
	params.neighbor1TextureOffset = neighbors[1] ? neighbors[1]->getChunkTextureOffset() : neighborFlags[1] && neighborFlags[1] & CHUNK_FLAG_SOLID ? ivec3(-2) : ivec3(-1);
	params.neighbor2TextureOffset = neighbors[2] ? neighbors[2]->getChunkTextureOffset() : neighborFlags[2] && neighborFlags[2] & CHUNK_FLAG_SOLID ? ivec3(-2) : ivec3(-1);
	params.neighbor3TextureOffset = neighbors[3] ? neighbors[3]->getChunkTextureOffset() : neighborFlags[3] && neighborFlags[3] & CHUNK_FLAG_SOLID ? ivec3(-2) : ivec3(-1);
	params.neighbor4TextureOffset = neighbors[4] ? neighbors[4]->getChunkTextureOffset() : neighborFlags[4] && neighborFlags[4] & CHUNK_FLAG_SOLID ? ivec3(-2) : ivec3(-1);
	params.neighbor5TextureOffset = neighbors[5] ? neighbors[5]->getChunkTextureOffset() : neighborFlags[5] && neighborFlags[5] & CHUNK_FLAG_SOLID ? ivec3(-2) : ivec3(-1);
	SDL_PushGPUComputeUniformData(cmdBuffer, 0, &params, sizeof(params));

	SDL_DispatchGPUCompute(computePass, CHUNK_SIZE / 8, CHUNK_SIZE / 8, CHUNK_SIZE / 8);

	SDL_EndGPUComputePass(computePass);
}

static void GenerateMeshGenVertices(ChunkMesher* mesher, Chunk* chunk, SDL_GPUBuffer* faceMaskBuffer, SDL_GPUBuffer* outputBuffer, SDL_GPUBuffer* claimedBuffer, SDL_GPUBuffer* indirectBuffer, TransferBuffer* indirectTransferBuffer, SDL_GPUTransferBuffer* readbackBuffer, SDL_GPUCommandBuffer* cmdBuffer)
{
	// clear claimed buffer
	{
		SDL_GPUStorageBufferReadWriteBinding bufferBinding = {};
		bufferBinding.buffer = claimedBuffer;
		bufferBinding.cycle = false;
		SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(cmdBuffer, nullptr, 0, &bufferBinding, 1);
		SDL_BindGPUComputePipeline(computePass, mesher->clearBufferShader->compute);

		SDL_DispatchGPUCompute(computePass, (CHUNK_SIZE * CHUNK_SIZE * 6 + 255) / 256, 1, 1);

		SDL_EndGPUComputePass(computePass);
	}
	// clear indirect buffer
	{
		SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuffer);

		void* mappedBuffer = SDL_MapGPUTransferBuffer(device, indirectTransferBuffer->buffer, indirectTransferBuffer->cycle);

		SDL_GPUIndirectDrawCommand defaultDrawCmd = {};
		defaultDrawCmd.num_vertices = 3;
		defaultDrawCmd.num_instances = 0;
		defaultDrawCmd.first_vertex = 0;
		defaultDrawCmd.first_instance = chunk->getVertexBufferOffset();
		SDL_memcpy(mappedBuffer, &defaultDrawCmd, sizeof(defaultDrawCmd));

		SDL_UnmapGPUTransferBuffer(device, indirectTransferBuffer->buffer);

		SDL_GPUTransferBufferLocation location = {};
		location.transfer_buffer = indirectTransferBuffer->buffer;
		location.offset = 0;

		SDL_GPUBufferRegion region = {};
		region.buffer = indirectBuffer;
		region.offset = chunk->getIndirectBufferOffset();
		region.size = sizeof(SDL_GPUIndirectDrawCommand);

		SDL_UploadToGPUBuffer(copyPass, &location, &region, false);

		SDL_EndGPUCopyPass(copyPass);
	}

	SDL_GPUStorageBufferReadWriteBinding bufferBindings[3];
	bufferBindings[0] = {};
	bufferBindings[0].buffer = outputBuffer;
	bufferBindings[0].cycle = false;
	bufferBindings[1] = {};
	bufferBindings[1].buffer = claimedBuffer;
	bufferBindings[1].cycle = false;
	bufferBindings[2] = {};
	bufferBindings[2].buffer = indirectBuffer;
	bufferBindings[2].cycle = false;
	SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(cmdBuffer, nullptr, 0, bufferBindings, 3);
	SDL_BindGPUComputePipeline(computePass, mesher->vertexGenShader->compute);

	SDL_BindGPUComputeStorageBuffers(computePass, 0, &faceMaskBuffer, 1);

	struct UniformData
	{
		int id;
		int vertexBufferOffset;
		ivec2 padding;
	};
	UniformData params = {};
	params.id = chunk->id;
	params.vertexBufferOffset = chunk->getVertexBufferOffset();
	SDL_PushGPUComputeUniformData(cmdBuffer, 0, &params, sizeof(params));

	SDL_DispatchGPUCompute(computePass, 1, 1, 6);

	SDL_EndGPUComputePass(computePass);

	// read back vertex count

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuffer);

	SDL_GPUBufferRegion src = {};
	src.buffer = indirectBuffer;
	src.offset = chunk->getIndirectBufferOffset();
	src.size = sizeof(SDL_GPUIndirectDrawCommand);

	SDL_GPUTransferBufferLocation dst = {};
	dst.transfer_buffer = readbackBuffer;
	dst.offset = 0;

	SDL_DownloadFromGPUBuffer(copyPass, &src, &dst);

	src.buffer = faceMaskBuffer;
	src.offset = 0;
	src.size = sizeof(uint32_t);

	dst.transfer_buffer = readbackBuffer;
	dst.offset = sizeof(SDL_GPUIndirectDrawCommand);

	SDL_DownloadFromGPUBuffer(copyPass, &src, &dst);

	SDL_EndGPUCopyPass(copyPass);
}

void ChunkMesherRun(ChunkMesher* mesher, ChunkGeneratorThreadData* threadData, Chunk* chunk, Chunk** neighbors, uint32_t* neighborFlags, SDL_GPUTransferBuffer* readbackBuffer, SDL_GPUCommandBuffer* cmdBuffer)
{
	SDL_assert(chunk->isActive);

	uint64_t before = SDL_GetTicksNS();

	/*
	mesher->numVertices = 0;
	InitHashMap(&mesher->greedyPlanes);

	GreedyMesh(mesher, chunk, neighbors, neighborFlags);
	*/

	GenerateMeshDetectFaces(mesher, chunk, neighbors, neighborFlags, threadData->game->chunkTexture, threadData->faceMaskBuffer, cmdBuffer);
	GenerateMeshGenVertices(mesher, chunk, threadData->faceMaskBuffer, threadData->game->chunkVertexBuffer->buffer, threadData->claimedFaceBuffer, threadData->game->chunkIndirectBuffer->buffer, threadData->chunkIndirectTransferBuffer, readbackBuffer, cmdBuffer);

	uint64_t after = SDL_GetTicksNS();
	//SDL_Log("meshing %d,%d,%d %.2f ms", chunk->gridPosition.x, chunk->gridPosition.y, chunk->gridPosition.z, (after - before) / 1e6f);
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
