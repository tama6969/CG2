#pragma once
#include <vector>
#include <cstdint>
#include <d3d12.h> 
#include "MatrixMath.h"
struct Sphere
{
    Vector3 center; 
    float radius;   
};

void GenerateSphereMesh(
    float radius,
    uint32_t subdivision,
    std::vector<VertexData>& vertices,
    std::vector<uint32_t>& indices
);