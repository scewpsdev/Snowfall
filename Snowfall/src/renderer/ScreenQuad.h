#pragma once

#include "graphics/VertexBuffer.h"
#include "graphics/Shader.h"

#include <SDL3/SDL.h>


struct ScreenQuad
{
	SDL_GPUGraphicsPipeline* pipeline;
	SDL_GPUSampler* sampler;

	VertexBuffer* vertexBuffer;
	Shader* shader;
};


void InitScreenQuad(ScreenQuad* quad, SDL_GPUCommandBuffer* cmdBuffer);
void DestroyScreenQuad(ScreenQuad* quad);
void RenderScreenQuad(ScreenQuad* quad, SDL_GPUTexture* texture, SDL_GPUCommandBuffer* cmdBuffer);
