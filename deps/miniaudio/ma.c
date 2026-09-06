// Single translation unit that compiles the miniaudio implementation exactly
// once into the `miniaudio` static library. miniaudio is a public-domain /
// MIT single-file C library (mackron) — no external dependencies.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
