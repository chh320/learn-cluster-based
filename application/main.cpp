#include "Encode.h"
#include "Mesh.h"
#include "VirtualMesh.h"
#include "Application.h"
#include "timer.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    Vk::RenderConfig config {};
    config.width = 1920;
    config.height = 1080;
    config.isWireFrame = false;
    config.useInstance = true;
    config.instanceXYZ = glm::vec3(1, 1, 1);
    config.maxMipSize = 1024;

    bool isRebuildVirtualMesh = false;

    Util::Timer timer;
    const std::string modelFileName = "../assets/models/happy_vrip.ply";
    std::string packedfileName = modelFileName.substr(0, modelFileName.find_last_of('.')) + ".txt";
    std::vector<uint32_t> packedData;

    if (isRebuildVirtualMesh || !Core::Encode::LoadingMeshData(packedfileName, packedData)) {
        // Generate cluster-based DAG ------------------
        // load mesh
        timer.reset();
        std::cerr << "--- Begin Loading Mesh ---\n\n";
        Core::Mesh mesh;

        bool isLoadMesh = mesh.LoadMesh(modelFileName);
        if (!isLoadMesh) {
            return -1;
        }
        timer.log("Success loading mesh");
        std::cerr << "After loading - verts : " << mesh.vertices.size() << " tris: " << mesh.indices.size() / 3 << "\n\n";

        // build virtual mesh
        Core::VirtualMesh vmesh;
        vmesh.Build(mesh);

        Core::Encode::PackingMeshData(modelFileName, vmesh, packedData);
        std::cout << std::endl;
    }

    // Render -----------------------------------------
#pragma region render
    timer.reset();
    Vk::Application renderer(config);
    timer.log("Success init vulkan");

    // renderer.Run(mesh, vmesh);
    renderer.Run(packedData);
#pragma endregion
    return 0;
}