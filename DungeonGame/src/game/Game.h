#pragma once

#include "math/Vector.h"
#include "math/Quaternion.h"

#include <SDL3/SDL.h>


struct GameState
{
	bool mouseLocked;
	vec3 cameraPosition;
	float cameraPitch, cameraYaw;
	Quaternion cameraRotation;
};
