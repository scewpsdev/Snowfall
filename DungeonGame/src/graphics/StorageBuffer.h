#pragma once

#include <SDL3/SDL.h>


struct StorageBuffer
{
	SDL_GPUBuffer* buffer;
	SDL_GPUTransferBuffer* transferBuffer;
	void* mappedBuffer;
};


StorageBuffer* CreateStorageBuffer(const uint8_t* data, uint32_t size, SDL_GPUCommandBuffer* cmdBuffer);
void DestroyStorageBuffer(StorageBuffer* storageBuffer);

void UpdateStorageBuffer(StorageBuffer* storageBuffer, const uint8_t* data, uint32_t size, SDL_GPUCommandBuffer* cmdBuffer);
