#include "StorageBuffer.h"

#include "Application.h"

#include <SDL3/SDL.h>


extern SDL_GPUDevice* device;
extern GraphicsState* graphics;


StorageBuffer* CreateStorageBuffer(const uint8_t* data, uint32_t size, SDL_GPUCommandBuffer* cmdBuffer)
{
	SDL_GPUBufferCreateInfo bufferInfo = {};
	bufferInfo.size = size;
	bufferInfo.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
	SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &bufferInfo);

	SDL_GPUTransferBufferCreateInfo transferInfo = {};
	transferInfo.size = size;
	transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);

	void* mappedBuffer = SDL_MapGPUTransferBuffer(device, transferBuffer, false);

	if (data)
	{
		SDL_memcpy(mappedBuffer, data, size);

		SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuffer);

		SDL_GPUTransferBufferLocation location = {};
		location.transfer_buffer = transferBuffer;
		location.offset = 0;

		SDL_GPUBufferRegion region = {};
		region.buffer = buffer;
		region.size = size;
		region.offset = 0;

		SDL_UploadToGPUBuffer(copyPass, &location, &region, false);
		SDL_EndGPUCopyPass(copyPass);
	}

	SDL_assert(graphics->numStorageBuffers < MAX_STORAGE_BUFFERS);

	StorageBuffer* storageBuffer = &graphics->storageBuffers[graphics->numStorageBuffers++];
	storageBuffer->buffer = buffer;
	storageBuffer->transferBuffer = transferBuffer;
	storageBuffer->mappedBuffer = mappedBuffer;

	return storageBuffer;
}

void DestroyStorageBuffer(StorageBuffer* storageBuffer)
{
	SDL_UnmapGPUTransferBuffer(device, storageBuffer->transferBuffer);
	storageBuffer->mappedBuffer = nullptr;
	SDL_ReleaseGPUTransferBuffer(device, storageBuffer->transferBuffer);

	SDL_ReleaseGPUBuffer(device, storageBuffer->buffer);
}

void UpdateStorageBuffer(StorageBuffer* storageBuffer, uint32_t offset, const uint8_t* data, uint32_t size, SDL_GPUCommandBuffer* cmdBuffer)
{
	SDL_memcpy(storageBuffer->mappedBuffer, data, size);

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuffer);

	SDL_GPUTransferBufferLocation location = {};
	location.transfer_buffer = storageBuffer->transferBuffer;
	location.offset = 0;

	SDL_GPUBufferRegion region = {};
	region.buffer = storageBuffer->buffer;
	region.size = size;
	region.offset = offset;

	SDL_UploadToGPUBuffer(copyPass, &location, &region, false);
	SDL_EndGPUCopyPass(copyPass);
}
