#include "IndirectBuffer.h"

#include "Application.h"

#include <SDL3/SDL.h>


extern SDL_GPUDevice* device;
extern GraphicsState* graphics;


IndirectBuffer* CreateIndirectBuffer(int maxDrawCommands, bool indexed)
{
	uint32_t drawCommandSize = indexed ? sizeof(SDL_GPUIndexedIndirectDrawCommand) : sizeof(SDL_GPUIndirectDrawCommand);

	SDL_GPUBufferCreateInfo bufferInfo = {};
	bufferInfo.size = maxDrawCommands * drawCommandSize;
	bufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDIRECT;
	SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &bufferInfo);

	SDL_GPUTransferBufferCreateInfo transferInfo = {};
	transferInfo.size = maxDrawCommands * drawCommandSize;
	transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);

	void* mappedBuffer = SDL_MapGPUTransferBuffer(device, transferBuffer, false);

	SDL_assert(graphics->numInstanceBuffers < MAX_INSTANCE_BUFFERS);

	IndirectBuffer* indirectBuffer = &graphics->indirectBuffers[graphics->numIndirectBuffers++];
	indirectBuffer->maxDrawCommands = maxDrawCommands;
	indirectBuffer->indexed = indexed;
	indirectBuffer->buffer = buffer;
	indirectBuffer->transferBuffer = transferBuffer;
	indirectBuffer->mappedBuffer = mappedBuffer;

	return indirectBuffer;
}

void UpdateIndirectBuffer(IndirectBuffer* buffer, const SDL_GPUIndirectDrawCommand* drawCommands, int numDrawCommands, SDL_GPUCommandBuffer* cmdBuffer)
{
	// TODO pack chunk vertex buffer tightly, for later chunk updates create a custom allocator that finds space in the gaps.

	uint32_t drawCommandSize = buffer->indexed ? sizeof(SDL_GPUIndexedIndirectDrawCommand) : sizeof(SDL_GPUIndirectDrawCommand);

	/*
	SDL_GPUTransferBufferCreateInfo transferInfo = {};
	transferInfo.size = numDrawCommands * drawCommandSize;
	transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
	*/

	//void* vertexData = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
	SDL_memcpy(buffer->mappedBuffer, drawCommands, numDrawCommands * drawCommandSize);
	//SDL_UnmapGPUTransferBuffer(device, transferBuffer);

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuffer);

	SDL_GPUTransferBufferLocation location = {};
	location.transfer_buffer = buffer->transferBuffer;
	location.offset = 0;

	SDL_GPUBufferRegion region = {};
	region.buffer = buffer->buffer;
	region.size = numDrawCommands * drawCommandSize;
	region.offset = 0;

	SDL_UploadToGPUBuffer(copyPass, &location, &region, false);
	SDL_EndGPUCopyPass(copyPass);
}

void DestroyIndirectBuffer(IndirectBuffer* indirectBuffer)
{
	SDL_UnmapGPUTransferBuffer(device, indirectBuffer->transferBuffer);
	indirectBuffer->mappedBuffer = nullptr;

	SDL_ReleaseGPUTransferBuffer(device, indirectBuffer->transferBuffer);

	SDL_ReleaseGPUBuffer(device, indirectBuffer->buffer);
}
