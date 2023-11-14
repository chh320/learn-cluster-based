#pragma once

#include "Bound.h"
#include "Util.h"
#include "VirtualMesh.h"
#include <co/fs.h>
#include <iostream>
#include <stdint.h>
#include <string>
#include <timer.h>
#include <vector>

namespace Core {
class Encode final {
public:
    static void PackingMeshData(const std::string& modelFileName, const VirtualMesh& vmesh, std::vector<uint32_t>& packedData)
    {
        Util::Timer timer;

        packedData.push_back(vmesh.GetClusters().size());           // clusters num
        packedData.push_back(vmesh.GetClusterGroups().size());      // groups num
        packedData.push_back(0);                                    // group data offset
        packedData.push_back(0);

        for (auto& cluster : vmesh.GetClusters()) {
            packedData.push_back(cluster.verts.size());         // vertex nums
            packedData.push_back(0);                            // vertex data offset
            packedData.push_back(cluster.indices.size() / 3);   // triangle nums
            packedData.push_back(0);                            // vertex id data offset

            packedData.push_back(Util::Float2Uint(cluster.sphereBounds.center.x));
            packedData.push_back(Util::Float2Uint(cluster.sphereBounds.center.y));
            packedData.push_back(Util::Float2Uint(cluster.sphereBounds.center.z));
            packedData.push_back(Util::Float2Uint(cluster.sphereBounds.radius));

            packedData.push_back(Util::Float2Uint(cluster.lodBounds.center.x));
            packedData.push_back(Util::Float2Uint(cluster.lodBounds.center.y));
            packedData.push_back(Util::Float2Uint(cluster.lodBounds.center.z));
            packedData.push_back(Util::Float2Uint(cluster.lodBounds.radius));

            Sphere parentLodBounds = vmesh.GetClusterGroups()[cluster.groupId].lodBounds;
            float maxParentLodError = vmesh.GetClusterGroups()[cluster.groupId].maxParentLodError;

            packedData.push_back(Util::Float2Uint(parentLodBounds.center.x));
            packedData.push_back(Util::Float2Uint(parentLodBounds.center.y));
            packedData.push_back(Util::Float2Uint(parentLodBounds.center.z));
            packedData.push_back(Util::Float2Uint(parentLodBounds.radius));

            packedData.push_back(Util::Float2Uint(cluster.lodError));
            packedData.push_back(Util::Float2Uint(maxParentLodError));
            packedData.push_back(cluster.groupId);
            packedData.push_back(cluster.mipLevel);
        }

        packedData[2] = packedData.size();
        for (auto& group : vmesh.GetClusterGroups()) {
            packedData.push_back(group.clusters.size());    // group cluster num
            packedData.push_back(0);                        // group cluster offset
            packedData.push_back(Util::Float2Uint(group.maxParentLodError));
            packedData.push_back(0);

            packedData.push_back(Util::Float2Uint(group.lodBounds.center.x));
            packedData.push_back(Util::Float2Uint(group.lodBounds.center.y));
            packedData.push_back(Util::Float2Uint(group.lodBounds.center.z));
            packedData.push_back(Util::Float2Uint(group.lodBounds.radius));
        }

        auto i = 0;
        for (auto& cluster : vmesh.GetClusters()) {
            auto offset = 4 + 20 * i;
            packedData[offset + 1] = packedData.size();
            for (auto& v : cluster.verts) {
                packedData.push_back(Util::Float2Uint(v.x));
                packedData.push_back(Util::Float2Uint(v.y));
                packedData.push_back(Util::Float2Uint(v.z));
            }
            packedData[offset + 3] = packedData.size();
            for (auto i = 0; i < cluster.indices.size() / 3; i++) {
                auto i0 = cluster.indices[i * 3 + 0];
                auto i1 = cluster.indices[i * 3 + 1];
                auto i2 = cluster.indices[i * 3 + 2];
                assert(i0 < 256 && i1 < 256 && i2 < 256);

                auto triCornersId = (i0 | (i1 << 8) | (i2 << 16));
                packedData.push_back(triCornersId);
            }
            i++;
        }

        i = 0;
        for (auto& group : vmesh.GetClusterGroups()) {
            uint32_t offset = packedData[2] + 8 * i;
            packedData[offset + 1] = packedData.size();
            for (auto clusterId : group.clusters) {
                packedData.push_back(clusterId);
            }
            i++;
        }

        std::cout << "size: " << packedData.size() * 4 << " bytes\n";
        timer.log("Success pack mesh data");

        timer.reset();
        std::string packedfileName = modelFileName.substr(0, modelFileName.find_last_of('.')) + ".txt";
        fs::file file(packedfileName.c_str(), 'w');
        file.write(packedData.data(), packedData.size() * sizeof(uint32_t));
        timer.log("Success write to file");
    }

    static bool LoadingMeshData(const std::string& packedFileName, std::vector<uint32_t>& packedData)
    {
        fs::file file(packedFileName.c_str(), 'r');
        if (!file) {
            return false; // there is no packed file.
        }
        packedData.resize(file.size() / sizeof(uint32_t));

        Util::Timer timer;
        std::cout << "Loading packed mesh data ...\n";
        file.read(packedData.data(), file.size());
        timer.log("Success load packed mesh data");
        std::cerr << "Cluster nums : " << packedData[0] << "\n" << "Group nums : " << packedData[1] << "\n\n";

        return true;
    }
};
}