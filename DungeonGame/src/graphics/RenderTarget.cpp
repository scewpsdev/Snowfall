#include "RenderTarget.h"

#include "Application.h"


extern SDL_GPUDevice* device;
extern GraphicsState* graphics;


RenderTarget* CreateRenderTarget(int width, int height, SDL_GPUTextureFormat format)
{
	SDL_GPUTextureCreateInfo textureInfo = {};
	textureInfo.type = SDL_GPU_TEXTURETYPE_2D;          /**< The base dimensionality of the texture. */
	textureInfo.format = format;      /**< The pixel format of the texture. */
	textureInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;   /**< How the texture is intended to be used by the client. */
	textureInfo.width = width;                     /**< The width of the texture. */
	textureInfo.height = height;                    /**< The height of the texture. */
	textureInfo.layer_count_or_depth = 1;      /**< The layer count or depth of the texture. This value is treated as a layer count on 2D array textures, and as a depth value on 3D textures. */
	textureInfo.num_levels = 1;                /**< The number of mip levels in the texture. */
	textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;  /**< The number of samples per texel. Only applies if the texture is used as a render target. */

	SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &textureInfo);

	SDL_assert(graphics->numRenderTargets < MAX_RENDER_TARGETS);

	RenderTarget* renderTarget = &graphics->renderTargets[graphics->numRenderTargets++];

	renderTarget->texture = texture;
	renderTarget->width = width;
	renderTarget->height = height;
	renderTarget->format = format;

	return renderTarget;
}

void DestroyRenderTarget(RenderTarget* renderTarget)
{
	SDL_ReleaseGPUTexture(device, renderTarget->texture);
}
