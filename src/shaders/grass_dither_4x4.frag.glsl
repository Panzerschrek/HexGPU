#version 450

#extension GL_GOOGLE_include_directive : require

#define DITHER_FUNC AlphaDither4x4
#include "inc/base_grass.frag.glsl"
