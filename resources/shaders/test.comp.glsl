#version 430 core

// Use std430 for tightly packed scalars in SSBOs and bind at index 0
layout(std430, binding = 0) buffer Data {
    int values[];
};

// One invocation per work item in X
layout(local_size_x = 1) in;

void main()
{
    uint id = gl_GlobalInvocationID.x;
    // Assumes dispatch count matches data length
    values[id] *= 3;
}