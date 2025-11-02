#pragma once

#include <SDL3/SDL.h>


struct IndirectBuffer
{
	int maxDrawCommands;
	bool indexed;
	SDL_GPUBuffer* buffer;
	SDL_GPUTransferBuffer* transferBuffer;
	void* mappedBuffer;
};


IndirectBuffer* CreateIndirectBuffer(int maxDrawCommands, bool indexed);
void UpdateIndirectBuffer(IndirectBuffer* buffer, const SDL_GPUIndirectDrawCommand* drawCommands, int numDrawCommands, SDL_GPUCommandBuffer* cmdBuffer);
void DestroyIndirectBuffer(IndirectBuffer* indirectBuffer);