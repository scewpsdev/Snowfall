#pragma once

#include <stdint.h>


struct Simplex
{
	uint32_t seed;

	Simplex(uint32_t seed = 0);

	float sample1f(float x);
	float sample2f(float x, float y);
	float sample3f(float x, float y, float z);
	float sample4f(float x, float y, float z, float w);
};
