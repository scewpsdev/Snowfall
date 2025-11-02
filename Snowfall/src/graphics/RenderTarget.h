#pragma once

#include <SDL3/SDL.h>


struct RenderTarget
{
	SDL_GPUTexture* texture;
	int width, height;
	SDL_GPUTextureFormat format;
};


RenderTarget* CreateRenderTarget(int width, int height, SDL_GPUTextureFormat format);
void DestroyRenderTarget(RenderTarget* renderTarget);
