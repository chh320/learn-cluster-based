#pragma once
#include <assert.h>

#include <glm/glm.hpp>

#include <iostream>
#include <string>
#include <vector>

#include "HashTable.h"
#include "Util.h"

namespace Core {
class Mesh final {
public:
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;

    bool LoadMesh(std::string filePath);
    bool SimplifyMesh();
};
}
