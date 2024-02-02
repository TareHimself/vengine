#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "./ui.glsl"

float mapRangeUnClamped(float value, float fromMin, float fromMax, float toMin, float toMax) {

    // Calculate the normalized position of the value in the input range
    float normalizedPosition = (value - fromMin) / (fromMax - fromMin);

    // Map the normalized position to the output range [toMin, toMax]
    return mix(toMin, toMax, normalizedPosition);
}

layout(location = 0) out vec2 oUV;

//push constants block
layout( push_constant ) uniform constants
{
	vec4 extent;
    vec4 time;
} pRect;

void main() 
{
    vec2 screenRes = ui.viewport.zw;
    
	// Transform vertices based on input extent
    vec4 extent = pRect.extent;
    float xLeft = mapRangeUnClamped(extent.x,0.0,screenRes.x,-1.0,1.0);
    float xRight = mapRangeUnClamped(extent.x + extent.z,0.0,screenRes.x,-1.0,1.0);
    float yBottom = mapRangeUnClamped(extent.y + extent.w,0.0,screenRes.y,-1.0,1.0);
    float yTop = mapRangeUnClamped(extent.y,0.0,screenRes.y,-1.0,1.0);
    float isLeft = float (gl_VertexIndex == 0 || gl_VertexIndex == 3 || gl_VertexIndex == 5);
    float isTop = float (gl_VertexIndex == 0 || gl_VertexIndex == 1 || gl_VertexIndex == 3);
    vec2 vertex = vec2(mix(xLeft,xRight,isLeft),mix(yTop,yBottom,isTop));//vertices[gl_VertexIndex];

    gl_Position = vec4(vertex, 0.0, 1.0);
    
    oUV = vec2(mix(0.0,1.0,isLeft),mix(0.0,1.0,isTop));
}