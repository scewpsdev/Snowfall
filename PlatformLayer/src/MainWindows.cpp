#include <Windows.h>
#undef min
#undef max

#include <SDL3/SDL.h>

#include "Application.h"

#include "utils/BumpAllocator.h"


#define GAME_NAME "Snowfall"

#define GAME_CODE_DLL GAME_NAME ".dll"

#define Kilobytes(x) ((x) * 1024LL)
#define Megabytes(x) (Kilobytes(x) * 1024LL)
#define Gigabytes(x) (Megabytes(x) * 1024LL)
#define Terabytes(x) (Gigabytes(x) * 1024LL)


typedef SDL_AppResult(*GameInit_t)(GameMemory* memory, AppState* appState, int argc, char** argv);
typedef void(*GameDestroy_t)(GameMemory* memory, AppState* appState, SDL_AppResult result);
typedef void(*GameIterate_t)(GameMemory* memory, AppState* appState);
typedef SDL_AppResult(*GameOnEvent_t)(GameMemory* memory, AppState* appState, SDL_Event* ev);


SDL_Time GetWriteTime(const char* path)
{
	SDL_PathInfo pathInfo = {};
	if (SDL_GetPathInfo(path, &pathInfo))
		return pathInfo.modify_time;
	else
	{
		SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "%s", SDL_GetError());
		return 0;
	}
}

static SDL_SharedObject* LoadGameCode(GameInit_t* init, GameDestroy_t* destroy, GameIterate_t* iterate, GameOnEvent_t* onEvent)
{
	char tmpPath[256];
	SDL_snprintf(tmpPath, 256, GAME_NAME "-%llu.dll", __rdtsc());
	bool copySuccessful = false;
	for (int i = 0; i < 50; i++)
	{
		if (SDL_CopyFile(GAME_CODE_DLL, tmpPath))
		{
			copySuccessful = true;
			break;
		}
		Sleep(10);
		SDL_Log("retrying");
	}
	if (!copySuccessful)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to copy game code module: %s", SDL_GetError());
		return nullptr;
	}

	SDL_SharedObject* gameCode = SDL_LoadObject(tmpPath);
	if (!gameCode)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load game code module: %s", SDL_GetError());
		return nullptr;
	}

	*init = (GameInit_t)SDL_LoadFunction(gameCode, "AppInit");
	*destroy = (GameDestroy_t)SDL_LoadFunction(gameCode, "AppDestroy");
	*iterate = (GameIterate_t)SDL_LoadFunction(gameCode, "AppIterate");
	*onEvent = (GameOnEvent_t)SDL_LoadFunction(gameCode, "AppOnEvent");
	if (!*init)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load AppInit: %s", SDL_GetError());
		return nullptr;
	}
	if (!*destroy)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load AppDestroy: %s", SDL_GetError());
		return nullptr;
	}
	if (!*iterate)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load AppIterate: %s", SDL_GetError());
		return nullptr;
	}
	if (!*onEvent)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load AppOnEvent: %s", SDL_GetError());
		return nullptr;
	}

	return gameCode;
}

static void CompileResources()
{
	system("D:\\Dev\\Rainfall\\RainfallResourceCompiler\\bin\\x64\\Release\\RainfallResourceCompiler.exe " PROJECT_PATH "\\res res png ogg vsh fsh csh glsl ttf rfs gltf");
}

static void InitPlatformCallbacks(PlatformCallbacks* callbacks)
{
	callbacks->compileResources = CompileResources;
}

int main(int argc, char** argv)
{
	GameMemory memory = {};
	memory.constantMemorySize = Gigabytes(2);
	memory.transientMemorySize = Megabytes(256);

	void* baseAddress = (void*)Terabytes(2);
	uint64_t totalSize = memory.constantMemorySize + memory.transientMemorySize;
	memory.constantMemory = (uint8_t*)VirtualAlloc(baseAddress, totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
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

	bool gpuDebug = false;
#if _DEBUG
	gpuDebug = true;
#endif

	SDL_GPUDevice* device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, gpuDebug, nullptr);
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

	SDL_Log("Loading game code\n");

	GameInit_t init = nullptr;
	GameDestroy_t destroy = nullptr;
	GameIterate_t iterate = nullptr;
	GameOnEvent_t onEvent = nullptr;

	SDL_SharedObject* gameCode = LoadGameCode(&init, &destroy, &iterate, &onEvent);
	if (!gameCode)
		return 1;

	SDL_Time lastGameCodeWrite = GetWriteTime(GAME_CODE_DLL);

	SDL_AppResult result = init(&memory, appState, argc, argv);

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
				SDL_AppResult result = onEvent(&memory, appState, &event);
				if (result != SDL_APP_CONTINUE)
					running = false;
			}

			iterate(&memory, appState);

			ResetBumpAllocator(&memory.transientAllocator);

			SDL_Time gameCodeWriteTime = GetWriteTime(GAME_CODE_DLL);
			if (gameCodeWriteTime > lastGameCodeWrite)
			{
				SDL_Log("Reloading game code\n");

				SDL_UnloadObject(gameCode);
				if (SDL_SharedObject* newGameCode = LoadGameCode(&init, &destroy, &iterate, &onEvent))
					gameCode = newGameCode;

				lastGameCodeWrite = gameCodeWriteTime;
			}
		}
	}

	destroy(&memory, appState, result);

	VirtualFree(memory.constantMemory, 0, MEM_RELEASE);

	SDL_UnloadObject(gameCode);

	SDL_DestroyGPUDevice(device);
	SDL_DestroyWindow(window);

	SDL_Quit();

	system("del " GAME_NAME "-*.pdb");
	system("del " GAME_NAME "-*.dll");

	return 0;
}
