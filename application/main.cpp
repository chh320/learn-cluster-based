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
    config.width = 1280;
    config.height = 800;
    config.isWireFrame = false;
    config.useInstance = true;

    bool isRebuildVirtualMesh = false;

    Util::Timer timer;
    const std::string modelFileName = "../assets/models/sphere2.obj";
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

        // simplify mesh
        timer.reset();
        if (!mesh.SimplifyMesh()) {
            return -1;
        }
        timer.log("Success simplify mesh");
        std::cerr << "After remove duplicate vertex - verts : " << mesh.vertices.size() << " tris: " << mesh.indices.size() / 3 << "\n\n";

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