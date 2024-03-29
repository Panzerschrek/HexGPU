#version 450

#extension GL_GOOGLE_include_directive : require

#define DITHER_FUNC AlphaDither2x2
#include "inc/base_world.frag.glsl"
