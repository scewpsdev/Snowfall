#include "Application.h"

#include "utils/StringUtils.h"
#include "math/Math.h"


GameMemory* memory;
AppState* app;
GraphicsState* graphics;
GameState* game;

SDL_Window* window;
SDL_GPUDevice* device;

int width, height;
float deltaTime;
int fps;
float avgMs;
uint64_t memoryUsage;
uint64_t transientMemoryUsage;

SDL_GPUTexture* swapchain = nullptr;
SDL_GPUCommandBuffer* cmdBuffer = nullptr;


#include "game/Game.cpp"


static SDL_GPUTexture* CreateDepthTarget(int width, int height)
{
	SDL_GPUTextureCreateInfo depthTextureInfo = {};
	depthTextureInfo.format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
	depthTextureInfo.width = width;
	depthTextureInfo.height = height;
	depthTextureInfo.layer_count_or_depth = 1;
	depthTextureInfo.num_levels = 1;
	depthTextureInfo.type = SDL_GPU_TEXTURETYPE_2D;
	depthTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
	return SDL_CreateGPUTexture(device, &depthTextureInfo);
}

extern "C" __declspec(dllexport) SDL_AppResult AppInit(GameMemory* memory, AppState* appState, int argc, char** argv)
{
	::memory = memory;
	app = (AppState*)memory->constantMemory;
	graphics = &app->graphics;
	game = &app->game;
	window = app->window;
	device = app->device;

	SDL_GetWindowSizeInPixels(window, &width, &height);

	if (appState->platformCallbacks.compileResources)
		appState->platformCallbacks.compileResources();

	app->lastFrame = SDL_GetTicksNS();
	app->lastSecond = SDL_GetTicksNS();

	SDL_GetKeyboardState(&app->numKeys);
	app->lastKeys = (bool*)BumpAllocatorCalloc(&memory->constantAllocator, app->numKeys, sizeof(bool));

	SDL_GetMouseState(&app->lastMousePosition.x, &app->lastMousePosition.y);

	app->depthTexture = CreateDepthTarget(width, height);

	cmdBuffer = SDL_AcquireGPUCommandBuffer(device);

	GameInit();

	SDL_SubmitGPUCommandBuffer(cmdBuffer);
	cmdBuffer = nullptr;

	return SDL_APP_CONTINUE;
}

void AppResize(int newWidth, int newHeight)
{
	if (app->depthTexture)
		SDL_ReleaseGPUTexture(device, app->depthTexture);
	app->depthTexture = CreateDepthTarget(newWidth, newHeight);

	width = newWidth;
	height = newHeight;
}

extern "C" __declspec(dllexport) void AppDestroy(GameMemory* memory, AppState* appState, SDL_AppResult result)
{
	SDL_Log("Shutting down...");

	::memory = memory;
	app = (AppState*)memory->constantMemory;
	graphics = &app->graphics;
	game = &app->game;
	window = app->window;
	device = app->device;

	GameDestroy();
}

extern "C" __declspec(dllexport) SDL_AppResult AppOnEvent(GameMemory* memory, AppState* appState, SDL_Event* event)
{
	if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
		return SDL_APP_SUCCESS;


	if (event->type == SDL_EVENT_WINDOW_RESIZED)
		AppResize(event->window.data1, event->window.data2);

	return SDL_APP_CONTINUE;
}

extern "C" __declspec(dllexport) void AppIterate(GameMemory* memory, AppState* appState)
{
	::memory = memory;
	app = (AppState*)memory->constantMemory;
	graphics = &app->graphics;
	game = &app->game;
	window = app->window;
	device = app->device;

	app->now = SDL_GetTicksNS();
	deltaTime = (app->now - app->lastFrame) / 1e9f;

	if (app->now - app->lastSecond >= 1e9)
	{
		fps = app->frameCounter;

		avgMs = (app->now - app->lastSecond) / 1e6f / app->frameCounter;

		app->frameCounter = 0;
		app->lastSecond = app->now;

		char memoryUsageStr[16];
		MemoryString(memoryUsageStr, 16, memoryUsage);

		char transientMemoryUsageStr[16];
		MemoryString(transientMemoryUsageStr, 16, transientMemoryUsage);

		SDL_Log("%d fps, %.3f ms | %s, %s", fps, avgMs, memoryUsageStr, transientMemoryUsageStr);
		SDL_Log("%d, %d, %d", (int)floorf(game->cameraPosition.x), (int)floorf(game->cameraPosition.y), (int)floorf(game->cameraPosition.z));

		memoryUsage = 0;
		transientMemoryUsage = 0;
	}

	app->keys = SDL_GetKeyboardState(&app->numKeys);
	app->mouseButtons = SDL_GetMouseState(&app->mousePosition.x, &app->mousePosition.y);
	SDL_GetRelativeMouseState(&app->mouseDelta.x, &app->mouseDelta.y);

	cmdBuffer = SDL_AcquireGPUCommandBuffer(device);

	Uint32 swapchainWidth, swapchainHeight;
	SDL_WaitAndAcquireGPUSwapchainTexture(cmdBuffer, app->window, &swapchain, &swapchainWidth, &swapchainHeight);

	GameUpdate();
	GameRender();

	SDL_SubmitGPUCommandBuffer(cmdBuffer);
	cmdBuffer = nullptr;
	swapchain = nullptr;

	app->frameCounter++;

	memoryUsage = max(memoryUsage, memory->constantAllocator.offset);
	transientMemoryUsage = max(transientMemoryUsage, memory->transientAllocator.offset);

	app->lastFrame = app->now;

	SDL_memcpy(app->lastKeys, app->keys, app->numKeys * sizeof(bool));
	app->lastMousePosition = app->mousePosition;
	app->lastMouseButtons = app->mouseButtons;
}
