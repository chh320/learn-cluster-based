#pragma once

#include "Cluster.h"
#include "Mesh.h"
#include <vector>


namespace Core {
class VirtualMesh final {
public:
    void Build(Mesh& mesh);
    //void Compact(Mesh& mesh);

    const std::vector<Cluster>& GetClusters() const { return _clusters; }
    const std::vector<ClusterGroup>& GetClusterGroups() const { return _clusterGroups; }
    const uint32_t& GetMipLevelNums() const { return _mipLevelNums; }

private:
    std::vector<Cluster> _clusters;
    std::vector<ClusterGroup> _clusterGroups;
    uint32_t _mipLevelNums;
};
}