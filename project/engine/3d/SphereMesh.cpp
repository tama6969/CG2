#include "SphereMesh.h"
#include <cmath>

void GenerateSphereMesh(float radius, uint32_t subdivision, std::vector<VertexData>& vertices, std::vector<uint32_t>& indices) {
    vertices.clear();
    indices.clear();

    const float kPi = 3.1415926535f;
    const float kLonEvery = (2.0f * kPi) / float(subdivision);
    const float kLatEvery = kPi / float(subdivision);

    // 頂点の生成
    for (uint32_t latIndex = 0; latIndex <= subdivision; ++latIndex) {
        float lat = -kPi / 2.0f + kLatEvery * latIndex;
        for (uint32_t lonIndex = 0; lonIndex <= subdivision; ++lonIndex) {
            float lon = lonIndex * kLonEvery;

            VertexData vertex;
            vertex.position.x = radius * std::cos(lat) * std::cos(lon);
            vertex.position.y = radius * std::sin(lat);
            vertex.position.z = radius * std::cos(lat) * std::sin(lon);
            vertex.position.w = 1.0f;

          
            vertex.normal.x = std::cos(lat) * std::cos(lon);
            vertex.normal.y = std::sin(lat);
            vertex.normal.z = std::cos(lat) * std::sin(lon);

            // テクスチャを貼る場合のUV座標 
            vertex.texcoord.x = float(lonIndex) / float(subdivision);
            vertex.texcoord.y = 1.0f - (float(latIndex) / float(subdivision));

            vertices.push_back(vertex);
        }
    }

    for (uint32_t latIndex = 0; latIndex < subdivision; ++latIndex) {
        for (uint32_t lonIndex = 0; lonIndex < subdivision; ++lonIndex) {
            uint32_t startRow = latIndex * (subdivision + 1);
            uint32_t nextRow = (latIndex + 1) * (subdivision + 1);

            // 三角形1
            indices.push_back(startRow + lonIndex);
            indices.push_back(nextRow + lonIndex);
            indices.push_back(startRow + lonIndex + 1);

            // 三角形2
            indices.push_back(startRow + lonIndex + 1);
            indices.push_back(nextRow + lonIndex);
            indices.push_back(nextRow + lonIndex + 1);
        }
    }
}