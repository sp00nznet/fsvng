// Browser build: WebGL 2 is OpenGL ES 3.0, and there's nothing to load (see cmake/Web.cmake).
#pragma once
#include <GLES3/gl3.h>

typedef void* (*GLADloadfunc)(const char* name);
static inline int gladLoadGL(GLADloadfunc) { return 1; }
