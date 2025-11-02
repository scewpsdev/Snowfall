#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL.h>


#define Kilobytes(x) ((x) * 1024LL)
#define Megabytes(x) (Kilobytes(x) * 1024LL)
#define Gigabytes(x) (Megabytes(x) * 1024LL)
#define Terabytes(x) (Gigabytes(x) * 1024LL)


struct GameMemory
{
	uint64_t constantMemorySize;
	uint8_t* constantMemory;

	uint64_t transientMemorySize;
	uint8_t* transientMemory;
};


extern "C" __declspec(dllexport) SDL_AppResult AppInit(GameMemory* memory, int argc, char** argv);
extern "C" __declspec(dllexport) void AppDestroy(GameMemory* memory, SDL_AppResult result);
extern "C" __declspec(dllexport) SDL_AppResult AppIterate(GameMemory* memory);


int main(int argc, char** argv)
{
	GameMemory memory = {};
	memory.constantMemorySize = Megabytes(64);
	memory.transientMemorySize = Megabytes(256);

	void* baseAddress = (void*)Terabytes(2);
	uint64_t totalSize = memory.constantMemorySize + memory.transientMemorySize;
	memory.constantMemory = (uint8_t*)SDL_malloc(totalSize);
	memory.transientMemory = memory.constantMemory + memory.constantMemorySize;

	SDL_AppResult result = AppInit(&memory, argc, argv);
	if (result == SDL_APP_CONTINUE)
	{
		while ((result = AppIterate(&memory)) == SDL_APP_CONTINUE)
		{
		}
	}
	AppDestroy(&memory, result);

	SDL_free(memory.constantMemory); 

	return 0;
}
