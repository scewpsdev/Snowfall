#pragma once

#include <stdint.h>


struct Simplex
{
	int perm[512];
};


void InitSimplex(Simplex* simplex);

float Simplex2f(Simplex* simplex, float x, float y);
float Simplex3f(Simplex* simplex, float x, float y, float z);
float Simplex4f(Simplex* simplex, float x, float y, float z, float w);
