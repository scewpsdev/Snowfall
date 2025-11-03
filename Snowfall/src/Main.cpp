#include <SDL3/SDL.h>

#include "Application.h"

#include "utils/BumpAllocator.h"


#define GAME_CODE_DLL "Snowfall.dll"

#define Kilobytes(x) ((x) * 1024LL)
#define Megabytes(x) (Kilobytes(x) * 1024LL)
#define Gigabytes(x) (Megabytes(x) * 1024LL)
#define Terabytes(x) (Gigabytes(x) * 1024LL)


extern "C" __declspec(dllexport) SDL_AppResult AppInit(GameMemory * memory, AppState * appState, int argc, char** argv);
extern "C" __declspec(dllexport) void AppDestroy(GameMemory * memory, AppState * appState, SDL_AppResult result);
extern "C" __declspec(dllexport) SDL_AppResult AppOnEvent(GameMemory * memory, AppState * appState, SDL_Event * event);
extern "C" __declspec(dllexport) void AppIterate(GameMemory * memory, AppState * appState);


static void InitPlatformCallbacks(PlatformCallbacks* callbacks)
{
	callbacks->compileResources = nullptr;
}

int main(int argc, char** argv)
{
	GameMemory memory = {};
	memory.constantMemorySize = Gigabytes(2);
	memory.transientMemorySize = Megabytes(256);

	uint64_t totalSize = memory.constantMemorySize + memory.transientMemorySize;
	memory.constantMemory = (uint8_t*)SDL_malloc(totalSize);
	SDL_memset(memory.constantMemory, 0, totalSize);

	memory.transientMemory = memory.constantMemory + memory.constantMemorySize;

	InitBumpAllocator(&memory.constantAllocator, memory.constantMemory, memory.constantMemorySize);
	InitBumpAllocator(&memory.transientAllocator, memory.transientMemory, memory.transientMemorySize);

	AppState* appState = (AppState*)(BumpAllocatorMalloc(&memory.constantAllocator, sizeof(AppState)));
	InitPlatformCallbacks(&appState->platformCallbacks);

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
	{
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize SDL: %s", SDL_GetError());
		return 1;
	}

	SDL_Log("SDL %s", SDL_GetRevision());

	const char* title = "abc";
	int width = 1280;
	int height = 720;
	SDL_Window* window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
	if (!window)
	{
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window: %s", SDL_GetError());
		return 1;
	}
	SDL_Log("Display %dx%d", width, height);

	SDL_GPUDevice* device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, nullptr);
	if (!device)
	{
		SDL_LogCritical(SDL_LOG_CATEGORY_GPU, "Failed to create graphics device: %s", SDL_GetError());
		return 1;
	}
	if (!SDL_ClaimWindowForGPUDevice(device, window))
	{
		SDL_LogCritical(SDL_LOG_CATEGORY_GPU, "Failed to create swapchain: %s", SDL_GetError());
		return 1;
	}

	SDL_Log("Video driver %s-%s", SDL_GetCurrentVideoDriver(), SDL_GetGPUDeviceDriver(device));

	SDL_GPUSwapchainComposition swapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
	SDL_GPUPresentMode presentMode = SDL_GPU_PRESENTMODE_IMMEDIATE;
	if (!SDL_SetGPUSwapchainParameters(device, window, swapchainComposition, presentMode))
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to set swapchain parameters: %s", SDL_GetError());
	}

	appState->window = window;
	appState->device = device;

	SDL_AppResult result = AppInit(&memory, appState, argc, argv);

	if (!SDL_ShowWindow(window))
	{
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
		return 1;
	}

	SDL_Log("Initialization complete");


	if (result == SDL_APP_CONTINUE)
	{
		bool running = true;
		while (running)
		{
			SDL_Event event = {};
			while (SDL_PollEvent(&event))
			{
				SDL_AppResult result = AppOnEvent(&memory, appState, &event);
				if (result != SDL_APP_CONTINUE)
					running = false;
			}

			AppIterate(&memory, appState);

			ResetBumpAllocator(&memory.transientAllocator);
		}
	}

	AppDestroy(&memory, appState, result);

	SDL_free(memory.constantMemory);

	SDL_DestroyGPUDevice(device);
	SDL_DestroyWindow(window);

	SDL_Quit();

	return 0;
}
