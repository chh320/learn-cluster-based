#include "VirtualMesh.h"
#include "timer.h"

namespace Core {
void VirtualMesh::Build(Mesh& mesh)
{
    Util::Timer timer;

    auto& vertices = mesh.vertices;
    auto& indices = mesh.indices;

    MeshSimplifier meshSimplifier(vertices.data(), vertices.size(), indices.data(), indices.size());
    meshSimplifier.Simplify(indices.size());
    vertices.resize(meshSimplifier.RemainingVertNum());
    indices.resize(meshSimplifier.RemainingTriangleNum() * 3);
    timer.log("Success simplify mesh");
    std::cerr << "After remove duplicate vertex - verts : " << vertices.size() << " tris: " << indices.size() / 3 << "\n\n";

    timer.reset();
    std::cerr << "--- Begin Build Clusters ---\n\n";
    Cluster::BuildClusters(vertices, indices, _clusters);
    timer.log("Success build clusters");
    std::cerr << "Cluster size: " << _clusters.size() << "\n\n";

    std::cerr << "--- Begin Build DAG ---\n\n";
    uint32_t levelOffset = 0, mipLevel = 0;
    while (1) {
        std::cout << "- Level: " << mipLevel << "\nClusters num is: " << _clusters.size() - levelOffset << "\n";

        auto clusterNums = _clusters.size() - levelOffset;
        if (clusterNums <= 1) {
            _clusters[_clusters.size() - 1].groupId = 0;
            break;
        }

        auto preClusterNums = _clusters.size();
        auto preGroupNums = _clusterGroups.size();

        timer.reset();
        ClusterGroup::BuildClusterGroups(_clusters, levelOffset, clusterNums, mipLevel, _clusterGroups);
        std::cout << "Group num is: " << _clusterGroups.size() - preGroupNums << "\n";
        for (auto i = preGroupNums; i < _clusterGroups.size(); i++) {
            ClusterGroup::BuildParentClusters(_clusterGroups[i], _clusters);
        }
        timer.log("Success build level " + std::to_string(mipLevel) + " DAG.");
        levelOffset = preClusterNums;
        mipLevel++;

        std::cout << std::endl;
    }
    _mipLevelNums = mipLevel + 1;

    std::cout << "\nThe total num of clusters is: " << _clusters.size() << "\n\n";
    std::cout << "--- End Process Mesh ---\n\n";
}
}